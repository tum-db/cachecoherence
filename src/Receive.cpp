//
// Created by Magdalena Pröbstl on 2019-06-15.
//


#include "Node.h"
#include "../util/socket/tcp.h"

l5::util::Socket Node::connectServerSocket() {
    auto remoteAddr = rdma::Address{network.getGID(), rcqp.getQPN(), network.getLID()};
    l5::util::tcp::listen(socket);
    auto acced = l5::util::tcp::accept(socket);
    l5::util::tcp::write(acced, &remoteAddr, sizeof(remoteAddr));
    l5::util::tcp::read(acced, &remoteAddr, sizeof(remoteAddr));
    rcqp.connect(remoteAddr);
    return acced;
}

bool Node::receive(l5::util::Socket *acced) {
    auto &cq = network.getSharedCompletionQueue();
    auto recvbuf = malloc(defs::BIGBADBUFFER_SIZE * 2);
    auto recvmr = network.registerMr(recvbuf, defs::BIGBADBUFFER_SIZE * 2,
                                     {ibv::AccessFlag::LOCAL_WRITE, ibv::AccessFlag::REMOTE_WRITE});
    auto remoteMr = ibv::memoryregion::RemoteAddress{reinterpret_cast<uintptr_t>(recvbuf),
                                                     recvmr->getRkey()};
    l5::util::tcp::write(*acced, &remoteMr, sizeof(remoteMr));
    l5::util::tcp::read(*acced, &remoteMr, sizeof(remoteMr));
    auto recv = ibv::workrequest::Recv{};
    recv.setSge(nullptr, 0);
    rcqp.postRecvRequest(recv);
    auto wc = cq.pollRecvWorkCompletionBlocking();
    auto immData = wc.getImmData();
    std::cout << "got this immdata: " << immData << std::endl;
    switch (immData) {
        case defs::IMMDATA::MALLOC:  //immdata = 1, if it comes from another server
        {
            handleAllocation(recvbuf, remoteMr, &cq);
            return true;
        }
        case defs::IMMDATA::READ: //immdata = 2, if it is a read
        {
            handleRead(recvbuf, remoteMr, &cq);
            return true;
        }
        case defs::IMMDATA::FREE://immdata = 3, it should be freed
        {
            handleFree(recvbuf, remoteMr, &cq);
            return true;
        }
        case defs::IMMDATA::WRITE:  //immdata = 4, write
        {
            auto shouldbeinvalidated = handleWrite(recvbuf, remoteMr, &cq);
            return shouldbeinvalidated;
        }
        case defs::IMMDATA::LOCKS:  //immdata = 5, save lock
        {
            handleLocks(recvbuf, remoteMr, &cq);
            return true;
        }
        case defs::IMMDATA::RESET: //immdata = 6, reset state
        {
            std::cout << "RESETING NODES!!!!" << std::endl;
            rcqp.setToResetState();
            *acced = connectServerSocket();
            return true;
        }
        case defs::IMMDATA::INVALIDATE: {
            std::cout << "now Invalidation" << std::endl;
            handleInvalidation(recvbuf, remoteMr, &cq);

            return false;
        }
        default: {
            return true;
        }
    }
}

void Node::connectAndReceive() {
    l5::util::tcp::bind(socket, defs::port);
    auto acced = connectServerSocket();
    bool connected = true;
    while (connected) {
        connected = receive(&acced);
    }
}

void Node::handleAllocation(void *recvbuf, ibv::memoryregion::RemoteAddress remoteAddr,
                            rdma::CompletionQueuePair *cq) {
    auto sga = reinterpret_cast<defs::SendGlobalAddr *>(recvbuf);
    auto gaddr = defs::GlobalAddress(*sga);
    auto newgaddr = Malloc(&gaddr.size).sendable();
    auto sendmr = network.registerMr(&newgaddr, sizeof(defs::GlobalAddress), {});
    std::cout << newgaddr.size << ", " << newgaddr.id << ", " << newgaddr.ptr
              << std::endl;
    auto write = defs::createWriteWithImm(sendmr->getSlice(), remoteAddr, defs::IMMDATA::DEFAULT);
    rcqp.postWorkRequest(write);
    cq->pollSendCompletionQueueBlocking(ibv::workcompletion::Opcode::RDMA_WRITE);
}

void Node::handleFree(void *recvbuf, ibv::memoryregion::RemoteAddress
remoteAddr, rdma::CompletionQueuePair *cq) {
    auto sga = reinterpret_cast<defs::SendGlobalAddr *>(recvbuf);
    auto gaddr = defs::GlobalAddress(*sga);
    auto res = Free(gaddr).sendable();
    auto sendmr = network.registerMr(&res, sizeof(defs::SendGlobalAddr), {});
    auto write = defs::createWriteWithImm(sendmr->getSlice(), remoteAddr, defs::IMMDATA::DEFAULT);
    rcqp.postWorkRequest(write);
    cq->pollSendCompletionQueueBlocking(ibv::workcompletion::Opcode::RDMA_WRITE);
}

void Node::handleLocks(void *recvbuf, ibv::memoryregion::RemoteAddress remoteAddr,
                       rdma::CompletionQueuePair *cq) {
    auto l = reinterpret_cast<defs::Lock *>(recvbuf);
    auto lock = setLock(l->id, l->state);
    auto sendmr = network.registerMr(&lock, sizeof(bool), {});
    auto write = defs::createWriteWithImm(sendmr->getSlice(), remoteAddr, defs::IMMDATA::DEFAULT);
    rcqp.postWorkRequest(write);
    cq->pollSendCompletionQueueBlocking(ibv::workcompletion::Opcode::RDMA_WRITE);
}

void Node::handleRead(void *recvbuf, ibv::memoryregion::RemoteAddress remoteAddr,
                      rdma::CompletionQueuePair *cq) {
    auto sga = reinterpret_cast<defs::SendGlobalAddr *>(recvbuf);
    auto gaddr = defs::GlobalAddress(*sga);
    auto data = performRead(gaddr);
    data->iscached = defs::CACHE_DIRECTORY_STATE::SHARED;
    std::cout << "datasize: " << sizeof(data->data) << ", data: " << data << std::endl;
    auto sendmr = network.registerMr(&data->data, sizeof(uint64_t), {});
    auto write = defs::createWriteWithImm(sendmr->getSlice(), remoteAddr, defs::IMMDATA::DEFAULT);
    rcqp.postWorkRequest(write);
    cq->pollSendCompletionQueueBlocking(ibv::workcompletion::Opcode::RDMA_WRITE);

}

bool Node::handleWrite(void *recvbuf, ibv::memoryregion::RemoteAddress remoteAddr,
                       rdma::CompletionQueuePair *cq) {
    auto senddata = reinterpret_cast<defs::SendingData *>(recvbuf);
    std::cout << "Write, SendData: data: " << senddata->data << ", ga-ID: " << senddata->sga.id
              << ", ga-size:" << senddata->sga.size << ", ptr: " << senddata->sga.ptr << ", size: "
              << senddata->size << std::endl;
    auto data = defs::Data(*senddata);
    std::cout << id << std::endl;
    if (isLocal(data.ga)) {
        auto olddata = performRead(data.ga);
        if (olddata->iscached != defs::CACHE_DIRECTORY_STATE::UNSHARED) {
            startInvalidations(data, remoteAddr, cq, olddata->sharerNodes);
            return true;
        }
    } else {
        auto result = performWrite(&data).sendable();
        auto sendmr = network.registerMr(&result, sizeof(defs::SendGlobalAddr), {});
        auto write = defs::createWriteWithImm(sendmr->getSlice(), remoteAddr,
                                              defs::IMMDATA::DEFAULT);
        rcqp.postWorkRequest(write);
        cq->pollSendCompletionQueueBlocking(ibv::workcompletion::Opcode::RDMA_WRITE);
        return true;
    }
}

void Node::handleInvalidation(void *recvbuf, ibv::memoryregion::RemoteAddress remoteAddr,
                              rdma::CompletionQueuePair *cq) {
    auto sga = reinterpret_cast<defs::SendGlobalAddr *>(recvbuf);
    cache.removeCacheItem(*sga);
    rcqp.setToResetState();
}


void Node::startInvalidations(defs::Data data, ibv::memoryregion::RemoteAddress remoteAddr,
                              rdma::CompletionQueuePair *cq,
                              std::array<uint16_t, defs::maxSharerNodes> nodes) {
    std::cout << "going to invalidate" << std::endl;
    auto invalidation = data.ga.sendable();
    auto sendmr1 = network.registerMr(&invalidation, sizeof(defs::SendGlobalAddr), {});
    auto write1 = defs::createWriteWithImm(sendmr1->getSlice(), remoteAddr,
                                           defs::IMMDATA::INVALIDATE);
    rcqp.postWorkRequest(write1);
    cq->pollSendCompletionQueueBlocking(ibv::workcompletion::Opcode::RDMA_WRITE);
    auto result = performWrite(&data).sendable();
    auto sendmr = network.registerMr(&result, sizeof(defs::SendGlobalAddr), {});
    auto write = defs::createWriteWithImm(sendmr->getSlice(), remoteAddr,
                                          defs::IMMDATA::DEFAULT);
   // socket.close();
    rcqp.postWorkRequest(write);
    cq->pollSendCompletionQueueBlocking(ibv::workcompletion::Opcode::RDMA_WRITE);
  //  rcqp.setToResetState();
    broadcastInvalidations(nodes, data.ga);

}
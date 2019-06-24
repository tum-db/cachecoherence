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

void Node::receive(l5::util::Socket *acced) {
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
            break;
        }
        case defs::IMMDATA::READ: //immdata = 2, if it is a read
        {
            handleRead(recvbuf, remoteMr, &cq);
            break;
        }
        case defs::IMMDATA::FREE://immdata = 3, it should be freed
        {
            handleFree(recvbuf, remoteMr, &cq);
            break;
        }
        case defs::IMMDATA::WRITE:  //immdata = 4, write
        {
            handleWrite(recvbuf, remoteMr, &cq);
            break;
        }
        case defs::IMMDATA::LOCKS:  //immdata = 5, save lock
        {
            handleLocks(recvbuf, remoteMr, &cq);
            break;
        }
        case defs::IMMDATA::RESET: {
            rcqp.setToResetState();
            *acced = connectServerSocket();
            break;
        }
        default: {
            return;
        }
    }
}

void Node::connectAndReceive() {
    l5::util::tcp::bind(socket, defs::port);
    auto acced = connectServerSocket();
    while (true) {
        receive(&acced);
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
    auto sendmr = network.registerMr(&lock, sizeof(defs::Lock), {});
    auto write = defs::createWriteWithImm(sendmr->getSlice(), remoteAddr, defs::IMMDATA::DEFAULT);
    rcqp.postWorkRequest(write);
    cq->pollSendCompletionQueueBlocking(ibv::workcompletion::Opcode::RDMA_WRITE);
}

void Node::handleRead(void *recvbuf, ibv::memoryregion::RemoteAddress remoteAddr,
                      rdma::CompletionQueuePair *cq) {
    auto sga = reinterpret_cast<defs::SendGlobalAddr *>(recvbuf);
    auto gaddr = defs::GlobalAddress(*sga);
    auto data = read(gaddr);
    std::cout << "datasize: " << sizeof(data) << ", data: " << data << std::endl;
    auto sendmr = network.registerMr(&data, sizeof(uint64_t), {});
    auto write = defs::createWriteWithImm(sendmr->getSlice(), remoteAddr, defs::IMMDATA::DEFAULT);
    rcqp.postWorkRequest(write);
    cq->pollSendCompletionQueueBlocking(ibv::workcompletion::Opcode::RDMA_WRITE);

}

void Node::handleWrite(void *recvbuf, ibv::memoryregion::RemoteAddress remoteAddr,
                       rdma::CompletionQueuePair *cq) {
    auto senddata = reinterpret_cast<defs::SendingData *>(recvbuf);
    std::cout << "Write, SendData: data: " << senddata->data << ", ga-ID: " << senddata->sga.id
              << ", ga-size:" << senddata->sga.size << ", ptr: " << senddata->sga.ptr << ", size: "
              << senddata->size << std::endl;
    auto data = defs::SendData(*senddata);
    std::cout << "Write, SendData: data: " << data.data << ", ga-ID: " << data.ga.id
              << ", ga-size:" << data.ga.size << ", ptr: " << data.ga.ptr << ", size: "
              << data.size << std::endl;
    std::cout << id << std::endl;
    auto result = write(&data).sendable();
    auto sendmr = network.registerMr(&result, sizeof(defs::SendGlobalAddr), {});
    auto write = defs::createWriteWithImm(sendmr->getSlice(), remoteAddr, defs::IMMDATA::DEFAULT);
    rcqp.postWorkRequest(write);
    cq->pollSendCompletionQueueBlocking(ibv::workcompletion::Opcode::RDMA_WRITE);
}
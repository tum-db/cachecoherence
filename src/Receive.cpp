//
// Created by Magdalena Pröbstl on 2019-06-15.
//


#include "Node.h"
#include "../util/socket/tcp.h"
#include "../buffermanager/buffer_manager.h"


bool Node::receive(Connection &c) {
    auto &cq = network.getSharedCompletionQueue();
    auto recvbuf = malloc(defs::BIGBADBUFFER_SIZE * 2);
    auto recvmr = network.registerMr(recvbuf, defs::BIGBADBUFFER_SIZE * 2,
                                     {ibv::AccessFlag::LOCAL_WRITE, ibv::AccessFlag::REMOTE_WRITE});
    auto remoteMr = ibv::memoryregion::RemoteAddress{reinterpret_cast<uintptr_t>(recvbuf),
                                                     recvmr->getRkey()};
    l5::util::tcp::write(c.socket, &remoteMr, sizeof(remoteMr));
    l5::util::tcp::read(c.socket, &remoteMr, sizeof(remoteMr));
    auto recv = ibv::workrequest::Recv{};
    recv.setSge(nullptr, 0);
    c.rcqp->postRecvRequest(recv);
    auto wc = cq.pollRecvWorkCompletionBlocking();
    auto immData = wc.getImmData();
    //std::cout << "got this immdata: " << immData << std::endl;
    switch (immData) {
        case defs::IMMDATA::MALLOC:  //immdata = 1, if it comes from another server
        {
            handleAllocation(recvbuf, remoteMr, cq, c);
            return true;
        }
        case defs::IMMDATA::READ: //immdata = 2, if it is a read
        {
            handleRead(recvbuf, remoteMr, cq, c);
            return true;
        }
        case defs::IMMDATA::FREE://immdata = 3, it should be freed
        {
            handleFree(recvbuf, remoteMr, cq, c);
            return true;
        }
        case defs::IMMDATA::WRITE:  //immdata = 4, write
        {

            auto res = handleWrite(recvbuf, remoteMr, cq, c);
            std::cout << "result of write: " << res << std::endl;
            return res;
        }
        case defs::IMMDATA::LOCKS:  //immdata = 5, save lock
        {
            handleLocks(recvbuf, remoteMr, cq, c);
            return true;
        }
        case defs::IMMDATA::RESET: //immdata = 6, reset state
        {
            handleReset(remoteMr, cq, c);
            return false;
        }
        case defs::IMMDATA::INVALIDATE: {
            handleInvalidation(recvbuf, remoteMr, cq, c);
            return true;
        }
        case defs::IMMDATA::FILE: {
            handleFile(recvbuf, remoteMr, cq, c);
            return true;
        }
        default: {
            return true;
        }
    }
}

void Node::connectAndReceive(uint16_t port) {
    auto soc = l5::util::Socket::create();
    auto qp = std::make_unique<rdma::RcQueuePair>(
            rdma::RcQueuePair(network, network.getSharedCompletionQueue()));
    auto c = Connection{std::move(qp),
                        l5::util::Socket::create()};
    l5::util::tcp::bind(soc, port);
    auto remoteAddr = rdma::Address{network.getGID(), c.rcqp->getQPN(), network.getLID()};
    l5::util::tcp::listen(soc);
    std::cout << "now listening... ";
    c.socket = l5::util::tcp::accept(soc);
    soc.close();
    std::cout << "and accepted" << std::endl;
    l5::util::tcp::write(c.socket, &remoteAddr, sizeof(remoteAddr));
    l5::util::tcp::read(c.socket, &remoteAddr, sizeof(remoteAddr));
    c.rcqp->connect(remoteAddr);
    bool connected = true;
    while (connected) {
        connected = receive(c);
    }
}

void Node::handleAllocation(void *recvbuf, ibv::memoryregion::RemoteAddress remoteAddr,
                            rdma::CompletionQueuePair &cq, Connection &c) {
    auto sga = reinterpret_cast<defs::SendGlobalAddr *>(recvbuf);
    auto gaddr = defs::GlobalAddress(*sga);
    auto newgaddr = Malloc(gaddr.size).sendable(sga->srcID);
    auto sendmr = network.registerMr(&newgaddr, sizeof(defs::GlobalAddress), {});
    auto write = defs::createWriteWithImm(sendmr->getSlice(), remoteAddr, defs::IMMDATA::DEFAULT);
    c.rcqp->postWorkRequest(write);
    cq.pollSendCompletionQueueBlocking(ibv::workcompletion::Opcode::RDMA_WRITE);
}

void Node::handleFree(void *recvbuf, ibv::memoryregion::RemoteAddress
remoteAddr, rdma::CompletionQueuePair &cq, Connection &c) {
    auto sga = reinterpret_cast<defs::SendGlobalAddr *>(recvbuf);
    auto gaddr = defs::GlobalAddress(*sga);
    auto res = Free(gaddr).sendable(sga->srcID);
    auto sendmr = network.registerMr(&res, sizeof(defs::SendGlobalAddr), {});
    auto write = defs::createWriteWithImm(sendmr->getSlice(), remoteAddr, defs::IMMDATA::DEFAULT);
    c.rcqp->postWorkRequest(write);
    cq.pollSendCompletionQueueBlocking(ibv::workcompletion::Opcode::RDMA_WRITE);
}

void Node::handleLocks(void *recvbuf, ibv::memoryregion::RemoteAddress remoteAddr,
                       rdma::CompletionQueuePair &cq, Connection &c) {
    auto l = reinterpret_cast<defs::Lock *>(recvbuf);
    auto lock = setLock(l->id, l->state);
    auto sendmr = network.registerMr(&lock, sizeof(bool), {});
    auto write = defs::createWriteWithImm(sendmr->getSlice(), remoteAddr, defs::IMMDATA::DEFAULT);
    c.rcqp->postWorkRequest(write);
    cq.pollSendCompletionQueueBlocking(ibv::workcompletion::Opcode::RDMA_WRITE);
}

void Node::handleRead(void *recvbuf, ibv::memoryregion::RemoteAddress remoteAddr,
                      rdma::CompletionQueuePair &cq, Connection &c) {
    auto sga = reinterpret_cast<defs::SendGlobalAddr *>(recvbuf);
    auto gaddr = defs::GlobalAddress(*sga);
    auto data = performRead(gaddr, sga->srcID);
    data->iscached = defs::CACHE_DIRECTORY_STATE::SHARED;
    data->sharerNodes.push_back(sga->srcID);
//    std::cout << "datasize: " << sizeof(data->data) << ", data: " << data << std::endl;
    auto sendmr = network.registerMr(&data->data, sizeof(uint64_t), {});
    auto write = defs::createWriteWithImm(sendmr->getSlice(), remoteAddr, defs::IMMDATA::DEFAULT);
    c.rcqp->postWorkRequest(write);
    cq.pollSendCompletionQueueBlocking(ibv::workcompletion::Opcode::RDMA_WRITE);

}

bool Node::handleWrite(void *recvbuf, ibv::memoryregion::RemoteAddress remoteAddr,
                       rdma::CompletionQueuePair &cq, Connection &c) {
    auto senddata = reinterpret_cast<defs::SendingData *>(recvbuf);
    /*   std::cout << "Write, SendData: data: " << senddata->data << ", ga-ID: " << senddata->sga.id
                 << ", ga-size:" << senddata->sga.size << ", ptr: " << senddata->sga.ptr << ", size: "
                 << senddata->size << std::endl;*/
    auto data = defs::Data(*senddata);
    auto olddata = performRead(data.ga, senddata->sga.srcID);
    if (olddata != nullptr) {
        /*std::cout << "olddata: " << olddata->data << ", is cached: " << olddata->iscached
                  << ", first sharernode: " << olddata->sharerNodes[0] << std::endl;*/
        if ((olddata->iscached > defs::CACHE_DIRECTORY_STATE::UNSHARED) &&
            (!olddata->sharerNodes.empty()) && olddata->iscached < 3) {
            startInvalidations(data, remoteAddr, cq, olddata->sharerNodes, senddata->sga.srcID, c);
            return false;
        } else {
            auto result = performWrite(&data, senddata->sga.srcID).sendable(senddata->sga.srcID);
            auto sendmr = network.registerMr(&result, sizeof(defs::SendGlobalAddr), {});
            auto write = defs::createWriteWithImm(sendmr->getSlice(), remoteAddr,
                                                  defs::IMMDATA::DEFAULT);
            c.rcqp->postWorkRequest(write);
            cq.pollSendCompletionQueueBlocking(ibv::workcompletion::Opcode::RDMA_WRITE);
        }
    } else {
        std::cout << "helloq3" << std::endl;
        auto result = performWrite(&data, senddata->sga.srcID).sendable(senddata->sga.srcID);
        auto sendmr = network.registerMr(&result, sizeof(defs::SendGlobalAddr), {});
        auto write = defs::createWriteWithImm(sendmr->getSlice(), remoteAddr,
                                              defs::IMMDATA::DEFAULT);
        c.rcqp->postWorkRequest(write);
        cq.pollSendCompletionQueueBlocking(ibv::workcompletion::Opcode::RDMA_WRITE);
    }
    return true;
}

void Node::handleInvalidation(void *recvbuf, ibv::memoryregion::RemoteAddress remoteAddr,
                              rdma::CompletionQueuePair &cq, Connection &c) {
    auto sga = reinterpret_cast<defs::SendGlobalAddr *>(recvbuf);
    auto res = cache.removeCacheItem(defs::GlobalAddress(*sga));
    std::cout << "removed cacheitem" << std::endl;
    auto sendmr = network.registerMr(&res, sizeof(uint64_t), {});
    auto write = defs::createWriteWithImm(sendmr->getSlice(), remoteAddr, defs::IMMDATA::DEFAULT);
    c.rcqp->postWorkRequest(write);
    cq.pollSendCompletionQueueBlocking(ibv::workcompletion::Opcode::RDMA_WRITE);
}

void Node::handleReset(ibv::memoryregion::RemoteAddress
                       remoteAddr, rdma::CompletionQueuePair &cq, Connection &c) {
    bool result = false;
    auto sendmr = network.registerMr(&result, sizeof(bool), {});
    auto write = defs::createWriteWithImm(sendmr->getSlice(), remoteAddr,
                                          defs::IMMDATA::RESET);
    c.rcqp->postWorkRequest(write);
    cq.pollSendCompletionQueueBlocking(ibv::workcompletion::Opcode::RDMA_WRITE);
    c.rcqp->setToResetState();
    c.socket.close();
}


void Node::startInvalidations(defs::Data data, ibv::memoryregion::RemoteAddress remoteAddr,
                              rdma::CompletionQueuePair &cq,
                              std::vector<uint16_t> nodes, uint16_t srcID, Connection &c) {
    std::cout << "going to prepareForInvalidate" << std::endl;
    auto invalidation = data.ga.sendable(srcID);
    auto sendmr1 = network.registerMr(&invalidation, sizeof(defs::SendGlobalAddr), {});
    auto write1 = defs::createWriteWithImm(sendmr1->getSlice(), remoteAddr,
                                           defs::IMMDATA::INVALIDATE);
    c.rcqp->postWorkRequest(write1);
    cq.pollSendCompletionQueueBlocking(ibv::workcompletion::Opcode::RDMA_WRITE);
    auto result = performWrite(&data, srcID).sendable(srcID);
    auto sendmr = network.registerMr(&result, sizeof(defs::SendGlobalAddr), {});
    auto write = defs::createWriteWithImm(sendmr->getSlice(), remoteAddr,
                                          defs::IMMDATA::DEFAULT);
    c.rcqp->postWorkRequest(write);
    cq.pollSendCompletionQueueBlocking(ibv::workcompletion::Opcode::RDMA_WRITE);
    c.rcqp->setToResetState();
    c.socket.close();
    broadcastInvalidations(nodes, data.ga);

}

void Node::handleFile(void *recvbuf, ibv::memoryregion::RemoteAddress remoteAddr,
                      rdma::CompletionQueuePair &cq, Connection &c) {
    auto fileinfo = reinterpret_cast<defs::FileInfo *>(recvbuf);
    auto file = moderndbs::File::open_file(fileinfo->filename, moderndbs::File::WRITE);
    file->resize(fileinfo->size);
    auto ok = true;
    auto sendmr = network.registerMr(&ok, sizeof(bool), {});
    auto write = defs::createWriteWithImm(sendmr->getSlice(), remoteAddr, defs::IMMDATA::DEFAULT);
    c.rcqp->postWorkRequest(write);
    cq.pollSendCompletionQueueBlocking(ibv::workcompletion::Opcode::RDMA_WRITE);
    const char *data;
    size_t offset = 0;
    while (offset < fileinfo->size) {
        auto recv = ibv::workrequest::Recv{};
        recv.setSge(nullptr, 0);
        c.rcqp->postRecvRequest(recv);
        auto wc = cq.pollRecvWorkCompletionBlocking();
        auto immData = wc.getImmData();
        offset = offset +immData;
        data = reinterpret_cast<char *>(recvbuf);
        file->write_block(data, offset, immData);
        write = defs::createWriteWithImm(sendmr->getSlice(), remoteAddr, defs::IMMDATA::DEFAULT);
        c.rcqp->postWorkRequest(write);
        cq.pollSendCompletionQueueBlocking(ibv::workcompletion::Opcode::RDMA_WRITE);
    }
}
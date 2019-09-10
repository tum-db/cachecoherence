//
// Created by Magdalena Pröbstl on 2019-06-15.
//


#include "Node.h"


bool
Node::receive(Connection &c, rdma::CompletionQueuePair &cq) {

    auto recv = ibv::workrequest::Recv{};
    recv.setSge(nullptr, 0);
    c.rcqp->postRecvRequest(recv);

    auto recvrfd = reinterpret_cast<defs::ReadFileData *>(c.recvreg);
    defs::ReadFileData rfd = *recvrfd;
    auto ack = true;
    memcpy(c.sendreg, &ack, sizeof(bool));
    auto write = defs::createWriteWithImm(c.sendmr->getSlice(), remoteMr,
                                          defs::IMMDATA::DEFAULT);
    c.rcqp->postWorkRequest(write);
    cq.pollSendCompletionQueueBlocking(ibv::workcompletion::Opcode::RDMA_WRITE);

    recv = ibv::workrequest::Recv{};
    recv.setSge(nullptr, 0);
    c.rcqp->postRecvRequest(recv);


    auto wc = cq.pollRecvWorkCompletionBlocking();
    auto immData = wc.getImmData();

    std::cout << "got this immdata: " << immData << std::endl;
    auto res = true;
    switch (immData) {
        case defs::IMMDATA::MALLOC:  //immdata = 1, if it comes from another server
        {

            handleAllocation(cq, c, rfd);
            break;
        }
        case defs::IMMDATA::READ: //immdata = 2, if it is a read
        {

            handleRead(cq, c, rfd);
            break;
        }
        case defs::IMMDATA::FREE://immdata = 3, it should be freed
        {

            handleFree(cq, c, rfd);
            break;
        }
        case defs::IMMDATA::WRITE:  //immdata = 4, write
        {

            res = handleWrite(cq, c, rfd);
            std::cout << "result of write: " << res << std::endl;
            break;

        }
        case defs::IMMDATA::LOCKS:  //immdata = 5, save lock
        {

            handleLocks(cq, c, rfd);
            break;
        }
        case defs::IMMDATA::RESET: //immdata = 6, reset state
        {
            handleReset(cq, c, rfd);
            res = false;
            break;
        }
        case defs::IMMDATA::INVALIDATE: {
            handleInvalidation(cq, c, rfd);
            break;
        }
        case defs::IMMDATA::FILE: {
            handleFile(cq, c, rfd);
            break;
        }
        case defs::IMMDATA::MALLOCFILE: {
            handleMallocFile(cq, c, rfd);
            break;
        }
        case defs::IMMDATA::READFILE: {
            handleReadFile(cq, c, rfd);
            break;
        }
        case defs::IMMDATA::WRITEFILE: {
            handleWriteFile(cq, c, rfd);
            break;
        }
        default: {
            break;
        }
    }
    return res;
}

void Node::connectAndReceive(uint16_t port) {

    auto soc = l5::util::Socket::create();
    auto qp = std::make_unique<rdma::RcQueuePair>(
            rdma::RcQueuePair(network, network.getSharedCompletionQueue()));
    auto c = Connection{std::move(qp),
                        l5::util::Socket::create()};
    l5::util::tcp::bind(soc, port);
    auto remoteAddr = rdma::Address{network.getGID(), c.rcqp->getQPN(), network.getLID()};
    std::cout << "now listening... " << std::endl;
    l5::util::tcp::listen(soc);

    c.socket = l5::util::tcp::accept(soc);
    std::cout << "and accepted" << std::endl;
    soc.close();
    c.recvreg = static_cast<char *>(malloc(defs::BIGBADBUFFER_SIZE * 2));
    c.sendreg = static_cast<char *>(malloc(defs::BIGBADBUFFER_SIZE * 2));

    c.sendmr = network.registerMr(c.recvreg, defs::BIGBADBUFFER_SIZE * 2, {});
    c.recvmr = network.registerMr(c.recvreg, defs::BIGBADBUFFER_SIZE * 2,
                                  {ibv::AccessFlag::LOCAL_WRITE, ibv::AccessFlag::REMOTE_WRITE});

    l5::util::tcp::write(c.socket, &remoteAddr, sizeof(remoteAddr));
    l5::util::tcp::read(c.socket, &remoteAddr, sizeof(remoteAddr));

    auto c.remoteMr = ibv::memoryregion::RemoteAddress{reinterpret_cast<uintptr_t>(c.recvreg),
                                                       c.recvmr->getRkey()};
    l5::util::tcp::write(c.socket, &c.remoteMr, sizeof(c.remoteMr));
    l5::util::tcp::read(c.socket, &c.remoteMr, sizeof(c.remoteMr));

    c.rcqp->connect(remoteAddr);


    auto &cq = network.getSharedCompletionQueue();

    bool connected = true;
    while (connected) {
        connected = receive(c, cq);
    }
}

void Node::handleAllocation(rdma::CompletionQueuePair &cq, Connection &c, defs::ReadFileData rfd) {
    auto sga = reinterpret_cast<defs::SendGlobalAddr *>(c.recvreg);

    auto gaddr = defs::GlobalAddress(*sga);

    auto newgaddr = Malloc(gaddr.size, sga->srcID).sendable(sga->srcID);
    auto sendmr = network.registerMr(&newgaddr, sizeof(defs::SendGlobalAddr), {});

    auto write = defs::createWriteWithImm(sendmr->getSlice(), c.remoteMr, defs::IMMDATA::DEFAULT);
    c.rcqp->postWorkRequest(write);
    cq.pollSendCompletionQueueBlocking(ibv::workcompletion::Opcode::RDMA_WRITE);
}

void Node::handleFree(rdma::CompletionQueuePair &cq, Connection &c, defs::ReadFileData rfd) {
    auto sga = reinterpret_cast<defs::SendGlobalAddr *>(c.recvreg);
    auto gaddr = defs::GlobalAddress(rfd.sga);
    auto res = Free(gaddr).sendable(sga->srcID);
    auto sendmr = network.registerMr(&res, sizeof(defs::SendGlobalAddr), {});
    auto write = defs::createWriteWithImm(sendmr->getSlice(), c.remoteMr, defs::IMMDATA::DEFAULT);
    c.rcqp->postWorkRequest(write);
    cq.pollSendCompletionQueueBlocking(ibv::workcompletion::Opcode::RDMA_WRITE);
}

void Node::handleLocks(rdma::CompletionQueuePair &cq, Connection &c, defs::ReadFileData rfd) {
    auto l = reinterpret_cast<Lock *>(c.recvreg);
    auto lock = setLock(l->id, l->state);
    auto sendmr = network.registerMr(&lock, sizeof(bool), {});
    auto write = defs::createWriteWithImm(sendmr->getSlice(), c.remoteMr, defs::IMMDATA::DEFAULT);
    c.rcqp->postWorkRequest(write);
    cq.pollSendCompletionQueueBlocking(ibv::workcompletion::Opcode::RDMA_WRITE);
}

void Node::handleRead(rdma::CompletionQueuePair &cq, Connection &c, defs::ReadFileData rfd) {
    auto sga = reinterpret_cast<defs::SendGlobalAddr *>(c.recvreg);

    auto gaddr = defs::GlobalAddress(rfd.sga);
    auto data = performRead(gaddr, sga->srcID);
    data->iscached = defs::CACHE_DIRECTORY_STATE::SHARED;
    data->sharerNodes.push_back(sga->srcID);
    std::cout << "datasize: " << sizeof(data->data) << ", data: " << data << std::endl;
    auto sendmr = network.registerMr(&data->data, sizeof(uint64_t), {});
    auto write = defs::createWriteWithImm(sendmr->getSlice(), c.remoteMr, defs::IMMDATA::DEFAULT);
    c.rcqp->postWorkRequest(write);
    cq.pollSendCompletionQueueBlocking(ibv::workcompletion::Opcode::RDMA_WRITE);

}

bool Node::handleWrite(rdma::CompletionQueuePair &cq, Connection &c, defs::ReadFileData rfd) {
    auto senddata = reinterpret_cast<defs::SendingData *>(c.recvreg);

    std::cout << "Write, SendData: data: " << senddata->data << ", ga-ID: " << senddata->sga.id
              << ", ga-size:" << senddata->sga.size << ", ptr: " << senddata->sga.ptr << ", size: "
              << senddata->size << std::endl;
    auto data = defs::Data(*senddata);

    auto olddata = performRead(data.ga, senddata->sga.srcID);
    if (olddata != nullptr) {
        /*std::cout << "olddata: " << olddata->data << ", is cached: " << olddata->iscached
                  << ", first sharernode: " << olddata->sharerNodes[0] << std::endl;*/
        if ((olddata->iscached > defs::CACHE_DIRECTORY_STATE::UNSHARED) &&
            (!olddata->sharerNodes.empty()) && olddata->iscached < 3) {
            startInvalidations(data, c.remoteMr, cq, olddata->sharerNodes, senddata->sga.srcID,
                               c);
            return false;
        } else {
            auto result = performWrite(data, senddata->sga.srcID).sendable(
                    senddata->sga.srcID);
            auto sendmr = network.registerMr(&result, sizeof(defs::SendGlobalAddr), {});
            auto write = defs::createWriteWithImm(sendmr->getSlice(), c.remoteMr,
                                                  defs::IMMDATA::DEFAULT);
            c.rcqp->postWorkRequest(write);
            cq.pollSendCompletionQueueBlocking(ibv::workcompletion::Opcode::RDMA_WRITE);
        }
    } else {
        auto result = performWrite(data, senddata->sga.srcID).sendable(senddata->sga.srcID);
        auto sendmr = network.registerMr(&result, sizeof(defs::SendGlobalAddr), {});
        auto write = defs::createWriteWithImm(sendmr->getSlice(), c.remoteMr,
                                              defs::IMMDATA::DEFAULT);
        c.rcqp->postWorkRequest(write);
        cq.pollSendCompletionQueueBlocking(ibv::workcompletion::Opcode::RDMA_WRITE);
    }
    return true;

}

void Node::handleInvalidation(rdma::CompletionQueuePair &cq, Connection &c,
                              defs::ReadFileData rfd) {
    auto sga = reinterpret_cast<defs::SendGlobalAddr *>(c.recvreg);

    auto res = cache.removeCacheItem(defs::GlobalAddress(*sga));
    //  std::cout << "removed cacheitem" << std::endl;
    auto sendmr = network.registerMr(&res, sizeof(uint64_t), {});
    auto write = defs::createWriteWithImm(sendmr->getSlice(), c.remoteMr, defs::IMMDATA::DEFAULT);
    c.rcqp->postWorkRequest(write);
    cq.pollSendCompletionQueueBlocking(ibv::workcompletion::Opcode::RDMA_WRITE);
}

void Node::handleReset(rdma::CompletionQueuePair &cq, Connection &c,
                       defs::ReadFileData rfd) {
    bool result = false;
    auto sendmr = network.registerMr(&result, sizeof(bool), {});
    auto write = defs::createWriteWithImm(sendmr->getSlice(), c.remoteMr,
                                          defs::IMMDATA::RESET);
    c.rcqp->postWorkRequest(write);
    cq.pollSendCompletionQueueBlocking(ibv::workcompletion::Opcode::RDMA_WRITE);
    c.rcqp->setToResetState();
    c.socket.close();
}


void Node::startInvalidations(defs::Data data, ibv::memoryregion::RemoteAddress remoteAddr,
                              rdma::CompletionQueuePair &cq,

                              std::vector<uint16_t> nodes, uint16_t srcID, Connection &c) {
    //  std::cout << "going to prepareForInvalidate" << std::endl;
    auto invalidation = data.ga.sendable(srcID);
    auto sendmr1 = network.registerMr(&invalidation, sizeof(defs::SendGlobalAddr), {});
    auto write1 = defs::createWriteWithImm(sendmr1->getSlice(), remoteAddr,
                                           defs::IMMDATA::INVALIDATE);
    c.rcqp->postWorkRequest(write1);
    cq.pollSendCompletionQueueBlocking(ibv::workcompletion::Opcode::RDMA_WRITE);
    auto result = performWrite(data, srcID).sendable(srcID);
    auto sendmr = network.registerMr(&result, sizeof(defs::SendGlobalAddr), {});
    auto write = defs::createWriteWithImm(sendmr->getSlice(), remoteAddr,
                                          defs::IMMDATA::DEFAULT);
    c.rcqp->postWorkRequest(write);
    cq.pollSendCompletionQueueBlocking(ibv::workcompletion::Opcode::RDMA_WRITE);
    c.rcqp->setToResetState();
    c.socket.close();
    broadcastInvalidations(nodes, data.ga);

}

void Node::handleFile(rdma::CompletionQueuePair &cq, Connection &c, defs::ReadFileData rfd) {
    auto fitmp = reinterpret_cast<defs::FileInfo *>(c.recvreg);

    auto fileinfo = new defs::FileInfo(*fitmp);
    std::cout << fileinfo->size << ", " << fileinfo->blocksize << std::endl;

    auto file = MaFile(getNextFileName(), moderndbs::File::WRITE);
    auto ok = true;
    auto sendmr = network.registerMr(&ok, sizeof(bool), {});
    auto write = defs::createWriteWithImm(sendmr->getSlice(), c.remoteMr, defs::IMMDATA::DEFAULT);
    const char *block;
    size_t offset = 0;
    c.rcqp->postWorkRequest(write);
    cq.pollSendCompletionQueueBlocking(ibv::workcompletion::Opcode::RDMA_WRITE);
    std::cout << "now starting to receive file" << std::endl;

    while (offset < fileinfo->size) {
        auto recv = ibv::workrequest::Recv{};
        recv.setSge(nullptr, 0);
        c.rcqp->postRecvRequest(recv);
        auto wc = cq.pollRecvWorkCompletionBlocking();
        auto immData = wc.getImmData();
        try {
            auto data = reinterpret_cast<uint64_t *>(c.recvreg);
            block = reinterpret_cast<char *>(data);

            file.write_block(block, offset, immData);
        } catch (std::exception &e) {
            ok = false;
            std::cout << "not sending file done" << std::endl;

        }
        c.rcqp->postWorkRequest(write);
        cq.pollSendCompletionQueueBlocking(ibv::workcompletion::Opcode::RDMA_WRITE);
        offset = offset + immData;
    }
    file.resize(fileinfo->size);

    std::cout << "receiving file done" << std::endl;
    file.close();
}

void Node::handleMallocFile(rdma::CompletionQueuePair &cq, Connection &c, defs::ReadFileData rfd) {

    defs::SendGlobalAddr newgaddr{};
    std::cout << "mallocfile" << std::endl;

    auto sga = reinterpret_cast<defs::SendGlobalAddr *>(c.recvreg);
    auto filename = getNextFileName();
    auto nf = MaFile(filename, MaFile::Mode::WRITE);

    if (nf.enough_space(0, sga->size)) {
        newgaddr = defs::GlobalAddress(sga->size, filename, id, true).sendable(sga->srcID);
    } else {
        newgaddr = defs::GlobalAddress(0, filename, 0, true).sendable(0);

    }
    auto sendmr = network.registerMr(&newgaddr, sizeof(defs::SendGlobalAddr), {});
    auto write = defs::createWriteWithImm(sendmr->getSlice(), c.remoteMr, defs::IMMDATA::DEFAULT);
    c.rcqp->postWorkRequest(write);
    cq.pollSendCompletionQueueBlocking(ibv::workcompletion::Opcode::RDMA_WRITE);
}

void Node::handleReadFile(rdma::CompletionQueuePair &cq, Connection &c, defs::ReadFileData rfd) {
    auto gaddr = defs::GlobalAddress(rfd.sga);
    if (gaddr.isFile) {
        std::vector<char> block;
        block.resize(rfd.size);

        char *result = &block[0];
        if (isLocal(gaddr)) {
            auto f = MaFile(reinterpret_cast<char *>(gaddr.ptr), MaFile::Mode::READ);
            f.read_block(rfd.offset, rfd.size, result);

//    std::cout << "datasize: " << sizeof(data->data) << ", data: " << data << std::endl;
            auto sendmr = network.registerMr(result, rfd.size, {});
            auto write = defs::createWriteWithImm(sendmr->getSlice(), c.remoteMr,
                                                  defs::IMMDATA::DEFAULT);
            c.rcqp->postWorkRequest(write);
            cq.pollSendCompletionQueueBlocking(ibv::workcompletion::Opcode::RDMA_WRITE);
        }
    }
}


void Node::handleWriteFile(rdma::CompletionQueuePair &cq, Connection &c, defs::ReadFileData rfd) {
    auto gaddr = defs::GlobalAddress(rfd.sga);
    if (gaddr.isFile) {

        auto castdata = reinterpret_cast<uint64_t *>(c.recvreg);

        std::cout << castdata << std::endl;
        auto data = reinterpret_cast<char *>(castdata);

        auto f = MaFile(reinterpret_cast<char *>(gaddr.ptr), MaFile::Mode::WRITE);
        f.resize(gaddr.size + rfd.size);
        std::cout << data << std::endl;
        std::cout << "local, size: " << rfd.size << ", gaddr-size: " << gaddr.size
                  << ", filesize: "
                  << f.size() << std::endl;
        f.write_block(data, gaddr.size, rfd.size);
        gaddr.resize(rfd.size + gaddr.size);

        auto sendmr = network.registerMr(&gaddr, sizeof(defs::SendGlobalAddr), {});
        auto write = defs::createWriteWithImm(sendmr->getSlice(), c.remoteMr,
                                              defs::IMMDATA::DEFAULT);
        c.rcqp->postWorkRequest(write);
        cq.pollSendCompletionQueueBlocking(ibv::workcompletion::Opcode::RDMA_WRITE);


    }
}

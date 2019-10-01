//
// Created by Magdalena Pröbstl on 2019-06-15.
//


#include <cassert>
#include "Node.h"


bool
Node::receive(Connection &c, rdma::CompletionQueuePair &cq) {
    auto recv = ibv::workrequest::Recv{};
    recv.setSge(nullptr, 0);
    c.rcqp->postRecvRequest(recv);
    auto wc = cq.pollRecvWorkCompletionBlocking();

    auto recvrfd = reinterpret_cast<defs::ReadFileData *>(c.recvreg);
    defs::ReadFileData rfd = *recvrfd;
    if (!rfd.simplerequest) {
        auto ack = true;
        memcpy(c.sendreg, &ack, sizeof(bool));
        auto write = defs::createWriteWithImm(c.sendmr->getSlice(), c.remoteMr,
                                              defs::IMMDATA::DEFAULT);
        c.rcqp->postWorkRequest(write);

        cq.pollSendCompletionQueueBlocking(ibv::workcompletion::Opcode::RDMA_WRITE);

        recv = ibv::workrequest::Recv{};
        recv.setSge(nullptr, 0);
        c.rcqp->postRecvRequest(recv);
        wc = cq.pollRecvWorkCompletionBlocking();
    }
    auto immData = wc.getImmData();
  //  std::cout << "got this immdata: " << immData << std::endl;
    auto res = true;
    switch (immData) {
        case defs::IMMDATA::MALLOC:  //immdata = 1, if it comes from another server
        {
            handleAllocation(cq, rfd);
            break;
        }
        case defs::IMMDATA::READ: //immdata = 2, if it is a read
        {

            handleRead(cq, rfd);
            break;
        }
        case defs::IMMDATA::FREE://immdata = 3, it should be freed
        {
            handleFree(cq, rfd);
            break;
        }
        case defs::IMMDATA::WRITE:  //immdata = 4, write
        {

            res = handleWrite(cq, rfd);
   //         std::cout << "result of write: " << res << std::endl;
            break;

        }
        case defs::IMMDATA::LOCKS:  //immdata = 5, save lock
        {

            handleLocks(cq, rfd);
            break;
        }
        case defs::IMMDATA::RESET: //immdata = 6, reset state
        {
            handleReset(cq);
            res = false;
            break;
        }
        case defs::IMMDATA::INVALIDATE: { //immdata = 7, invalidation
            handleInvalidation(cq, rfd);
            break;
        }
        case defs::IMMDATA::FILE: {
            handleFile(cq, rfd);
            break;
        }
        case defs::IMMDATA::MALLOCFILE: {
            handleMallocFile(cq, rfd);
            break;
        }
        case defs::IMMDATA::READFILE: {
            handleReadFile(cq, rfd);
            break;
        }
        case defs::IMMDATA::WRITEFILE: {
            handleWriteFile(cq, rfd);
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
    l5::util::tcp::bind(soc, port);
    std::cout << "now listening... " << std::endl;
    l5::util::tcp::listen(soc);

    auto acc = l5::util::tcp::accept(soc);
  //  std::cout << "and accepted" << std::endl;
    soc.close();

    c.connect(network, acc);
    auto recv = ibv::workrequest::Recv{};
    recv.setSge(nullptr, 0);
    c.rcqp->postRecvRequest(recv);

    auto &cq = network.getSharedCompletionQueue();

    bool connected = true;
    while (connected) {
        connected = receive(c, cq);
    }
}

void Node::handleAllocation(rdma::CompletionQueuePair &cq, defs::ReadFileData rfd) {
    auto newgaddr = Malloc(rfd.sga.size, rfd.sga.srcID).sendable(rfd.sga.srcID);

    memcpy(c.sendreg, &newgaddr, sizeof(defs::SendGlobalAddr));
    auto write = defs::createWriteWithImm(c.sendmr->getSlice(), c.remoteMr, defs::IMMDATA::DEFAULT);
    c.rcqp->postWorkRequest(write);
    cq.pollSendCompletionQueueBlocking(ibv::workcompletion::Opcode::RDMA_WRITE);
}

void Node::handleFree(rdma::CompletionQueuePair &cq, defs::ReadFileData rfd) {
    auto gaddr = defs::GlobalAddress(rfd.sga);
    auto res = Free(gaddr, rfd.sga.srcID).sendable(rfd.sga.srcID);
    std::memcpy(c.sendreg, &res, sizeof(defs::SendGlobalAddr));
    auto write = defs::createWriteWithImm(c.sendmr->getSlice(), c.remoteMr, defs::IMMDATA::DEFAULT);
    c.rcqp->postWorkRequest(write);
    cq.pollSendCompletionQueueBlocking(ibv::workcompletion::Opcode::RDMA_WRITE);
}

void Node::handleLocks(rdma::CompletionQueuePair &cq, defs::ReadFileData rfd) {
    auto l = reinterpret_cast<Lock *>(c.recvreg);
    auto lock = setLock(l->id, l->state, l->nodeId);
    std::memcpy(c.sendreg, &lock, sizeof(Lock));
    auto write = defs::createWriteWithImm(c.sendmr->getSlice(), c.remoteMr, defs::IMMDATA::DEFAULT);
    c.rcqp->postWorkRequest(write);
    std::cout << "send" << std::endl;
    cq.pollSendCompletionQueueBlocking(ibv::workcompletion::Opcode::RDMA_WRITE);
}

void Node::handleRead(rdma::CompletionQueuePair &cq, defs::ReadFileData rfd) {
    auto gaddr = defs::GlobalAddress(rfd.sga);
    auto data = performRead(gaddr, rfd.sga.srcID);
    std::memcpy(c.sendreg, data, rfd.sga.size);
    auto write = defs::createWriteWithImm(c.sendmr->getSlice(), c.remoteMr, defs::IMMDATA::DEFAULT);
    c.rcqp->postWorkRequest(write);
    cq.pollSendCompletionQueueBlocking(ibv::workcompletion::Opcode::RDMA_WRITE);

}

bool Node::handleWrite(rdma::CompletionQueuePair &cq, defs::ReadFileData rfd) {
    auto data = defs::Data{rfd.size, c.recvreg, defs::GlobalAddress(rfd.sga)};
    auto olddata = reinterpret_cast<defs::SaveData *>(rfd.sga.ptr);

    if (olddata != nullptr) {
        if ((olddata->iscached > defs::CACHE_DIRECTORY_STATE::UNSHARED) &&
            (!olddata->sharerNodes.empty()) && (olddata->ownerNode != rfd.sga.srcID) &&
            olddata->iscached < 3) {
            startInvalidations(data, c.remoteMr, cq, olddata->sharerNodes, rfd.sga.srcID);
            olddata->sharerNodes = {};
            return false;
        } else {
            auto result = performWrite(data, rfd.sga.srcID).sendable(
                    rfd.sga.srcID);
            std::memcpy(c.sendreg, &result, sizeof(defs::SendGlobalAddr));
            auto write = defs::createWriteWithImm(c.sendmr->getSlice(), c.remoteMr,
                                                  defs::IMMDATA::DEFAULT);
            c.rcqp->postWorkRequest(write);
            cq.pollSendCompletionQueueBlocking(ibv::workcompletion::Opcode::RDMA_WRITE);
        }
    } else {
        auto result = performWrite(data, rfd.sga.srcID).sendable(rfd.sga.srcID);
        std::memcpy(c.sendreg, &result, sizeof(defs::SendGlobalAddr));
        auto write = defs::createWriteWithImm(c.sendmr->getSlice(), c.remoteMr,
                                              defs::IMMDATA::DEFAULT);
        c.rcqp->postWorkRequest(write);
        cq.pollSendCompletionQueueBlocking(ibv::workcompletion::Opcode::RDMA_WRITE);
    }
    return true;

}

void Node::handleInvalidation(rdma::CompletionQueuePair &cq,
                              defs::ReadFileData rfd) {
    auto res = cache.removeCacheItem(defs::GlobalAddress(rfd.sga));
    //  std::cout << "removed cacheitem" << std::endl;
    std::memcpy(c.sendreg, &res, sizeof(uint64_t));
    auto write = defs::createWriteWithImm(c.sendmr->getSlice(), c.remoteMr, defs::IMMDATA::DEFAULT);
    c.rcqp->postWorkRequest(write);
    cq.pollSendCompletionQueueBlocking(ibv::workcompletion::Opcode::RDMA_WRITE);
}

void Node::handleReset(rdma::CompletionQueuePair &cq) {
    bool result = false;
    std::memcpy(c.sendreg, &result, sizeof(bool));
    auto write = defs::createWriteWithImm(c.sendmr->getSlice(), c.remoteMr,
                                          defs::IMMDATA::RESET);
    c.rcqp->postWorkRequest(write);
    cq.pollSendCompletionQueueBlocking(ibv::workcompletion::Opcode::RDMA_WRITE);
    c.close();
    std::cout << "closed" << std::endl;
}


void Node::startInvalidations(defs::Data data, ibv::memoryregion::RemoteAddress remoteAddr,
                              rdma::CompletionQueuePair &cq,
                              std::vector<uint16_t> nodes, uint16_t srcID) {
    //tell sender that we need to invalidate
    std::cout << "tell sender that we need to invalidate" << std::endl;
    auto invalidation = data.ga.sendable(srcID);
    std::memcpy(c.sendreg, &invalidation, sizeof(defs::SendGlobalAddr));
    auto write = defs::createWriteWithImm(c.sendmr->getSlice(), remoteAddr,
                                          defs::IMMDATA::INVALIDATE);
    c.rcqp->postWorkRequest(write);
    cq.pollSendCompletionQueueBlocking(ibv::workcompletion::Opcode::RDMA_WRITE);
    std::cout << "do write and send result of write" << std::endl;

    //do write and send result of write
    auto result = performWrite(data, srcID).sendable(srcID);
    std::memcpy(c.sendreg, &result, sizeof(defs::SendGlobalAddr));
    write = defs::createWriteWithImm(c.sendmr->getSlice(), remoteAddr,
                                     defs::IMMDATA::DEFAULT);
    c.rcqp->postWorkRequest(write);
    // std::this_thread::sleep_for(std::chrono_literals::operator ""ms(100));
    std::cout << "waiting" << std::endl;

    cq.pollSendCompletionQueueBlocking(ibv::workcompletion::Opcode::RDMA_WRITE);

    std::cout << "reset qp and start invalidation" << std::endl;
    //reset qp and start invalidation
    c.close();
    broadcastInvalidations(nodes, data.ga);
}

void Node::handleFile(rdma::CompletionQueuePair &cq, defs::ReadFileData rfd) {
    auto fitmp = reinterpret_cast<defs::FileInfo *>(c.recvreg);

    auto fileinfo = new defs::FileInfo(*fitmp);
    std::cout << fileinfo->size << ", " << fileinfo->blocksize << std::endl;

    auto file = MaFile(getNextFileName(), moderndbs::File::WRITE);
    auto ok = true;
    std::memcpy(c.sendreg, &ok, sizeof(bool));
    auto write = defs::createWriteWithImm(c.sendmr->getSlice(), c.remoteMr, defs::IMMDATA::DEFAULT);
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
            block = const_cast<const char *>(c.recvreg);

            file.write_block(block, offset, immData);
        } catch (std::exception &e) {
            ok = false;
            std::cout << "not sending file done" << std::endl;

        }

        std::memcpy(c.sendreg, &ok, sizeof(bool));

        c.rcqp->postWorkRequest(write);
        cq.pollSendCompletionQueueBlocking(ibv::workcompletion::Opcode::RDMA_WRITE);
        offset = offset + immData;
    }
    file.resize(fileinfo->size);

    std::cout << "receiving file done" << std::endl;
    file.close();
}

void Node::handleMallocFile(rdma::CompletionQueuePair &cq, defs::ReadFileData rfd) {
    defs::SendGlobalAddr newgaddr{};
    std::cout << "mallocfile" << std::endl;

    auto filename = getNextFileName();
    auto nf = MaFile(filename, MaFile::Mode::WRITE);

    if (nf.enough_space(0, rfd.sga.size)) {
        newgaddr = defs::GlobalAddress(rfd.sga.size, filename, id, true).sendable(rfd.sga.srcID);
    } else {
        newgaddr = defs::GlobalAddress(0, filename, 0, true).sendable(0);

    }
    std::memcpy(c.sendreg, &newgaddr, sizeof(defs::SendGlobalAddr));
    auto write = defs::createWriteWithImm(c.sendmr->getSlice(), c.remoteMr, defs::IMMDATA::DEFAULT);
    c.rcqp->postWorkRequest(write);
    cq.pollSendCompletionQueueBlocking(ibv::workcompletion::Opcode::RDMA_WRITE);
}

void Node::handleReadFile(rdma::CompletionQueuePair &cq, defs::ReadFileData rfd) {
    auto gaddr = defs::GlobalAddress(rfd.sga);
    if (gaddr.isFile) {
        std::vector<char> block;
        block.resize(rfd.size);

        char *result = &block[0];

        assert(isLocal(gaddr));

        auto f = MaFile(gaddr.ptr, MaFile::Mode::READ);
        f.read_block(rfd.offset, rfd.size, result);

        //    std::cout << "datasize: " << sizeof(data->data) << ", data: " << data << std::endl;
        std::memcpy(c.sendreg, result, rfd.size);
        auto write = defs::createWriteWithImm(c.sendmr->getSlice(), c.remoteMr,
                                              defs::IMMDATA::DEFAULT);
        c.rcqp->postWorkRequest(write);
        cq.pollSendCompletionQueueBlocking(ibv::workcompletion::Opcode::RDMA_WRITE);
    }
}

void Node::handleWriteFile(rdma::CompletionQueuePair &cq, defs::ReadFileData rfd) {
    auto gaddr = defs::GlobalAddress(rfd.sga);
    if (gaddr.isFile) {
        auto castdata = reinterpret_cast<uint64_t *>(c.recvreg);

        std::cout << castdata << std::endl;
        auto data = reinterpret_cast<char *>(castdata);

        auto f = MaFile(gaddr.ptr, MaFile::Mode::WRITE);
        f.resize(gaddr.size + rfd.size);
        std::cout << data << std::endl;
        std::cout << "local, size: " << rfd.size << ", gaddr-size: " << gaddr.size
                  << ", filesize: "
                  << f.size() << std::endl;
        f.write_block(data, gaddr.size, rfd.size);
        gaddr.resize(rfd.size + gaddr.size);

        std::memcpy(c.sendreg, &gaddr, sizeof(defs::SendGlobalAddr));
        auto write = defs::createWriteWithImm(c.sendmr->getSlice(), c.remoteMr,
                                              defs::IMMDATA::DEFAULT);
        c.rcqp->postWorkRequest(write);
        cq.pollSendCompletionQueueBlocking(ibv::workcompletion::Opcode::RDMA_WRITE);
    }
}

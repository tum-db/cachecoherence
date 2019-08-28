//
// Created by Magdalena Pröbstl on 2019-06-15.
//

#include "Node.h"


Connection Node::connectClientSocket(uint16_t port) {
    auto qp = std::make_unique<rdma::RcQueuePair>(
            rdma::RcQueuePair(network, network.getSharedCompletionQueue()));
    auto c = Connection{std::move(qp), l5::util::Socket::create()};
    auto remoteAddr = rdma::Address{network.getGID(), c.rcqp->getQPN(), network.getLID()};
    for (int i = 0;; ++i) {
        try {
            l5::util::tcp::connect(c.socket, defs::ip, port);
            break;
        } catch (std::exception &e) {
            std::this_thread::sleep_for(std::chrono_literals::operator ""ms(20));
            if (i > 10) throw;
        }
    }
    std::cout << "remoteAddr: " << &remoteAddr << ", socket: " << &c.socket << std::endl;

    l5::util::tcp::write(c.socket, &remoteAddr, sizeof(remoteAddr));
    l5::util::tcp::read(c.socket, &remoteAddr, sizeof(remoteAddr));
    //  std::cout << "connecting queuepairs, socket: "<< c.socket.get() << std::endl;
    c.rcqp->connect(remoteAddr);
    return std::move(c);
}

void Node::closeClientSocket(Connection &c) {
    std::cout << "closing socket: " << c.socket.get() << std::endl;
    auto fakeLock = Lock{id, LOCK_STATES::UNLOCKED};
    sendLock(fakeLock, defs::RESET, c);
    c.rcqp->setToResetState();
    c.socket.close();
}


void *Node::sendAddress(defs::SendGlobalAddr data, defs::IMMDATA immData, Connection &c) {
    auto &cq = network.getSharedCompletionQueue();
    auto size = sizeof(defs::SendGlobalAddr) + data.size;
    auto sendmr = network.registerMr(&data, sizeof(data) + data.size, {});
    auto recvbuf = malloc(size);
    auto recvmr = network.registerMr(recvbuf, size,
                                     {ibv::AccessFlag::LOCAL_WRITE, ibv::AccessFlag::REMOTE_WRITE});
    auto remoteMr = ibv::memoryregion::RemoteAddress{reinterpret_cast<uintptr_t>(recvbuf),
                                                     recvmr->getRkey()};

    l5::util::tcp::write(c.socket, &remoteMr, sizeof(remoteMr));
    l5::util::tcp::read(c.socket, &remoteMr, sizeof(remoteMr));

    auto write = defs::createWriteWithImm(sendmr->getSlice(), remoteMr, immData);
    c.rcqp->postWorkRequest(write);

    cq.pollSendCompletionQueueBlocking(ibv::workcompletion::Opcode::RDMA_WRITE);

    auto recv = ibv::workrequest::Recv{};
    recv.setSge(nullptr, 0);
    c.rcqp->postRecvRequest(recv);

    cq.pollRecvWorkCompletionBlocking();
    return recvbuf;

}


defs::GlobalAddress Node::sendData(defs::SendingData data, defs::IMMDATA immData, Connection &c) {
    std::cout << "sendable: " << data.data << std::endl;
    auto &cq = network.getSharedCompletionQueue();
    auto sendmr = network.registerMr(&data, sizeof(defs::SendingData), {});
    auto recvbuf = malloc(sizeof(defs::SendGlobalAddr));
    auto recvmr = network.registerMr(recvbuf, sizeof(defs::SendGlobalAddr),
                                     {ibv::AccessFlag::LOCAL_WRITE, ibv::AccessFlag::REMOTE_WRITE});
    auto remoteMr = ibv::memoryregion::RemoteAddress{reinterpret_cast<uintptr_t>(recvbuf),
                                                     recvmr->getRkey()};

    l5::util::tcp::write(c.socket, &remoteMr, sizeof(remoteMr));
    l5::util::tcp::read(c.socket, &remoteMr, sizeof(remoteMr));

    auto write = defs::createWriteWithImm(sendmr->getSlice(), remoteMr, immData);
    c.rcqp->postWorkRequest(write);
    cq.pollSendCompletionQueueBlocking(ibv::workcompletion::Opcode::RDMA_WRITE);

    auto recv = ibv::workrequest::Recv{};
    recv.setSge(nullptr, 0);
    c.rcqp->postRecvRequest(recv);

    auto wc = cq.pollRecvWorkCompletionBlocking();

    auto newImmData = wc.getImmData();
    std::cout << "got immdata: " << newImmData << std::endl;
    if (newImmData == defs::IMMDATA::INVALIDATE) {
        std::cout << "before invalidation: " << c.socket.get() << ", " << c.rcqp->getQPN()
                  << std::endl;
        prepareForInvalidate(cq, c);
        std::cout << "should be changed: " << c.socket.get() << ", " << c.rcqp->getQPN() << &c
                  << std::endl;

    }
    auto sga = reinterpret_cast<defs::SendGlobalAddr *>(recvbuf);
    return defs::GlobalAddress(*sga);
}

bool Node::sendLock(Lock lock, defs::IMMDATA immData, Connection &c) {
    auto &cq = network.getSharedCompletionQueue();
    auto sendmr = network.registerMr(&lock, sizeof(Lock), {});
    auto recvbuf = new Lock();
    auto recvmr = network.registerMr(recvbuf, sizeof(Lock),
                                     {ibv::AccessFlag::LOCAL_WRITE, ibv::AccessFlag::REMOTE_WRITE});
    auto remoteMr = ibv::memoryregion::RemoteAddress{reinterpret_cast<uintptr_t>(recvbuf),
                                                     recvmr->getRkey()};

    l5::util::tcp::write(c.socket, &remoteMr, sizeof(remoteMr));
    l5::util::tcp::read(c.socket, &remoteMr, sizeof(remoteMr));

    auto write = defs::createWriteWithImm(sendmr->getSlice(), remoteMr, immData);
    c.rcqp->postWorkRequest(write);
    auto recv = ibv::workrequest::Recv{};
    recv.setSge(nullptr, 0);
    c.rcqp->postRecvRequest(recv);
    cq.pollSendCompletionQueueBlocking(ibv::workcompletion::Opcode::RDMA_WRITE);

    cq.pollRecvWorkCompletionBlocking();
    auto result = reinterpret_cast<bool *>(recvbuf);
    return *result;

}


void Node::prepareForInvalidate(rdma::CompletionQueuePair &cq, Connection &c) {
    std::cout << "invalidating cache" << std::endl;
    auto recv = ibv::workrequest::Recv{};
    recv.setSge(nullptr, 0);
    c.rcqp->postRecvRequest(recv);
    cq.pollRecvWorkCompletionBlocking();
    c.rcqp->setToResetState();
    c.socket.close();
    connectAndReceive(id);
    auto newcon = connectClientSocket(3000);
    std::cout << c.socket.get() << ", " << c.rcqp->getQPN() << std::endl;
    std::cout << c.socket.get() << ", " << c.rcqp->getQPN() << &c << std::endl;
    c = std::move(newcon);
}

void Node::broadcastInvalidations(std::vector<uint16_t> nodes,
                                  defs::GlobalAddress gaddr) {
    for (const auto &n: nodes) {
        std::cout << "invalidation of node " << n << std::endl;
        auto invalidateClient = fork();
        if (invalidateClient == 0) {
            auto connection = connectClientSocket(n);
            sendAddress(gaddr.sendable(id), defs::IMMDATA::INVALIDATE, connection);
            closeClientSocket(connection);
        }

    }
    std::cout << "done invaldating" << std::endl;
}


void Node::sendFile(Connection &c, MaFile &file) {
    std::cout << "filesize: " << file.size() << std::endl;
    size_t blocksize = defs::MAX_BLOCK_SIZE;
    auto fileinfo = defs::FileInfo{file.size(), blocksize};

    auto &cq = network.getSharedCompletionQueue();
    auto sendmr = network.registerMr(&fileinfo, blocksize,
                                     {ibv::AccessFlag::LOCAL_WRITE, ibv::AccessFlag::REMOTE_WRITE});
    auto recvbuf = true;
    auto recvmr = network.registerMr(&recvbuf, sizeof(bool),
                                     {ibv::AccessFlag::LOCAL_WRITE, ibv::AccessFlag::REMOTE_WRITE});
    auto remoteMr = ibv::memoryregion::RemoteAddress{reinterpret_cast<uintptr_t>(&recvbuf),
                                                     recvmr->getRkey()};


    l5::util::tcp::write(c.socket, &remoteMr, sizeof(remoteMr));
    l5::util::tcp::read(c.socket, &remoteMr, sizeof(remoteMr));

    auto write = defs::createWriteWithImm(sendmr->getSlice(), remoteMr, defs::IMMDATA::FILE);
    c.rcqp->postWorkRequest(write);
    cq.pollSendCompletionQueueBlocking(ibv::workcompletion::Opcode::RDMA_WRITE);

    auto recv = ibv::workrequest::Recv{};
    recv.setSge(nullptr, 0);
    c.rcqp->postRecvRequest(recv);

    cq.pollRecvWorkCompletionBlocking();
    if (recvbuf) {
        size_t offset = 0;

        std::vector<char> better_data;
        better_data.resize(blocksize);

        char *block = &better_data[0];

        while (offset < file.size()) {
            auto sizetoread =
                    (file.size() - offset) > blocksize ? blocksize : (file.size() - offset);
            file.read_block(offset, sizetoread, block);
            offset = offset + sizetoread;
            //    std::cout << "blocksize: " << blocksize << ", block: " << block << std::endl;
            auto data = reinterpret_cast<uint64_t *>(block);
            sendmr = network.registerMr(data, sizeof(block), {ibv::AccessFlag::LOCAL_WRITE,
                                                              ibv::AccessFlag::REMOTE_WRITE});
            write = defs::createWriteWithImm(sendmr->getSlice(), remoteMr,
                                             static_cast<defs::IMMDATA>(sizetoread));
            c.rcqp->postWorkRequest(write);
            cq.pollSendCompletionQueueBlocking(ibv::workcompletion::Opcode::RDMA_WRITE);

            recv = ibv::workrequest::Recv{};
            recv.setSge(nullptr, 0);
            c.rcqp->postRecvRequest(recv);
            cq.pollRecvWorkCompletionBlocking();
            if (!recvbuf) { throw; }
        }
    }
    file.close();
}

void
Node::sendReadFile(defs::ReadFileData data, defs::IMMDATA immData, Connection &c, char *block) {
    auto &cq = network.getSharedCompletionQueue();
    auto sendmr = network.registerMr(&data, sizeof(defs::ReadFileData), {});
    auto recvmr = network.registerMr(block, data.size,
                                     {ibv::AccessFlag::LOCAL_WRITE, ibv::AccessFlag::REMOTE_WRITE});
    auto remoteMr = ibv::memoryregion::RemoteAddress{reinterpret_cast<uintptr_t>(block),
                                                     recvmr->getRkey()};

    l5::util::tcp::write(c.socket, &remoteMr, sizeof(remoteMr));
    l5::util::tcp::read(c.socket, &remoteMr, sizeof(remoteMr));

    auto write = defs::createWriteWithImm(sendmr->getSlice(), remoteMr, immData);
    c.rcqp->postWorkRequest(write);
    cq.pollSendCompletionQueueBlocking(ibv::workcompletion::Opcode::RDMA_WRITE);
    auto recv = ibv::workrequest::Recv{};
    recv.setSge(nullptr, 0);
    c.rcqp->postRecvRequest(recv);

    cq.pollRecvWorkCompletionBlocking();
}

defs::GlobalAddress
Node::sendWriteFile(defs::ReadFileData data, defs::IMMDATA immData, Connection &c,
                    uint64_t *block) {
    auto &cq = network.getSharedCompletionQueue();
    auto sendmr = network.registerMr(&data, sizeof(defs::ReadFileData), {});
    auto recvbuf = malloc(sizeof(defs::SendGlobalAddr));
    auto recvmr = network.registerMr(recvbuf, sizeof(defs::SendGlobalAddr),
                                     {ibv::AccessFlag::LOCAL_WRITE, ibv::AccessFlag::REMOTE_WRITE});
    auto remoteMr = ibv::memoryregion::RemoteAddress{reinterpret_cast<uintptr_t>(recvbuf),
                                                     recvmr->getRkey()};

    l5::util::tcp::write(c.socket, &remoteMr, sizeof(remoteMr));
    l5::util::tcp::read(c.socket, &remoteMr, sizeof(remoteMr));

    auto write = defs::createWriteWithImm(sendmr->getSlice(), remoteMr, immData);
    c.rcqp->postWorkRequest(write);
    cq.pollSendCompletionQueueBlocking(ibv::workcompletion::Opcode::RDMA_WRITE);
    auto recv = ibv::workrequest::Recv{};
    recv.setSge(nullptr, 0);
    c.rcqp->postRecvRequest(recv);

    cq.pollRecvWorkCompletionBlocking();
    if (recvbuf) {
        sendmr = network.registerMr(block, data.size,
                                    {ibv::AccessFlag::LOCAL_WRITE, ibv::AccessFlag::REMOTE_WRITE});
        write = defs::createWriteWithImm(sendmr->getSlice(), remoteMr, immData);
        c.rcqp->postWorkRequest(write);
        cq.pollSendCompletionQueueBlocking(ibv::workcompletion::Opcode::RDMA_WRITE);
        auto recv2 = ibv::workrequest::Recv{};
        recv2.setSge(nullptr, 0);
        c.rcqp->postRecvRequest(recv2);

        cq.pollRecvWorkCompletionBlocking();

        auto sga = reinterpret_cast<defs::SendGlobalAddr *>(recvbuf);
        return defs::GlobalAddress(*sga);
    }
    else {
        throw std::runtime_error("Remote Node did not send ACK.");

    }
}
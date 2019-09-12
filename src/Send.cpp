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


    c.recvreg = static_cast<char *>(malloc(defs::BIGBADBUFFER_SIZE*2));
    c.sendreg = static_cast<char *>(malloc(defs::MAX_BLOCK_SIZE));

    c.sendmr = network.registerMr(c.sendreg, defs::MAX_BLOCK_SIZE, {});
    c.recvmr = network.registerMr(c.recvreg, defs::BIGBADBUFFER_SIZE*2,
                                  {ibv::AccessFlag::LOCAL_WRITE, ibv::AccessFlag::REMOTE_WRITE});

    l5::util::tcp::write(c.socket, &remoteAddr, sizeof(remoteAddr));
    l5::util::tcp::read(c.socket, &remoteAddr, sizeof(remoteAddr));

    c.remoteMr = ibv::memoryregion::RemoteAddress{reinterpret_cast<uintptr_t>(c.recvreg),
                                                     c.recvmr->getRkey()};

    l5::util::tcp::write(c.socket, &c.remoteMr, sizeof(c.remoteMr));
    l5::util::tcp::read(c.socket, &c.remoteMr, sizeof(c.remoteMr));

    c.rcqp->connect(remoteAddr);
    std::cout << "connected queuepair." << std::endl;


    return c;
}

void Node::closeClientSocket(Connection &c) {
    std::cout << "closing socket: " << c.socket.get() << std::endl;
    auto fakeLock = Lock{id, LOCK_STATES::UNLOCKED};

    sendLock(fakeLock, defs::RESET, c);
    c.rcqp->setToResetState();
    c.socket.close();
}


void *Node::sendAddress(defs::SendGlobalAddr sga, defs::IMMDATA immData, Connection &c) {
    auto &cq = network.getSharedCompletionQueue();
    auto data = defs::ReadFileData{true, sga, 0, 0};
    std::memcpy(c.sendreg, &data, sizeof(data));

    auto write = defs::createWriteWithImm(c.sendmr->getSlice(), c.remoteMr, immData);

    c.rcqp->postWorkRequest(write);

    cq.pollSendCompletionQueueBlocking(ibv::workcompletion::Opcode::RDMA_WRITE);

    auto recv = ibv::workrequest::Recv{};
    recv.setSge(nullptr, 0);
    c.rcqp->postRecvRequest(recv);

    cq.pollRecvWorkCompletionBlocking();

    return c.recvreg;

}


defs::SendGlobalAddr Node::sendData(defs::SendingData sd, defs::IMMDATA immData, Connection &c) {
    auto &cq = network.getSharedCompletionQueue();
    auto data = defs::ReadFileData{false, sd.sga, 0, sd.size};

    std::memcpy(c.sendreg, &data, sizeof(data));
    auto write = defs::createWriteWithImm(c.sendmr->getSlice(), c.remoteMr, immData);
    c.rcqp->postWorkRequest(write);
    cq.pollSendCompletionQueueBlocking(ibv::workcompletion::Opcode::RDMA_WRITE);

    auto recv = ibv::workrequest::Recv{};
    recv.setSge(nullptr, 0);
    c.rcqp->postRecvRequest(recv);

    cq.pollRecvWorkCompletionBlocking();
    std::memcpy(c.sendreg, sd.data, sd.size);
    write = defs::createWriteWithImm(c.sendmr->getSlice(), c.remoteMr, immData);
    c.rcqp->postWorkRequest(write);
    cq.pollSendCompletionQueueBlocking(ibv::workcompletion::Opcode::RDMA_WRITE);

    recv = ibv::workrequest::Recv{};
    recv.setSge(nullptr, 0);
    c.rcqp->postRecvRequest(recv);

    auto wc = cq.pollRecvWorkCompletionBlocking();

    auto newImmData = wc.getImmData();

    std::cout << "got immdata: " << newImmData << std::endl;

    if (newImmData == defs::IMMDATA::INVALIDATE) {
        prepareForInvalidate(cq, c);
    }
    defs::SendGlobalAddr *sga = static_cast<defs::SendGlobalAddr *>(malloc(
            sizeof(defs::SendGlobalAddr)));
    std::memcpy(sga, c.recvreg, sizeof(defs::SendGlobalAddr));
    return *sga;

}

bool Node::sendLock(Lock lock, defs::IMMDATA immData, Connection &c) {
    auto &cq = network.getSharedCompletionQueue();
    auto data = defs::ReadFileData{false, defs::SendGlobalAddr{0, 0,0,id,false}, 0, 0};

    auto recv = ibv::workrequest::Recv{};
    recv.setSge(nullptr, 0);
    c.rcqp->postRecvRequest(recv);

    std::memcpy(c.sendreg, &data, sizeof(data));
    auto write = defs::createWriteWithImm(c.sendmr->getSlice(), c.remoteMr, immData);
    c.rcqp->postWorkRequest(write);

    std::cout << "send" << std::endl;

    cq.pollSendCompletionQueueBlocking(ibv::workcompletion::Opcode::RDMA_WRITE);


    std::cout << "receive" << std::endl;

    cq.pollRecvWorkCompletionBlocking();
    std::memcpy(c.sendreg, &lock, sizeof(Lock));

    write = defs::createWriteWithImm(c.sendmr->getSlice(), c.remoteMr, immData);
    c.rcqp->postWorkRequest(write);
    std::cout << "send" << std::endl;
    recv = ibv::workrequest::Recv{};
    recv.setSge(nullptr, 0);
    c.rcqp->postRecvRequest(recv);

    cq.pollSendCompletionQueueBlocking(ibv::workcompletion::Opcode::RDMA_WRITE);


    std::cout << "receive" << std::endl;

    cq.pollRecvWorkCompletionBlocking();
    bool res = false;
    memcpy(&res, c.recvreg, sizeof(bool));
    return res;

}


void Node::prepareForInvalidate(rdma::CompletionQueuePair &cq, Connection &c) {
    auto recv = ibv::workrequest::Recv{};
    recv.setSge(nullptr, 0);
    c.rcqp->postRecvRequest(recv);
    cq.pollRecvWorkCompletionBlocking();
    c.rcqp->setToResetState();
    c.socket.close();
    connectAndReceive(id);
    auto newcon = connectClientSocket(3000);
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

    memcpy(c.sendreg, &data, sizeof(defs::ReadFileData));
    auto write = defs::createWriteWithImm(c.sendmr->getSlice(), c.remoteMr, immData);
    c.rcqp->postWorkRequest(write);
    cq.pollSendCompletionQueueBlocking(ibv::workcompletion::Opcode::RDMA_WRITE);
    auto recv = ibv::workrequest::Recv{};
    recv.setSge(nullptr, 0);
    c.rcqp->postRecvRequest(recv);

    cq.pollRecvWorkCompletionBlocking();
    memcpy(block,c.recvreg,data.size);
}

void Node::sendWriteFile(defs::ReadFileData data, defs::IMMDATA immData, Connection &c,
                    uint64_t *block, defs::SendGlobalAddr * buffer) {
    auto &cq = network.getSharedCompletionQueue();
    auto sendmr = network.registerMr(&data, sizeof(defs::ReadFileData), {});
    auto recvmr = network.registerMr(buffer, sizeof(defs::SendGlobalAddr),
                                     {ibv::AccessFlag::LOCAL_WRITE, ibv::AccessFlag::REMOTE_WRITE});
    auto remoteMr = ibv::memoryregion::RemoteAddress{reinterpret_cast<uintptr_t>(buffer),
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
    if (buffer) {
        sendmr = network.registerMr(block, data.size,
                                    {ibv::AccessFlag::LOCAL_WRITE, ibv::AccessFlag::REMOTE_WRITE});
        write = defs::createWriteWithImm(sendmr->getSlice(), remoteMr, immData);
        c.rcqp->postWorkRequest(write);
        cq.pollSendCompletionQueueBlocking(ibv::workcompletion::Opcode::RDMA_WRITE);
        auto recv2 = ibv::workrequest::Recv{};
        recv2.setSge(nullptr, 0);
        c.rcqp->postRecvRequest(recv2);

        cq.pollRecvWorkCompletionBlocking();
    }
    else {
        throw std::runtime_error("Remote Node did not send ACK.");

    }
}
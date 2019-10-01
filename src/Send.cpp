//
// Created by Magdalena Pröbstl on 2019-06-15.
//

#include "Node.h"


void Node::connectClientSocket(uint16_t port) {
    auto socket = l5::util::Socket::create();
    for (int i = 0;; ++i) {
        try {
            l5::util::tcp::connect(socket, defs::ip, port);
            break;
        } catch (std::exception &e) {
            std::this_thread::sleep_for(std::chrono_literals::operator ""ms(20));
            if (i > 10) throw;
        }
    }
    c.connect(network, socket);
    auto recv = ibv::workrequest::Recv{};
    recv.setSge(nullptr, 0);
    c.rcqp->postRecvRequest(recv);
    // TODO: move to ctor and check if free is needed
}

void Node::closeClientSocket() {
    auto fakeLock = Lock{id, LOCK_STATES::UNLOCKED, id};

    sendLock(fakeLock, defs::RESET);
    c.close();
}


char *Node::sendAddress(defs::SendGlobalAddr sga, defs::IMMDATA immData) {
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


defs::SendGlobalAddr Node::sendData(defs::SendingData sd, defs::IMMDATA immData) {
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
        recv = ibv::workrequest::Recv{};
        recv.setSge(nullptr, 0);
        c.rcqp->postRecvRequest(recv);
        std::cout << "waiting" << std::endl;

        cq.pollRecvWorkCompletionBlocking();

        defs::SendGlobalAddr sga{};
        memcpy(&sga, c.recvreg, sizeof(defs::SendGlobalAddr));

        prepareForInvalidate();
        std::this_thread::sleep_for(std::chrono_literals::operator ""ms(100));

        return sga;
    }

    defs::SendGlobalAddr sga{};
    std::memcpy(&sga, c.recvreg, sizeof(defs::SendGlobalAddr));
    return sga;
}

bool Node::sendLock(Lock lock, defs::IMMDATA immData) {
    auto &cq = network.getSharedCompletionQueue();
    auto data = defs::ReadFileData{false, defs::SendGlobalAddr{0, 0, 0, id, false}, 0, 0};

    auto recv = ibv::workrequest::Recv{};
    recv.setSge(nullptr, 0);
    c.rcqp->postRecvRequest(recv);

    std::memcpy(c.sendreg, &data, sizeof(data));
    auto write = defs::createWriteWithImm(c.sendmr->getSlice(), c.remoteMr, immData);
    c.rcqp->postWorkRequest(write);

    cq.pollSendCompletionQueueBlocking(ibv::workcompletion::Opcode::RDMA_WRITE);

    cq.pollRecvWorkCompletionBlocking();
    std::memcpy(c.sendreg, &lock, sizeof(Lock));

    write = defs::createWriteWithImm(c.sendmr->getSlice(), c.remoteMr, immData);
    c.rcqp->postWorkRequest(write);

    recv = ibv::workrequest::Recv{};
    recv.setSge(nullptr, 0);
    c.rcqp->postRecvRequest(recv);

    cq.pollSendCompletionQueueBlocking(ibv::workcompletion::Opcode::RDMA_WRITE);

    cq.pollRecvWorkCompletionBlocking();
    bool res = false;
    memcpy(&res, c.recvreg, sizeof(bool));
    return res;

}

void Node::prepareForInvalidate() {
    c.close();
    connectAndReceive(id);
}

void Node::broadcastInvalidations(std::vector<uint16_t> &nodes,
                                  defs::GlobalAddress gaddr) {
    for (const auto &n: nodes) {
        std::cout << "invalidation of node " << n << std::endl;
        auto invalidateClient = fork();
        if (invalidateClient == 0) {
            connectClientSocket(n);
            sendAddress(gaddr.sendable(id), defs::IMMDATA::INVALIDATE);
            closeClientSocket();
        }
    }
    std::cout << "done invaldating" << std::endl;
}


void Node::sendFile(MaFile &file) {
    std::cout << "filesize: " << file.size() << std::endl;
    size_t blocksize = defs::MAX_BLOCK_SIZE;
    auto fileinfo = defs::FileInfo{file.size(), blocksize};

    auto &cq = network.getSharedCompletionQueue();
    memcpy(c.sendreg, &fileinfo, sizeof(defs::FileInfo));

    auto write = defs::createWriteWithImm(c.sendmr->getSlice(), c.remoteMr, defs::IMMDATA::FILE);
    c.rcqp->postWorkRequest(write);
    cq.pollSendCompletionQueueBlocking(ibv::workcompletion::Opcode::RDMA_WRITE);

    auto recv = ibv::workrequest::Recv{};
    recv.setSge(nullptr, 0);
    c.rcqp->postRecvRequest(recv);

    cq.pollRecvWorkCompletionBlocking();
    if (c.recvreg) {
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
            memcpy(c.sendreg, data, blocksize);
            write = defs::createWriteWithImm(c.sendmr->getSlice(), c.remoteMr,
                                             static_cast<defs::IMMDATA>(sizetoread));
            c.rcqp->postWorkRequest(write);
            cq.pollSendCompletionQueueBlocking(ibv::workcompletion::Opcode::RDMA_WRITE);

            recv = ibv::workrequest::Recv{};
            recv.setSge(nullptr, 0);
            c.rcqp->postRecvRequest(recv);
            cq.pollRecvWorkCompletionBlocking();
            if (!c.recvreg) { throw; }
        }
    }
    file.close();
}

void
Node::sendReadFile(defs::ReadFileData data, defs::IMMDATA immData, char *block) {
    auto &cq = network.getSharedCompletionQueue();

    memcpy(c.sendreg, &data, sizeof(defs::ReadFileData));
    auto write = defs::createWriteWithImm(c.sendmr->getSlice(), c.remoteMr, immData);
    c.rcqp->postWorkRequest(write);
    cq.pollSendCompletionQueueBlocking(ibv::workcompletion::Opcode::RDMA_WRITE);
    auto recv = ibv::workrequest::Recv{};
    recv.setSge(nullptr, 0);
    c.rcqp->postRecvRequest(recv);

    cq.pollRecvWorkCompletionBlocking();
    memcpy(block, c.recvreg, data.size);
}

defs::GlobalAddress
Node::sendWriteFile(defs::ReadFileData data, defs::IMMDATA immData, uint64_t *block) {
    auto &cq = network.getSharedCompletionQueue();
    memcpy(c.sendreg, &data, sizeof(defs::ReadFileData));

    auto write = defs::createWriteWithImm(c.sendmr->getSlice(), c.remoteMr, immData);
    c.rcqp->postWorkRequest(write);
    cq.pollSendCompletionQueueBlocking(ibv::workcompletion::Opcode::RDMA_WRITE);
    auto recv = ibv::workrequest::Recv{};
    recv.setSge(nullptr, 0);
    c.rcqp->postRecvRequest(recv);

    cq.pollRecvWorkCompletionBlocking();
    if (c.recvreg) {
        memcpy(c.sendreg, block, data.size);
        write = defs::createWriteWithImm(c.sendmr->getSlice(), c.remoteMr, immData);
        c.rcqp->postWorkRequest(write);
        cq.pollSendCompletionQueueBlocking(ibv::workcompletion::Opcode::RDMA_WRITE);
        auto recv2 = ibv::workrequest::Recv{};
        recv2.setSge(nullptr, 0);
        c.rcqp->postRecvRequest(recv2);

        cq.pollRecvWorkCompletionBlocking();
        return defs::GlobalAddress(*reinterpret_cast<defs::SendGlobalAddr *>(c.recvreg));
    } else {
        throw std::runtime_error("Remote Node did not send ACK.");
    }
}
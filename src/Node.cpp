//
// Created by Magdalena Pröbstl on 2019-04-11.
//

#include <arpa/inet.h>
#include "Node.h"
#include "../util/socket/tcp.h"
#include <random>
#include <cstdint>
#include <malloc.h>


constexpr size_t BIGBADBUFFER_SIZE = 1024 * 1024 * 8; // 8MB
auto generator = std::default_random_engine{};
auto randomDistribution = std::uniform_int_distribution<uint32_t>{0, 100};


Node::Node() : network(), id(), rcqp(network, network.getSharedCompletionQueue()), locklist() {
    id = randomDistribution(generator);
}

bool Node::isLocal(GlobalAddress *gaddr) {
    return getNodeId(gaddr) == id;
}

GlobalAddress *Node::sendAddress(void *data, size_t size, uint32_t immData) {
    auto &cq = network.getSharedCompletionQueue();
    auto socket = l5::util::Socket::create();
    auto remoteAddr = rdma::Address{network.getGID(), rcqp.getQPN(), network.getLID()};
    auto sendmr = network.registerMr(data, size, {});
    auto recvbuf = malloc(size);
    auto recvmr = network.registerMr(recvbuf, size,
                                     {ibv::AccessFlag::LOCAL_WRITE, ibv::AccessFlag::REMOTE_WRITE});

    l5::util::tcp::connect(socket, ip, port);
    l5::util::tcp::write(socket, &remoteAddr, sizeof(remoteAddr));
    l5::util::tcp::read(socket, &remoteAddr, sizeof(remoteAddr));
    auto remoteMr = ibv::memoryregion::RemoteAddress{reinterpret_cast<uintptr_t>(recvbuf),
                                                     recvmr->getRkey()};
    l5::util::tcp::write(socket, &remoteMr, sizeof(remoteMr));
    l5::util::tcp::read(socket, &remoteMr, sizeof(remoteMr));

    rcqp.connect(remoteAddr);
    auto write = ibv::workrequest::Simple<ibv::workrequest::WriteWithImm>{};
    write.setLocalAddress(sendmr->getSlice());
    write.setInline();
    write.setSignaled();
    write.setRemoteAddress(remoteMr);
    write.setImmData(immData);
    rcqp.postWorkRequest(write);
    cq.pollSendCompletionQueueBlocking(ibv::workcompletion::Opcode::RDMA_WRITE);

    auto recv = ibv::workrequest::Recv{};
    recv.setId(randomDistribution(generator));
    recv.setSge(nullptr, 0);
    // *first* post recv to always have a recv pending, so incoming send don't get swallowed
    rcqp.postRecvRequest(recv);
    auto wc = cq.pollRecvWorkCompletionBlocking();
    auto recvImmData = wc.getImmData();
    std::cout << "got this immdata: " << recvImmData << std::endl;

    return reinterpret_cast<GlobalAddress *>(recvbuf);
}


void *Node::sendData(SendData *data, uint32_t immData) {
    auto &cq = network.getSharedCompletionQueue();
    auto socket = l5::util::Socket::create();
    auto remoteAddr = rdma::Address{network.getGID(), rcqp.getQPN(), network.getLID()};
    auto recvbuf = malloc(data->size);
    auto recvmr = network.registerMr(recvbuf, data->size,
                                     {ibv::AccessFlag::LOCAL_WRITE, ibv::AccessFlag::REMOTE_WRITE});
    auto buf = std::vector<uint8_t>{};
    SendData d;
    d.size = data->size;
    buf.resize(data->size + sizeof(SendData));

    memcpy(&buf[0], &d, sizeof(SendData));
    memcpy(&buf[sizeof(SendData)], data, data->size);
    auto sendmr = network.registerMr(buf.data(), data->size, {});

    l5::util::tcp::connect(socket, ip, port);
    l5::util::tcp::write(socket, &remoteAddr, sizeof(remoteAddr));
    l5::util::tcp::read(socket, &remoteAddr, sizeof(remoteAddr));
    auto remoteMr = ibv::memoryregion::RemoteAddress{reinterpret_cast<uintptr_t>(recvbuf),
                                                     recvmr->getRkey()};
    l5::util::tcp::write(socket, &remoteMr, sizeof(remoteMr));
    l5::util::tcp::read(socket, &remoteMr, sizeof(remoteMr));

    rcqp.connect(remoteAddr);
    auto write = ibv::workrequest::Simple<ibv::workrequest::WriteWithImm>{};
    write.setLocalAddress(sendmr->getSlice());
    write.setInline();
    write.setSignaled();
    write.setRemoteAddress(remoteMr);
    write.setImmData(immData);
    rcqp.postWorkRequest(write);
    cq.pollSendCompletionQueueBlocking(ibv::workcompletion::Opcode::RDMA_WRITE);
    auto recv = ibv::workrequest::Recv{};
    recv.setId(randomDistribution(generator));
    recv.setSge(nullptr, 0);
    // *first* post recv to always have a recv pending, so incoming send don't get swallowed
    rcqp.postRecvRequest(recv);
    cq.pollRecvWorkCompletionBlocking();
    return recvbuf;
}


void Node::receive() {
    auto &cq = network.getSharedCompletionQueue();
    auto socket = l5::util::Socket::create();
    auto recvbuf = malloc(BIGBADBUFFER_SIZE * 2);
    auto recvmr = network.registerMr(recvbuf, BIGBADBUFFER_SIZE * 2,
                                     {ibv::AccessFlag::LOCAL_WRITE, ibv::AccessFlag::REMOTE_WRITE});
    auto remoteAddr = rdma::Address{network.getGID(), rcqp.getQPN(), network.getLID()};
    l5::util::tcp::bind(socket, port);
    l5::util::tcp::listen(socket);
    auto acced = l5::util::tcp::accept(socket);
    auto recv = ibv::workrequest::Recv{};
    recv.setId(randomDistribution(generator));
    recv.setSge(nullptr, 0);
    rcqp.postRecvRequest(recv);

    l5::util::tcp::write(acced, &remoteAddr, sizeof(remoteAddr));
    l5::util::tcp::read(acced, &remoteAddr, sizeof(remoteAddr));
    auto remoteMr = ibv::memoryregion::RemoteAddress{reinterpret_cast<uintptr_t>(recvbuf),
                                                     recvmr->getRkey()};
    l5::util::tcp::write(acced, &remoteMr, sizeof(remoteMr));
    l5::util::tcp::read(acced, &remoteMr, sizeof(remoteMr));

    rcqp.connect(remoteAddr);

    auto wc = cq.pollRecvWorkCompletionBlocking();
    auto immData = wc.getImmData();

    if (immData == 1) { //immdata = 1, if it comes from another server
        auto size = reinterpret_cast<size_t *>(recvbuf);
        auto newgaddr = Malloc(size);
        auto sendmr = network.registerMr(newgaddr, sizeof(GlobalAddress), {});
        std::cout << newgaddr->size << ", " << newgaddr->id << ", " << newgaddr->ptr << std::endl;

        auto write = ibv::workrequest::Simple<ibv::workrequest::WriteWithImm>{};
        write.setLocalAddress(sendmr->getSlice());
        write.setInline();
        write.setSignaled();
        write.setRemoteAddress(remoteMr);
        write.setImmData(0);

        rcqp.postWorkRequest(write);
        cq.pollSendCompletionQueueBlocking(ibv::workcompletion::Opcode::RDMA_WRITE);
    } else if (immData == 2) { //immdata = 2, if it is a reply
    } else if (immData == 3) { //immdata = 3, it should be freed
        Free(reinterpret_cast<GlobalAddress *>(recvbuf));
    } else if (immData == 4) { //immdata = 4, write
        auto data = reinterpret_cast<SendData *>(recvbuf);
    } else {
        return;
    }
}


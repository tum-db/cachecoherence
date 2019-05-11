//
// Created by Magdalena Pröbstl on 2019-04-11.
//

#include <arpa/inet.h>
#include "Node.h"
#include "../util/socket/tcp.h"
#include <random>
#include <cstdint>


constexpr size_t BIGBADBUFFER_SIZE = 1024 * 1024 * 8; // 8MB
std::default_random_engine generator;
std::uniform_int_distribution<int> distribution(0, 100);


Node::Node() : network(), id(), rcqp(network, network.getSharedCompletionQueue()) {
    id = distribution(generator);
}

bool Node::isLocal(GlobalAddress gaddr) {
    return getNodeId(gaddr) == id;
}

void Node::send(void *data, size_t size) {

    auto &cq = network.getSharedCompletionQueue();
    auto socket = l5::util::Socket::create();
    auto remoteAddr = rdma::Address{network.getGID(), rcqp.getQPN(), network.getLID()};
    //  auto sendbuf = new std::vector<std::byte>(size);
    auto sendmr = network.registerMr(data, size, {});
    auto recvbuf = malloc(size);
    auto recvmr = network.registerMr(recvbuf, size,
                                     {ibv::AccessFlag::LOCAL_WRITE, ibv::AccessFlag::REMOTE_WRITE});

    l5::util::tcp::connect(socket, ip, port);

//    std::copy(data.begin(), data.end(), sendbuf->begin());
    auto recv = ibv::workrequest::Recv{};
    recv.setId(42);
    recv.setSge(nullptr, 0);
    // *first* post recv to always have a recv pending, so incoming send don't get swallowed
    rcqp.postRecvRequest(recv);

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
    write.setImmData(0);

    rcqp.postWorkRequest(write);
    cq.pollSendCompletionQueueBlocking(ibv::workcompletion::Opcode::RDMA_WRITE);


}

void *Node::receive() {
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
    recv.setId(42);
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

    rcqp.postRecvRequest(recv);

    auto recvPos = wc.getImmData();
    std::cout << recvPos << std::endl;
    //recvbuf.begin() = recvbuf.begin()+recvPos;
    return recvbuf;

}



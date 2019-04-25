//
// Created by Magdalena Pröbstl on 2019-04-11.
//

#include <arpa/inet.h>
#include <random>
#include "Node.h"
#include "../util/socket/tcp.h"

constexpr size_t BIGBADBUFFER_SIZE = 1024 * 1024 * 8; // 8MB



Node::Node(rdma::Network &network) :
        rcqp(network, network.getSharedCompletionQueue()) {
}

auto generator = std::default_random_engine{};
auto randomDistribution = std::uniform_int_distribution<uint32_t>{0, BIGBADBUFFER_SIZE};

void Node::send(rdma::Network &network, std::string data, uint16_t port, char *ip) {

    auto &cq = network.getSharedCompletionQueue();
    sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    inet_pton(AF_INET, ip, &addr.sin_addr);
    auto socket = l5::util::Socket::create();
    auto remoteAddr = rdma::Address{network.getGID(), rcqp.getQPN(), network.getLID()};
    auto sendbuf = std::vector<char>(data.size());
    auto sendmr = network.registerMr(sendbuf.data(), sendbuf.size(), {});
    auto recvbuf = std::vector<char>(
            BIGBADBUFFER_SIZE * 2); // *2 just to be sure everything fits
    auto recvmr = network.registerMr(recvbuf.data(), recvbuf.size(),
                                     {ibv::AccessFlag::LOCAL_WRITE, ibv::AccessFlag::REMOTE_WRITE});

    l5::util::tcp::connect(socket, addr);

    //connectSocket(socket);
    std::copy(data.begin(), data.end(), sendbuf.begin());
    auto recv = ibv::workrequest::Recv{};
    recv.setId(42);
    recv.setSge(nullptr, 0);
    // *first* post recv to always have a recv pending, so incoming send don't get swallowed
    rcqp.postRecvRequest(recv);

    l5::util::tcp::write(socket, &remoteAddr, sizeof(remoteAddr));
    l5::util::tcp::read(socket, &remoteAddr, sizeof(remoteAddr));
    auto remoteMr = ibv::memoryregion::RemoteAddress{
            reinterpret_cast<uintptr_t>(recvbuf.data()),
            recvmr->getRkey()};
    l5::util::tcp::write(socket, &remoteMr, sizeof(remoteMr));
    l5::util::tcp::read(socket, &remoteMr, sizeof(remoteMr));

    rcqp.connect(remoteAddr);
    auto write = ibv::workrequest::Simple<ibv::workrequest::WriteWithImm>{};
    write.setLocalAddress(sendmr->getSlice());
    write.setInline();
    write.setSignaled();

    auto destPos = randomDistribution(generator);
    write.setRemoteAddress(remoteMr.offset(destPos));
    write.setImmData(destPos);

    rcqp.postWorkRequest(write);
    cq.pollSendCompletionQueueBlocking(ibv::workcompletion::Opcode::RDMA_WRITE);


}

std::vector<char, std::allocator<char>> Node::receive(rdma::Network &network, uint16_t port, char *ip) {
    auto &cq = network.getSharedCompletionQueue();
    sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    inet_pton(AF_INET, ip, &addr.sin_addr);
    auto socket = l5::util::Socket::create();
    auto recvbuf = std::vector<char>(BIGBADBUFFER_SIZE * 2);
    auto recvmr = network.registerMr(recvbuf.data(), recvbuf.size(),
                                     {ibv::AccessFlag::LOCAL_WRITE, ibv::AccessFlag::REMOTE_WRITE});
    auto remoteAddr = rdma::Address{network.getGID(), rcqp.getQPN(), network.getLID()};
    l5::util::tcp::bind(socket, addr);
    l5::util::tcp::listen(socket);
    auto acced = l5::util::tcp::accept(socket);
    auto recv = ibv::workrequest::Recv{};
    recv.setId(42);
    recv.setSge(nullptr, 0);
    // *first* post recv to always have a recv pending, so incoming send don't get swallowed
    rcqp.postRecvRequest(recv);

    l5::util::tcp::write(acced, &remoteAddr, sizeof(remoteAddr));
    l5::util::tcp::read(acced, &remoteAddr, sizeof(remoteAddr));
    auto remoteMr = ibv::memoryregion::RemoteAddress{
            reinterpret_cast<uintptr_t>(recvbuf.data()), recvmr->getRkey()};

    l5::util::tcp::write(acced, &remoteMr, sizeof(remoteMr));
    l5::util::tcp::read(acced, &remoteMr, sizeof(remoteMr));

    rcqp.connect(remoteAddr);

    auto wc = cq.pollRecvWorkCompletionBlocking();

    rcqp.postRecvRequest(recv);

    auto recvPos = wc.getImmData();

    return recvbuf;

}

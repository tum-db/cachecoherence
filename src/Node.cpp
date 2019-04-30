//
// Created by Magdalena Pröbstl on 2019-04-11.
//

#include <arpa/inet.h>
#include "Node.h"
#include "../util/socket/tcp.h"

constexpr size_t BIGBADBUFFER_SIZE = 1024 * 1024 * 8; // 8MB



Node::Node(rdma::Network &network) :
        rcqp(network, network.getSharedCompletionQueue()) {
}

void Node::send(rdma::Network &network, std::string data,  GlobalAddress gaddr) {

    auto &cq = network.getSharedCompletionQueue();
    auto socket = l5::util::Socket::create();
    auto remoteAddr = rdma::Address{network.getGID(), rcqp.getQPN(), network.getLID()};
    auto sendbuf = std::vector<char>(data.size());
    auto sendmr = network.registerMr(sendbuf.data(), sendbuf.size(), {});
    auto recvbuf = std::vector<char>(
            BIGBADBUFFER_SIZE * 2); // *2 just to be sure everything fits
    auto recvmr = network.registerMr(recvbuf.data(), recvbuf.size(),
                                     {ibv::AccessFlag::LOCAL_WRITE, ibv::AccessFlag::REMOTE_WRITE});

    l5::util::tcp::connect(socket, gaddr.getSockAddr());

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
    write.setRemoteAddress(remoteMr);
    write.setImmData(0);

    rcqp.postWorkRequest(write);
    cq.pollSendCompletionQueueBlocking(ibv::workcompletion::Opcode::RDMA_WRITE);


}

std::vector<char, std::allocator<char>> Node::receive(rdma::Network &network, GlobalAddress gaddr) {
    auto &cq = network.getSharedCompletionQueue();
    auto socket = l5::util::Socket::create();
    auto recvbuf = std::vector<char>(BIGBADBUFFER_SIZE * 2);
    auto recvmr = network.registerMr(recvbuf.data(), recvbuf.size(),
                                     {ibv::AccessFlag::LOCAL_WRITE, ibv::AccessFlag::REMOTE_WRITE});
    auto remoteAddr = rdma::Address{network.getGID(), rcqp.getQPN(), network.getLID()};
    l5::util::tcp::bind(socket, gaddr.getSockAddr());
    l5::util::tcp::listen(socket);
    auto acced = l5::util::tcp::accept(socket);
    auto recv = ibv::workrequest::Recv{};
    recv.setId(42);
    recv.setSge(nullptr, 0);
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
    recvbuf.begin() = recvbuf.begin()+recvPos;
    return recvbuf;

}



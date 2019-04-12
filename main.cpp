#include <iostream>
#include <cstddef>
#include <libibverbscpp.h>
#include <vector>
#include <thread>
#include <arpa/inet.h>
#include "rdma/Network.hpp"
#include "rdma/QueuePair.hpp"
#include "rdma/RcQueuePair.h"
#include "util/RDMANetworking.h"
#include "src/Node.h"
#include "util/socket/tcp.h"
#include "util/socket/domain.h"
#include <netinet/in.h>
#include <chrono>

constexpr uint16_t port = 1337;
const char *ip = "127.0.0.1";

void connectSocket(const l5::util::Socket &socket) {
    sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    inet_pton(AF_INET, ip, &addr.sin_addr);
    for (int i = 0;; ++i) {
        try {
            l5::util::tcp::connect(socket, addr);
            break;
        } catch (...) {
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
            if (i > 30) throw;
        }
    }
}

int main() {
    std::string data(2u, 'A');

    auto net = rdma::Network();
    auto completionqp = net.newCompletionQueuePair();
    auto node = Node(net, completionqp);

    // connect QueuePair
    auto localQpn = node.rcqp.getQPN();
    auto localLid = net.getLID();
    auto localGid = net.getGID();
    std::cout << "Local QueuePair number: " << localQpn << '\n';
    std::cout << "Local Lid: " << localLid << '\n';
    std::cout << "Local Gid: Subnetprefix: " << localGid.getSubnetPrefix() << ", InterfaceId: "
              << localGid.getInterfaceId() << '\n';
    std::cout
            << "--------------------------------------------------------------------------------\n";
    std::cout << "Please enter remote adddress\n";
    std::cout << "Remote QueuePair number: ";
    uint32_t remoteQpn;
    std::cin >> remoteQpn;
    std::cout << "Remote Lid: ";
    uint16_t remoteLid;
    std::cin >> remoteLid;
    std::cout << "Remote Gid: Subnetprefix: ";
    uint64_t subnetPrefix;
    std::cin >> subnetPrefix;
    std::cout << "InterfaceId: ";
    uint64_t interfaceId;
    std::cin >> interfaceId;

    auto rawGid = ibv_gid{}; // FIXME: add a proper constructor for Gid
    rawGid.global.subnet_prefix = subnetPrefix;
    rawGid.global.interface_id = interfaceId;
    auto remoteGid = ibv::Gid{rawGid};

    auto add = rdma::Address{remoteGid, remoteQpn, remoteLid};
    node.rcqp.connect(add);
    constexpr size_t BIGBADBUFFER_SIZE = 1024 * 1024 * 8; // 8MB
    auto recvbuf = std::vector<char>(BIGBADBUFFER_SIZE * 2); // *2 just to be sure everything fits
    auto recvmr = net.registerMr(recvbuf.data(), recvbuf.size(),
                                 {ibv::AccessFlag::LOCAL_WRITE, ibv::AccessFlag::REMOTE_WRITE});
    /* auto wr = ibv::workrequest::Simple<ibv::workrequest::Write>();
     remoteMr.address = add;
     wr.setRemoteAddress(remoteMr);
     wr.setLocalAddress(rdma::Address{localGid, localQpn, localLid});
     node.rcqp.postWorkRequest(wr);
 */
    auto sendbuf = std::vector<char>(data.size());
    auto sendmr = net.registerMr(sendbuf.data(), sendbuf.size(), {});


    auto socket = l5::util::Socket::create();
    l5::util::domain::connect(socket, );

    std::copy(data.begin(), data.end(), sendbuf.begin());

    auto recv = ibv::workrequest::Recv{};
    recv.setId(42);
    // *first* post recv to always have a recv pending, so incoming send don't get swallowed
    node.rcqp.postRecvRequest(recv);

    auto remoteAddr = rdma::Address{net.getGID(), localQpn, net.getLID()};
    l5::util::tcp::write(socket, &remoteAddr, sizeof(remoteAddr));
    l5::util::tcp::read(socket, &remoteAddr, sizeof(remoteAddr));
    auto remoteMr = ibv::memoryregion::RemoteAddress{reinterpret_cast<uintptr_t>(recvbuf.data()),
                                                     recvmr->getRkey()};
    l5::util::tcp::write(socket, &remoteMr, sizeof(remoteMr));
    l5::util::tcp::read(socket, &remoteMr, sizeof(remoteMr));

    node.rcqp.connect(remoteAddr);

    auto write = ibv::workrequest::Simple<ibv::workrequest::WriteWithImm>{};
    write.setLocalAddress(sendmr->getSlice());
    write.setInline();
    write.setSignaled();


    std::cout << "Hello" << std::endl;
    // no explicit teardown needed
}
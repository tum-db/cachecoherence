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
#include <fstream>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <random>

constexpr uint16_t port = 3000;
const char *ip = "127.0.0.1";
constexpr size_t BIGBADBUFFER_SIZE = 1024 * 1024 * 8; // 8MB
auto generator = std::default_random_engine{};

auto randomDistribution = std::uniform_int_distribution<uint32_t>{0, BIGBADBUFFER_SIZE};


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

void error(const char *msg) {
    perror(msg);
    exit(1);
}

int main() {
    auto dataSize = 2u;
    std::string data(dataSize, 'A');
    auto net = rdma::Network();
    auto &cq = net.getSharedCompletionQueue();
    auto node = Node(net);
    //setup client or server
    std::cout << "Server or Client? (0 = server, 1 = client): ";
    uint16_t servOcli; // 0 = server, 1 = client
    std::cin >> servOcli;
    sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    inet_pton(AF_INET, ip, &addr.sin_addr);
    auto socket = l5::util::Socket::create();
    auto localQpn = node.rcqp.getQPN();
    auto localLid = net.getLID();
    auto localGid = net.getGID();
    auto recvbuf = std::vector<char>(
            BIGBADBUFFER_SIZE * 2); // *2 just to be sure everything fits
    auto recvmr = net.registerMr(recvbuf.data(), recvbuf.size(),
                                 {ibv::AccessFlag::LOCAL_WRITE, ibv::AccessFlag::REMOTE_WRITE});


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
    auto remoteAddr = rdma::Address{remoteGid, remoteQpn, remoteLid};

    auto sendbuf = std::vector<char>(data.size());
    auto sendmr = net.registerMr(sendbuf.data(), sendbuf.size(), {});

    if (servOcli == 0) {
        l5::util::tcp::bind(socket, addr);
        l5::util::tcp::listen(socket);

        auto acced = l5::util::tcp::accept(socket);

        auto recv = ibv::workrequest::Recv{};
        recv.setId(42);
        recv.setSge(nullptr, 0);
        // *first* post recv to always have a recv pending, so incoming send don't get swallowed
        node.rcqp.postRecvRequest(recv);

        l5::util::tcp::write(acced, &remoteAddr, sizeof(remoteAddr));
        l5::util::tcp::read(acced, &remoteAddr, sizeof(remoteAddr));
        auto remoteMr = ibv::memoryregion::RemoteAddress{
                reinterpret_cast<uintptr_t>(recvbuf.data()), recvmr->getRkey()};
        l5::util::tcp::write(acced, &remoteMr, sizeof(remoteMr));
        l5::util::tcp::read(acced, &remoteMr, sizeof(remoteMr));

        node.rcqp.connect(remoteAddr);

        //   l5::util::tcp::read(socket, &remoteMr, sizeof(remoteMr));

        auto write = ibv::workrequest::Simple<ibv::workrequest::WriteWithImm>{};
        write.setLocalAddress(sendmr->getSlice());
        write.setInline();
        write.setSignaled();

        auto wc = node.rcqp..pollRecvWorkCompletionBlocking();
        node.rcqp.postRecvRequest(recv);

        auto recvPos = wc.getImmData();

        auto begin = recvbuf.begin() + recvPos;
        auto end = begin + dataSize;
        std::copy(begin, end, sendbuf.begin());
        // echo back the received data
        auto destPos = randomDistribution(generator);
        write.setRemoteAddress(remoteMr.offset(destPos));
        write.setImmData(destPos);
        node.rcqp.postWorkRequest(write);
        cq.pollSendCompletionQueueBlocking(ibv::workcompletion::Opcode::RDMA_WRITE);

    } else if (servOcli == 1) {
        connectSocket(socket);

        std::copy(data.begin(), data.end(), sendbuf.begin());
        auto recv = ibv::workrequest::Recv{};
        recv.setId(42);
        recv.setSge(nullptr, 0);
        // *first* post recv to always have a recv pending, so incoming send don't get swallowed
        node.rcqp.postRecvRequest(recv);


        l5::util::tcp::write(socket, &remoteAddr, sizeof(remoteAddr));
        l5::util::tcp::read(socket, &remoteAddr, sizeof(remoteAddr));
        auto remoteMr = ibv::memoryregion::RemoteAddress{
                reinterpret_cast<uintptr_t>(recvbuf.data()),
                recvmr->getRkey()};
        l5::util::tcp::write(socket, &remoteMr, sizeof(remoteMr));
        l5::util::tcp::read(socket, &remoteMr, sizeof(remoteMr));

        node.rcqp.connect(remoteAddr);
        auto write = ibv::workrequest::Simple<ibv::workrequest::Write>{};
        write.setLocalAddress(sendmr->getSlice());
        write.setInline();
        write.setSignaled();

        auto destPos = randomDistribution(generator);
        write.setRemoteAddress(remoteMr.offset(destPos));
        write.setImmData(destPos);
        node.rcqp.postWorkRequest(write);
        cq.pollSendCompletionQueueBlocking(ibv::workcompletion::Opcode::RDMA_WRITE);

        auto wc = cq.pollRecvWorkCompletionBlocking();
        auto recvPos = wc.getImmData();

        node.rcqp.postRecvRequest(recv);

        auto begin = recvbuf.begin() + recvPos;
        auto end = begin + data.size();
        // check if the data is still the same
        if (not std::equal(begin, end, data.begin(), data.end())) {
            throw;
        }

    }


    std::cout << "Hello" << std::endl;
    // no explicit teardown needed
}
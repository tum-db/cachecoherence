#include <iostream>
#include <cstddef>
#include <libibverbscpp.h>
#include <vector>
#include "rdma/Network.hpp"
#include "rdma/QueuePair.hpp"
#include "rdma/RcQueuePair.h"
#include "util/RDMANetworking.h"
#include "src/Node.h"

int main() {
    auto net = rdma::Network();
    auto completionqp = net.newCompletionQueuePair();
    auto node = Node(net,completionqp);
    auto sock = l5::util::Socket::create(AF_INET,SOCK_STREAM,0);
    auto rdmaNetworking = l5::util::RDMANetworking(sock);

    // connect QueuePair
    auto localQpn = node.rcqp.getQPN();
    auto localLid = net.getLID();
    auto localGid = net.getGID();
    std::cout << "Local QueuePair number: " << localQpn << '\n';
    std::cout << "Local Lid: " << localLid << '\n';
    std::cout << "Local Gid: Subnetprefix: " << localGid.getSubnetPrefix() << ", InterfaceId: "
              << localGid.getInterfaceId() << '\n';
    std::cout << "--------------------------------------------------------------------------------\n";
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

    auto add = rdma::Address();
    add.gid =remoteGid;
    add.lid = remoteLid;
    add.qpn = remoteQpn;
    node.rcqp.connect(add);

    /*
    auto wr = ibv::workrequest::Simple<ibv::workrequest::Write>();

    wr.setRemoteAddress(remoteMr);
    wr.setLocalAddress(mr->getSlice());
    ibv::workrequest::SendWr *bad = nullptr;
    qp->postSend(wr, bad);
*/
    std::cout << "Hello" << std::endl;
    // no explicit teardown needed
}
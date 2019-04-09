#include <iostream>
#include <cstddef>
#include <libibverbscpp.h>
#include <iostream>
#include <vector>
#include "src/Network.h"


int main() {
    char msg[8];
    auto list = ibv::device::DeviceList();
    auto ctx = list[0]->open();
    auto pd = ctx->allocProtectionDomain();
    auto mr = pd->registerMemoryRegion(&msg, 8, {/* no remote access */});

    auto sendCec = ctx->createCompletionEventChannel();
    auto sendCq = ctx->createCompletionQueue(100, nullptr, *sendCec, 0);
    auto recvCec = ctx->createCompletionEventChannel();
    auto recvCq = ctx->createCompletionQueue(100, nullptr, *recvCec, 0);
    auto qpAttr = NetworksetupQpInitAttr(*sendCq, *recvCq);

    auto qp = pd->createQueuePair(*qpAttr.queuePairAttributes);

    // connect QueuePair
    auto localQpn = qp->getNum();
    auto localLid = ctx->queryPort(1).getLid();
    auto localGid = ctx->queryGid(1, 0);
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

    connectRcQueuePair(*qp, remoteQpn, remoteLid, remoteGid, 1, 5);

    auto wr = ibv::workrequest::Simple<ibv::workrequest::Write>();
    // TODO: properly set up remote address
    uint32_t BIGBADBUFFER_SIZE = 1024 * 1024 * 8;
    auto recvbuf = std::vector<char>(BIGBADBUFFER_SIZE * 2); // *2 just to be sure everything fits
    auto recvmr = pd->registerMemoryRegion(recvbuf.data(), recvbuf.size(),
                                                         {ibv::AccessFlag::LOCAL_WRITE, ibv::AccessFlag::REMOTE_WRITE});
    auto remoteMr = ibv::memoryregion::RemoteAddress{reinterpret_cast<uintptr_t>(recvbuf.data()),
                                                     recvmr->getRkey()};

    wr.setRemoteAddress(remoteMr);
    wr.setLocalAddress(mr->getSlice());
    ibv::workrequest::SendWr *bad = nullptr;
    qp->postSend(wr, bad);




    // no explicit teardown needed
}
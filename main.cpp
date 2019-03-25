#include <iostream>
#include <cstddef>
#include "include/libibverbscpp/libibverbscpp.h"

int main() {

    std::byte msg[8];
    auto list = ibv::device::DeviceList();
    auto ctx = list[0]->open();
    auto pd = ctx->allocProtectionDomain();
    auto mr = pd->registerMemoryRegion(&msg, 8, {/* no remote access */});
    auto qpAttr = ibv::queuepair::InitAttributes();
// TODO: properly set up and connect QueuePair
    auto qp = pd->createQueuePair(qpAttr);
    auto wr = ibv::workrequest::Simple<ibv::workrequest::Write>();
// TODO: properly set up remote address
    wr.setRemoteAddress(ibv::memoryregion::RemoteAddress());
    wr.setLocalAddress(mr->getSlice());
    ibv::workrequest::SendWr *bad;
    qp->postSend(wr, bad);
// no explicit teardown needed
    std::cout << "Hello, World!" << std::endl;
    return 0;
}


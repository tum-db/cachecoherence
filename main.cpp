#include <iostream>
#include <cstddef>
#include <libibverbscpp.h>


int main() {
    struct ibv::protectiondomain::ProtectionDomain *pd;
    struct ibv::completions::CompletionQueue *cq;
    struct ibv::queuepair::QueuePair *qp;
    struct ibv::queuepair::InitAttributes qp_init_attr;
    struct ibv::queuepair::Capabilities cap;


    memset(&qp_init_attr, 0, sizeof(qp_init_attr));

    qp_init_attr.setSendCompletionQueue(cq);
    qp_init_attr.setRecvCompletionQueue(cq);
    qp_init_attr.setType(ibv::queuepair::Type::RC);
    cap.setMaxSendWr(2);
    cap.setMaxRecvWr(2);
    cap.setMaxSendSge(1);
    cap.setMaxRecvSge(1);
    qp_init_attr.setCapabilities(cap);

    auto qptest = pd->createQueuePair(qp_init_attr);
    if (!qp) {
        fprintf(stderr, "Error, ibv_create_qp() failed\n");
        return -1;
    }
    delete(&qptest);

   /* std::byte msg[8];
    auto list = ibv::device::DeviceList();
    auto ctx = list[0]->open();
    auto att = ctx->queryAttributes();
    auto pd = ctx->allocProtectionDomain();
    auto mr = pd->registerMemoryRegion(&msg, 8, {});
    auto qpAttr = ibv::queuepair::InitAttributes();
    auto cec = ctx->createCompletionEventChannel();
//auto cq = ctx->createCompletionQueue(0, &ctx, *cec, 0);
    qpAttr.setContext(&ctx);
    qpAttr.setType(ibv::queuepair::Type::RC);
    qpAttr.setCapabilities(ibv::queuepair::Capabilities());
    qpAttr.setSignalAll(true);
    // TODO: properly set up and connect QueuePair
    auto qp = pd->createQueuePair(qpAttr);
    auto wr = ibv::workrequest::Simple<ibv::workrequest::Write>();
    // TODO: properly set up remote address
    wr.setRemoteAddress(ibv::memoryregion::RemoteAddress());
    wr.setLocalAddress(mr->getSlice());
    ibv::workrequest::SendWr *bad;
    qp->postSend(wr, bad);*/

// no explicit teardown needed
    std::cout << "Hello, World!" << std::endl;
    return 0;
}


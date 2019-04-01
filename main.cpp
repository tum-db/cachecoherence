#include <iostream>
#include <cstddef>
#include <libibverbscpp.h>


int main() {
    static constexpr int CQ_SIZE = 100;
    static constexpr void *contextPtr = nullptr;
    static constexpr int completionVector = 0;
    std::unique_ptr<ibv::completions::CompletionEventChannel> channel;
    /// The send completion queue
    std::unique_ptr<ibv::completions::CompletionQueue> sendQueue;
    /// The receive completion queue
    std::unique_ptr<ibv::completions::CompletionQueue> receiveQueue;
    std::byte msg[8];
    auto list = ibv::device::DeviceList();
    auto ctx = list[0]->open();
    auto att = ctx->queryAttributes();
    auto pd = ctx->allocProtectionDomain();
    auto mr = pd->registerMemoryRegion(&msg, 8, {});
    auto qpAttr = ibv::queuepair::InitAttributes();
    channel = (ctx->createCompletionEventChannel());
    sendQueue =(ctx->createCompletionQueue(CQ_SIZE, contextPtr, *channel, completionVector));
    receiveQueue = (ctx->createCompletionQueue(CQ_SIZE, contextPtr, *channel, completionVector));
    qpAttr.setContext(&ctx);
    qpAttr.setType(ibv::queuepair::Type::RC);
    qpAttr.setRecvCompletionQueue(*receiveQueue);
    qpAttr.setSendCompletionQueue(*sendQueue);
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


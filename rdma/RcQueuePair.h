#ifndef L5RDMA_RCQUEUEPAIR_H
#define L5RDMA_RCQUEUEPAIR_H

#include "QueuePair.hpp"

namespace rdma {
    class RcQueuePair : public QueuePair {
    public:
        explicit RcQueuePair(Network &network) : QueuePair(network, ibv::queuepair::Type::RC) {}

        RcQueuePair(Network &network, CompletionQueuePair &completionQueuePair) :
                QueuePair(network, ibv::queuepair::Type::RC, completionQueuePair) {}

        void connect(const Address &address) override;

        void connect(const Address &address, uint8_t port, uint8_t retryCount = 5);

        inline void setToResetState(){
            using Mod = ibv::queuepair::AttrMask;
            ibv::queuepair::Attributes attributes{};
            attributes.setQpState(ibv::queuepair::State::RESET);
            qp->modify(attributes, {Mod::STATE});};
    };
}

#endif //L5RDMA_RCQUEUEPAIR_H

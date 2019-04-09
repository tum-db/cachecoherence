//
// Created by Magdalena Pröbstl on 2019-04-09.
//

#ifndef MEDMM_QUEUEPAIR_H
#define MEDMM_QUEUEPAIR_H

struct QpInitAttr {
    std::unique_ptr <ibv::queuepair::InitAttributes> queuePairAttributes;
    std::unique_ptr <ibv::queuepair::Capabilities> capabilities;

};
class QueuePair {
public:
    QueuePair();

    QpInitAttr setupQpInitAttr(ibv::completions::CompletionQueue &sendCompletionQueue,
                               ibv::completions::CompletionQueue &receiveCompletionQueue);

    void connectRcQueuePair(ibv::queuepair::QueuePair &qp, uint32_t remoteQpn, uint16_t remoteLid,
                            const ibv::Gid &remoteGid, uint8_t localPort, uint8_t retryCount);
};


#endif //MEDMM_QUEUEPAIR_H

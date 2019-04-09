//
// Created by Magdalena Pröbstl on 2019-04-09.
//

#include "Network.h"

using namespace std;

struct QpInitAttr {
    std::unique_ptr <ibv::queuepair::InitAttributes> queuePairAttributes;
    std::unique_ptr <ibv::queuepair::Capabilities> capabilities;

};

QpInitAttr setupQpInitAttr(ibv::completions::CompletionQueue &sendCompletionQueue,
                           ibv::completions::CompletionQueue &receiveCompletionQueue) {
    QpInitAttr init;
    init.queuePairAttributes = std::make_unique<ibv::queuepair::InitAttributes>();
    init.queuePairAttributes->setSendCompletionQueue(sendCompletionQueue);
    init.queuePairAttributes->setRecvCompletionQueue(receiveCompletionQueue);
    //queuePairAttributes.setSharedReceiveQueue(receiveQueue); // not needed
    // Capabilities can be queried with ctx->queryAttributes()
    init.capabilities = std::make_unique<ibv::queuepair::Capabilities>();
    init.capabilities->setMaxSendWr(16351);
    init.capabilities->setMaxRecvWr(16351);
    init.capabilities->setMaxSendSge(1);
    init.capabilities->setMaxRecvSge(1);
    init.capabilities->setMaxInlineData(512);
    init.queuePairAttributes->setCapabilities(*init.capabilities);
    init.queuePairAttributes->setType(ibv::queuepair::Type::RC);
    init.queuePairAttributes->setSignalAll(true);
    return init;
}

void connectRcQueuePair(ibv::queuepair::QueuePair &qp, uint32_t remoteQpn, uint16_t remoteLid,
                        const ibv::Gid &remoteGid, uint8_t localPort, uint8_t retryCount) {
    using Access = ibv::AccessFlag;
    using Mod = ibv::queuepair::AttrMask;

    {   // First initialize the QP
        ibv::queuepair::Attributes attributes{};
        attributes.setQpState(ibv::queuepair::State::INIT);
        attributes.setPkeyIndex(0); // Partition the queue pair belongs to
        attributes.setPortNum(localPort); // The local physical port
        // Allowed access flags of the remote operations for incoming packets (i.e., none, RDMA read, RDMA write, or atomics)
        attributes.setQpAccessFlags({Access::REMOTE_WRITE, Access::REMOTE_READ, Access::REMOTE_ATOMIC});

        qp.modify(attributes, {Mod::STATE, Mod::PKEY_INDEX, Mod::PORT, Mod::ACCESS_FLAGS});
    }

    {   // RTR (ready to receive)
        ibv::queuepair::Attributes attributes{};
        attributes.setQpState(ibv::queuepair::State::RTR);
        attributes.setPathMtu(ibv::Mtu::_4096);             // Maximum payload size
        attributes.setDestQpNum(remoteQpn);                 // The remote QP number
        attributes.setRqPsn(0);                             // The packet sequence number of received packets
        attributes.setMaxDestRdAtomic(16); // The number of outstanding RDMA reads & atomic operations (destination)
        attributes.setMinRnrTimer(12);                      // The time before a RNR NACK is sent
        ibv::ah::Attributes ahAttributes{};
        ahAttributes.setIsGlobal(false);                    // Whether there is a global routing header
        ahAttributes.setDlid(remoteLid);                  // The LID of the remote host
        ahAttributes.setSl(0);                              // The service level (which determines the virtual lane)
        ahAttributes.setSrcPathBits(0);                     // Use the port base LID
        ahAttributes.setPortNum(localPort);                      // The local physical port
        // see rc_pingpong.c::pp_connect_ctx
        if (remoteGid.getInterfaceId()) {
            ahAttributes.setIsGlobal(true);
            ibv::GlobalRoute globalRoute{};
            globalRoute.setHopLimit(1);
            globalRoute.setDgid(remoteGid);
            ahAttributes.setGrh(globalRoute);
        }
        attributes.setAhAttr(ahAttributes);

        qp.modify(attributes, {Mod::STATE, Mod::AV, Mod::PATH_MTU, Mod::DEST_QPN, Mod::RQ_PSN,
                               Mod::MAX_DEST_RD_ATOMIC, Mod::MIN_RNR_TIMER});
    }

    {   // RTS (ready to send)
        ibv::queuepair::Attributes attributes{};
        attributes.setQpState(ibv::queuepair::State::RTS);
        attributes.setSqPsn(0);             // The packet sequence number of sent packets
        attributes.setTimeout(0);           // The minimum timeout before retransmitting the packet (0 = infinite)
        attributes.setRetryCnt(retryCount); // How often to retry sending (7 = infinite)
        attributes.setRnrRetry(retryCount); // How often to retry sending when RNR NACK was received (7 = infinite)
        attributes.setMaxRdAtomic(128);     // The number of outstanding RDMA reads & atomic operations (initiator)
        qp.modify(attributes, {Mod::STATE, Mod::TIMEOUT, Mod::RETRY_CNT, Mod::RNR_RETRY, Mod::SQ_PSN,
                               Mod::MAX_QP_RD_ATOMIC});
    }
}

unique_ptr <ibv::memoryregion::MemoryRegion> Network::registerMr(void *addr, size_t length,
                                                                 initializer_list <ibv::AccessFlag> flags) {
    return protectionDomain->registerMemoryRegion(addr, length, flags);
}

ibv::protectiondomain::ProtectionDomain &Network::getProtectionDomain() {
    return *protectionDomain;
}


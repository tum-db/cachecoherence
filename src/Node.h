//
// Created by Magdalena Pröbstl on 2019-04-11.
//

#ifndef MEDMM_NODE_H
#define MEDMM_NODE_H


#include "../util/RDMANetworking.h"
#include "../rdma/CompletionQueuePair.hpp"
#include "../util/defs.h"
#include <cstddef>
#include <map>


class Node {
private:
    rdma::Network network;
    uint16_t id;
    rdma::RcQueuePair rcqp;
    std::map<uint16_t, defs::CACHE_DIRECTORY_STATES> locks;
    l5::util::Socket socket;

    void sendLockToHomeNode(defs::CACHE_DIRECTORY_STATES state);

    defs::Lock getLock(uint16_t id);

    void handleReceivedLocks(void *recvbuf);

    void handleGetLocks(void *recvbuf, ibv::memoryregion::RemoteAddress
    remoteAddr, rdma::CompletionQueuePair *cq);

    void handleAllocation(void *recvbuf, ibv::memoryregion::RemoteAddress remoteAddr,
                          rdma::CompletionQueuePair *cq);

    void handleFree(void *recvbuf, ibv::memoryregion::RemoteAddress remoteAddr,
                    rdma::CompletionQueuePair *cq);

    void handleRead(void *recvbuf, ibv::memoryregion::RemoteAddress remoteAddr,
                    rdma::CompletionQueuePair *cq);

    void handleWrite(void *recvbuf, ibv::memoryregion::RemoteAddress
    remoteAddr, rdma::CompletionQueuePair *cq);

    l5::util::Socket connectServerSocket();

public:

    explicit Node();

    void connectClientSocket();

    void closeClientSocket();

    defs::GlobalAddress *sendAddress(void *data, defs::IMMDATA immData);

    defs::GlobalAddress *sendData(defs::SendData *data, defs::IMMDATA immData);

    void sendLock(defs::Lock *lock, defs::IMMDATA immData);

    void connectAndReceive();

    void receive(l5::util::Socket *acced);

    defs::GlobalAddress *Malloc(size_t *size);

    defs::GlobalAddress *Free(defs::GlobalAddress *gaddr);

    defs::GlobalAddress *write(defs::SendData *data);

    void *read(defs::GlobalAddress *gaddr);

    bool isLocal(defs::GlobalAddress *gaddr);

    inline uint16_t getID() { return id; }

    inline void setID(uint16_t newID) { id = newID; }

    defs::Lock *getLockFromRemote(uint16_t nodeId, defs::IMMDATA immData);

    };


#endif //MEDMM_NODE_H

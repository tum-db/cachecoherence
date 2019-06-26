//
// Created by Magdalena Pröbstl on 2019-04-11.
//

#ifndef MEDMM_NODE_H
#define MEDMM_NODE_H


#include "../util/RDMANetworking.h"
#include "../rdma/CompletionQueuePair.hpp"
#include "../util/defs.h"
#include "Cache.h"
#include <cstddef>
#include <unordered_map>


class Node {
private:
    rdma::Network network;
    uint16_t id;
    rdma::RcQueuePair rcqp;
    std::unordered_map<uint16_t, defs::LOCK_STATES> locks;
    l5::util::Socket socket;
    Cache cache;


    void handleLocks(void *recvbuf, ibv::memoryregion::RemoteAddress remoteAddr,
                             rdma::CompletionQueuePair *cq);

   void handleAllocation(void *recvbuf, ibv::memoryregion::RemoteAddress remoteAddr,
                          rdma::CompletionQueuePair *cq);

    void handleFree(void *recvbuf, ibv::memoryregion::RemoteAddress remoteAddr,
                    rdma::CompletionQueuePair *cq);

    void handleRead(void *recvbuf, ibv::memoryregion::RemoteAddress remoteAddr,
                    rdma::CompletionQueuePair *cq);

    void handleWrite(void *recvbuf, ibv::memoryregion::RemoteAddress
    remoteAddr, rdma::CompletionQueuePair *cq);

    l5::util::Socket connectServerSocket();

    bool setLock(uint16_t lockId, defs::LOCK_STATES state);

    bool sendLock(defs::Lock lock, defs::IMMDATA immData);


public:

    explicit Node();

    void connectClientSocket();

    void closeClientSocket();

    void *sendAddress(defs::SendGlobalAddr data, defs::IMMDATA immData);

    defs::GlobalAddress sendData(defs::SendingData data, defs::IMMDATA immData);


    void connectAndReceive();

    void receive(l5::util::Socket *acced);

    defs::GlobalAddress Malloc(size_t *size);

    defs::GlobalAddress Free(defs::GlobalAddress gaddr);

    defs::GlobalAddress write(defs::SendData *data);

    uint64_t read(defs::GlobalAddress gaddr);

    bool isLocal(defs::GlobalAddress gaddr);

    inline uint16_t getID() { return id; }

    inline void setID(uint16_t newID) { id = newID; }

};


#endif //MEDMM_NODE_H

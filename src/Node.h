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

struct Connection{
    rdma::RcQueuePair rcqp;
    l5::util::Socket socket;
};

class Node {
private:
    rdma::Network network;
    uint16_t id;
    std::unordered_map<uint16_t, defs::LOCK_STATES> locks;
    Cache cache;


    void handleLocks(void *recvbuf, ibv::memoryregion::RemoteAddress remoteAddr,
                     rdma::CompletionQueuePair *cq, Connection *c);

    void handleAllocation(void *recvbuf, ibv::memoryregion::RemoteAddress remoteAddr,
                          rdma::CompletionQueuePair *cq,  Connection *c);

    void handleFree(void *recvbuf, ibv::memoryregion::RemoteAddress remoteAddr,
                    rdma::CompletionQueuePair *cq,  Connection *c);

    void handleRead(void *recvbuf, ibv::memoryregion::RemoteAddress remoteAddr,
                    rdma::CompletionQueuePair *cq,  Connection *c);

    void handleWrite(void *recvbuf, ibv::memoryregion::RemoteAddress
    remoteAddr, rdma::CompletionQueuePair *cq,  Connection *c);

    void handleInvalidation(void *recvbuf, ibv::memoryregion::RemoteAddress
    remoteAddr, rdma::CompletionQueuePair *cq,  Connection *c);

    l5::util::Socket connectServerSocket(Connection *c);

    bool setLock(uint16_t lockId, defs::LOCK_STATES state, Connection *c);

    bool sendLock(defs::Lock lock, defs::IMMDATA immData, Connection *c);

    defs::GlobalAddress performWrite(defs::Data *data, uint16_t srcID, Connection *c);

    defs::SaveData *performRead(defs::GlobalAddress gaddr, uint16_t srcID, Connection *c);

    void invalidate(rdma::CompletionQueuePair *cq, Connection *c);

    void startInvalidations(defs::Data data, ibv::memoryregion::RemoteAddress remoteAddr,
                            rdma::CompletionQueuePair *cq, std::vector<uint16_t> nodes, uint16_t srcID,  Connection *c);


    void broadcastInvalidations(std::vector<uint16_t> nodes, defs::GlobalAddress gaddr);


public:

    explicit Node();

    Connection connectClientSocket(uint16_t port);

    void closeClientSocket(Connection *c);

    void *sendAddress(defs::SendGlobalAddr data, defs::IMMDATA immData, Connection *c);

    defs::GlobalAddress sendData(defs::SendingData data, defs::IMMDATA immData, Connection *c);


    void connectAndReceive(uint16_t port);

    bool receive(l5::util::Socket *acced, Connection *c);

    defs::GlobalAddress Malloc(size_t *size, Connection *c);

    defs::GlobalAddress Free(defs::GlobalAddress gaddr, Connection *c);

    defs::GlobalAddress write(defs::Data *data, Connection *c);

    uint64_t read(defs::GlobalAddress gaddr, Connection *c);

    bool isLocal(defs::GlobalAddress gaddr);

    inline uint16_t getID() { return id; }

    inline void setID(uint16_t newID) { id = newID; }

};


#endif //MEDMM_NODE_H

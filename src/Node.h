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

    void sendLockToHomeNode(defs::CACHE_DIRECTORY_STATES state);

    defs::CACHE_DIRECTORY_STATES getLock(uint16_t id);

    void handleReceivedLocks(void *recvbuf);

    void handleAllocation(void *recvbuf, ibv::memoryregion::RemoteAddress remoteAddr,
                          rdma::CompletionQueuePair *cq);
    void handleWrite(void* recvbuf, ibv::memoryregion::RemoteAddress
    remoteAddr, rdma::CompletionQueuePair *cq);

public:

    explicit Node();

    defs::GlobalAddress *sendAddress(void *data, size_t size, uint32_t immData);

    defs::GlobalAddress *sendData(defs::SendData *data, uint32_t immData);
    void sendLock(defs::Lock *lock, uint32_t immData);


    void receive();

    defs::GlobalAddress *Malloc(size_t *size);

    void Free(defs::GlobalAddress *gaddr);

    defs::GlobalAddress *write(defs::SendData *data);

    void read(defs::GlobalAddress *gaddr, void *data);

    bool isLocal(defs::GlobalAddress *gaddr);

    inline uint16_t getID() { return id; }

    inline void setID(uint16_t newID) { id = newID; }
};




#endif //MEDMM_NODE_H

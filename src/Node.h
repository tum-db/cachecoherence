//
// Created by Magdalena Pröbstl on 2019-04-11.
//

#ifndef MEDMM_NODE_H
#define MEDMM_NODE_H


#include "../util/RDMANetworking.h"
#include "../rdma/CompletionQueuePair.hpp"
#include "../util/defs.h"
#include <cstddef>


enum CACHE_DIRECTORY_STATES {
    UNSHARED = 0,
    SHARED = 1,
    DIRTY = 2
};

class Node {
private:
    rdma::Network network;
    uint16_t id;
    rdma::RcQueuePair rcqp;
    uint64_t* locklist;

public:

    explicit Node();

    GlobalAddress *sendAddress(void *data, size_t size, uint32_t immData);

    void *sendData(SendData *data, uint32_t immData);

    void receive();

    GlobalAddress *Malloc(size_t *size);

    void Free(GlobalAddress *gaddr);

    void *write(GlobalAddress *gaddr, SendData *data);

    void read(GlobalAddress *gaddr, void *data);

    bool isLocal(GlobalAddress *gaddr);

    inline uint16_t getID() { return id; }

    inline void setID(uint16_t newID) {id = newID;}


};


#endif //MEDMM_NODE_H

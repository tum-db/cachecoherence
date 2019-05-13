//
// Created by Magdalena Pröbstl on 2019-04-11.
//

#ifndef MEDMM_NODE_H
#define MEDMM_NODE_H


#include "../util/RDMANetworking.h"
#include "../rdma/CompletionQueuePair.hpp"
#include "../util/defs.h"
#include "WorkRequest.h"
#include <cstddef>


class Node {
private:
    rdma::Network network;
    uint16_t id;
    rdma::RcQueuePair rcqp;

public:

    explicit Node();

    void send(void* data, size_t size, uint32_t immData);

    GlobalAddress receive();

    GlobalAddress Malloc(size_t size);

    GlobalAddress sendRemoteMalloc(size_t size);

    bool isLocal(GlobalAddress gaddr);

    inline uint16_t getID(){return id;}
};


#endif //MEDMM_NODE_H

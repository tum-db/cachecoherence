//
// Created by Magdalena Pröbstl on 2019-04-11.
//

#ifndef MEDMM_NODE_H
#define MEDMM_NODE_H


#include "../util/RDMANetworking.h"
#include "../rdma/CompletionQueuePair.hpp"

class Node {
private:

public:
    rdma::RcQueuePair rcqp;

    Node(rdma::Network &network, rdma::CompletionQueuePair &completionQueuePair);
    void send(const uint8_t *data, size_t length);

    std::vector<uint8_t> receive();

};


#endif //MEDMM_NODE_H

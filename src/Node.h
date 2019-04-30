//
// Created by Magdalena Pröbstl on 2019-04-11.
//

#ifndef MEDMM_NODE_H
#define MEDMM_NODE_H


#include "../util/RDMANetworking.h"
#include "../rdma/CompletionQueuePair.hpp"
#include "GlobalAddress.h"

class Node {
private:
    rdma::RcQueuePair rcqp;

public:

    explicit Node(rdma::Network &network);

    void send(rdma::Network &network, std::string data, GlobalAddress gaddr);

    std::vector<char, std::allocator<char>> receive(rdma::Network &network,  GlobalAddress gaddr);

};


#endif //MEDMM_NODE_H

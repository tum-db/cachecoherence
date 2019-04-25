//
// Created by Magdalena Pröbstl on 2019-04-11.
//

#ifndef MEDMM_NODE_H
#define MEDMM_NODE_H


#include "../util/RDMANetworking.h"
#include "../rdma/CompletionQueuePair.hpp"

class Node {
private:
    rdma::RcQueuePair rcqp;

public:

    Node(rdma::Network &network);

    void send(rdma::Network &network, std::string data, uint16_t port,char *ip);

    std::vector<char, std::allocator<char>> receive(rdma::Network &network, uint16_t port, char *ip);

};


#endif //MEDMM_NODE_H

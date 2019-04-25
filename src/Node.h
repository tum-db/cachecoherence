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
    rdma::Network network;


public:

    Node();

    void send(std::string data, uint16_t port,char *ip);

    std::vector<uint8_t> receive(uint16_t port,char *ip);

};


#endif //MEDMM_NODE_H

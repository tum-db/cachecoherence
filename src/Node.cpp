//
// Created by Magdalena Pröbstl on 2019-04-11.
//

#include "Node.h"

Node::Node(rdma::Network &network) :
        rcqp(network) {

}

void send(const uint8_t *data, size_t length) {

}

std::vector<uint8_t> receive() {

}

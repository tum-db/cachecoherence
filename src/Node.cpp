//
// Created by Magdalena Pröbstl on 2019-04-11.
//

#include "Node.h"


Node::Node() : network(), id(), rcqp(network, network.getSharedCompletionQueue()), locks(), socket() {
    id = 0;
    socket = l5::util::Socket::create();
}

bool Node::isLocal(defs::GlobalAddress *gaddr) {
    return getNodeId(gaddr) == id;
}


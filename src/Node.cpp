//
// Created by Magdalena Pröbstl on 2019-04-11.
//

#include "Node.h"


Node::Node() : network(), id(), rcqp(network, network.getSharedCompletionQueue()), locks() {
    id = 0;
}

bool Node::isLocal(defs::GlobalAddress *gaddr) {
    return getNodeId(gaddr) == id;
}


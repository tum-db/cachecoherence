//
// Created by Magdalena Pröbstl on 2019-04-11.
//

#include "Node.h"


Node::Node() : network(), id(), locks(), cache() {
    id = 0;
}

bool Node::isLocal(defs::GlobalAddress gaddr) {
    return gaddr.getNodeId() == id;
}


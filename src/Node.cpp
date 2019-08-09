//
// Created by Magdalena Pröbstl on 2019-04-11.
//

#include "Node.h"


Node::Node() : network(), id(), locks(), cache() {
    id = 0;
}


Node::Node(uint16_t id) : network(), id(id), locks(), cache() {
}

bool Node::isLocal(defs::GlobalAddress gaddr) {
    return gaddr.getNodeId() == id;
}


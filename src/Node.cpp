//
// Created by Magdalena Pröbstl on 2019-04-11.
//

#include "Node.h"

#include <cstdlib>
Node::Node()
        : network(), id(0), locks(), cache(), c(network) {
}

bool Node::isLocal(defs::GlobalAddress gaddr) {
    return gaddr.getNodeId() == id;
}

char * Node::getNextFileName(){
    std::string res = std::to_string(filenamesnbr);
    filenamesnbr++;
    char* data = new char[res.length() + 1];
    memcpy(data, res.c_str(), res.length());
    data[res.length()] = '\0';
    return data; // TODO: leakt wie sau
}

Node::ScopedLock::ScopedLock(defs::GlobalAddress &gaddr, Node &node, LOCK_STATES ls) : addr(gaddr), node(node) {
    auto locked = node.setLock(GlobalAddressHash<defs::SendGlobalAddr>::generateLockId(gaddr.sendable(0)), ls, node.getID());
    if (!locked) {
        throw std::runtime_error("feck");
    }
}

Node::ScopedLock::~ScopedLock() {
    node.setLock(GlobalAddressHash<defs::SendGlobalAddr>::generateLockId(addr.sendable(0)), LOCK_STATES::UNLOCKED, node.getID());
}

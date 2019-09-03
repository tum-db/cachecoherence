//
// Created by Magdalena Pröbstl on 2019-04-11.
//

#include "Node.h"

#include <cstdlib>
Node::Node()
        : network(), id(), locks(), cache() {
    id = 0;
}

bool Node::isLocal(defs::GlobalAddress gaddr) {
    return gaddr.getNodeId() == id;
}

char * Node::getNextFileName(){
    std::string res = std::to_string(filenamesnbr);
    filenamesnbr++;
    return const_cast<char *>(res.c_str());
}
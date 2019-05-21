//
// Created by Magdalena Pröbstl on 2019-04-30.
//

#include <cstdint>
#include "Node.h"
#include <stdlib.h>
#include <cstdio>
#include "../util/defs.h"


GlobalAddress *Node::Malloc(size_t *size) {
    auto buffer = malloc(*size);
    if (buffer) {
        auto gaddr = new GlobalAddress{*size, static_cast<uint64_t *>(buffer), id};
        return gaddr;
    } else {
        auto result = sendAddress(size, *size, 1);
        auto newgaddr = reinterpret_cast<GlobalAddress *>(result);
        return newgaddr;
    }
}


void Node::Free(GlobalAddress *gaddr) {
    if(isLocal(gaddr)){
        free(gaddr->ptr);
    } else {
        sendAddress(gaddr, sizeof(GlobalAddress), 3);
    }
}

void Node::read(GlobalAddress *gaddr, void *data) {
    if (isLocal(gaddr)) {

    }
}

void *Node::write(GlobalAddress *gaddr, SendData *data) {
    if(isLocal(gaddr)){

    } else {
        auto immData = 4;
        auto result = sendData(data, immData);
        return result;
    }

}
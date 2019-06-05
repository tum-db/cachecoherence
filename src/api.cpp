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
        auto gaddr = new GlobalAddress{*size, reinterpret_cast<uint64_t *>(buffer), id};
        return gaddr;
    } else {
        return sendAddress(size, *size, 1);
    }
}


void Node::Free(GlobalAddress *gaddr) {
    if (isLocal(gaddr)) {
        free(gaddr->ptr);
    } else {
        sendAddress(gaddr, sizeof(GlobalAddress), 3);
    }
}


void Node::sendLockToHomeNode(CACHE_DIRECTORY_STATES state){
    auto lock = new Lock{state,id};


}

void Node::read(GlobalAddress *gaddr, void *data) {
    if (isLocal(gaddr)) {

    }
}

GlobalAddress *Node::write(SendData *data) {
    if (isLocal(data->ga)) {
        sendLockToHomeNode(DIRTY);
        if (data->ga->size == data->size) {
            data->ga->ptr = data->data;
        } else {
            data->ga->ptr = static_cast<uint64_t *>(realloc(data->ga->ptr, data->size));
            data->ga->ptr = data->data;
        }
        return data->ga;
    } else {
        auto immData = 4;
        auto result = sendData(data, immData);
        return result;
    }
}


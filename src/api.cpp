//
// Created by Magdalena Pröbstl on 2019-04-30.
//

#include <cstdint>
#include "Node.h"
#include <stdlib.h>
#include <cstdio>
#include "../util/defs.h"


defs::GlobalAddress *Node::Malloc(size_t *size) {
    auto buffer = malloc(*size);
    if (buffer) {
        auto gaddr = new defs::GlobalAddress{*size, reinterpret_cast<uint64_t *>(buffer), id};
        return gaddr;
    } else {
        return sendAddress(size, *size, 1);
    }
}


void Node::Free(defs::GlobalAddress *gaddr) {
    if (isLocal(gaddr)) {
        free(gaddr->ptr);
    } else {
        sendAddress(gaddr, sizeof(defs::GlobalAddress), 3);
    }
}


void Node::sendLockToHomeNode(defs::CACHE_DIRECTORY_STATES state){
    if (id == defs::homenode){
        locks.insert(std::pair<uint16_t,defs::CACHE_DIRECTORY_STATES>(id,state));
    }
    else {
        auto lock = new defs::Lock{id, state};
        sendLock(lock, 5);
    }

}

void Node::read(defs::GlobalAddress *gaddr, void *data) {
    if (isLocal(gaddr)) {

    }
}

defs::CACHE_DIRECTORY_STATES Node::getLock(uint16_t id){
    auto state = locks.find(id);
    return state->second;

}

defs::GlobalAddress *Node::write(defs::SendData *data) {
    if (isLocal(data->ga)) {
        sendLockToHomeNode(defs::DIRTY);
        if (data->ga->size == data->size) {
            data->ga->ptr = data->data;
        } else {
            data->ga->ptr = static_cast<uint64_t *>(realloc(data->ga->ptr, data->size));
            data->ga->ptr = data->data;
        }
    //    sendLockToHomeNode(defs::UNSHARED);
        return data->ga;
    } else {
        auto l = getLock(data->ga->id);
        if (l==defs::UNSHARED) {
            auto immData = 4;
            auto result = sendData(data, immData);
            return result;
        } else {
            return nullptr;
        }
    }
}


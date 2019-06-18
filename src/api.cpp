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
        auto gaddr = new defs::GlobalAddress{*size, reinterpret_cast<uint64_t>(buffer), id};
        return gaddr;
    } else {
        return sendAddress(size, defs::IMMDATA::MALLOC);
    }
}


defs::GlobalAddress *Node::Free(defs::GlobalAddress *gaddr) {
    std::cout << "Freeing whoop" << std::endl;
    if (isLocal(gaddr)) {
        free(reinterpret_cast<void *>(gaddr->ptr));
        std::cout << "Freed..." << std::endl;
        return new defs::GlobalAddress{0, 0, id};
    } else {
        std::cout << "sending address to free" << std::endl;
        auto res = sendAddress(gaddr, defs::IMMDATA::FREE);
        std::cout << "freed, id: " << res->id << std::endl;
        return res;
    }
}


void Node::sendLockToHomeNode(defs::CACHE_DIRECTORY_STATES state) {
    if (id == defs::homenode) {
        locks.insert(std::pair<uint16_t, defs::CACHE_DIRECTORY_STATES>(id, state));
    } else {
        auto lock = new defs::Lock{id, state};
        sendLock(lock, defs::IMMDATA::LOCKS);
    }
}

void *Node::read(defs::GlobalAddress *gaddr) {
    if (isLocal(gaddr)) {
        sendLockToHomeNode(defs::CACHE_DIRECTORY_STATES::SHARED);
        auto data = reinterpret_cast<void*>(gaddr->ptr);
        sendLockToHomeNode(defs::CACHE_DIRECTORY_STATES::UNSHARED);
        return data;
    } else {
        auto l = getLock(gaddr->id);
        if (l.state != defs::CACHE_DIRECTORY_STATES::DIRTY) {
            auto result = sendAddress(gaddr, defs::IMMDATA::READ);
            return result;
        } else {
            return nullptr;
        }
    }
}

defs::Lock Node::getLock(uint16_t lockId) {
    if (id == defs::homenode) {
        auto state = locks.find(lockId);
        auto result = new defs::Lock{lockId, state->second};
        return *result;
    } else {
        auto result = getLockFromRemote(lockId, defs::IMMDATA::GETLOCK);
        return *result;
    }
}

defs::GlobalAddress *Node::write(defs::SendData *data) {
    if (isLocal(&data->ga)) {
        sendLockToHomeNode(defs::CACHE_DIRECTORY_STATES::DIRTY);
        if (data->ga.size == data->size) {
            data->ga.ptr = data->data;
        } else {
            data->ga.ptr = *static_cast<uint64_t *>(realloc(reinterpret_cast<void *>(data->ga.ptr), data->size));
            data->ga.ptr = data->data;
        }
        sendLockToHomeNode(defs::CACHE_DIRECTORY_STATES::UNSHARED);
        return &data->ga;
    } else {
        auto l = getLock(data->ga.id);
        if (l.state == defs::CACHE_DIRECTORY_STATES::UNSHARED) {
            sendLockToHomeNode(defs::CACHE_DIRECTORY_STATES::DIRTY);
            auto result = sendData(data, defs::IMMDATA::WRITE);
            return result;
        } else {
            return nullptr;
        }
    }
}


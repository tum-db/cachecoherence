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
        std::cout << buffer << std::endl;
        auto gaddr = new defs::GlobalAddress{*size, reinterpret_cast<uint64_t>(buffer), id};
        return gaddr;
    } else {
        auto res = sendAddress(defs::GlobalAddress{*size, 0, 0}, defs::IMMDATA::MALLOC);
        return reinterpret_cast<defs::GlobalAddress *>(res);
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
        auto res = sendAddress(*gaddr, defs::IMMDATA::FREE);
        auto add = reinterpret_cast<defs::GlobalAddress*>(res);
        std::cout << "freed, id: " << add->id << std::endl;
        return add;
    }
}


void Node::sendLockToHomeNode(uint16_t lockId, defs::CACHE_DIRECTORY_STATES state) {
    if (id == defs::homenode) {
        auto existing = locks.find(lockId);
        if (existing != locks.end()) {
            existing->second = state;
        } else {
            locks.insert(std::pair<uint16_t, defs::CACHE_DIRECTORY_STATES>(lockId, state));
        }
    } else {
        sendLock(defs::Lock{lockId, state}, defs::IMMDATA::LOCKS);
    }
}

uint64_t Node::read(defs::GlobalAddress *gaddr) {
    if (isLocal(gaddr)) {
        sendLockToHomeNode(gaddr->id, defs::CACHE_DIRECTORY_STATES::SHARED);
        auto data = reinterpret_cast<uint64_t *>(gaddr->ptr);
        sendLockToHomeNode(gaddr->id, defs::CACHE_DIRECTORY_STATES::UNSHARED);
        return *data;
    } else {
        auto l = getLock(gaddr->id);
        if (l.state != defs::CACHE_DIRECTORY_STATES::DIRTY) {
            auto result = sendAddress(*gaddr, defs::IMMDATA::READ);
            return *reinterpret_cast<uint64_t *>(result);
        } else {
            return 0;
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
        sendLockToHomeNode(data->ga.id, defs::CACHE_DIRECTORY_STATES::DIRTY);
            auto ptr = reinterpret_cast<uint64_t *>(data->ga.ptr);
            std::memcpy(ptr,&data->data, data->size);
        sendLockToHomeNode(data->ga.id, defs::CACHE_DIRECTORY_STATES::UNSHARED);
        return &data->ga;
    } else {
        auto l = getLock(data->ga.id);
        if (l.state == defs::CACHE_DIRECTORY_STATES::UNSHARED) {
            sendLockToHomeNode(data->ga.id, defs::CACHE_DIRECTORY_STATES::DIRTY);
            auto result = sendData(*data, defs::IMMDATA::WRITE);
            return result;
        } else {
            return nullptr;
        }
    }
}


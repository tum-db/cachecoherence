//
// Created by Magdalena Pröbstl on 2019-04-30.
//

#include <cstdint>
#include "Node.h"
#include <stdlib.h>
#include <cstdio>
#include "../util/defs.h"


defs::GlobalAddress Node::Malloc(size_t *size) {
    auto buffer = malloc(*size);
    if (buffer) {
        std::cout << buffer << std::endl;
        auto gaddr = defs::GlobalAddress{*size, buffer, id};
        return gaddr;
    } else {
        auto res = sendAddress(defs::GlobalAddress{*size, 0, 0}.sendable(), defs::IMMDATA::MALLOC);
        auto sga = reinterpret_cast<defs::SendGlobalAddr *>(res);
        return defs::GlobalAddress(*sga);
    }
}


defs::GlobalAddress Node::Free(defs::GlobalAddress gaddr) {
    std::cout << "Freeing whoop" << std::endl;
    if (isLocal(gaddr)) {
        free(gaddr.ptr);
        gaddr.ptr = nullptr;
        gaddr.size = 0;
        std::cout << "Freed..." << std::endl;
        return gaddr;
    } else {
        std::cout << "sending address to free" << std::endl;
        auto res = sendAddress(gaddr.sendable(), defs::IMMDATA::FREE);
        auto sga = reinterpret_cast<defs::SendGlobalAddr *>(res);
        auto add = defs::GlobalAddress(*sga);
        std::cout << "freed, id: " << add.id << std::endl;
        return add;
    }
}

uint64_t Node::read(defs::GlobalAddress gaddr) {
    if (isLocal(gaddr)) {
        if (setLock(gaddr.id, defs::LOCK_STATES::SHARED)) {
            auto data = reinterpret_cast<uint64_t *>(gaddr.ptr);
            std::cout << gaddr.ptr << ", " << *data << std::endl;
            setLock(gaddr.id, defs::LOCK_STATES::UNSHARED);
            return *data;
        } else {
            return 0;
        }
    } else {
        auto cacheItem = cache.getCacheItem(gaddr);
        if (!cacheItem.lastused) {
            auto data = sendAddress(gaddr.sendable(), defs::IMMDATA::READ);
            auto result = *reinterpret_cast<uint64_t *>(data);
            cacheItem = CacheItem{result, std::chrono::system_clock::now(),
                                  std::chrono::system_clock::now()};
            cache.addCacheItem(gaddr, cacheItem);
            return result;
        } else {
            return cacheItem.data;
        }
    }
}


bool Node::setLock(uint16_t lockId, defs::LOCK_STATES state) {
    if (id == defs::homenode) {
        auto existing = locks.find(lockId);
        if (existing != locks.end()) {
            if (state == defs::LOCK_STATES::UNSHARED) {
                existing->second = state;
                return true;
            } else if (state == defs::LOCK_STATES::DIRTY &&
                       existing->second == defs::LOCK_STATES::UNSHARED) {
                existing->second = state;
                return true;
            } else if (state == defs::LOCK_STATES::SHARED &&
                       existing->second != defs::LOCK_STATES::DIRTY) {
                existing->second = state;
                return true;
            } else {
                return false;
            }
        } else {
            locks.insert(std::pair<uint16_t, defs::LOCK_STATES>(lockId, state));
            return true;
        }

    } else {
        return sendLock(defs::Lock{lockId, state}, defs::IMMDATA::LOCKS);
    }
}

defs::GlobalAddress Node::write(defs::SendData *data) {
    if (isLocal(data->ga)) {
        if (setLock(data->ga.id, defs::LOCK_STATES::DIRTY)) {
            std::memcpy(data->ga.ptr, &data->data, data->size);
            setLock(data->ga.id, defs::LOCK_STATES::UNSHARED);
            return data->ga;
        } else { return defs::GlobalAddress(); }
    } else {
        auto result = sendData(data->sendable(), defs::IMMDATA::WRITE);
        return result;

    }
}


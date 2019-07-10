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
    if (isLocal(gaddr)) {
        free(gaddr.ptr);
        gaddr.ptr = nullptr;
        gaddr.size = 0;
        std::cout << "Freed..." << std::endl;
        return gaddr;
    } else {
        auto res = sendAddress(gaddr.sendable(), defs::IMMDATA::FREE);
        auto sga = reinterpret_cast<defs::SendGlobalAddr *>(res);
        auto add = defs::GlobalAddress(*sga);
        return add;
    }
}

uint64_t Node::read(defs::GlobalAddress gaddr) {
    if (setLock(gaddr.id, defs::LOCK_STATES::SHAREDLOCK)) {
        auto result = performRead(gaddr);
        setLock(gaddr.id, defs::LOCK_STATES::UNLOCKED);
        return result->data;
    } else {
        return 0;
    }
}

defs::SaveData *Node::performRead(defs::GlobalAddress gaddr) {
    if (isLocal(gaddr)) {
        auto data = reinterpret_cast<defs::SaveData *>(gaddr.ptr);
        std::cout << gaddr.ptr << ", " << data->data << std::endl;
        return data;
    } else {
        auto cacheItem = cache.getCacheItem(gaddr);
        if (cacheItem == nullptr) {
            auto data = sendAddress(gaddr.sendable(), defs::IMMDATA::READ);
            auto result = reinterpret_cast<defs::SaveData *>(data);
            auto newCacheItem = CacheItem{gaddr, result->data, std::chrono::system_clock::now(),
                                          std::chrono::system_clock::now()};
            cache.addCacheItem(gaddr, newCacheItem);
            return result;
        } else {
            return new defs::SaveData{cacheItem->data,defs::CACHE_DIRECTORY_STATE::SHARED};
        }
    }
}


bool Node::setLock(uint16_t lockId, defs::LOCK_STATES state) {
    if (id == defs::locknode) {
        auto existing = locks.find(lockId);
        if (existing != locks.end()) {
            if (state == defs::LOCK_STATES::UNLOCKED) {
                existing->second = state;
                return true;
            } else if (state == defs::LOCK_STATES::EXCLUSIVE &&
                       existing->second == defs::LOCK_STATES::UNLOCKED) {
                existing->second = state;
                return true;
            } else if (state == defs::LOCK_STATES::SHAREDLOCK &&
                       existing->second != defs::LOCK_STATES::EXCLUSIVE) {
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

defs::GlobalAddress Node::write(defs::Data *data) {
    if (setLock(data->ga.id, defs::LOCK_STATES::EXCLUSIVE)) {
        auto result = performWrite(data);
        setLock(data->ga.id, defs::LOCK_STATES::UNLOCKED);
        return result;
    } else {
        return defs::GlobalAddress{};
    }
}

defs::GlobalAddress Node::performWrite(defs::Data *data) {
    if (isLocal(data->ga)) {
        std::memcpy(data->ga.ptr, &data->data, data->size);
        return data->ga;
    } else {
        cache.removeCacheItem(data->ga.sendable());
        auto result = sendData(data->sendable(), defs::IMMDATA::WRITE);
        return result;
    }
}


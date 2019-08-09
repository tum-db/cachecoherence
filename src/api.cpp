//
// Created by Magdalena Pröbstl on 2019-04-30.
//

#include <cstdint>
#include "Node.h"
#include <stdlib.h>
#include <cstdio>
#include "../util/defs.h"


defs::GlobalAddress Node::Malloc(size_t size) {
    auto buffer = malloc(size);
    if (buffer) {
        std::cout << buffer << std::endl;
        auto gaddr = defs::GlobalAddress{size, buffer, id};
        return gaddr;
    } else {
        auto port = defs::port;
        auto c = connectClientSocket(port);

        auto res = sendAddress(defs::GlobalAddress{size, nullptr, 0}.sendable(id), defs::IMMDATA::MALLOC,
                               c);
        auto sga = reinterpret_cast<defs::SendGlobalAddr *>(res);

        c.socket.close();
        c.rcqp->setToResetState();
        return defs::GlobalAddress(*sga);
    }
}


defs::GlobalAddress Node::Free(defs::GlobalAddress gaddr) {
    if (isLocal(gaddr)) {
        auto d = reinterpret_cast<defs::SaveData *>(gaddr.ptr);

        if (d->iscached >= 0 && !d->sharerNodes.empty()) {
            broadcastInvalidations(d->sharerNodes,gaddr);
        }

        free(gaddr.ptr);
        gaddr.ptr = nullptr;
        gaddr.size = 0;
        return gaddr;
    } else {
        auto port = defs::port;
        auto c = connectClientSocket(port);

        auto res = sendAddress(gaddr.sendable(id), defs::IMMDATA::FREE, c);

        auto sga = reinterpret_cast<defs::SendGlobalAddr *>(res);
        auto add = defs::GlobalAddress(*sga);

        closeClientSocket(c);
        return add;
    }
}

uint64_t Node::read(defs::GlobalAddress gaddr) {
    if (setLock(gaddr.id, defs::LOCK_STATES::SHAREDLOCK)) {
        std::cout << "reading..." << std::endl;
        auto result = performRead(gaddr, id);
        setLock(gaddr.id, defs::LOCK_STATES::UNLOCKED);
        std::cout << result->data << std::endl;
        return result->data;
    } else {
        return 0;
    }
}

defs::SaveData *Node::performRead(defs::GlobalAddress gaddr, uint16_t srcID) {
    if (isLocal(gaddr)) {
        auto data = reinterpret_cast<defs::SaveData *>(gaddr.ptr);
        if (data->iscached < 0) {
            return nullptr;
        }
        return data;
    } else {
        auto cacheItem = cache.getCacheItem(gaddr);
        if (cacheItem == nullptr) {
            auto port = defs::port;
            auto c = connectClientSocket(port);

            auto data = sendAddress(gaddr.sendable(srcID), defs::IMMDATA::READ, c);

            auto result = reinterpret_cast<defs::SaveData *>(data);
            auto newCacheItem = CacheItem{gaddr, result->data, std::chrono::system_clock::now(),
                                          std::chrono::system_clock::now()};
            cache.addCacheItem(gaddr, newCacheItem);

            closeClientSocket(c);

            return result;
        } else {
            return new defs::SaveData{cacheItem->data, defs::CACHE_DIRECTORY_STATE::SHARED, 0,
                                      std::vector<uint16_t>()};
        }
    }
}


bool Node::setLock(uint16_t lockId, defs::LOCK_STATES state) {
    std::cout << "locks" << std::endl;
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
        auto c = connectClientSocket(defs::locknode);
        auto res = sendLock(defs::Lock{lockId, state}, defs::IMMDATA::LOCKS, c);
        closeClientSocket(c);
        return res;
    }
}

defs::GlobalAddress Node::write(defs::Data *data) {
    auto result = defs::GlobalAddress{};
    if (setLock(data->ga.id, defs::LOCK_STATES::EXCLUSIVE)) {

        if (isLocal(data->ga)) {
            auto d = reinterpret_cast<defs::SaveData *>(data->ga.ptr);

            std::cout <<"getting cachedata for write"<< d->iscached << ", "<<d->sharerNodes.empty() << std::endl;

            if (d->iscached >= 0 && !d->sharerNodes.empty()) {
                broadcastInvalidations(d->sharerNodes, data->ga);
            }

            auto writtenData = defs::SaveData{data->data, defs::CACHE_DIRECTORY_STATE::UNSHARED, id,
                                              {}};
            std::memcpy(data->ga.ptr, &writtenData, sizeof(writtenData));
            result = data->ga;
        } else {
            result = performWrite(data, id);
        }

        setLock(data->ga.id, defs::LOCK_STATES::UNLOCKED);
    }
    return result;

}

defs::GlobalAddress Node::performWrite(defs::Data *data, uint16_t srcID) {
    if (isLocal(data->ga)) {
        auto writtenData = defs::SaveData{data->data, defs::CACHE_DIRECTORY_STATE::EXCLUS, srcID, {}};
        std::memcpy(data->ga.ptr, &writtenData, sizeof(writtenData));
        return data->ga;
    } else {
        auto port = defs::port;
        auto c = connectClientSocket(port);
        auto ci = CacheItem{data->ga, defs::CACHE_DIRECTORY_STATE::EXCLUS, std::chrono::system_clock::now(),
                            std::chrono::system_clock::now()};
        cache.alterCacheItem(ci,data->ga);
        auto result = sendData(data->sendable(srcID), defs::IMMDATA::WRITE, c);

        std::cout << c.socket.get() << ", " << c.rcqp->getQPN() << std::endl;

        closeClientSocket(c);
        return result;
    }
}


//
// Created by Magdalena Pröbstl on 2019-06-24.
//

#include "Cache.h"
#include "../util/GlobalAddressHash.h"
#include <iostream>


Cache::Cache() : maxsize(), availablesize(), items(), state() {
    maxsize = 512;
    availablesize = 512;
    state = defs::CACHE_DIRECTORY_STATE::UNSHARED;
}

void Cache::addCacheItem(defs::GlobalAddress gaddr, char * data) {
    if (gaddr.size <= maxsize) {
        if (availablesize >= gaddr.size) {
            auto& ci = items.emplace(
                    std::piecewise_construct,
                    std::forward_as_tuple(GlobalAddressHash<defs::SendGlobalAddr>()(gaddr.sendable(0))),
                    std::forward_as_tuple(gaddr, data, std::chrono::system_clock::now(), std::chrono::system_clock::now())
            ).first->second;
            memcpy( ci.data, data, gaddr.size);
            availablesize = availablesize - gaddr.size;
        } else {
            while (availablesize < gaddr.size) {
                removeOldestItem();
            }
        }
    } else {
        throw;
    }
}

uint64_t Cache::removeCacheItem(defs::GlobalAddress ga) {
    auto iterator = GlobalAddressHash<defs::SendGlobalAddr>()(ga.sendable(0));
    auto res = items.erase(iterator);
    return res;
}

void Cache::removeOldestItem() {
    std::pair<uint64_t, CacheItem> latest;

    for (auto &it: items) {
        if (latest.second.lastused.time_since_epoch() < it.second.lastused.time_since_epoch()) {
            latest = it;
        }
        std::cout << std::chrono::system_clock::to_time_t(latest.second.lastused) << std::endl;
    }
    availablesize = availablesize + latest.second.globalAddress.size;
    items.erase(latest.first);
}

CacheItem *Cache::getCacheItem(defs::GlobalAddress ga) {
    auto h = GlobalAddressHash<defs::SendGlobalAddr>()(ga.sendable(0));
    auto cacheItem = items.find(h);
    if (cacheItem != items.end()) {
        cacheItem->second.lastused = std::chrono::system_clock::now();
        items[h] = cacheItem->second;
        return &cacheItem->second;
    } else {
        return nullptr;
    }
}

void Cache::alterCacheItem(char * data, defs::GlobalAddress ga) {
    auto h = GlobalAddressHash<defs::SendGlobalAddr>()(ga.sendable(0));
    auto cacheItem = items.find(h);
    if (cacheItem != items.end()) {
        memcpy(cacheItem->second.data, data, ga.size);
        cacheItem->second.lastused = std::chrono::system_clock::now();
        items[h] = cacheItem->second;
    } else {
        addCacheItem(ga, data);
    }
}

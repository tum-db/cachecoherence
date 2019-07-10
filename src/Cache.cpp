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

void Cache::addCacheItem(defs::GlobalAddress gaddr, CacheItem cacheItem) {
    if (gaddr.size <= maxsize) {
        if (availablesize >= gaddr.size) {
            items.insert(std::pair<uint64_t, CacheItem>(
                    GlobalAddressHash<defs::SendGlobalAddr>()(gaddr.sendable()), cacheItem));
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

void Cache::removeCacheItem(defs::SendGlobalAddr sga) {
    auto iterator = GlobalAddressHash<defs::SendGlobalAddr>()(sga);
    items.erase(iterator);
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
    auto cacheItem = items.find(GlobalAddressHash<defs::SendGlobalAddr>()(ga.sendable()));
    if (cacheItem != items.end()) {
        cacheItem->second.lastused = std::chrono::system_clock::now();
        return &cacheItem->second;
    } else {
        return nullptr;
    }
}

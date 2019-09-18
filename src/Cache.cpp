//
// Created by Magdalena Pröbstl on 2019-06-24.
//

#include "Cache.h"
#include "../util/GlobalAddressHash.h"
#include <iostream>


Cache::Cache() : availablesize(), items(), state() {
    availablesize = 512;
    state = defs::CACHE_DIRECTORY_STATE::UNSHARED;
}

void Cache::addCacheItem(defs::GlobalAddress gaddr, char *data, size_t size) {
    if (size <= defs::MAX_CACHE_SIZE) {
        if (availablesize >= size) {
            std::vector<char> itemdata;
            itemdata.resize(size);
            memcpy(&itemdata[0], data, size);
            auto hash = GlobalAddressHash<defs::SendGlobalAddr>()(gaddr.sendable(0));
            items.emplace(
                    std::piecewise_construct, std::forward_as_tuple(hash),
                    std::forward_as_tuple(gaddr, itemdata, std::chrono::system_clock::now(),
                                          std::chrono::system_clock::now())
            );
            availablesize = availablesize - size;
            auto test = items[hash];
            return;
        } else {
            while (availablesize < size) {
                removeOldestItem();
            }
        }
    } else {
        throw std::runtime_error("too large for cache");
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

void Cache::alterCacheItem(char *data, defs::GlobalAddress ga, size_t size) {
    auto h = GlobalAddressHash<defs::SendGlobalAddr>()(ga.sendable(0));
    auto cacheItem = items.find(h);
    if (cacheItem != items.end()) {
        auto item = cacheItem->second;
        item.data.resize(size);
        memcpy(&item.data[0], data, size);
        item.lastused = std::chrono::system_clock::now();
        items[h] = item;
    } else {
        addCacheItem(ga, data, size);
    }
}

//
// Created by Magdalena Pröbstl on 2019-06-24.
//

#include "Cache.h"
#include <iostream>


Cache::Cache() : maxsize(), availablesize(), items() {
    maxsize = 512;
    availablesize = 512;
    items = new std::unordered_map<defs::GlobalAddress, CacheItem>();
}

void Cache::addCacheItem(defs::GlobalAddress gaddr, CacheItem cacheItem) {
    if (gaddr.size <= maxsize){
            if (availablesize >= gaddr.size) {
                items.insert(std::pair<defs::GlobalAddress, CacheItem>(gaddr, cacheItem));
                availablesize = availablesize - gaddr.size;
            }
            else {
                while(availablesize < gaddr.size) {
                    removeOldestItem();
                }
            }
    }
    else {
        throw;
    }
}

void Cache::removeOldestItem(){
    std::pair<defs::GlobalAddress, CacheItem> latest;

    for (auto& it: items) {
        if(latest.second.lastused.time_since_epoch() < it.second.lastused.time_since_epoch()){
            latest = it;
        }
        std::cout << std::chrono::system_clock::to_time_t(latest.second.lastused) << std::endl;
        std::cout << it.second.lastused.time_since_epoch() << std::endl;
    }
    availablesize = availablesize+latest.first.size;
    items.erase(latest.first);


}

CacheItem Cache::getCacheItem(defs::GlobalAddress ga){
    auto cacheItem = items.find(ga);
    if(cacheItem){
        return cacheItem;
    }
    else {
        return cacheItem{};
    }
}

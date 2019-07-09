//
// Created by Magdalena Pröbstl on 2019-06-24.
//

#ifndef MEDMM_CACHE_H
#define MEDMM_CACHE_H


#include <cstddef>
#include <cstdint>
#include <chrono>
#include "../util/defs.h"
#include <map>



struct CacheItem {
    defs::GlobalAddress globalAddress;
    uint64_t data;
    std::chrono::time_point<std::chrono::system_clock> created;
    std::chrono::time_point<std::chrono::system_clock> lastused;
};

class Cache {
private:
    size_t maxsize; //is set 512 in constructor
    size_t availablesize;
    std::map<uint64_t, CacheItem> items;

    void removeOldestItem();
public:
    explicit Cache();

    void addCacheItem(defs::GlobalAddress gaddr, CacheItem cacheItem);

    void removeCacheItem(defs::GlobalAddress gaddr);

    CacheItem *getCacheItem(defs::GlobalAddress ga);

    defs::CACHE_DIRECTORY_STATE state;
};




#endif //MEDMM_CACHE_H

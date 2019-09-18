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
    defs::GlobalAddress globalAddress{};
    std::vector<char> data;
    std::chrono::time_point<std::chrono::system_clock> created;
    std::chrono::time_point<std::chrono::system_clock> lastused;

    CacheItem(defs::GlobalAddress ga, std::vector<char> d, std::chrono::time_point<std::chrono::system_clock> c,
    std::chrono::time_point<std::chrono::system_clock> l){
        globalAddress = ga;
        data = d;
        created = c;
        lastused = l;
    }
    CacheItem() = default;
};

class Cache {
private:
    size_t availablesize;
    std::map<uint64_t, CacheItem> items;

    void removeOldestItem();
public:
    explicit Cache();

    void addCacheItem(defs::GlobalAddress gaddr, char * data, size_t size);

    uint64_t removeCacheItem(defs::GlobalAddress ga);

    CacheItem *getCacheItem(defs::GlobalAddress ga);

    void alterCacheItem(char * data, defs::GlobalAddress ga, size_t size);

    defs::CACHE_DIRECTORY_STATE state;
};




#endif //MEDMM_CACHE_H

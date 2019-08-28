//
// Created by Magdalena Pröbstl on 2019-07-02.
//

#ifndef MEDMM_GLOBALADDRESSHASH_H
#define MEDMM_GLOBALADDRESSHASH_H


#include <functional>
#include "defs.h"

template<class T> class GlobalAddressHash;

    template<>
    class GlobalAddressHash<defs::SendGlobalAddr> {
    public:
        uint64_t operator()(const defs::SendGlobalAddr &ga) const
        {
            uint64_t h1 = std::hash<size_t>()(ga.size);
            uint64_t h2 = std::hash<uint64_t>()(ga.ptr);
            uint64_t h3 = std::hash<uint64_t>()(ga.id);
            uint64_t h4 = std::hash<bool>()(ga.isFile);
            return (h1 ^ (h2 ^(h3 ^  (h4<<1))));
        }
    };


#endif //MEDMM_GLOBALADDRESSHASH_H

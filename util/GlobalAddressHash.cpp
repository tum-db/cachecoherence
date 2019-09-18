//
// Created by Magdalena Pröbstl on 18.09.19.
//
#include "GlobalAddressHash.h"

uint64_t GlobalAddressHash<defs::SendGlobalAddr>::operator()(const defs::SendGlobalAddr &ga) const {
    uint64_t h1 = std::hash<uintptr_t>()(ga.ptr);
    uint64_t h2 = std::hash<uint64_t>()(ga.id);
    uint64_t h3 = std::hash<bool>()(ga.isFile);
    return (h1 ^ (h2 ^ (h3 << 1)));
}

uint64_t GlobalAddressHash<defs::SendGlobalAddr>::generateLockId(defs::SendGlobalAddr sga) {
    return GlobalAddressHash<defs::SendGlobalAddr>()(sga);
}
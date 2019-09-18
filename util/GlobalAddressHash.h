//
// Created by Magdalena Pröbstl on 2019-07-02.
//

#ifndef MEDMM_GLOBALADDRESSHASH_H
#define MEDMM_GLOBALADDRESSHASH_H


#include <functional>
#include "defs.h"

template<class T>
class GlobalAddressHash;

template<>
class GlobalAddressHash<defs::SendGlobalAddr> {
public:
    uint64_t operator()(const defs::SendGlobalAddr &ga) const;

    static uint64_t generateLockId(defs::SendGlobalAddr sga);

};


#endif //MEDMM_GLOBALADDRESSHASH_H

//
// Created by Magdalena Pröbstl on 28.08.19.
//

#ifndef MEDMM_LOCK_H
#define MEDMM_LOCK_H


#include "GlobalAddressHash.h"

enum LOCK_STATES {
    UNLOCKED = 0,
    SHAREDLOCK = 1,
    EXCLUSIVE = 2
};

struct __attribute__ ((packed)) Lock {
    uint64_t id;
    LOCK_STATES state;
    uint16_t nodeId;
};



#endif //MEDMM_LOCK_H

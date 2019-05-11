//
// Created by Magdalena Pröbstl on 2019-05-07.
//

#ifndef MEDMM_WORKREQUEST_H
#define MEDMM_WORKREQUEST_H


#include <cstdint>
#include "../util/defs.h"


enum CACHE_DIRECTORY_STATES {
    UNSHARED = 0,
    SHARED = 1,
    DIRTY = 2
};

enum LOCALREMOTE {
    LOCAL = 0,
    REMOTE = 1
};

class WorkRequest {
    uint16_t id;
    GlobalAddress addr;

};

#endif //MEDMM_WORKREQUEST_H

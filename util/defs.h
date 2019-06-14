//
// Created by Magdalena Pröbstl on 2019-05-07.
//
#include <cstddef>

#ifndef MEDMM_DEFS_H
#define MEDMM_DEFS_H

const char ip[] = "127.0.0.1";
const uint16_t port = 3000;

const uint16_t homenode = 1;


struct __attribute__ ((packed)) GlobalAddress {
    size_t size;
    uint64_t *ptr;
    uint16_t id;
};

struct __attribute__ ((packed)) SendData {
    size_t size;
    uint64_t *data;
    GlobalAddress *ga;
};

inline uint16_t getNodeId(GlobalAddress *gaddr) {
    return gaddr->id;
}


enum CACHE_DIRECTORY_STATES {
    UNSHARED = 0,
    SHARED = 1,
    DIRTY = 2
};

struct __attribute__ ((packed)) Lock {
    uint16_t id;
    CACHE_DIRECTORY_STATES state;
};
/*
 * #define WID(gaddr) ((gaddr) >> 48)

int GetWorkerId() {
    //TODO
    return 0;
}
bool IsLocal(GlobalAddress addr) {
    return WID(addr) == GetWorkerId();
}

*/
#endif //MEDMM_DEFS_H

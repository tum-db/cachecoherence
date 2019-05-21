//
// Created by Magdalena Pröbstl on 2019-05-07.
//
#include <cstddef>

#ifndef MEDMM_DEFS_H
#define MEDMM_DEFS_H

const char ip[] = "127.0.0.1";
const uint16_t port = 3000;


struct GlobalAddress {
    size_t size;
    uint64_t *ptr;
    uint16_t id;
};

struct SendData {
    size_t size;
    uint8_t *data;
};

inline uint16_t getNodeId(GlobalAddress *gaddr) {
    return gaddr->id;
}
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

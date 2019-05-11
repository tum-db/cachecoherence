//
// Created by Magdalena Pröbstl on 2019-05-07.
//

#ifndef MEDMM_DEFS_H
#define MEDMM_DEFS_H

const char ip[] = "127.0.0.1";
const uint16_t port = 3000;


typedef uint64_t GlobalAddress;

inline uint16_t getNodeId(GlobalAddress gaddr){
    return gaddr >> 16 &0xFF;
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

//
// Created by Magdalena Pröbstl on 2019-04-30.
//

#include <cstdint>
#include "GlobalAddress.h"

enum class CACHE_DIRECTORY_STATES {
    UNSHARED = 0,
    SHARED = 1,
    DIRTY = 2
};

enum class LOCALREMOTE {
    LOCAL = 0,
    REMOTE = 1
};

void malloc(GlobalAddress gaddr, uint16_t size, CACHE_DIRECTORY_STATES cds, LOCALREMOTE lr){

}

void free(GlobalAddress gaddr){

}

void read(GlobalAddress gaddr, LOCALREMOTE lr){

};

void write(GlobalAddress gaddr, LOCALREMOTE lr, uint16_t size, ){

}
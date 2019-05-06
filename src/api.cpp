//
// Created by Magdalena Pröbstl on 2019-04-30.
//

#include <cstdint>
#include "GlobalAddress.h"
#include "Node.h"
#include <stdlib.h>
#include <cstdio>


enum class CACHE_DIRECTORY_STATES {
    UNSHARED = 0,
    SHARED = 1,
    DIRTY = 2
};

enum class LOCALREMOTE {
    LOCAL = 0,
    REMOTE = 1
};

void* malloc(Node node, GlobalAddress gaddr, uint16_t size, LOCALREMOTE lr){
    if(lr == LOCALREMOTE::LOCAL){
        auto buffer = malloc(size);
        return buffer;
    }
    else if (lr == LOCALREMOTE::REMOTE){
      /*  auto sendbuf = std::vector<char>(size);
        auto sendmr = network.registerMr(sendbuf.data(), sendbuf.size(), {});
        auto write = ibv::workrequest::Simple<ibv::workrequest::WriteWithImm>{};
        write.setLocalAddress(sendmr->getSlice());
        write.setInline();
        write.setSignaled();
        write.setRemoteAddress(remoteMr);
        write.setImmData(0);*/
      return nullptr;
    }
    else {
        perror("This should not happen!");
        return nullptr;
    }
}

void free(GlobalAddress gaddr){
    void* buffer; //TODO how is the location of the data determined?

    free(buffer);
}

void read(GlobalAddress gaddr, LOCALREMOTE lr){
    if(lr == LOCALREMOTE::LOCAL){

    }
}

void write(GlobalAddress gaddr, LOCALREMOTE lr, uint16_t size){

}
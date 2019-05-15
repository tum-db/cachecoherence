//
// Created by Magdalena Pröbstl on 2019-04-30.
//

#include <cstdint>
#include "Node.h"
#include <stdlib.h>
#include <cstdio>
#include "../util/defs.h"
#include "WorkRequest.h"


GlobalAddress *Node::sendRemoteMalloc(size_t *size) {
    auto addr = send(size, *size, 1);
    return addr;
}

GlobalAddress *Node::Malloc(size_t *size) {
    auto buffer = malloc(*size);
    if (buffer) {
        auto *gaddr = new GlobalAddress{static_cast<uint64_t *>(buffer), id};
        return gaddr;
    } else {
        return sendRemoteMalloc(size);
    }

}


void free(GlobalAddress gaddr) {
    void *buffer; //TODO how is the location of the data determined?

    free(buffer);
}

void read(WorkRequest wr, GlobalAddress gaddr, LOCALREMOTE lr) {
    if (lr == LOCALREMOTE::LOCAL) {

    }
}

void write(GlobalAddress gaddr, LOCALREMOTE lr, uint16_t size) {

}
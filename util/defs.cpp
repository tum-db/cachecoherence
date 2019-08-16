//
// Created by Magdalena Pröbstl on 2019-06-15.
//

#include <libibverbscpp.h>
#include "defs.h"

ibv::workrequest::Simple<ibv::workrequest::WriteWithImm>
defs::createWriteWithImm(ibv::memoryregion::Slice slice,
                         ibv::memoryregion::RemoteAddress remoteMr, IMMDATA immData) {
    auto write = ibv::workrequest::Simple<ibv::workrequest::WriteWithImm>{};
    write.setLocalAddress(slice);
    write.setInline();
    write.setSignaled();
    write.setRemoteAddress(remoteMr);
    write.setImmData(immData);
    return write;
}
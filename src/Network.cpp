//
// Created by Magdalena Pröbstl on 2019-04-09.
//

#include "Network.h"



std::unique_ptr<ibv::memoryregion::MemoryRegion> Network::registerMr(void *addr, size_t length,
                                                                 std::initializer_list <ibv::AccessFlag> flags) {
    return protectionDomain->registerMemoryRegion(addr, length, flags);
}

ibv::protectiondomain::ProtectionDomain &Network::getProtectionDomain() {
    return *protectionDomain;
}


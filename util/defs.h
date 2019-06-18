//
// Created by Magdalena Pröbstl on 2019-05-07.
//
#include <cstddef>

#ifndef MEDMM_DEFS_H
#define MEDMM_DEFS_H
namespace defs {

    const char ip[] = "127.0.0.1";
    const uint16_t port = 3000;

    const uint16_t homenode = 1;

    constexpr size_t BIGBADBUFFER_SIZE = 1024 * 1024 * 8; // 8MB



    struct __attribute__ ((packed)) GlobalAddress {
        size_t size;
        uint64_t ptr;
        uint16_t id;
    };

    struct __attribute__ ((packed)) SendData {
        size_t size;
        uint64_t data;
        GlobalAddress ga;
    };

    inline uint16_t getNodeId(GlobalAddress *gaddr) {
        return gaddr->id;
    }


    enum CACHE_DIRECTORY_STATES {
        UNSHARED = 0,
        SHARED = 1,
        DIRTY = 2
    };

    enum IMMDATA {
        DEFAULT = 0,
        MALLOC = 1,
        READ = 2,
        FREE = 3,
        WRITE = 4,
        LOCKS = 5,
        RESET = 6,
        GETLOCK = 7
    };

    struct __attribute__ ((packed)) Lock {
        uint16_t id;
        CACHE_DIRECTORY_STATES state;
    };


    ibv::workrequest::Simple<ibv::workrequest::WriteWithImm>
    createWriteWithImm(ibv::memoryregion::Slice slice,
                       ibv::memoryregion::RemoteAddress remoteMr, IMMDATA immData);
}
#endif //MEDMM_DEFS_H

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


    struct __attribute__ ((packed)) SendGlobalAddr {
        size_t size;
        uint64_t ptr;
        uint16_t id;
    };

    struct __attribute__ ((packed)) GlobalAddress {
        size_t size{};
        void *ptr{};
        uint16_t id{};

        SendGlobalAddr sendable(){
            SendGlobalAddr sga{};
            sga.size = size;
            sga.ptr = reinterpret_cast<uint64_t >(ptr);
            sga.id = id;
            return sga;
        };
        GlobalAddress(){};
        GlobalAddress(size_t s, void* p, uint16_t i){
            size = s;
            ptr = p;
            id = i;
        };

        explicit GlobalAddress(SendGlobalAddr sga){
            size = sga.size;
            ptr = reinterpret_cast<void *>(sga.ptr);
            id = sga.id;
        };

        uint16_t getNodeId() {
            return id;
        };
    };



    struct __attribute__ ((packed)) SendingData {
        size_t size;
        uint64_t data;
        SendGlobalAddr sga;

    };



    struct __attribute__ ((packed)) SendData{
        size_t size;
        uint64_t data;
        GlobalAddress ga;
        SendingData sendable() {
            SendingData sd{};
            sd.sga = ga.sendable();
            sd.size = size;
            sd.data = data;
            return sd;
        }
        SendData(){};

        SendData(size_t s, uint64_t d, GlobalAddress g){
            size = s;
            data = d;
            ga = g;
        };

        explicit SendData(SendingData sd){
            size = sd.size;
            data = sd.data;
            ga = GlobalAddress(sd.sga);
        }
    };

    enum LOCK_STATES {
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
        RESET = 6
    };



    struct __attribute__ ((packed)) Lock {
        uint16_t id;
        LOCK_STATES state;
    };


    ibv::workrequest::Simple<ibv::workrequest::WriteWithImm>
    createWriteWithImm(ibv::memoryregion::Slice slice,
                       ibv::memoryregion::RemoteAddress remoteMr, IMMDATA immData);
}
#endif //MEDMM_DEFS_H

//
// Created by Magdalena Pröbstl on 2019-05-07.
//
#include <cstddef>
#include "../include/libibverbscpp/libibverbscpp.h"
#include <vector>

#ifndef MEDMM_DEFS_H
#define MEDMM_DEFS_H

namespace defs {

    const char ip[] = "127.0.0.1";
    const uint16_t port = 3000;

    constexpr size_t MAX_BLOCK_SIZE = 1024;//512 + 256 +128 +16; // 912, bigger generates error
    const uint16_t locknode = 2000;

    constexpr size_t MAX_TEST_MEMORY_SIZE = 1024*1024 *1024; //1024*100

    constexpr size_t BIGBADBUFFER_SIZE = 1024 * 1024 * 8; // 8MB

    enum CACHE_DIRECTORY_STATE {
        UNSHARED = 0,
        SHARED = 1,
        EXCLUS = 2
    };

    struct RDNNBR {
        uint16_t rdnnbr = 0;
        const char * name;

        RDNNBR() {
            rdnnbr++;
            std::string s = std::to_string(rdnnbr);
            name = s.c_str();
        };
    };


    struct __attribute__ ((packed)) SendGlobalAddr {
        size_t size;
        uint64_t ptr;
        uint16_t id;
        uint16_t srcID;
        bool isFile;
    };

    struct __attribute__ ((packed)) GlobalAddress {
        size_t size;
        void *ptr;
        uint16_t id;
        bool isFile;

        SendGlobalAddr sendable(uint16_t srcID) {
            SendGlobalAddr sga{};
            sga.size = size;
            sga.ptr = reinterpret_cast<uint64_t >(ptr);
            sga.id = id;
            sga.srcID = srcID;
            sga.isFile = isFile;
            return sga;
        };

        GlobalAddress() = default;

        GlobalAddress(size_t s, void *p, uint16_t i, bool iF) {
            size = s;
            ptr = p;
            id = i;
            isFile = iF;
        };

        explicit GlobalAddress(SendGlobalAddr sga) {
            size = sga.size;
            ptr = reinterpret_cast<void *>(sga.ptr);
            id = sga.id;
            isFile = sga.isFile;
        };

        uint16_t getNodeId() {
            return id;
        };

        void resize(size_t newsize){
            size = newsize;
        };
    };


    struct __attribute__ ((packed)) SendingData {
        size_t size;
        uint64_t data;
        SendGlobalAddr sga;

    };


    struct __attribute__ ((packed)) Data {
        size_t size;
        uint64_t data;
        GlobalAddress ga;

        SendingData sendable(uint16_t id) {
            SendingData sd{};
            sd.sga = ga.sendable(id);
            sd.size = size;
            sd.data = data;
            return sd;
        };

        Data() = default;

        Data(size_t s, uint64_t d, GlobalAddress g) {
            size = s;
            data = d;
            ga = g;
        };

        explicit Data(SendingData sd) {
            size = sd.size;
            data = sd.data;
            ga = GlobalAddress(sd.sga);
        };
    };

    struct SaveData {
        uint64_t data;
        CACHE_DIRECTORY_STATE iscached;
        uint16_t ownerNode;
        std::vector<uint16_t> sharerNodes;

        SaveData (const SaveData &) = default;
    };


    enum IMMDATA {
        DEFAULT = 0,
        MALLOC = 1,
        READ = 2,
        FREE = 3,
        WRITE = 4,
        LOCKS = 5,
        RESET = 6,
        INVALIDATE = 7,
        FILE = 8,
        MALLOCFILE = 9,
        READFILE = 10,
        WRITEFILE = 11
    };


    struct __attribute__ ((packed)) FileInfo {
        size_t size;
        size_t blocksize;
    };

    struct __attribute__ ((packed)) ReadFileData {
        SendGlobalAddr sga;
        size_t offset;
        size_t size;
    };

    ibv::workrequest::Simple<ibv::workrequest::WriteWithImm>
    createWriteWithImm(ibv::memoryregion::Slice slice,
                       ibv::memoryregion::RemoteAddress remoteMr, IMMDATA immData);

}
#endif //MEDMM_DEFS_H

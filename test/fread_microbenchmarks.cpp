//
// Created by Magdalena Pröbstl on 04.10.19.
//

#include <src/Node.h>
#include "PerfEvent.hpp"


int main() {
    auto clientnode = Node();
    clientnode.setID(2000);
// Define some global params
    BenchmarkParameters params;
    params.setParam("name", "Local FReadF");
    params.setParam("dataSize", defs::MAX_BLOCK_SIZE);
    auto f = MaFile("test", moderndbs::File::READ);
    std::vector<char> block;
    block.resize(defs::MAX_BLOCK_SIZE);
    char *readed = &block[0];
    f.read_block(0, defs::MAX_BLOCK_SIZE, readed);
    int maxThreads = 1000;
    char *filename = "test1";
    auto fileaddress = defs::GlobalAddress(defs::MAX_BLOCK_SIZE, filename, 2000, true);
    auto remotefileaddress = defs::GlobalAddress(defs::MAX_BLOCK_SIZE, filename, 3000, true);
    clientnode.FprintF(readed,fileaddress,defs::MAX_BLOCK_SIZE,0);
    clientnode.FprintF(readed,remotefileaddress,defs::MAX_BLOCK_SIZE,0);

    for (int threads = 1; threads < maxThreads; ++threads) {

// Change local parameters like num threads
        params.setParam("threads", threads);

// Only print the header for the first iteration
        bool printHeader = threads == 1;

        PerfEventBlock e(1, params, printHeader);
// Counter are started in constructor

        clientnode.FreadF(fileaddress, defs::MAX_BLOCK_SIZE, 0, readed);

// Benchmark counters are automatically stopped and printed on destruction of e
    }

    params.setParam("name", "Remote FReadF");
    for (int threads = 1; threads < maxThreads; ++threads) {

// Change local parameters like num threads
        params.setParam("threads", threads);

// Only print the header for the first iteration
        bool printHeader = false;

        PerfEventBlock e(1, params, printHeader);
// Counter are started in constructor
        clientnode.FreadF(remotefileaddress, defs::MAX_BLOCK_SIZE, 0, readed);

// Benchmark counters are automatically stopped and printed on destruction of e
    }
    return 1;
}
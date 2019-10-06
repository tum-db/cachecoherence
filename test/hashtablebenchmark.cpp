//
// Created by Magdalena Pröbstl on 05.10.19.
//

//
// Created by Magdalena Pröbstl on 04.10.19.
//

#include <src/Node.h>
#include "PerfEvent.hpp"
#include "app/HashTable.h"


int main() {
    auto clientnode = Node();
    clientnode.setID(2000);
    HashTable<std::vector<uint64_t>> ht = HashTable<std::vector<uint64_t>>(&clientnode);
// Define some global params
    BenchmarkParameters params;
    params.setParam("name", "Hashtable insert");
    std::vector<uint64_t> testdata(defs::MAX_BLOCK_SIZE / sizeof(uint64_t), 123);
    params.setParam("dataSize", testdata.size() * sizeof(uint64_t));

    int maxThreads = 1000;

    for (int threads = 1; threads < maxThreads; ++threads) {

        params.setParam("threads", threads);

        bool printHeader = threads == 1;

        PerfEventBlock e(1, params, printHeader);

        ht.insert(threads, testdata, testdata.size()*sizeof(uint64_t));
    }
    std::vector<uint64_t> testdata2(defs::MAX_BLOCK_SIZE / sizeof(uint64_t), 456);

    params.setParam("name", "Hashtable update");
    for (int threads = 1; threads < maxThreads; ++threads) {

        params.setParam("threads", threads);

        bool printHeader = false;

        PerfEventBlock e(1, params, printHeader);
        ht.update(threads, testdata2);
    }

    return 1;
}
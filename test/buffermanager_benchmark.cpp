//
// Created by Magdalena Pröbstl on 07.10.19.
//

//
// Created by Magdalena Pröbstl on 05.10.19.
//

//
// Created by Magdalena Pröbstl on 04.10.19.
//

#include <src/Node.h>
#include <buffermanager/buffer_manager.h>
#include "PerfEvent.hpp"


int main() {
    auto n = Node();
    n.setID(2000);
    auto buffer_manager = std::make_unique<moderndbs::BufferManager>(defs::MAX_BLOCK_SIZE, 60, &n,
                                                                     HashTable<moderndbs::BufferFrame>(
                                                                             &n));

    BenchmarkParameters params;
    std::vector<uint64_t> testdata(defs::MAX_BLOCK_SIZE / sizeof(uint64_t), 123);
    params.setParam("dataSize", testdata.size() * sizeof(uint64_t));
    int threads = 1;
    for (uint16_t segment = 0; segment < 5; ++segment) {
        for (uint64_t segment_page = 0; segment_page < 60; ++segment_page) {
            params.setParam("threads", threads);
            params.setParam("segment", segment);
            params.setParam("segment_page", segment_page);
            params.setParam("name", "fixpage");
            bool printHeader = threads == 1;
            uint64_t page_id = 1000*segment+segment_page;

            moderndbs::BufferFrame page;
            {
                PerfEventBlock e(1, params, printHeader);
                page = buffer_manager->fix_page(page_id, true);
            }
            page.get_data();
            auto value = const_cast<char *>(std::to_string(segment * 100 + segment_page).c_str());
            buffer_manager->insert_data(page, value, sizeof(char) * strlen(value));
            params.setParam("name", "unfixpage");
            {
                PerfEventBlock e(1, params, false);
                buffer_manager->unfix_page(page, true);
            }
            threads++;
        }
    }
// Destroy the buffer manager and create a new one.
    buffer_manager = std::make_unique<moderndbs::BufferManager>(defs::MAX_BLOCK_SIZE, 60, &n,
                                                                HashTable<moderndbs::BufferFrame>(
                                                                        &n));
    for (uint16_t segment = 0; segment < 5; ++segment) {
        for (uint64_t segment_page = 0; segment_page < 60; ++segment_page) {

            params.setParam("threads", threads);
            params.setParam("segment", segment);
            params.setParam("segment_page", segment_page);
            params.setParam("name", "fixpage");
            uint64_t page_id =  1000*segment+segment_page;
            moderndbs::BufferFrame page;
            {
                PerfEventBlock e(1, params, false);
                page = buffer_manager->fix_page(page_id, false);
            }
            page.get_data();
            auto value = const_cast<char *>(std::to_string(segment * 50 + segment_page).c_str());
            buffer_manager->insert_data(page, value, sizeof(char) * strlen(value));
            params.setParam("name", "unfixpage");
            {
                PerfEventBlock e(1, params, false);
                buffer_manager->unfix_page(page, false);
            }
            threads++;
        }
    }

    return 1;
}
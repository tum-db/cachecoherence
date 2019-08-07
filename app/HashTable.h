//
// Created by Magdalena Pröbstl on 2019-08-01.
//

#ifndef MEDMM_HASHTABLE_H
#define MEDMM_HASHTABLE_H


#include <cstdint>
#include <optional>
#include <vector>
#include <array>
#include "HashBucket.h"
#include "../src/Node.h"
#include <iostream>
#include <cmath>


const uint32_t amountNodes = 3;
const uint16_t ns[] = {2000, 3000, 4000};

template<typename V>
class HashTable {

    Node node;

    static constexpr uint32_t hash(uint32_t key) {
        key = (key ^ 61) ^ (key >> 16);
        key = key + (key << 3);
        key = key ^ (key >> 4);
        key = key * 0x27d4eb2d;
        key = key ^ (key >> 15);
        return key;
    }

    struct Elem {
        uint32_t key;
        defs::GlobalAddress gaddr;
        Elem *next;
    };


    std::size_t amountElements = 0;


    std::vector<Elem *> storage;

    uint32_t hashBucket(uint32_t key) const { return hash(key) % storage.size(); }

public:
    /**
     * Constructor
     */

    HashTable() : node() {
        // buckets = new std::array<Buck, amountNodes>;
//        uint32_t lb = 0;
//        uint32_t ub = 0;
//        for (int i = 0; i < amountNodes; ++i) {
//            if(i == amountNodes-1){
//                ub = std::numeric_limits<uint32_t>::max();
//            } else {
//                ub = lb + std::numeric_limits<uint32_t>::max()/ amountNodes;
//            }
//            buckets[i] = HashBucket<V>(ns[i], lb, ub);
//            lb = ub;
//            std::cout << buckets[i].getId() << ", " << buckets[i].getLb() << ", " << buckets[i].getUb() << std::endl;
//        }
    }


    /**
     * Destructor
     */
    ~HashTable() { clear(); }

    /**
     * insert value to HT if not exists
     *
     * used for non-overwriting inserts
     *
     * @param key
     * @param value
     */
    void insert(uint32_t key, V value);


    void erase(uint32_t key);

    std::optional<V> get(uint32_t key) const;

    V &operator[](uint32_t key);


    /**
     * get count of contained elements
     *
     * used for containment check
     *
     * @param key
     * @return 0 if not contained, 1 if contained
     */
    uint64_t count(uint32_t key) const {
        return get(key).has_value();
    }

    /**
     * get number of stored element
     *
     * @return HT size
     */
    std::size_t size() const {
        return amountElements;
    }

    /**
     * is HT empty
     *
     * @return true if HT empty
     */
    bool empty() const {

        return !size();
    }

    /**
     * empty the HT
     */
    void clear() {
        for (auto &b: storage) {
            node.Free(b->gaddr);
        }
        amountElements = 0;
    }

};


#endif //MEDMM_HASHTABLE_H

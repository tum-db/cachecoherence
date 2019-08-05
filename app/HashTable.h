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
#include <iostream>


const uint32_t amountNodes = 2;
const uint16_t ns[] = {2000, 3000};

template<typename V>
struct Element{
    uint32_t key;
    V value;
    Element *next;
};

template<typename V>
class HashTable {

    static constexpr uint32_t hash(uint32_t key) {
        key = (key ^ 61) ^ (key >> 16);
        key = key + (key << 3);
        key = key ^ (key >> 4);
        key = key * 0x27d4eb2d;
        key = key ^ (key >> 15);
        return key;
    }

    struct Buck{
        uint16_t id;
        uint64_t lb; //lowerbound
        uint64_t ub; //upperbound
    };

    std::array<Buck, amountNodes> buckets;

    std::size_t amountElements = 0;
public:
    /**
     * Constructor
     */

   HashTable() {
       // buckets = new std::array<Buck, amountNodes>;
        auto lb = 0;
        for (int i = 0; i < amountNodes; ++i) {
            buckets[i] = Buck{ns[i], lb, lb + sizeof(lb) / amountNodes};
            lb = lb + sizeof(lb) / amountNodes;
            std::cout << buckets[i].id << ", " << buckets[i].lb << ", " << buckets[i].ub << std::endl;
        }
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

    /**
     * remove element from HT
     *
     * used for remove
     *
     * @param key
     */
    void erase(uint32_t key) {

        // ## INSERT CODE HERE
    }

    /**
     * get element from HT wrapped in optional
     *
     * used for const lookup
     *
     * @param key
     * @return optional containing the element or nothing if not exists
     */
    std::optional<V> get(uint32_t key) const {
        // ## INSERT CODE HERE
        return std::nullopt;
    }

    /**
     * get reference to existing HT element or insert new at key
     *
     * used for lookups, inserts, and editing the HT elements
     *
     * @param key
     * @return reference to HT element
     */
    V &operator[](uint32_t key) {
        // ## INSERT CODE HERE
        // return 0;
    }

    /**
     * get count of contained elements
     *
     * used for containment check
     *
     * @param key
     * @return 0 if not contained, 1 if contained
     */
    uint64_t count(uint32_t key) const {
        // ## INSERT CODE HERE
        return 0;
    }

    /**
     * get number of stored element
     *
     * @return HT size
     */
    std::size_t size() const {
        // ## INSERT CODE HERE
        return 0;
    }

    /**
     * is HT empty
     *
     * @return true if HT empty
     */
    bool empty() const {
        // ## INSERT CODE HERE
        return false;
    }

    /**
     * empty the HT
     */
    void clear() {
        // ## INSERT CODE HERE
    }

};


#endif //MEDMM_HASHTABLE_H

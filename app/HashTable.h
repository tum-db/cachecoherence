//
// Created by Magdalena Pröbstl on 2019-08-01.
//

#ifndef MEDMM_HASHTABLE_H
#define MEDMM_HASHTABLE_H


#include <cstdint>
#include <optional>
#include <vector>
#include <array>
#include "../src/Node.h"
#include <iostream>
#include <cmath>


const uint32_t amountNodes = 3;
const uint16_t ns[] = {2000, 3000, 4000};

template<typename V>
class HashTable {
private:
    Node *node;

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

    explicit HashTable(Node *n) {
        node = n;
        storage.resize(64);
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
    void insert(uint32_t key, V value) {
        uint32_t b = hashBucket(key);
        auto gadd = node->Malloc(sizeof(V), node->getID());
        storage[b] = new Elem({key, gadd, storage[b]});
        ++amountElements;
        auto data = new defs::Data(sizeof(V), static_cast<uint64_t>(value), gadd);
        node->write(data);
    }


/**
    * remove element from HT
    *
    * used for remove
    *
    * @param key
    */

    void erase(uint32_t key) {
        uint32_t b = hashBucket(key);
        Elem *bucket = storage[b];
        Elem *oldBucket = nullptr;
        while (bucket != nullptr) {
            if (bucket->key == key) {
                if (oldBucket) {
                    oldBucket->next = bucket->next;
                } else {
                    storage[b] = bucket->next;
                }
                node->Free(bucket->gaddr);
                delete (bucket);
                --amountElements;
                return;
            }
            oldBucket = bucket;
            bucket = bucket->next;
        }
    }

/**
    * get element from HT wrapped in optional
    *
    * used for const lookup
    *
    * @param key
    * @return optional containing the element or nothing if not exists
    */
    std::optional<V> get(uint32_t key) {
        uint32_t b = hashBucket(key);
        Elem *bucket = storage[b];
        while (bucket != nullptr) {
            if (bucket->key == key) {
                auto value = node->read(bucket->gaddr);
                auto result = reinterpret_cast<V *>(&value);
                return *result;
            }
            bucket = bucket->next;
        }
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
        uint32_t b = hashBucket(key);
        Elem *bucket = storage[b];
        while (bucket != nullptr) {
            if (bucket->key == key) {
                auto addr = bucket->gaddr;
                uint64_t value = node->read(addr);
                auto result = reinterpret_cast<V *>(&value);
                return *result;
            }
            bucket = bucket->next;
        }
        auto gadd = node->Malloc(sizeof(V), node->getID());
        storage[b] = new Elem({key, gadd, storage[b]});
        ++amountElements;
        uint64_t value = node->read(gadd);
        auto result = reinterpret_cast<V *>(&value);
        return *result;

    }


    /**
     * get count of contained elements
     *
     * used for containment check
     *
     * @param key
     * @return 0 if not contained, 1 if contained
     */
    uint64_t count(uint32_t key) {
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
            while (b != nullptr) {
                Elem *oldBucket = b;
                b = oldBucket->next;
                node->Free(oldBucket->gaddr);
            }
        }
        amountElements = 0;
    }

};


#endif //MEDMM_HASHTABLE_H

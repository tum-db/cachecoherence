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


template<typename V>
class HashTable {
private:
    Node *node;

    static constexpr uint64_t hash(uint64_t key) {
        key = (key ^ 61) ^ (key >> 16);
        key = key + (key << 3);
        key = key ^ (key >> 4);
        key = key * 0x27d4eb2d;
        key = key ^ (key >> 15);
        return key;
    }

    struct Elem {
        uint64_t key;
        defs::GlobalAddress gaddr;
        Elem *next;

        bool operator==(const Elem &a) {
            return (a.next == next) && (a.gaddr.size == gaddr.size) &&
                   (a.gaddr.isFile == gaddr.isFile) && (a.gaddr.id == gaddr.id) &&
                   (a.gaddr.ptr == gaddr.ptr) && (a.key == key);
        }
    };

    struct Iter {

        Elem *e;
        std::vector<Elem *> store;
        Node *node;
        char *value;

        Iter(Elem *el, std::vector<Elem *> &s, Node *n, char *v) {
            e = el;
            store = s;
            node = n;
            value = v;
        }

        bool operator==(const Iter &a) {
            return (e->next == a.e->next) && (a.e->gaddr.size == e->gaddr.size) &&
                   (a.e->gaddr.isFile == e->gaddr.isFile) && (a.e->gaddr.id == e->gaddr.id) &&
                   (a.e->gaddr.ptr == e->gaddr.ptr) && (a.e->key == e->key);
        }

        bool operator!=(const Iter &a) {
            return !operator==(a);
        }

        V *operator*() {
            return reinterpret_cast<V *>(value);
        }

        Iter &operator++() {
            Iter current = *this;
            if (e->next != nullptr) {
                e = e->next;
            } else {
                Elem *tmp = new Elem{};
                for (auto &b: store) {
                    if (b != nullptr) {
                        //   std::cout << "key of next value: " << b->key << std::endl;
                        if (e == tmp) {
                            e = b;
                            break;
                        }
                        tmp = b;
                    }
                }
            }
            value = node->read(e->gaddr);
            if (*this == current) {
                e = new Elem{};
                value = "\0";
            }
            return *this;
        }

        Iter operator++(int) {
            Iter ret = *this;
            this->operator++();
            return ret;
        }


    };

    std::size_t amountElements = 0;


    std::vector<Elem *> storage;


    [[nodiscard]] uint64_t hashBucket(uint64_t key) const { return hash(key) % storage.size(); }

public:
    /**
     * Constructor
     */

    explicit HashTable(Node *n) {
        node = n;
        storage = std::vector<Elem *>(64, nullptr);
    }


    HashTable(const HashTable &) = default;

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
    void insert(uint64_t key, V value, size_t size = sizeof(V)) {
        uint64_t b = hashBucket(key);
        auto gadd = node->Malloc(size, node->getID());
        storage[b] = new Elem({key, gadd, storage[b]});
        ++amountElements;
        auto castdata = reinterpret_cast<char *>(&value);
        auto data = defs::Data(size, castdata, gadd);
        node->write(data);
    }


/**
    * remove element from HT
    *
    * used for remove
    *
    * @param key
    */

    void erase(uint64_t key) {
        uint64_t b = hashBucket(key);
        Elem *bucket = storage[b];
        Elem *oldBucket = nullptr;
        while (bucket != nullptr) {
            if (bucket->key == key) {
                if (oldBucket) {
                    oldBucket->next = bucket->next;
                } else {
                    storage[b] = bucket->next;
                }
                node->Free(bucket->gaddr, node->getID());
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
    std::optional<V> get(uint64_t key) {
        uint64_t b = hashBucket(key);
        Elem *bucket = storage[b];
        while (bucket != nullptr) {
            if (bucket->key == key) {
                auto value = node->read(bucket->gaddr);
                auto result = reinterpret_cast<V *>(value);
                return std::make_optional<V>(*result);
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
    const V &operator[](uint64_t key) {
        uint64_t b = hashBucket(key);
        Elem *bucket = storage[b];
        while (bucket != nullptr) {
            if (bucket->key == key) {
                auto value = node->read(bucket->gaddr);
                auto result = reinterpret_cast<V *>(value);
                return *result;
            }
            bucket = bucket->next;
        }
    }


    Iter begin() {
        for (auto &b: storage) {
            if (b != nullptr) {
                return Iter(b, storage, node, node->read(b->gaddr));
            }
        }
    }

    Iter begin() const {
        for (auto &b: storage) {
            if (b != nullptr) {
                return Iter(b, storage, node, node->read(b->gaddr));
            }
        }
    }

    Iter end() { return Iter{new Elem{}, storage, node, "\0"}; }

    Iter end() const { return Iter{new Elem{}, storage, node, "\0"}; }


    void update(uint64_t key, V value) {
        uint64_t b = hashBucket(key);
        Elem *bucket = storage[b];
        while (bucket != nullptr) {
            if (bucket->key == key) {
                auto castdata = reinterpret_cast<char *>(&value);
                auto data = defs::Data(bucket->gaddr.size, castdata, bucket->gaddr);
                node->write(data);
                return;
            }
            bucket = bucket->next;
        }
    }

/**
 * get count of contained elements
 *
 * used for containment check
 *
 * @param key
 * @return 0 if not contained, 1 if contained
 */
    uint64_t count(uint64_t key) {
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

        return size() == 0;
    }

/**
 * empty the HT
 */
    void clear() {

        for (auto &b: storage) {
            while (b != nullptr) {
                Elem *oldBucket = b;
                b = oldBucket->next;
                node->Free(oldBucket->gaddr, node->getID());
            }
        }
        amountElements = 0;
    }

};


#endif //MEDMM_HASHTABLE_H

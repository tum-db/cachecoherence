//
// Created by Magdalena Pröbstl on 2019-08-01.
//

#include "HashTable.h"

template<typename V>
void HashTable<V>::insert(uint32_t key, V value) {
    uint32_t b = hashBucket(key);
    auto gadd = node.Malloc(sizeof(V));
    storage[b] = new Elem({key, gadd, storage[b]});
    ++amountElements;
    auto data = new defs::Data(sizeof(V), reinterpret_cast<uint64_t >(value), gadd);
    node.write(data);
}

/**
   * get reference to existing HT element or insert new at key
   *
   * used for lookups, inserts, and editing the HT elements
   *
   * @param key
   * @return reference to HT element
   */
template<typename V>
V &HashTable<V>::operator[](uint32_t key) {
    uint32_t b = hashBucket(key);
    Elem *bucket = storage[b];
    while (bucket != nullptr) {
        if (bucket->key == key) {
            auto addr = bucket->gaddr;
            auto value = node.read(addr);
            return reinterpret_cast<V>(value);
        }
        bucket = bucket->next;
    }
    auto gadd = node.Malloc(sizeof(V));
    storage[b] = new Elem({key, gadd, storage[b]});
    ++amountElements;
    return reinterpret_cast<V>(node.read(gadd));
}

/**
    * remove element from HT
    *
    * used for remove
    *
    * @param key
    */
template<typename V>
void HashTable<V>::erase(uint32_t key) {
    uint32_t b = hashBucket(key);
    Elem *bucket = storage[b];
    Elem *oldbucket = nullptr;
    node.Free(bucket->gaddr);

}
//
///**
//   * empty the HT
//   */
//template<typename V>
//void HashTable<V>::clear() {
//    for (auto &b: storage) {
//
//        node.Free(b.gaddr);
//    }
//    amountElements = 0;
//}

/**
    * get element from HT wrapped in optional
    *
    * used for const lookup
    *
    * @param key
    * @return optional containing the element or nothing if not exists
    */
template<typename V>
std::optional<V> HashTable<V>::get(uint32_t key) const {
    uint32_t b = hashBucket(key);
    Elem *bucket = storage[b];
    while (bucket != nullptr) {
        if (bucket->key == key) {
            auto value = node.read(bucket->gaddr);
            return reinterpret_cast<V>(value);
        }
        bucket = bucket->next;
    }
    return std::nullopt;
}



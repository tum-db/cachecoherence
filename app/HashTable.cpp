//
// Created by Magdalena Pröbstl on 2019-08-01.
//

#include "HashTable.h"

template<typename V>
void HashTable<V>::insert(uint32_t key, V value) {
    if (!count(key)) {
        operator[](key) = value;
    }
}

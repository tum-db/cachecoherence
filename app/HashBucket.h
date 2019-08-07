//
// Created by Magdalena Pröbstl on 2019-08-05.
//

#ifndef MEDMM_HASHBUCKET_H
#define MEDMM_HASHBUCKET_H

#include <vector>
#include "../util/defs.h"


template<typename V>
class HashBucket {


    struct Element {
        uint32_t key;
        defs::GlobalAddress gaddr;
        defs::GlobalAddress next;
    };

    uint16_t id = 0;
    uint32_t lb = 0; //lowerbound
    uint32_t ub = 0; //upperbound

   // std::vector<Element *> items = nullptr;
    std::size_t amountElements = 0;

public:
    HashBucket(uint16_t id, uint32_t lb, uint32_t ub): id(id), lb(lb),ub(ub){
     //   items = nullptr;
        amountElements = 0;
    }

   HashBucket(){
        id = 0;
        lb = 0;
        ub = 0;
     //   items = nullptr;
        amountElements = 0;
    }


    uint64_t getLb() { return lb; }

    uint64_t getUb() { return ub; }

    uint64_t getId() { return id; }
};


#endif //MEDMM_HASHBUCKET_H

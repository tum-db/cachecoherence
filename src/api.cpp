//
// Created by Magdalena Pröbstl on 2019-04-30.
//

#include "Node.h"


/**
 * own malloc, allocates memory in the distributed system
 * @param size = size that should be allocated
 * @return GlobalAddress of allocated Memory
 */
defs::GlobalAddress Node::Malloc(size_t size, uint16_t srcID) {

    auto msize = sizeof(defs::SaveData) + size;
    void *alloced = malloc(msize);
    if (alloced && allocated < defs::MAX_TEST_MEMORY_SIZE) {
        auto *buffer = new(alloced) defs::SaveData();
        allocated = allocated + msize;
        auto gaddr = defs::GlobalAddress{size, buffer, id, false};
        return gaddr;
    } else {
        free(alloced);
     //   std::cout <<"remote"<< std::endl;

        if (srcID != id) {
            return defs::GlobalAddress{size, nullptr, 0, true};
        }
        auto port = defs::port;
        connectClientSocket(port);

        defs::SendGlobalAddr *sga = reinterpret_cast<defs::SendGlobalAddr *>(sendAddress(
                defs::GlobalAddress{size, nullptr, 0, false}.sendable(srcID),
                defs::IMMDATA::MALLOC));

        if (sga == nullptr) {
            throw std::runtime_error("No more free memory!");
        }
        closeClientSocket();
        return defs::GlobalAddress(*sga);

    }
}


defs::GlobalAddress Node::Free(defs::GlobalAddress gaddr, uint16_t srcID) {
    if (isLocal(gaddr)) {

        auto d = reinterpret_cast<defs::SaveData *>(gaddr.ptr);


        if (d->iscached > defs::CACHE_DIRECTORY_STATE::UNSHARED &&
            d->iscached <= defs::CACHE_DIRECTORY_STATE::SHARED &&
            !d->sharerNodes.empty() && (d->ownerNode != srcID)) {

            broadcastInvalidations(d->sharerNodes, gaddr);
        }
        allocated = allocated - (sizeof(defs::SaveData) + gaddr.size);
        free(gaddr.ptr);

        gaddr.ptr = nullptr;
        gaddr.size = 0;
        return gaddr;
    } else {
        auto port = defs::port;
        connectClientSocket(port);
        auto sga = reinterpret_cast<defs::SendGlobalAddr *>(sendAddress(gaddr.sendable(id),
                                                                        defs::IMMDATA::FREE));

        auto add = defs::GlobalAddress(*sga);

        closeClientSocket();
        return add;
    }
}

char *Node::read(defs::GlobalAddress gaddr) {
    // TODO: do this for everything else
    ScopedLock l(gaddr, *this, LOCK_STATES::SHAREDLOCK);
    char *result = performRead(gaddr, id);
    return result;
}

char *Node::performRead(defs::GlobalAddress gaddr, uint16_t srcID) {
    if (isLocal(gaddr)) {
        auto data = reinterpret_cast<defs::SaveData *>(gaddr.ptr);

        if (data->iscached < 0 || data->iscached > 2) {
         //   std::cout << data->iscached << std::endl;
            return nullptr;
        }
        if (srcID != id) {
            data->iscached = defs::CACHE_DIRECTORY_STATE::SHARED;
            data->sharerNodes.push_back(srcID);
        }
        return gaddr.ptr + sizeof(defs::SaveData);
    } else {
        //     std::cout << "not local" << std::endl;
        auto cacheItem = cache.getCacheItem(gaddr);
        if (cacheItem == nullptr || gaddr.size != cacheItem->globalAddress.size) {
       //     std::cout << "not cached" << std::endl;

            auto port = defs::port;
            connectClientSocket(port);
            char *res = sendAddress(gaddr.sendable(srcID), defs::IMMDATA::READ);
            cache.addCacheItem(gaddr, res, gaddr.size);

            closeClientSocket();
            return &cache.getCacheItem(gaddr)->data[0];
        } else {
         //   std::cout << "cached" << std::endl;
       //     std::cout << &cacheItem->data[0] << ", " << reinterpret_cast<void *>(cacheItem->globalAddress.ptr) << std::endl;
            return &cacheItem->data[0];
        }
    }
}


bool Node::setLock(uint64_t lockId, LOCK_STATES state, uint16_t nodeId) {

    if (id == defs::locknode) {
        auto existing = locks.find(lockId);
        if (existing != locks.end()) {
            if (state == LOCK_STATES::UNLOCKED && nodeId == existing->second.nodeId) {
                locks[lockId].state = state;
                return true;
            } else if (state == LOCK_STATES::EXCLUSIVE &&
                       existing->second.state == LOCK_STATES::UNLOCKED) {
                locks[lockId].state = state;
                return true;
            } else if (state == LOCK_STATES::SHAREDLOCK &&
                       existing->second.state != LOCK_STATES::EXCLUSIVE) {
                locks[lockId].state = state;
                return true;
            } else {
                return false;
            }
        } else {
            locks.emplace(lockId, Lock{lockId, state, nodeId});
            return true;
        }
    } else {
        connectClientSocket(defs::locknode);
        auto res = sendLock(Lock{lockId, state, nodeId}, defs::IMMDATA::LOCKS);
        closeClientSocket();
        return res;
    }
}

defs::GlobalAddress Node::write(defs::Data data) {
    defs::GlobalAddress result{};
    ScopedLock l(data.ga, *this, LOCK_STATES::EXCLUSIVE);
    if (isLocal(data.ga)) {
        auto d = reinterpret_cast<defs::SaveData *>(data.ga.ptr);

        if (d->iscached > defs::CACHE_DIRECTORY_STATE::UNSHARED &&
            d->iscached <= defs::CACHE_DIRECTORY_STATE::SHARED &&
            !d->sharerNodes.empty()) {
       //     std::cout << "invalidating??" << std::endl;
            broadcastInvalidations(d->sharerNodes, data.ga);
        }
        d->sharerNodes = {};
        d->ownerNode = id;
        d->iscached = defs::CACHE_DIRECTORY_STATE::UNSHARED;
        memcpy(data.ga.ptr + sizeof(defs::SaveData), data.data, data.size);
        data.ga.size = data.size;
        result = data.ga;

    } else {
        result = performWrite(data, id);
    }
    return result;
}


defs::GlobalAddress Node::performWrite(defs::Data data, uint16_t srcID) {
    if (isLocal(data.ga)) {
        new(data.ga.ptr)defs::SaveData{defs::CACHE_DIRECTORY_STATE::EXCLUS,
                                       srcID, {}};
        memcpy(data.ga.ptr + sizeof(defs::SaveData), data.data, data.size);
        data.ga.size = data.size;

        return data.ga;
    } else {
        auto port = defs::port;
        connectClientSocket(port);
        cache.alterCacheItem(data.data, data.ga, data.size);

        defs::SendGlobalAddr sga = sendData(data.sendable(srcID), defs::IMMDATA::WRITE);

        closeClientSocket();
        return defs::GlobalAddress(sga);
    }
}


defs::GlobalAddress
Node::FprintF(char *data, defs::GlobalAddress gaddr, size_t size, size_t offset) {
    if (!gaddr.isFile) {
        return gaddr;
    }
 //   ScopedLock l(gaddr, *this, LOCK_STATES::EXCLUSIVE);
    if (isLocal(gaddr)) {

        auto f = MaFile(gaddr.ptr, MaFile::Mode::WRITE);
        auto newsize = gaddr.size > (size + offset) ? gaddr.size : (size + offset);
        if (f.size() != gaddr.size || f.size() < newsize) {
            f.resize(newsize);
            gaddr.resize(newsize);
        }
        /*    std::cout << std::strlen(data) << ", " << data << std::endl;
            std::cout << "local, size: " << size << ", gaddr-size: " << gaddr.size
                      << ", filesize: "
                      << f.size() << std::endl; */
        f.write_block(data, offset, size);

        return gaddr;
    } else {
        auto port = defs::port;
        connectClientSocket(port);

        auto senddata = defs::ReadFileData{false, gaddr.sendable(id), offset, size};
        auto ga = sendWriteFile(senddata, defs::IMMDATA::WRITEFILE, data);

        closeClientSocket();

        return ga;
    }


}

void Node::FreadF(defs::GlobalAddress gaddr, size_t size, size_t offset, char *res) {
    if (!gaddr.isFile) {
        return;
    }
  //  ScopedLock l(gaddr, *this, LOCK_STATES::SHAREDLOCK);
    if (isLocal(gaddr)) {
        auto f = MaFile(gaddr.ptr, MaFile::Mode::READ);
        f.read_block(offset, size, res);
    } else {
        auto port = defs::port;
        connectClientSocket(port);
        auto sendData = defs::ReadFileData{true, gaddr.sendable(id), offset, size};
        sendReadFile(sendData, defs::IMMDATA::READFILE, res);

        closeClientSocket();
    }
}
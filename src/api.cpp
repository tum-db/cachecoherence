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

    auto msize = size + sizeof(defs::SaveData);
    auto buffer = malloc(msize);
    if (buffer) {
        //allocated = allocated + msize;
        memset(buffer,0,size);
        auto gaddr = defs::GlobalAddress{size, buffer, id, false};
        return gaddr;
    } else {
        if (srcID != id) {
            return defs::GlobalAddress{size, nullptr, 0, true};
        }
        auto port = defs::port;
        auto c = connectClientSocket(port);

        defs::SendGlobalAddr *sga = nullptr;
        sendAddress(defs::GlobalAddress{size, nullptr, 0, false}.sendable(srcID),
                               defs::IMMDATA::MALLOC,
                               c, sga);


        if (sga == nullptr) {
            throw std::runtime_error("No more free memory!");

            //TODO throw
            /* auto filename = getNextFileName();
             auto nf = MaFile(filename, MaFile::Mode::WRITE);
             if (nf.enough_space(0, size)) {
                 c.socket.close();
                 c.rcqp->setToResetState();
                 return defs::GlobalAddress(sga->size, filename, id, true);
             } else {
                 auto result = sendAddress(
                         defs::GlobalAddress{size, nullptr, 0, true}.sendable(srcID),
                         defs::IMMDATA::MALLOCFILE,
                         c);

                 sga = reinterpret_cast<defs::SendGlobalAddr *>(result);
                 if (sga->size != size) {
                     throw std::runtime_error("No more free memory!");

                 }
             }*/

        }
        c.socket.close();
        c.rcqp->setToResetState();

        return defs::GlobalAddress(*sga);

    }
}


defs::GlobalAddress Node::Free(defs::GlobalAddress gaddr) {
    if (isLocal(gaddr)) {

        auto d = reinterpret_cast<defs::SaveData *>(gaddr.ptr);


        if (d->iscached > defs::CACHE_DIRECTORY_STATE::UNSHARED &&
            d->iscached <= defs::CACHE_DIRECTORY_STATE::SHARED &&
            !d->sharerNodes.empty()) {
            std::cout << d->data << ", " << d->iscached << ", " << d->ownerNode << ", "
                      << d->sharerNodes[0] << std::endl;

            broadcastInvalidations(d->sharerNodes, gaddr);
        }
        //  allocated = allocated - (sizeof(defs::SaveData) + gaddr.size);
        free(gaddr.ptr);

        gaddr.ptr = nullptr;
        gaddr.size = 0;
        return gaddr;
    } else {
        auto port = defs::port;
        auto c = connectClientSocket(port);
        defs::SendGlobalAddr *sga = nullptr;

        sendAddress(gaddr.sendable(id), defs::IMMDATA::FREE, c, sga);

        auto add = defs::GlobalAddress(*sga);

        closeClientSocket(c);
        return add;
    }
}

uint64_t Node::read(defs::GlobalAddress gaddr) {

    auto locked = setLock(generateLockId(gaddr.sendable(0)), LOCK_STATES::EXCLUSIVE);
    if (locked) {
        auto result = performRead(gaddr, id);
        setLock(generateLockId(gaddr.sendable(0)), LOCK_STATES::UNLOCKED);
        return result->data;
    } else {
        return 0;
    }
}

defs::SaveData *Node::performRead(defs::GlobalAddress gaddr, uint16_t srcID) {
    if (isLocal(gaddr)) {
        auto data = reinterpret_cast<defs::SaveData *>(gaddr.ptr);

        if (data->iscached < 0 || data->iscached > 2) {
            std::cout << data->iscached << std::endl;
            return nullptr;
        }
        return data;
    } else {
        std::cout << "not local" << std::endl;
        auto cacheItem = cache.getCacheItem(gaddr);
        if (cacheItem == nullptr) {
            std::cout << "not cached" << std::endl;


            auto port = defs::port;
            auto c = connectClientSocket(port);
            void *data;
            sendAddress(gaddr.sendable(srcID), defs::IMMDATA::READ, c, data);

            auto result = reinterpret_cast<defs::SaveData *>(data);
            auto newCacheItem = CacheItem{gaddr, result->data, std::chrono::system_clock::now(),
                                          std::chrono::system_clock::now()};
            cache.addCacheItem(gaddr, newCacheItem);

            closeClientSocket(c);

            return result;
        } else {
            std::cout << "cached" << std::endl;
            std::cout << cacheItem->data << ", " << cacheItem->globalAddress.ptr << std::endl;
            return new defs::SaveData{cacheItem->data, defs::CACHE_DIRECTORY_STATE::SHARED, 0,
                                      std::vector<uint16_t>()};
        }
    }
}


bool Node::setLock(uint64_t lockId, LOCK_STATES state) {
    if (id == defs::locknode) {
        auto existing = locks.find(lockId);

        if (existing != locks.end()) {

            if (state == LOCK_STATES::UNLOCKED) {
                existing->second = state;
                return true;
            } else if (state == LOCK_STATES::EXCLUSIVE &&
                       existing->second == LOCK_STATES::UNLOCKED) {
                existing->second = state;
                return true;
            } else if (state == LOCK_STATES::SHAREDLOCK &&
                       existing->second != LOCK_STATES::EXCLUSIVE) {
                existing->second = state;
                return true;
            } else {
                return false;
            }
        } else {
            locks.insert(std::pair<uint16_t, LOCK_STATES>(lockId, state));
            return true;
        }
    } else {
        auto c = connectClientSocket(defs::locknode);
        auto res = false;
        sendLock(Lock{lockId, state}, defs::IMMDATA::LOCKS, c, &res);
        closeClientSocket(c);
        return res;
    }
}

defs::GlobalAddress Node::write(defs::Data data) {
    auto result = defs::GlobalAddress{};
    auto locked = setLock(generateLockId(data.ga.sendable(0)), LOCK_STATES::EXCLUSIVE);
    if (locked) {

        if (isLocal(data.ga)) {
            auto d = reinterpret_cast<defs::SaveData *>(data.ga.ptr);

            if (d->iscached >= 0 && !d->sharerNodes.empty() && d->iscached < 3) {
                broadcastInvalidations(d->sharerNodes, data.ga);
            }


            new(data.ga.ptr) defs::SaveData{data.data, defs::CACHE_DIRECTORY_STATE::UNSHARED,
                                             id,
                                             {}};
            result = data.ga;

        } else {
            result = performWrite(data, id);
        }
        setLock(generateLockId(data.ga.sendable(0)), LOCK_STATES::UNLOCKED);
    }
    return result;

}


defs::GlobalAddress Node::performWrite(defs::Data data, uint16_t srcID) {
    if (isLocal(data.ga)) {
        new  (data.ga.ptr)defs::SaveData{data.data, defs::CACHE_DIRECTORY_STATE::EXCLUS,
                                          srcID, {}};
        return data.ga;
    } else {
        auto port = defs::port;
        auto c = connectClientSocket(port);
        auto ci = CacheItem{data.ga, data.data, std::chrono::system_clock::now(),
                            std::chrono::system_clock::now()};
        cache.alterCacheItem(ci, data.ga);
        std::cout << ci.data << ", " << ci.globalAddress.ptr << std::endl;

        defs::SendGlobalAddr *sga = nullptr;
        sendData(data->sendable(srcID), defs::IMMDATA::WRITE, c, sga);

        std::cout << c.socket.get() << ", " << c.rcqp->getQPN() << std::endl;

        closeClientSocket(c);
        return defs::GlobalAddress(*sga);
    }
}


defs::GlobalAddress
Node::FprintF(char *data, defs::GlobalAddress gaddr, size_t size, size_t offset) {
    if (!gaddr.isFile) {
        return gaddr;
    }

    if (isLocal(gaddr)) {
        auto locked = setLock(generateLockId(gaddr.sendable(0)), LOCK_STATES::EXCLUSIVE);
        if (locked) {
            auto f = MaFile(reinterpret_cast<char *>(gaddr.ptr), MaFile::Mode::WRITE);
            auto newsize = gaddr.size > (size + offset) ? gaddr.size : (size + offset);
            if (f.size() != gaddr.size || f.size() < newsize) {
                f.resize(newsize);
                gaddr.resize(newsize);
            }
            std::cout << std::strlen(data) << std::endl;
            std::cout << "local, size: " << size << ", gaddr-size: " << gaddr.size
                      << ", filesize: "
                      << f.size() << std::endl;
            f.write_block(data, offset, size);
            setLock(generateLockId(gaddr.sendable(0)), LOCK_STATES::UNLOCKED);
        }
        return gaddr;
    } else {
        auto port = defs::port;
        auto c = connectClientSocket(port);
        auto castdata = reinterpret_cast<uint64_t *>(data);
        auto senddata = defs::ReadFileData{gaddr.sendable(id), offset, size};
        defs::SendGlobalAddr *sga = nullptr;
        sendWriteFile(senddata, defs::IMMDATA::WRITEFILE, c, castdata, sga);
        closeClientSocket(c);
        return defs::GlobalAddress(*sga);
    }


}

char *Node::FreadF(defs::GlobalAddress gaddr, size_t size, size_t offset) {
    if (!gaddr.isFile) {
        return nullptr;
    }
    std::vector<char> block;
    block.resize(size);

    char *result = &block[0];
    if (isLocal(gaddr)) {

        auto f = MaFile(reinterpret_cast<char *>(gaddr.ptr), MaFile::Mode::READ);
        f.read_block(offset, size, result);


    } else {
        auto port = defs::port;
        auto c = connectClientSocket(port);
        auto sendData = defs::ReadFileData{gaddr.sendable(id), offset, size};
        sendReadFile(sendData, defs::IMMDATA::READFILE, c, result);

        closeClientSocket(c);
    }
    return result;

}
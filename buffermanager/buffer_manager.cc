#include "buffer_manager.h"

#include <cassert>
#include <cstring>
#include <memory>
#include <mutex>
#include <string>
#include <tuple>
#include <utility>
#include <iostream>
#include <malloc.h>


/*
This implementation of a buffer manager uses a single latch ("directory latch")
to protect the page table and the queues. This means that on every call to
fix_page() and unfix_page() this latch must be locked. To hold the latch as
short as possible, it is unlocked while doing I/O (i.e. calls to
File.read/write_block()). This means that other threads will modify the page
table and the queues while one thread is doing I/O. So the state of the buffer
manager after the File function returns may not be the same as before the call.

To keep the buffer manager consistent while still being able to do I/O without
holding the directory latch, each BufferFrame object is in one of five states
that determine how the BufferFrame is allowed to be used. The five states are
as follows:

NEW: The page is being created. It is not checked yet, if there is enough
    space for the space or if it is possible to evict another page to be
    replaced by this one. The page also not inserted into the queues, yet.

LOADING: Space for the page was allocated and it is inserted into a queue. Data
    for the page is still being read from disk.

LOADED: Page lies in memory and can be used.

EVICTING: Page is currently being evicted. When it is dirty, it is being
    written to disk.

RELOADED: While in the EVICTING state this page was requested in another call
    to fix_page() so the eviction is cancelled. While the I/O to write the page
    to disk is still ongoing, its state will remain RELOADED. After it is
    written, the state changes back to LOADED.

Only pages that are in the LOADED or RELOADED states are allowed to be returned
from fix_page(). When a page is created it is in NEW mode. The creating thread
immediately locks the page exclusively before doing I/O which potentially
releases the directory latch. When another page could be evicted to make space
for the new one, the state is changed to LOADING. Only when the page was read
completely the state is changed to LOADED and the page is unlocked.

When another thread finds a page in NEW mode it also tries to lock the page
exclusively. When it eventually acquires the lock, the page will either be in
the LOADED state in which case it can be returned or it will still be in the
NEW state. This means that the thread that created the page could not evict
another page. So the num_users counter is decreased and the page is removed
when the counter reaches 0. Then, fix_page() will start from the beginning.

When a thread finds a page in the LOADING state, it simply treats it as if it
were in the LOADED state, i.e. it tries to lock the page according to the
exclusive parameter. As a page is locked exclusively by the thread that is
doing the I/O to read the page from disk, a page will never be returned by
fix_page() while it is still being loaded.

When a thread finds a page in the EVICTING state, it sets the state to RELOADED
and the returns the page. This prevents the thread that is eviciting the page
from reusing the page's data for a new page.

When a page is in the RELOADED state it can be used as if it were in the LOADED
state, so it is simply returned.

When fix_page() is called with a page id that is currently not loaded into
memory, another page must be evicted. For this the queues are scanned for a
page that is unused (i.e. page.num_users == 0) and is in the LOADED state. When
it is not dirty, no I/O must be done to write the page to disk, so the page is
removed directly without releasing the directory latch. When it is dirty, its
state is set to EVICTING. Then, its data is copied to a temporary buffer that
can then be written to disk. This is done so that if the eviction is stopped by
another thread that sets the state to RELOADED, the write to disk can still be
completed without interference from other threads that can potentially modify
the data. While the temporary copy is written to disk, the directory latch is
unlocked. This means that in the meantime another thread can have changed the
state of the page to RELOADED. When the I/O operation is done, the state of the
page is checked. When it is still in the EVICITING state, the eviction was
successful and it can be removed and its data reused for a new page. When it is
in RELOADED mode, the eviction was unsuccessful so another page to evict must
be found.
*/

namespace moderndbs {

    char *BufferFrame::get_data() {
        return &data[0];
    }


    BufferManager::BufferManager(size_t page_size, size_t page_count, Node *n,
                                 const HashTable<BufferFrame>& pages)
            : page_size(page_size), page_count(page_count),
              loaded_pages{std::make_unique<char[]>(page_count * page_size)}, pages(pages) {
        node = n;
    }


    BufferManager::~BufferManager() {
        std::unique_lock latch{directory_latch};
        for (auto entry : pages) {
            write_out_page(*entry, latch);
        }

    }


    BufferFrame BufferManager::fix_page(uint64_t page_id, bool exclusive) {
        std::unique_lock latch{directory_latch};
        while (true) {
            if (pages.get(page_id).has_value() == 1) {
                // Page is already loaded.
                auto page = pages[page_id];
                ++page.num_users;
                if (page.state == BufferFrame::EVICTING) {
                    // Page is being evicted but we want to use it, so "revive" it
                    // by setting the state to RELOADED.
                    page.state = BufferFrame::RELOADED;
                } else if (page.state == BufferFrame::NEW) {
                    // Another thread is trying to evict another page for this.
                    // Wait for the other thread to finish by locking the page
                    // exclusively.
                    latch.unlock();
                    //               assert(node->setLock(generateLockId(page.gaddr.sendable(0)), LOCK_STATES::EXCLUSIVE));
                    // page.lock(true);
                    //         node->setLock(generateLockId(page.gaddr.sendable(0)), LOCK_STATES::UNLOCKED);
                    //  page.unlock();
                    latch.lock();
                    if (page.state == BufferFrame::NEW) {
                        // Other thread failed to evict another page.
                        --page.num_users;
                        if (page.num_users == 0) {
                            // Remove the failed page.
                            assert(page.fifo_position == fifo.end() &&
                                   page.lru_position == lru.end());
                            pages.erase(page_id);
                        }
                        continue;
                    }
                }
                if (page.lru_position != lru.end()) {
                    // Page is in LRU list, move it to the end.
                    lru.erase(page.lru_position);
                    page.lru_position = lru.insert(lru.end(), page_id);
                } else {
                    assert(page.fifo_position != fifo.end());
                    // Page was in FIFO list and is fixed again, so move it to LRU.
                    fifo.erase(page.fifo_position);
                    page.fifo_position = fifo.end();
                    page.lru_position = lru.insert(lru.end(), page_id);
                }
                latch.unlock();
                //    node->setLock(generateLockId(page.gaddr.sendable(0)), LOCK_STATES::EXCLUSIVE);
                return page;
            } else {
                break;
            }
        }
        // Create a new page and don't insert it in the queues, yet.
        assert(pages.get(page_id).has_value() == 0);
        auto newpage = BufferFrame(page_id, nullptr, 0, fifo.end(), lru.end());

        pages.insert(page_id, newpage, sizeof(BufferFrame) + page_size);
        auto page = pages[page_id];
        ++page.num_users;

        //  page.lock(true);
        char *data;
        if (pages.size() - 1 < page_count) {
            // We still have unused pages in loaded_pages so use that.
            data = &loaded_pages[(pages.size() - 1) * page_size];
        } else {
            data = evict_page(latch);
            if (data == nullptr) {
                // No page could be evicted, so throw a buffer_full_error
                --page.num_users;
                //         node->setLock(generateLockId(page.gaddr.sendable(0)), LOCK_STATES::UNLOCKED);
                //   page.unlock();
                if (page.num_users == 0) {
                    assert(page.fifo_position == fifo.end() && page.lru_position == lru.end());
                    pages.erase(page_id);
                }
                throw buffer_full_error{};
            }
        }
        page.state = BufferFrame::LOADING;
        memcpy(page.get_data(), data, defs::MAX_BLOCK_SIZE);
        page.fifo_position = fifo.insert(fifo.end(), page_id);

        load_page(page, latch);
        //page.unlock();
        // node->setLock(generateLockId(page->gaddr.sendable(0)), LOCK_STATES::UNLOCKED);

        latch.unlock();
        //    node->setLock(generateLockId(page->gaddr.sendable(0)), LOCK_STATES::EXCLUSIVE);

        //   page.lock(exclusive);

        pages.update(page_id, page);
        return page;
    }


    void BufferManager::unfix_page(BufferFrame page, bool is_dirty) {
        //  page.unlock();
        //      node->setLock(generateLockId(page.gaddr.sendable(0)), LOCK_STATES::UNLOCKED);

        // Unlock the page latch before acquiring the directory latch to avoid
        // deadlocks.
        std::unique_lock latch{directory_latch};
        if (is_dirty) {
            page.is_dirty = true;
        }
        --page.num_users;

        //   std::cout << "pagedata: " << page.data << std::endl;

        pages.update(page.page_id, page);

    }


    std::vector<uint64_t> BufferManager::get_fifo_list() const {
        std::vector<uint64_t> fifo_list;
        fifo_list.reserve(fifo.size());
        for (auto page : fifo) {
            fifo_list.push_back(page);
        }
        return fifo_list;
    }


    std::vector<uint64_t> BufferManager::get_lru_list() const {
        std::vector<uint64_t> lru_list;
        lru_list.reserve(lru.size());
        for (auto page : lru) {
            lru_list.push_back(page);
        }
        return lru_list;
    }


    void BufferManager::load_page(BufferFrame &page, std::unique_lock<std::mutex> &latch) {
        assert(page.state == BufferFrame::LOADING);
        auto segment_id = get_segment_id(page.page_id);

        auto segment_page_id = get_segment_page_id(page.page_id);
        //  std::cout << "segmentid: "<< segment_page_id << std::endl;
        defs::GlobalAddress segment_file{};

        if (auto it = segment_files.find(segment_id); it != segment_files.end()) {
            // File was opened already.
            segment_file = it->second;
        } else {
            // new to avoid segfault
            auto getfilename = new std::string(std::to_string(segment_id));
            // Open file in WRITE mode because we may have to write dirty pages to
            // it.
            auto filename = const_cast<char *>(getfilename->c_str());
            auto f = MaFile::open_file(filename, MaFile::Mode::WRITE);
            auto file_size = f->size();
            segment_file = segment_files.emplace(
                    std::piecewise_construct, std::forward_as_tuple(segment_id),
                    std::forward_as_tuple(file_size, filename, node->getID(), true)).first->second;
        }
        {
            node->setLock(generateLockId(segment_file.sendable(0)), LOCK_STATES::EXCLUSIVE,
                          node->getID());
            if (segment_file.size < (segment_page_id + 1) * page_size) {
                // When the file is too small, resize it and zero out the data for it.
                // As the bytes in the file are zeroed anyway, we don't have to read
                // the zeroes from disk.
                segment_files[segment_id].size = (segment_page_id + 1) * page_size;

            } else {
                node->setLock(generateLockId(segment_file.sendable(0)),
                              LOCK_STATES::UNLOCKED, node->getID());
                latch.unlock();
                node->FreadF(segment_file, page_size,
                             segment_page_id * page_size, page.get_data());
                //  file.read_block(segment_page_id * page_size, page_size, page.data);
                latch.lock();
            }
        }

        page.state = BufferFrame::LOADED;
        page.is_dirty = false;

    }


    void BufferManager::write_out_page(BufferFrame &page, std::unique_lock<std::mutex> &latch) {
        auto segment_id = get_segment_id(page.page_id);
        auto segment_page_id = get_segment_page_id(page.page_id);
        auto gaddr =segment_files[segment_id];
        latch.unlock();
        node->FprintF(page.get_data(), gaddr, page_size, segment_page_id * page_size);
        //  file.write_block(page.data, segment_page_id * page_size, page_size);
        latch.lock();
        page.is_dirty = false;
    }


    BufferFrame BufferManager::find_page_to_evict() {
        // Try FIFO list first
        for (auto page_id : fifo) {
            auto page = pages[page_id];
            if (page.num_users == 0 && page.state == BufferFrame::LOADED) {
                return page;
            }
        }
        // If FIFO list is empty or all pages in it are in use, try LRU
        for (auto page_id : lru) {
            auto page = pages[page_id];

            if (page.num_users == 0 && page.state == BufferFrame::LOADED) {
                return page;
            }
        }
        return BufferFrame(0, nullptr, 0, fifo.end(), lru.end());
    }


    char *BufferManager::evict_page(std::unique_lock<std::mutex> &latch) {
        BufferFrame page_to_evict;
        while (true) {
            // Need to evict another page. If no page can be evicted,
            // find_page_to_evict() returns nullptr.
            page_to_evict = find_page_to_evict();
            if (page_to_evict.get_data() == nullptr) {
                return nullptr;
            }
            assert(page_to_evict.state == BufferFrame::LOADED);
            page_to_evict.state = BufferFrame::EVICTING;
            if (!page_to_evict.is_dirty) {
                break;
            }
            // Create a copy of the page that is then written to the file so that
            // other threads can continue using it while it is being written.
            {
                auto page_data = std::make_unique<char[]>(page_size);
                std::memcpy(page_data.get(), page_to_evict.get_data(), page_size);
                BufferFrame page_copy{page_to_evict.page_id, page_data.get(), page_size, fifo.end(),
                                      lru.end()};
                write_out_page(page_copy, latch);
            }
            assert(
                    page_to_evict.state == BufferFrame::EVICTING ||
                    page_to_evict.state == BufferFrame::RELOADED
            );
            if (page_to_evict.state == BufferFrame::EVICTING) {
                // Nobody claimed the page while we were evicting it.
                // Otherwise we have to retry.
                break;
            }
            page_to_evict.state = BufferFrame::LOADED;
        }
        if (page_to_evict.lru_position != lru.end()) {
            lru.erase(page_to_evict.lru_position);
        } else {
            assert(page_to_evict.fifo_position != fifo.end());
            fifo.erase(page_to_evict.fifo_position);
        }
        char *data = page_to_evict.get_data();
        pages.erase(page_to_evict.page_id);
        return data;
    }

    void BufferManager::insert_data(BufferFrame &page, char *newdata, size_t size) {
        memcpy(&page.data[0], newdata, size);
    //    std::cout << "pagedata: " << page.get_data() << ", newdata: " << newdata  << std::endl;
        pages.update(page.page_id, page);
    }

}  // namespace moderndbs

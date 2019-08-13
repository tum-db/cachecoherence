#ifndef INCLUDE_MODERNDBS_BUFFER_MANAGER_H
#define INCLUDE_MODERNDBS_BUFFER_MANAGER_H

#include <cstddef>
#include <cstdint>
#include <exception>
#include <list>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <unordered_map>
#include <vector>
#include "moderndbs/file.h"


namespace moderndbs {

class BufferFrame {
private:
    friend class BufferManager;

    using list_position = std::list<BufferFrame*>::iterator;

    enum State { NEW, LOADING, LOADED, EVICTING, RELOADED };

    uint64_t page_id;
    char* data;
    State state = NEW;

    /// How many times this page has been fixed.
    size_t num_users = 0;

    bool is_dirty = false;

    /// Position of this page in the FIFO list.
    list_position fifo_position;

    /// Position of this page in the LRU list.
    list_position lru_position;

    std::shared_mutex latch;

    bool locked_exclusively = false;

    void lock(bool exclusive);

    void unlock();

public:
    BufferFrame(
        uint64_t page_id, char* data, list_position fifo_position, list_position lru_position
    ) : page_id(page_id), data(data), fifo_position(fifo_position), lru_position(lru_position) {}

    /// Returns a pointer to this page's data.
    char* get_data();
};


class buffer_full_error
: public std::exception {
public:
    const char* what() const noexcept override {
        return "buffer is full";
    }
};


class BufferManager {
private:
    struct SegmentFile {
        /// This latch ensures that a call to size() followed by resize() is
        /// executed atomically.
        std::mutex file_latch;
        std::unique_ptr<File> file;

        explicit SegmentFile(std::unique_ptr<File> file) : file{std::move(file)} {}
    };

    size_t page_size;
    size_t page_count;

    /// Latch that protects all of the following member variables.
    std::mutex directory_latch;

    /// Memory storage for all loaded pages.
    std::unique_ptr<char[]> loaded_pages;

    /// Maps segment ids to their files.
    std::unordered_map<uint16_t, SegmentFile> segment_files;

    /// Maps page_ids to BufferFrame objects of all pages that are currently
    /// in memory.
    std::unordered_map<uint64_t, BufferFrame> pages;

    /// FIFO list of pages.
    std::list<BufferFrame*> fifo;

    /// LRU list of pages.
    std::list<BufferFrame*> lru;

    /// Loads the page from disk. `latch` must be the locked directory latch.
    /// Unlocks `latch` while doing I/O.
    void load_page(BufferFrame& page, std::unique_lock<std::mutex>& latch);

    /// Writes the page to disk if it is dirty. `latch` must be the locked
    /// directory latch. Unlocks `latch` while doing I/O.
    void write_out_page(BufferFrame& page, std::unique_lock<std::mutex>& latch);

    /// Returns the next page that can be evicted. Caller must hold
    /// directory_latch. When no page can be evicted, returns nullptr.
    BufferFrame* find_page_to_evict();

    /// Evicts a page from the buffer manager. `latch` must be the locked
    /// directory latch. Returns the data pointer of the evicted page or
    /// nullptr when no page can be evicted. May temporarily unlock and lock
    /// `latch`.
    char* evict_page(std::unique_lock<std::mutex>& latch);

public:
    /// Constructor.
    /// @param[in] page_size  Size in bytes that all pages will have.
    /// @param[in] page_count Maximum number of pages that should reside in
    //                        memory at the same time.
    BufferManager(size_t page_size, size_t page_count);

    /// Destructor. Writes all dirty pages to disk.
    ~BufferManager();

    /// Returns size of a page
    size_t get_page_size() { return page_size; }

    /// Returns a reference to a `BufferFrame` object for a given page id. When
    /// the page is not loaded into memory, it is read from disk. Otherwise the
    /// loaded page is used.
    /// When the page cannot be loaded because the buffer is full, throws the
    /// exception `buffer_full_error`.
    /// Is thread-safe w.r.t. other concurrent calls to `fix_page()` and
    /// `unfix_page()`.
    /// @param[in] page_id   Page id of the page that should be loaded.
    /// @param[in] exclusive If `exclusive` is true, the page is locked
    ///                      exclusively. Otherwise it is locked
    ///                      non-exclusively (shared).
    BufferFrame& fix_page(uint64_t page_id, bool exclusive);

    /// Takes a `BufferFrame` reference that was returned by an earlier call to
    /// `fix_page()` and unfixes it. When `is_dirty` is / true, the page is
    /// written back to disk eventually.
    void unfix_page(BufferFrame& page, bool is_dirty);

    /// Returns the page ids of all pages (fixed and unfixed) that are in the
    /// FIFO list in FIFO order.
    /// Is not thread-safe.
    std::vector<uint64_t> get_fifo_list() const;

    /// Returns the page ids of all pages (fixed and unfixed) that are in the
    /// LRU list in LRU order.
    /// Is not thread-safe.
    std::vector<uint64_t> get_lru_list() const;

    /// Returns the segment id for a given page id which is contained in the 16
    /// most significant bits of the page id.
    static constexpr uint16_t get_segment_id(uint64_t page_id) {
        return page_id >> 48;
    }

    /// Returns the page id within its segment for a given page id. This
    /// corresponds to the 48 least significant bits of the page id.
    static constexpr uint64_t get_segment_page_id(uint64_t page_id) {
        return page_id & ((1ull << 48) - 1);
    }
};


}  // namespace moderndbs

#endif

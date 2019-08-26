//
// Created by Magdalena Pröbstl on 2019-08-16.
//

#include "MaFile.h"

namespace moderndbs {
    namespace {
        [[noreturn]] static void throw_errno() {
            throw std::system_error{errno, std::system_category()};
        }
    }
}

MaFile::MaFile(Mode mode, int fd, size_t size) : mode(mode), fd(fd), cached_size(size) {}

MaFile::MaFile(const char *filename, Mode mode) : mode(mode) {
    switch (mode) {
        case READ:
            fd = ::open(filename, O_RDONLY | O_SYNC);
            break;
        case WRITE:
            fd = ::open(filename, O_RDWR | O_CREAT | O_SYNC, 0666);
    }
    if (fd < 0) {
        moderndbs::throw_errno();
    }
    cached_size = read_size();
}

moderndbs::File::Mode MaFile::get_mode() const {
    return mode;
}

size_t MaFile::size() const {
    return cached_size;
}

void MaFile::resize(size_t newsize) {
    if (newsize == cached_size) {
        return;
    }
    if (::ftruncate(fd, newsize) < 0) {
        moderndbs::throw_errno();
    }
    cached_size = newsize;
}

void MaFile::read_block(size_t offset, size_t size, char *block) {
    size_t total_bytes_read = 0;
    while (total_bytes_read < size) {
        ssize_t bytes_read = ::pread(
                fd,
                block + total_bytes_read,
                size - total_bytes_read,
                offset + total_bytes_read
        );
        if (bytes_read == 0) {
            // end of file, i.e. size was probably larger than the file
            // size
            return;
        }
        if (bytes_read < 0) {
            moderndbs::throw_errno();
        }
        total_bytes_read += static_cast<size_t>(bytes_read);
    }
}

void MaFile::write_block(const char *block, size_t offset, size_t size) {
    size_t total_bytes_written = 0;
    while (total_bytes_written < size) {
        ssize_t bytes_written = ::pwrite(
                fd,
                block + total_bytes_written,
                size - total_bytes_written,
                offset + total_bytes_written
        );
        if (bytes_written == 0) {
            // This should probably never happen. Return here to prevent
            // an infinite loop.
            return;
        }
        if (bytes_written < 0) {
            moderndbs::throw_errno();
        }
        total_bytes_written += static_cast<size_t>(bytes_written);
    }
}

std::unique_ptr<moderndbs::File> MaFile::make_temporary_file() {
    char file_template[] = ".tmpfile-XXXXXX";
    int fd = ::mkstemp(file_template);
    if (fd < 0) {
        moderndbs::throw_errno();
    }
    if (::unlink(file_template) < 0) {
        ::close(fd);
        moderndbs::throw_errno();
    }
    return std::make_unique<MaFile>(File::WRITE, fd, 0);
}

std::unique_ptr<moderndbs::File>
MaFile::open_file(const char *filename, moderndbs::File::Mode mode) {
    return std::make_unique<MaFile>(filename, mode);
}

size_t MaFile::read_size() {
    struct ::stat file_stat;
    if (::fstat(fd, &file_stat) < 0) {
        moderndbs::throw_errno();
    }
    return file_stat.st_size;
}

void MaFile::close() {
    ::close(fd);
}

bool MaFile::enough_space(size_t offset, size_t size) {
    auto res = posix_fallocate(fd,offset,size);
    return res == 0;
}

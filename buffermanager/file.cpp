//
// Created by Magdalena Pröbstl on 2019-08-14.
//
#include <memory>
#include <zconf.h>
#include "file.h"
#include "posix_file.h"


namespace moderndbs {
std::unique_ptr <File> File::open_file(const char *filename, Mode mode) {
    return std::make_unique<PosixFile>(filename, mode);
}


std::unique_ptr <File> File::make_temporary_file() {
    char file_template[] = ".tmpfile-XXXXXX";
    int fd = ::mkstemp(file_template);
    if (fd < 0) {
        throw_errno();
    }
    if (::unlink(file_template) < 0) {
        ::close(fd);
        throw_errno();
    }
    return std::make_unique<PosixFile>(File::WRITE, fd, 0);
}

}
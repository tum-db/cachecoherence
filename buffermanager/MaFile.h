//
// Created by Magdalena Pröbstl on 2019-08-16.
//

#ifndef MEDMM_MAFILE_H
#define MEDMM_MAFILE_H


#include "file.h"
#include <fcntl.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <cerrno>
#include <memory>
#include <system_error>
#include <iostream>


class MaFile : public moderndbs::File {
private:
    Mode mode;
    int fd;
    size_t cached_size;

    size_t read_size();


public:
    MaFile(Mode mode, int fd, size_t size);

    MaFile(const char *filename, Mode mode);

    ~MaFile() override {
        // Don't check return value here, as we don't want a throwing
        // destructor. Also, even when close() fails, the fd will always be
        // freed (see man 2 close).
        ::close(fd);
    }


    Mode get_mode() const;

    size_t size() const;

    void resize(size_t newsize) override;

    void read_block(size_t offset, size_t size, char *block) override;

    void write_block(const char *block, size_t offset, size_t size) override;

    static std::unique_ptr<File> open_file(const char *filename, Mode mode);

    static std::unique_ptr<File> make_temporary_file();

    void close();

    bool enough_space(size_t offset, size_t size);
};


#endif //MEDMM_MAFILE_H

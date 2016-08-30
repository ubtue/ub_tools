/** \file    File.cc
 *  \brief   Implementation of class File.
 *  \author  Dr. Johannes Ruscheinski
 */

/*
 *  Copyright 2005-2008 Project iVia.
 *  Copyright 2005-2008 The Regents of The University of California.
    Copyright 2015-2016 Library of the University of Tübingen
 *
 *  This file is part of the libiViaCore package.
 *
 *  The libiViaCore package is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public License as published
 *  by the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  libiViaCore is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public License
 *  along with libiViaCore; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
#include "File.h"
#include "FileUtil.h"
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <cstring>


File::File(const std::string &filename, const std::string &mode, const ThrowOnOpenBehaviour throw_on_error_behaviour)
    : filename_(filename), buffer_ptr_(buffer_), read_count_(0), file_(nullptr), pushed_back_(false), precision_(6)
{
    if (mode == "w")
        open_mode_ = WRITING;
    else if (mode == "a")
        open_mode_ = WRITING;
    else if (mode == "r")
        open_mode_ = READING;
    else if (mode == "r+")
        open_mode_ = READING_AND_WRITING;
    else {
        if (throw_on_error_behaviour == THROW_ON_ERROR)
            throw std::runtime_error("in File::File: open mode \"" + mode + "\" not supported! (1)");
        return;
    }

    file_ = std::fopen(filename.c_str(), mode.c_str());
    if (file_ == nullptr) {
        if (throw_on_error_behaviour == THROW_ON_ERROR)
            throw std::runtime_error("in File::File: could not open \"" + filename + "\" w/ mode \"" + mode + "\"!");
    }

}


File::File(const int fd, const std::string &mode)
    : filename_("/proc/self/fd/" + std::to_string(fd)), buffer_ptr_(buffer_), read_count_(0), file_(nullptr),
      pushed_back_(false), precision_(6)
{
    std::string local_mode;
    if (mode.empty()) {
        // Determine the mode from "fd":
        int flags;
        if (unlikely((flags = ::fcntl(fd, F_GETFL, &flags)) == -1))
            throw std::runtime_error("in File::File: fcntl(2) failed (" + std::string(::strerror(errno)) + ")!");

        flags &= O_ACCMODE;
        if (flags == O_RDONLY) {
            local_mode = "r";
            open_mode_ = READING;
        } else if (flags == O_WRONLY) {
            local_mode = "w";
            open_mode_ = WRITING;
        } else { // We assume flags == O_RDWR.
            local_mode = "r+";
            open_mode_ = READING_AND_WRITING;
        }
    } else {
        if (mode == "r")
            open_mode_ = READING;
        else if (mode == "w")
            open_mode_ = WRITING;
        else if (mode == "r+")
            open_mode_ = READING_AND_WRITING;
        else
            throw std::runtime_error("in File::File: open mode \"" + mode + "\" not supported! (2)");
        local_mode = mode;
    }

    file_ = ::fdopen(fd, local_mode.c_str());
    if (unlikely(file_ == nullptr))
        throw std::runtime_error("in File::File: fdopen(3) on \"" + std::to_string(fd) + "\" with mode \""
                                 + mode + "\" failed (" + std::string(::strerror(errno)) + ") (3)!");
}


bool File::close() {
    if (file_ == nullptr) {
        errno = 0;
        return false;
    }

    const bool retval(std::fclose(file_) == 0);
    file_ = nullptr;
    return retval;
}


void File::fillBuffer() {
    read_count_ = std::fread(reinterpret_cast<void *>(buffer_), 1, BUFSIZ, file_);
    if (unlikely(std::ferror(file_) != 0))
        throw std::runtime_error("in File:fillBuffer: error while reading \"" + filename_ + "\"!");
    buffer_ptr_ = buffer_;
}


off_t File::size() const {
    if (unlikely(file_ == nullptr))
        throw std::runtime_error("in File::size: can't obtain the size of non-open File \"" + filename_
                                 + "\"!");

    struct stat stat_buf;
    if (unlikely(::fstat(fileno(file_), &stat_buf) == -1))
        throw std::runtime_error("in File::size: fstat(2) failed on \"" + filename_ + "\" ("
                                 + std::string(::strerror(errno)) + ")!");

    return stat_buf.st_size;
}


bool File::seek(const off_t offset, const int whence) {
    if (unlikely(file_ == nullptr))
        throw std::runtime_error("in File::seek: can't seek on non-open file \"" + filename_ + "\"!");

    if (::fseeko(file_, offset, whence) != 0)
        return false;

    read_count_ = 0;
    buffer_ptr_ = buffer_;

    return true;
}


size_t File::read(void * const buf, const size_t buf_size) {
    if (unlikely(file_ == nullptr))
        throw std::runtime_error("in File::read: can't read from non-open file \"" + filename_ + "\"!");

    return ::fread(buf, 1, buf_size, file_);
}


size_t File::write(const void * const buf, const size_t buf_size) {
    if (unlikely(file_ == nullptr))
        throw std::runtime_error("in File::write: can't write to non-open file \"" + filename_ + "\"!");

    return ::fwrite(buf, 1, buf_size, file_);
}


size_t File::getline(std::string * const line, const char terminator) {
    line->clear();

    size_t count(0);
    for (;;) {
        const int ch(get());
        if (unlikely(ch == terminator or ch == EOF))
            return count;
        *line += static_cast<char>(ch);
        ++count;
    }
}


File &File::operator<<(const char * const s) {
    std::fputs(s, file_);
    return *this;
}


File &File::operator<<(const char ch) {
    std::fputc(ch, file_);
    return *this;
}


File &File::operator<<(const int i) {
    std::fprintf(file_, "%1d", i);
    return *this;
}


File &File::operator<<(const unsigned u) {
    std::fprintf(file_, "%1u", u);
    return *this;
}


File &File::operator<<(const long l) {
    std::fprintf(file_, "%1ld", l);
    return *this;
}


File &File::operator<<(const unsigned long ul) {
    std::fprintf(file_, "%1lu", ul);
    return *this;
}


File &File::operator<<(const long long ll) {
    std::fprintf(file_, "%1lld", ll);
    return *this;
}


File &File::operator<<(const unsigned long long ull) {
    std::fprintf(file_, "%1llu", ull);
    return *this;
}


File &File::operator<<(const double d) {
    std::fprintf(file_, "%.*g", precision_, d);
    return *this;
}


bool File::append(const int fd) {
    const off_t original_offset(::lseek(fd, 0, SEEK_CUR));
    if (unlikely(not FileUtil::Rewind(fd)))
        return false;

    flush();
    const int target_fd(fileno(file_));
    char buf[BUFSIZ];
    ssize_t read_count;
    errno = 0;
    while ((read_count = ::read(fd, buf, sizeof(buf))) > 0) {
        if (unlikely(::write(target_fd, buf, read_count) != read_count)) {
            ::lseek(fd, original_offset, SEEK_SET);
            return false;
        }
    }

    ::lseek(fd, original_offset, SEEK_SET);
    return errno == 0 ? true : false;
}


bool File::append(const File &file) {
    if (unlikely(not file.flush()))
        return false;
    return append(fileno(file.file_));
}


bool File::truncate(const off_t new_length) {
    if (unlikely(file_ == nullptr))
        throw std::runtime_error("in File::setNewSize: can't get non-open file's size \"" + filename_ + "\"!");

    flush();
    return ::ftruncate(fileno(file_), new_length) == 0;
}

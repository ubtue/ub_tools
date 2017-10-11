/** \file    FileDescriptor.cc
 *  \brief   Implementation of class FileDescriptor.
 *  \author  Dr. Johannes Ruscheinski
 */

/*
 *  Copyright 2002-2009 Project iVia.
 *  Copyright 2002-2009 The Regents of The University of California.
 *  Copyright 2017 Universitätsbibliothek Tübingen.
 *
 *  This file is part of the libiViaCore package.
 *
 *  The libiViaCore package is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public License as
 *  published by the Free Software Foundation; either version 2 of the License,
 *  or (at your option) any later version.
 *
 *  libiViaCore is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public License
 *  along with libiViaCore; if not, write to the Free Software Foundation, Inc.,
 *  59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "FileDescriptor.h"
#include <stdexcept>
#include <string>
#include <cstring>
#include <cerrno>
#include <unistd.h>
#include "Compiler.h"


FileDescriptor::FileDescriptor(const FileDescriptor &rhs) {
    if (rhs.fd_ == -1)
        fd_ = -1;
    else {
        fd_ = ::dup(rhs.fd_);
        if (unlikely(fd_ == -1))
            throw std::runtime_error("in FileDescriptor::FileDescriptor: dup(2) failed ("
                                     + std::string(::strerror(errno)) + ")!");
    }
}


void FileDescriptor::close() {
    if (likely(fd_ != -1))
        ::close(fd_);

    fd_ = -1;
}


const FileDescriptor &FileDescriptor::operator=(const FileDescriptor &rhs) {
    // Prevent self-assignment!
    if (likely(&rhs != this)) {
        if (likely(fd_ != -1))
            ::close(fd_);

        fd_ = ::dup(rhs.fd_);
        if (unlikely(fd_ == -1))
            throw std::runtime_error("in FileDescriptor::operator=: dup(2) failed ("
                                     + std::string(::strerror(errno)) + ")!");
    }

    return *this;
}


const FileDescriptor &FileDescriptor::operator=(const int new_fd) {
    if (likely(fd_ != -1))
        ::close(fd_);

    fd_ = new_fd;

    return *this;
}


int FileDescriptor::release() {
    const int retval(fd_);
    fd_ = -1;
    return retval;
}

/** \file    FileLocker.cc
 *  \brief   Implementation of class FileLocker.
 *  \author  Dr. Johannes Ruscheinski
 */

/*
 *  Copyright 2004-2008 Project iVia.
 *  Copyright 2004-2008 The Regents of The University of California.
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

#include "FileLocker.h"
#include <stdexcept>
#include <string>
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <unistd.h>
#include "util.h"


FileLocker::FileLocker(const int fd, const LockType lock_type)
    : lock_fd_(fd)
{
    struct flock lock_struct;
    lock_struct.l_type   = static_cast<short>((lock_type == READ_ONLY) ? F_RDLCK : F_WRLCK); /* F_RDLCK, F_WRLCK, F_UNLCK    */
    lock_struct.l_whence = SEEK_SET;                                                         /* SEEK_SET, SEEK_CUR, SEEK_END */
    lock_struct.l_start  = 0;                                                                /* Offset from l_whence         */
    lock_struct.l_len    = 0;                                                                /* length, 0 = to EOF           */
    lock_struct.l_pid    = ::getpid();                                                       /* our PID                      */

    if (::fcntl(lock_fd_, F_SETLKW, &lock_struct) == -1)
        throw std::runtime_error("in FileLocker::FileLocker: fcntl(2) failed! (" + std::to_string(errno) + ")!");
}


FileLocker::~FileLocker() {
    struct flock lock_struct;
    lock_struct.l_type   = F_UNLCK;    /* F_RDLCK, F_WRLCK, F_UNLCK    */
    lock_struct.l_whence = SEEK_SET;   /* SEEK_SET, SEEK_CUR, SEEK_END */
    lock_struct.l_start  = 0;          /* Offset from l_whence         */
    lock_struct.l_len    = 0;          /* length, 0 = to EOF           */
    lock_struct.l_pid    = ::getpid(); /* our PID                      */

    if (::fcntl(lock_fd_, F_SETLKW, &lock_struct) == -1) {                /* F_GETLK, F_SETLK, F_SETLKW */
        ::close(lock_fd_);
        LOG_ERROR("in FileLocker::~FileLocker: fcntl(2) failed ! (" + std::to_string(errno) + ")!");
    }
}


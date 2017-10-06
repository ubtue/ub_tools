/** \file    FileDescriptor.h
 *  \brief   Declaration of class FileDescriptor.
 *  \author  Dr. Johannes Ruscheinski
 */

/*
 *  Copyright 2002-2008 Project iVia.
 *  Copyright 2002-2008 The Regents of The University of California.
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

#ifndef FILE_DESCRIPTOR_H
#define FILE_DESCRIPTOR_H


/** \brief   Class that avoids file descriptor leaks due to forgotten calls to close(2) and/or unexpected exceptions being thrown.
 *  \warning Be careful about copying these objects.  They use dup(2) internally upon copying!
 */
class FileDescriptor {
    int fd_;
public:
    FileDescriptor(): fd_(-1) { }
    explicit FileDescriptor(const int fd): fd_(fd) { }

    /** Creates a duplicate file descriptor using dup(2). */
    FileDescriptor(const FileDescriptor &rhs);

    ~FileDescriptor() { close();}

    void close();

    bool isValid() const { return fd_ != -1; }
    bool operator!() const { return fd_ == -1; }
    operator int() const { return fd_; }

    // Assignment operators:
    const FileDescriptor &operator=(const FileDescriptor &rhs);
    const FileDescriptor &operator=(const int new_fd);

    /** \brief   Reliquishes ownership.
     *  \warning The caller becomes responsible for closing of the returned file descriptor!
     */
    int release();
};


#endif // ifndef FILE_DESCRIPTOR_H

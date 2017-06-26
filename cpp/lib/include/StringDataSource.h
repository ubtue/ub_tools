/** \file    StringDataSource.h
 *  \brief   Declaration of class StringDataSource.
 *  \author  Dr. Johannes Ruscheinski
 */

/*
    Copyright 2017 Library of the University of Tübingen
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
#ifndef STRING_DATA_SOURCE_H
#define STRING_DATA_SOURCE_H


#include <string>
#include <fcntl.h> // For SEEK_SET etc.


class StringDataSource {
    std::string s_;
    std::string::const_iterator ch_;
    bool pushed_back_;
    char pushed_back_char_;
public:
    explicit StringDataSource(const std::string &s): s_(s), ch_(s_.cbegin()), pushed_back_(false) { }

    /** \return the next character or EOF if we have reached the end of our string. */
    int get();

    /** \note You can never call this twice in a row.  You must call get() after calling putback() before
     *        you may call it again! */
    void putback(const char ch);

    /** \return The next character that get() would have returned.
     *  \note must not be called after get returned EOF!
     */
    int peek();

    inline off_t tell() const { return ch_ - s_.cbegin(); }

    /** \brief  Set the location for the next read operation.
     *  \param  offset  Signed offset relative to the reference point specified by "whence".
     *  \param  whence  SEEK_SET, SEEK_END, or SEEK_CUR.
     *  \return True upon success, else false.  If false, you can consult the global "errno" variable for the type
     *          of error encountered.
     */
    bool seek(const off_t offset, const int whence = SEEK_SET);

    inline void rewind() { ch_ = s_.cbegin(); pushed_back_ = false; }
};


#endif // ifndef STRING_DATA_SOURCE_H

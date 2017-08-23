/** \file    GzippedFileDataSource.h
 *  \brief   Declaration of class GzippedFileDataSource.
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
#include <memory>


// Forward declarations:
class File;
class GzStream;


class GzippedFileDataSource {
    std::unique_ptr<File> input_;
    GzStream *gz_stream_;
    char uncompressed_data_[8192];
    char compressed_data_[8192];
    unsigned uncompressed_data_count_;
    char *next_ch_;
    unsigned total_processed_; // The number of bytes of the last input that we have processed so far.
    size_t last_read_count_;
    bool more_decompressed_data_available_;
public:
    explicit GzippedFileDataSource(File * const input);

    /** \return the next character or EOF if we have reached the end of our string. */
    int get();

    void rewind();
;


#endif // ifndef STRING_DATA_SOURCE_H

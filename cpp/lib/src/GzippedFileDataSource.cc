/** \file    GzippedFileDataSource.cc
 *  \brief   Implementation of class GzippedFileDataSource.
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
#include "GzippedFileDataSource.h"
#include <cstring>
#include "Compiler.h"
#include "FileUtil.h"
#include "GzStream.h"
#include "util.h"


GzippedFileDataSource::GzippedFileDataSource(File * const input)
    : input_(input), gz_stream_(new GzStream(GzStream::GUNZIP)),
      uncompressed_data_count_(0), next_ch_(uncompressed_data_), more_decompressed_data_available_(false)
{
}


int GzippedFileDataSource::get() {
    for (;;) {
        // Still decompressed data in our buffer?
        if (next_ch_ < uncompressed_data_ + uncompressed_data_count_)
            return *next_ch_++;

        unsigned bytes_consumed;
        if (input_->eof()) {
            if (not more_decompressed_data_available_)
                return EOF;
        
            more_decompressed_data_available_ = gz_stream_->decompress(nullptr, last_read_count_, uncompressed_data_,
                                                                       sizeof(uncompressed_data_), &bytes_consumed,
                                                                       &uncompressed_data_count_);
            next_ch_ = uncompressed_data_;
            continue;
        }

        // Decompressor has more data for us?
        if (more_decompressed_data_available_) {
            more_decompressed_data_available_ = gz_stream_->decompress(
                                                    compressed_data_ + total_processed_,
                                                    static_cast<unsigned>(last_read_count_) - total_processed_,
                                                    uncompressed_data_, sizeof uncompressed_data_, &bytes_consumed,
                                                    &uncompressed_data_count_);
            total_processed_ += bytes_consumed;
            next_ch_ = uncompressed_data_;
            continue;
        }
    
        // Refill the I/O buffer:
        last_read_count_ = input_->read(compressed_data_, sizeof compressed_data_);
        if (unlikely(static_cast<ssize_t>(last_read_count_) == -1))
            Error("in GzippedFileDataSource::get: unexpected error while reading! ("
                  + std::string(std::strerror(errno)) + ")");

        if (unlikely(last_read_count_ == 0))
            return EOF;
        more_decompressed_data_available_ = gz_stream_->decompress(
                                                compressed_data_,
                                                static_cast<unsigned>(last_read_count_) - total_processed_,
                                                uncompressed_data_,
                                                sizeof uncompressed_data_, &bytes_consumed,
                                                &uncompressed_data_count_);
        total_processed_ = bytes_consumed;
        next_ch_ = uncompressed_data_;
    }
}


void GzippedFileDataSource::rewind() {
    delete gz_stream_;
    gz_stream_ = new GzStream(GzStream::GUNZIP);
    input_->rewind();
}


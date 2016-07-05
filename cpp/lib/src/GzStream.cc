/** \file    GzStream.cc
 *  \brief   Implementation of wrapper around the low-level facilities of zlib.
 *  \author  Dr. Johannes Ruscheinski
 *  \author  Dr. Gordon W. Paynter
 */

/*
 *  Copyright 2002-2008 Project iVia.
 *  Copyright 2002-2008 The Regents of The University of California.
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

#include "GzStream.h"
#include <cstring>
#include "Compiler.h"


GzStream::GzStream(const Type type, const unsigned compression_level,
                   void *(*_zalloc)(void *, unsigned int, unsigned int),
                   void (*_zfree)(void *, void *))
    throw(std::exception): type_(type)
{
    std::memset(&stream_, '\0', sizeof stream_);
    stream_.zalloc = _zalloc;
    stream_.zfree  = _zfree;

    if (type_ == COMPRESS or type == GZIP) {
        const int retcode(::deflateInit2(&stream_, compression_level, Z_DEFLATED,
                                         /* windowBits = */ type == COMPRESS ? 15 : (15 + 16),
                                         /* Max. memLevel for highest speed = */ 9, Z_DEFAULT_STRATEGY));
        switch (retcode) {
        case Z_STREAM_ERROR:
            throw std::runtime_error("GzStream::GzStream: invalid compression level (should be >= 1 and <= 9)!");
        case Z_MEM_ERROR:
            ::deflateEnd(&stream_);
            throw std::runtime_error("GzStream::GzStream: not enough memory for deflation!");
        case Z_DATA_ERROR:
            ::deflateEnd(&stream_);
            throw std::runtime_error("GzStream::GzStream: invalid deflate data!");
        case Z_VERSION_ERROR:
            throw std::runtime_error("GzStream::GzStream: invalid library version for deflation!");
        case Z_OK:
            return;
        default:
            throw std::runtime_error("in GzStream::GzStream: unknown error code " + std::to_string(retcode)
                                     + " for deflateInit2()!");
        }
    } else { // assume type_ == DECOMPRESS or type == GUNZIP
        const int retcode(::inflateInit2(&stream_, /* windowBits = */ type == DECOMPRESS ? 15 : (15 + 16)));
        switch (retcode) {
        case Z_MEM_ERROR:
            ::inflateEnd(&stream_);
            throw std::runtime_error("GzStream::GzStream: not enough memory for inflation!");
        case Z_DATA_ERROR:
            ::inflateEnd(&stream_);
            throw std::runtime_error("GzStream::GzStream: invalid inflate data!");
        case Z_VERSION_ERROR:
            throw std::runtime_error("GzStream::GzStream: invalid library version for inflation!");
        case Z_OK:
            return;
        default:
            throw std::runtime_error("in GzStream::GzStream: unknown error code " + std::to_string(retcode)
                                     + " for deflateInit2()!");
        }
    }
}


GzStream::~GzStream() {
    if (type_ == COMPRESS or type_ == GZIP)
        ::deflateEnd(&stream_);
    else
        ::inflateEnd(&stream_);
}


bool GzStream::compress(const char * const input_data, unsigned input_data_size, char * const output_data,
                        unsigned output_data_size, unsigned * const bytes_consumed, unsigned * const bytes_produced)
    throw(std::exception)
{
    stream_.next_in   = reinterpret_cast<Bytef * const>(const_cast<char * const>(input_data));
    stream_.avail_in  = input_data_size;
    stream_.next_out  = reinterpret_cast<Bytef *>(output_data);
    stream_.avail_out = output_data_size;
    int flush = (input_data == nullptr) ? Z_FINISH : 0;
    int retval = ::deflate(&stream_, flush);
    *bytes_consumed = input_data_size  - stream_.avail_in;
    *bytes_produced = output_data_size - stream_.avail_out;

    switch (retval) {
    case Z_OK:
        return true;
    case Z_STREAM_END:
        return false;
    case Z_STREAM_ERROR:
        throw std::runtime_error("in GzStream::compress: inconsistent stream state!");
    case Z_BUF_ERROR:
        throw std::runtime_error("in GzStream::compress: no progress possible!");
    }

    throw std::runtime_error("in GzStream::compress: we should *never* get here (return code = "
                             + std::to_string(retval) + ")!");
}


bool GzStream::decompress(const char * const input_data, unsigned input_data_size, char * const output_data,
                          unsigned output_data_size, unsigned * const bytes_consumed,
                          unsigned * const bytes_produced) throw(std::exception)
{
    stream_.next_in   = reinterpret_cast<Bytef * const>(const_cast<char * const>(input_data));
    stream_.avail_in  = (input_data == nullptr) ? 0 : input_data_size;
    stream_.next_out  = reinterpret_cast<Bytef *>(output_data);
    stream_.avail_out = output_data_size;
    int flush = (input_data == nullptr) ? Z_FINISH : 0;
    int retval = ::inflate(&stream_, flush);
    *bytes_consumed = input_data_size  - stream_.avail_in;
    *bytes_produced = output_data_size - stream_.avail_out;
    switch (retval) {
    case Z_OK:
        return true;
    case Z_STREAM_END:
        return false;
    case Z_STREAM_ERROR:
        throw std::runtime_error("in GzStream::decompress: inconsistent stream state!");
    case Z_BUF_ERROR:
        throw std::runtime_error("in GzStream::decompress: no progress possible!");
    }

    throw std::runtime_error("in GzStream::decompress: we should *never* get here (return code = "
                             + std::to_string(retval) + ")!");
}


void GzStream::Decompress(const char * const compressed_data, const size_t compressed_data_size,
                          std::string * const s, const Type type)
{
    if (unlikely(type != GzStream::DECOMPRESS and type != GzStream::GUNZIP))
        throw std::runtime_error("in GzStream::Decompress: type must be either GzStream::DECOMPRESS or "
                                 "GzStream::GUNZIP!");

    GzStream stream(type);

    char uncompressed_data[64 * 1024];
    unsigned bytes_consumed, bytes_produced, total_processed = 0;
    bool more = false;
    do {
        more = stream.decompress(compressed_data + total_processed,
                                 static_cast<unsigned>(compressed_data_size - total_processed),
                                 uncompressed_data, sizeof(uncompressed_data) - 1, &bytes_consumed, &bytes_produced);
        s->append(uncompressed_data, bytes_produced);
        total_processed += bytes_consumed;
    } while (total_processed < compressed_data_size);

    while (more and bytes_produced > 0) {
        more = stream.decompress(nullptr, 0, uncompressed_data, sizeof(uncompressed_data) - 1, &bytes_consumed,
                                 &bytes_produced);
        s->append(uncompressed_data, bytes_produced);
    }
}


// CompressString -- compress from a std::string object
//
std::string GzStream::CompressString(const std::string &input, const Type type) {
    if (unlikely(type != GzStream::COMPRESS and type != GzStream::GZIP))
        throw std::runtime_error("in GzStream::CompressString: type must be either GzStream::COMPRESS or "
                                 "GzStream::GZIP!");

    // The compressed string to output
    std::string compressed_output;

    // The stream, and variables for incrementally feeding it.
    GzStream stream(type);
    size_t input_length = input.length();
    char compressed_data[10000];
    unsigned bytes_consumed, bytes_produced;
    unsigned total_processed = 0;
    bool more = true;

    // While there is more input, compress it
    do {
        more = stream.compress(input.c_str() + total_processed,
                               static_cast<unsigned>(input_length - total_processed), compressed_data,
                               sizeof(compressed_data), &bytes_consumed, &bytes_produced);
        compressed_output.append(compressed_data, bytes_produced);
        total_processed += bytes_consumed;
    } while (total_processed < input_length);

    // If there is input that is "buffered", compress it
    while (more) {
        more = stream.compress(nullptr, 0, compressed_data, sizeof(compressed_data), &bytes_consumed, &bytes_produced);
        compressed_output.append(compressed_data, bytes_produced);
    }

    return compressed_output;
}


// DecompressString -- decompress from a std::string object
//
std::string GzStream::DecompressString(const std::string &input, const Type type) {
    if (unlikely(type != GzStream::DECOMPRESS and type != GzStream::GUNZIP))
        throw std::runtime_error("in GzStream::DecompressString: type must be either GzStream::DECOMPRESS or "
                                 "GzStream::GUNZIP!");

    // The string to return
    std::string decompressed_output;

    // The stream, and variables for incrementally feeding it.
    GzStream stream(type);
    size_t input_length = input.length();
    char decompressed_data[1024];
    unsigned bytes_consumed, bytes_produced;
    bool more = false;
    unsigned total_processed = 0;

    // While there is more data to decompress, decompress it
    do {
        more = stream.decompress(input.c_str() + total_processed,
                                 static_cast<unsigned>(input_length - total_processed), decompressed_data,
                                 sizeof(decompressed_data), &bytes_consumed, &bytes_produced);
        decompressed_output.append(decompressed_data, bytes_produced);
        total_processed += bytes_consumed;
    } while (more and total_processed < input_length);

    // If there is any input that is "buffered", then decompress it.
    while (more) {
        more = stream.decompress(nullptr, 0, decompressed_data, sizeof(decompressed_data), &bytes_consumed, &bytes_produced);
        decompressed_output.append(decompressed_data, bytes_produced);
    }

    return decompressed_output;
}

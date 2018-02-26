/** \file    GzStream.h
 *  \brief   A thin wrapper around the low-level facilities of zlib.
 *  \author  Dr. Johannes Ruscheinski
 *  \author  Dr. Gordon W. Paynter
 */

/*
 *  Copyright 2002-2009 Project iVia.
 *  Copyright 2002-2009 The Regents of The University of California.
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

#ifndef GZ_STREAM_H
#define GZ_STREAM_H


#include <stdexcept>
#include <string>
#include <zlib.h>


#ifdef GZIP
#      undef GZIP
#endif
#ifdef GUNZIP
#      undef GUNZIP
#endif


/** \class  GzStream
 *  \brief  A wrapper around the low-level facilities of zlib.
 */
class GzStream {
    z_stream stream_;
public:
    enum Type { COMPRESS, DECOMPRESS, GZIP, GUNZIP };
private:
    Type type_;
public:
    explicit GzStream(const Type type, const unsigned compression_level = 9,
                      void *(*_zalloc)(void *, unsigned int, unsigned int) = nullptr, void (*_zfree)(void *, void *) = nullptr);
    ~GzStream();


    /** \brief  Compresses bytes taken from "input_data" and deposits the compressed output into "output_data_size".
     *  \param  input_data       The data that are to be compressed.
     *  \param  input_data_size  The size of "input_data" to be processed.
     *  \param  output_data      Where to put the compressed data.
     *  \param  output_data_size The available space in "output_data".
     *  \param  bytes_consumed   The actual number for bytes from "input_data" that have been compressed.
     *  \param  bytes_produced   The length of the compressed output "output_data" actually used.
     *  \return Returns true if more compressed data can be retrieved and false otherwise.
     *  \note   After passing in all data to be compressed you must call "compress" with "input_data" set to
     *          nullptr and retrieve "output_data" until "compress" returns false.
     */
    bool compress(const char * const input_data, unsigned input_data_size, char * const output_data,
                  unsigned output_data_size, unsigned * const bytes_consumed, unsigned * const bytes_produced);


    /** \brief  Decompresses bytes taken from "input_data" and deposits the decompressed output into "output_data."
     *  \param  input_data       The data that are to be decompressed.
     *  \param  input_data_size  The size of "input_data" to be processed.
     *  \param  output_data      Where to put the decompressed data.
     *  \param  output_data_size The available space in "output_data."
     *  \param  bytes_consumed   The actual number for bytes from "input_data" that have been decompressed.
     *  \param  bytes_produced   The length of the decompressed output "output_data" actually used.
     *  \return Returns true if more decompressed data can be retrieved and false otherwise.
     *  \note   After passing in all data to be decompressed you can call "decompress" with "input_data" set to
     *          nullptr and/or "input_data_size" set to 0 and retrieve "output_data" until "decompress" returns false.
     */
    bool decompress(const char * const input_data, unsigned input_data_size, char * const output_data,
                    unsigned output_data_size, unsigned * const bytes_consumed, unsigned * const bytes_produced);


    /** \brief   Decompress a string
     *  \param   compressed_data       The string to decompress
     *  \param   compressed_data_size  The size of the string to decompress
     *  \param   s                     The decompressed string will be stored here.
     *  \param   type                  Must be either DECOMPRESS or GUNZIP.
     */
    static void Decompress(const char * const compressed_data, const size_t compressed_data_size,
                           std::string * const s, const Type type = DECOMPRESS);
    static void Decompress(const std::string &compressed_data, std::string * const s, const Type type = DECOMPRESS)
        { Decompress(compressed_data.c_str(), compressed_data.length(), s, type); }


    /** \brief   Compress a string.
     *  \param   input  The string to compress.
     *  \param   type   Must be either COMPRESS or GZIP.
     *  \warning Do not pass this function a (char *) pointing at binary data because the automatic conversion
     *           to a std::string will fail when the binary input includes NUL characters.
     *  \return  The compressed string.
     */
    static std::string CompressString(const std::string &input, const Type type = COMPRESS);


    /** \brief   Decompress a string.
     *  \param   input  The string to decompress.
     *  \param   type   Must be either DECOMPRESS or GUNZIP.
     *  \return  The decompressed string
     *  \warning Do not pass this function a (char *) pointing at binary data because the automatic
     *           conversion to a std::string will fail when the binary input includes NUL characters which is
     *           very likely with compressed data.
     */
    static std::string DecompressString(const std::string &input, const Type type = DECOMPRESS);
private:
    GzStream(const GzStream &rhs);            // Intentionally unimplemented!
    GzStream &operator=(const GzStream &rhs); // Intentionally unimplemented!
};


#endif // ifndef GZ_STREAM_H

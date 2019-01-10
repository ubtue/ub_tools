/** \file    BinaryIO.h
 *  \brief   Declaration of binary I/O utility functions.
 *  \author  Dr. Johannes Ruscheinski
 */

/*
 *  Copyright 2004-2005 Project iVia.
 *  Copyright 2004-2005 The Regents of The University of California.
 *  Copyright 2019 Universitätsbibliothek Tübingen.
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
#pragma once


#include <fstream>
#include <string>
#include <cinttypes>


// Forward declaration:
class File;


/** \namespace BinaryIO
 *  \brief     Contains binary I/O functions that allow portable (between different hardware platforms) serialization and
 *             deserialisation of std::strings and numeric types.
 */
namespace BinaryIO {


/** \brief  Serializes a std::string to a binary stream.
 *  \param  output  The stream to write to.
 *  \param  s       The string to write.
 *  \return True if the operation succeeded and false if there was some I/O error.
 */
bool Write(std::ostream &output, const std::string &s);


/** \brief  Deserializes a std::string from a binary stream.
 *  \param  input  The stream to read from.
 *  \param  s      The string to read.
 *  \return True if the operation succeeded and false if there was some I/O error.
 */
bool Read(std::istream &input, std::string * const s);


/** \brief  Serializes a boolean variable to a binary stream.
 *  \param  output  The stream to write to.
 *  \param  b       The boolean variable to write.
 *  \return True if the operation succeeded and false if there was some I/O error.
 */
bool Write(std::ostream &output, const bool b);


/** \brief  Deserializes a boolean variable from a binary stream.
 *  \param  input  The stream to read from.
 *  \param  b      The boolean variable to read.
 *  \return True if the operation succeeded and false if there was some I/O error.
 */
bool Read(std::istream &input, bool * const b);


/** \brief  Serializes a uint8_t variable to a binary stream.
 *  \param  output  The stream to write to.
 *  \param  u8      The variable to write.
 *  \return True if the operation succeeded and false if there was some I/O error.
 */
bool Write(std::ostream &output, const uint8_t u8);


/** \brief  Deserializes a uint8_t variable from a binary stream.
 *  \param  input  The stream to read from.
 *  \param  u8     The variable to read.
 *  \return True if the operation succeeded and false if there was some I/O error.
 */
bool Read(std::istream &input, uint8_t * const u8);


/** \brief  Serializes an int8_t variable to a binary stream.
 *  \param  output  The stream to write to.
 *  \param  i8      The variable to write.
 *  \return True if the operation succeeded and false if there was some I/O error.
 */
bool Write(std::ostream &output, const int8_t i8);


/** \brief  Deserializes an int8_t variable from a binary stream.
 *  \param  input  The stream to read from.
 *  \param  i8     The variable to read.
 *  \return True if the operation succeeded and false if there was some I/O error.
 */
bool Read(std::istream &input, int8_t * const i8);


/** \brief  Serializes a uint16_t variable to a binary stream.
 *  \param  output  The stream to write to.
 *  \param  u16     The variable to write.
 *  \return True if the operation succeeded and false if there was some I/O error.
 */
bool Write(std::ostream &output, const uint16_t u16);


/** \brief  Deserializes a uint16_t variable from a binary stream.
 *  \param  input  The stream to read from.
 *  \param  u16    The variable to read.
 *  \return True if the operation succeeded and false if there was some I/O error.
 */
bool Read(std::istream &input, uint16_t * const u16);


/** \brief  Serializes an int16_t variable to a binary stream.
 *  \param  output  The stream to write to.
 *  \param  i16     The variable to write.
 *  \return True if the operation succeeded and false if there was some I/O error.
 */
bool Write(std::ostream &output, const int16_t i16);


/** \brief  Deserializes an int16_t variable from a binary stream.
 *  \param  input  The stream to read from.
 *  \param  i16    The variable to read.
 *  \return True if the operation succeeded and false if there was some I/O error.
 */
bool Read(std::istream &input, int16_t * const i16);


/** \brief  Serializes a uint32_t variable to a binary stream.
 *  \param  output  The stream to write to.
 *  \param  u32     The variable to write.
 *  \return True if the operation succeeded and false if there was some I/O error.
 */
bool Write(std::ostream &output, const uint32_t u32);


/** \brief  Deserializes a uint32_t variable from a binary stream.
 *  \param  input  The stream to read from.
 *  \param  u32    The variable to read.
 *  \return True if the operation succeeded and false if there was some I/O error.
 */
bool Read(std::istream &input, uint32_t * const u32);


/** \brief  Serializes an int32_t variable to a binary stream.
 *  \param  output  The stream to write to.
 *  \param  i32     The variable to write.
 *  \return True if the operation succeeded and false if there was some I/O error.
 */
bool Write(std::ostream &output, const int32_t i32);


/** \brief  Deserializes an int32_t variable from a binary stream.
 *  \param  input  The stream to read from.
 *  \param  i32    The variable to read.
 *  \return True if the operation succeeded and false if there was some I/O error.
 */
bool Read(std::istream &input, int32_t * const i32);


/** \brief  Serializes a uint64_t variable to a binary stream.
 *  \param  output  The stream to write to.
 *  \param  u64     The variable to write.
 *  \return True if the operation succeeded and false if there was some I/O error.
 */
bool Write(std::ostream &output, const uint64_t u64);


/** \brief  Deserializes a uint64_t variable from a binary stream.
 *  \param  input  The stream to read from.
 *  \param  u64    The variable to read.
 *  \return True if the operation succeeded and false if there was some I/O error.
 */
bool Read(std::istream &input, uint64_t * const u64);


/** \brief  Serializes an int64_t variable to a binary stream.
 *  \param  output  The stream to write to.
 *  \param  i64     The variable to write.
 *  \return True if the operation succeeded and false if there was some I/O error.
 */
bool Write(std::ostream &output, const int64_t i64);


/** \brief  Deserializes an int64_t variable from a binary stream.
 *  \param  input  The stream to read from.
 *  \param  i64    The variable to read.
 *  \return True if the operation succeeded and false if there was some I/O error.
 */
bool Read(std::istream &input, int64_t * const i64);


/** \brief  Serializes a float variable to a binary stream.
 *  \param  output  The stream to write to.
 *  \param  f       The variable to write.
 *  \return True if the operation succeeded and false if there was some I/O error.
 */
bool Write(std::ostream &output, const float f);


/** \brief  Deserializes a float variable from a binary stream.
 *  \param  input  The stream to read from.
 *  \param  f      The variable to read.
 *  \return True if the operation succeeded and false if there was some I/O error.
 */
bool Read(std::istream &input, float * const f);


/** \brief  Serializes a double variable to a binary stream.
 *  \param  output  The stream to write to.
 *  \param  d       The variable to write.
 *  \return True if the operation succeeded and false if there was some I/O error.
 */
bool Write(std::ostream &output, const double d);


/** \brief  Deserializes a double variable from a binary stream.
 *  \param  input  The stream to read from.
 *  \param  d      The variable to read.
 *  \return True if the operation succeeded and false if there was some I/O error.
 */
bool Read(std::istream &input, double * const d);


/** \brief  Serializes a std::string to a binary file.
 *  \param  output  The file to write to.
 *  \param  s       The string to write.
 *  \return True if the operation succeeded and false if there was some I/O error.
 */
bool Write(File &output, const std::string &s);


/** \brief  Deserializes a std::string from a binary file.
 *  \param  input  The file to read from.
 *  \param  s      The string to read.
 *  \return True if the operation succeeded and false if there was some I/O error.
 */
bool Read(File &input, std::string * const s);


/** \brief  Serializes a boolean variable to a binary file.
 *  \param  output  The file to write to.
 *  \param  b       The boolean variable to write.
 *  \return True if the operation succeeded and false if there was some I/O error.
 */
bool Write(File &output, const bool b);


/** \brief  Deserializes a boolean variable from a binary file.
 *  \param  input  The file to read from.
 *  \param  b      The boolean variable to read.
 *  \return True if the operation succeeded and false if there was some I/O error.
 */
bool Read(File &input, bool * const b);


/** \brief  Serializes a uint8_t variable to a binary file.
 *  \param  output  The file to write to.
 *  \param  u8      The variable to write.
 *  \return True if the operation succeeded and false if there was some I/O error.
 */
bool Write(File &output, const uint8_t u8);


/** \brief  Deserializes a uint8_t variable from a binary file.
 *  \param  input  The file to read from.
 *  \param  u8     The variable to read.
 *  \return True if the operation succeeded and false if there was some I/O error.
 */
bool Read(File &input, uint8_t * const u8);


/** \brief  Serializes an int8_t variable to a binary file.
 *  \param  output  The file to write to.
 *  \param  i8      The variable to write.
 *  \return True if the operation succeeded and false if there was some I/O error.
 */
bool Write(File &output, const int8_t i8);


/** \brief  Deserializes an int8_t variable from a binary file.
 *  \param  input  The file to read from.
 *  \param  i8     The variable to read.
 *  \return True if the operation succeeded and false if there was some I/O error.
 */
bool Read(File &input, int8_t * const i8);


/** \brief  Serializes a uint16_t variable to a binary file.
 *  \param  output  The file to write to.
 *  \param  u16     The variable to write.
 *  \return True if the operation succeeded and false if there was some I/O error.
 */
bool Write(File &output, const uint16_t u16);


/** \brief  Deserializes a uint16_t variable from a binary file.
 *  \param  input  The file to read from.
 *  \param  u16    The variable to read.
 *  \return True if the operation succeeded and false if there was some I/O error.
 */
bool Read(File &input, uint16_t * const u16);


/** \brief  Serializes an int16_t variable to a binary file.
 *  \param  output  The file to write to.
 *  \param  i16     The variable to write.
 *  \return True if the operation succeeded and false if there was some I/O error.
 */
bool Write(File &output, const int16_t i16);


/** \brief  Deserializes an int16_t variable from a binary file.
 *  \param  input  The file to read from.
 *  \param  i16    The variable to read.
 *  \return True if the operation succeeded and false if there was some I/O error.
 */
bool Read(File &input, int16_t * const i16);


/** \brief  Serializes a uint32_t variable to a binary file.
 *  \param  output  The file to write to.
 *  \param  u32     The variable to write.
 *  \return True if the operation succeeded and false if there was some I/O error.
 */
bool Write(File &output, const uint32_t u32);


/** \brief  Deserializes a uint32_t variable from a binary file.
 *  \param  input  The file to read from.
 *  \param  u32    The variable to read.
 *  \return True if the operation succeeded and false if there was some I/O error.
 */
bool Read(File &input, uint32_t * const u32);


/** \brief  Serializes an int32_t variable to a binary file.
 *  \param  output  The file to write to.
 *  \param  i32     The variable to write.
 *  \return True if the operation succeeded and false if there was some I/O error.
 */
bool Write(File &output, const int32_t i32);


/** \brief  Deserializes an int32_t variable from a binary file.
 *  \param  input  The file to read from.
 *  \param  i32    The variable to read.
 *  \return True if the operation succeeded and false if there was some I/O error.
 */
bool Read(File &input, int32_t * const i32);


/** \brief  Serializes a uint64_t variable to a binary file.
 *  \param  output  The file to write to.
 *  \param  u64     The variable to write.
 *  \return True if the operation succeeded and false if there was some I/O error.
 */
bool Write(File &output, const uint64_t u64);


/** \brief  Deserializes a uint64_t variable from a binary file.
 *  \param  input  The file to read from.
 *  \param  u64    The variable to read.
 *  \return True if the operation succeeded and false if there was some I/O error.
 */
bool Read(File &input, uint64_t * const u64);


/** \brief  Serializes an int64_t variable to a binary file.
 *  \param  output  The file to write to.
 *  \param  i64     The variable to write.
 *  \return True if the operation succeeded and false if there was some I/O error.
 */
bool Write(File &output, const int64_t i64);


/** \brief  Deserializes an int64_t variable from a binary file.
 *  \param  input  The file to read from.
 *  \param  i64    The variable to read.
 *  \return True if the operation succeeded and false if there was some I/O error.
 */
bool Read(File &input, int64_t * const i64);


/** \brief  Serializes a float variable to a binary file.
 *  \param  output  The file to write to.
 *  \param  f       The variable to write.
 *  \return True if the operation succeeded and false if there was some I/O error.
 */
bool Write(File &output, const float f);


/** \brief  Deserializes a float variable from a binary file.
 *  \param  input  The file to read from.
 *  \param  f      The variable to read.
 *  \return True if the operation succeeded and false if there was some I/O error.
 */
bool Read(File &input, float * const f);


/** \brief  Serializes a double variable to a binary file.
 *  \param  output  The file to write to.
 *  \param  d       The variable to write.
 *  \return True if the operation succeeded and false if there was some I/O error.
 */
bool Write(File &output, const double d);


/** \brief  Deserializes a double variable from a binary file.
 *  \param  input  The file to read from.
 *  \param  d      The variable to read.
 *  \return True if the operation succeeded and false if there was some I/O error.
 */
bool Read(File &input, double * const d);


} // namespace BinaryIO

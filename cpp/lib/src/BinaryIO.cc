/** \file    BinaryIO.cc
 *  \brief   Implementation of binary I/O utility functions.
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

#include "BinaryIO.h"
#include <cfloat>
#include <cmath>
#include <alloca.h>
#include <netinet/in.h>
#include "File.h"


namespace {


void ExtractExponentAndMantissa(const float f, int * const exponent, int32_t * const mantissa) {
    const float f1(::frexpf(f, exponent));
    *mantissa = static_cast<int32_t>(::scalbnf(f1, FLT_MANT_DIG));
}


void ExtractExponentAndMantissa(const double d, int * const exponent, int64_t * const mantissa) {
    const double d1(::frexp(d, exponent));
    *mantissa = static_cast<int64_t>(::scalbn(d1, DBL_MANT_DIG));
}


void ExponentAndMantissaToFloat(const int exponent, const int32_t mantissa, float * const f) {
    const float f1(::scalbnf(mantissa, -FLT_MANT_DIG));
    *f = ::ldexpf(f1, exponent);
}


void ExponentAndMantissaToDouble(const int exponent, const int64_t mantissa, double * const d) {
    const double d1(::scalbn(mantissa, -DBL_MANT_DIG));
    *d = ::ldexp(d1, exponent);
}


} // unnamed namespace


namespace BinaryIO {


bool Write(std::ostream &output, const std::string &s) {
    uint32_t size(s.size());
    size = htonl(size);
    output.write(reinterpret_cast<const char *>(&size), sizeof size);
    if (output.fail())
        return false;

    output.write(s.c_str(), s.size());
    if (output.fail())
        return false;

    return true;
}


bool Write(std::ostream &output, const std::wstring &s) {
    uint32_t size(s.size());
    size = htonl(size);
    output.write(reinterpret_cast<const char *>(&size), sizeof size);
    if (output.fail())
        return false;

    // Fix byte order:
    std::wstring s2;
    for (auto &wch : s2)
        wch = htonl(wch);

    output.write(reinterpret_cast<const char *>(s2.data()), s2.size() * sizeof(wchar_t));
    if (output.fail())
        return false;

    return true;
}


bool Read(std::istream &input, std::string * const s) {
    uint32_t size;
    input.read(reinterpret_cast<char *>(&size), sizeof size);
    if (input.fail())
        return false;

    size = ntohl(size);
    std::string buf(size, '\0');
    input.read(&buf.front(), size);
    if (input.fail())
        return false;

    s->swap(buf);

    return true;
}


bool Read(std::istream &input, std::wstring * const s) {
    uint32_t size;
    input.read(reinterpret_cast<char *>(&size), sizeof size);
    if (input.fail())
        return false;

    size = ntohl(size);
    std::wstring buf(size, L'\0');
    input.read((char *)buf.data(), size * sizeof(wchar_t));
    if (input.fail())
        return false;

    s->swap(buf);

    // Fix byte order:
    for (auto &wch : *s)
        wch = ntohl(wch);

    return true;
}


bool Write(std::ostream &output, const bool b) {
    const int8_t i8(b);
    output.write(reinterpret_cast<const char *>(&i8), sizeof i8);
    return not output.fail();
}


bool Read(std::istream &input, bool * const b) {
    int8_t i8;
    input.read(reinterpret_cast<char *>(&i8), sizeof i8);
    *b = i8;
    return not input.fail();
}


bool Write(std::ostream &output, const uint8_t u8) {
    output.write(reinterpret_cast<const char *>(&u8), sizeof u8);
    return not output.fail();
}


bool Read(std::istream &input, uint8_t * const u8) {
    input.read(reinterpret_cast<char *>(u8), sizeof *u8);
    return not input.fail();
}


bool Write(std::ostream &output, const int8_t i8) {
    output.write(reinterpret_cast<const char *>(&i8), sizeof i8);
    return not output.fail();
}


bool Read(std::istream &input, int8_t * const i8) {
    input.read(reinterpret_cast<char *>(i8), sizeof *i8);
    return not input.fail();
}


bool Write(std::ostream &output, const uint16_t u16) {
    uint16_t be_u16;
    be_u16 = htons(u16);
    output.write(reinterpret_cast<const char *>(&be_u16), sizeof be_u16);
    return not output.fail();
}


bool Read(std::istream &input, uint16_t * const u16) {
    uint16_t be_u16;
    input.read(reinterpret_cast<char *>(&be_u16), sizeof be_u16);
    *u16 = ntohs(be_u16);
    return not input.fail();
}


bool Write(std::ostream &output, const int16_t i16) {
    uint16_t be_u16;
    be_u16 = htons(i16);
    output.write(reinterpret_cast<const char *>(&be_u16), sizeof be_u16);
    return not output.fail();
}


bool Read(std::istream &input, int16_t * const i16) {
    uint16_t be_u16;
    input.read(reinterpret_cast<char *>(&be_u16), sizeof be_u16);
    *i16 = ntohs(be_u16);
    return not input.fail();
}


bool Write(std::ostream &output, const uint32_t u32) {
    uint32_t be_u32;
    be_u32 = htonl(u32);
    output.write(reinterpret_cast<const char *>(&be_u32), sizeof be_u32);
    return not output.fail();
}


bool Read(std::istream &input, uint32_t * const u32) {
    uint32_t be_u32;
    input.read(reinterpret_cast<char *>(&be_u32), sizeof be_u32);
    *u32 = ntohl(be_u32);
    return not input.fail();
}


bool Write(std::ostream &output, const int32_t i32) {
    uint32_t be_u32;
    be_u32 = htonl(i32);
    output.write(reinterpret_cast<const char *>(&be_u32), sizeof be_u32);
    return not output.fail();
}


bool Read(std::istream &input, int32_t * const i32) {
    uint32_t be_u32;
    input.read(reinterpret_cast<char *>(&be_u32), sizeof be_u32);
    *i32 = ntohl(be_u32);
    return not input.fail();
}


bool Write(std::ostream &output, const uint64_t u64) {
    const uint32_t lower(u64 & 0xFFFFFFFFu);
    if (not Write(output, lower))
        return false;

    const uint32_t upper(u64 >> 32u);
    return Write(output, upper);
}


bool Read(std::istream &input, uint64_t * const u64) {
    uint32_t lower;
    if (not Read(input, &lower))
        return false;

    uint32_t upper;
    if (not Read(input, &upper))
        return false;

    *u64 = (static_cast<uint64_t>(upper) << 32u) | lower;
    return true;
}


bool Write(std::ostream &output, const int64_t i64) {
    const uint32_t lower(i64 & 0xFFFFFFFFu);
    if (not Write(output, lower))
        return false;

    const uint32_t upper(i64 >> 32u);
    return Write(output, upper);
}


bool Read(std::istream &input, int64_t * const i64) {
    uint32_t lower;
    if (not Read(input, &lower))
        return false;

    uint32_t upper;
    if (not Read(input, &upper))
        return false;

    *i64 = (static_cast<uint64_t>(upper) << 32u) | lower;
    return true;
}


bool Write(std::ostream &output, const float f) {
    int exponent;
    int32_t mantissa;
    ExtractExponentAndMantissa(f, &exponent, &mantissa);
    if (not Write(output, static_cast<int32_t>(exponent)))
        return false;
    return Write(output, mantissa);
}


bool Read(std::istream &input, float * const f) {
    int32_t exponent;
    if (not Read(input, &exponent))
        return false;

    int32_t mantissa;
    if (not Read(input, &mantissa))
        return false;

    ExponentAndMantissaToFloat(exponent, mantissa, f);
    return true;
}


bool Write(std::ostream &output, const double d) {
    int exponent;
    int64_t mantissa;
    ExtractExponentAndMantissa(d, &exponent, &mantissa);
    if (not Write(output, static_cast<int32_t>(exponent)))
        return false;
    return Write(output, mantissa);
}


bool Read(std::istream &input, double * const d) {
    int32_t exponent;
    if (not Read(input, &exponent))
        return false;

    int64_t mantissa;
    if (not Read(input, &mantissa))
        return false;

    ExponentAndMantissaToDouble(exponent, mantissa, d);
    return true;
}


bool Write(File &output, const std::string &s) {
    uint32_t size(s.size());
    size = htonl(size);
    output.write(reinterpret_cast<const char *>(&size), sizeof size);
    if (output.fail())
        return false;

    output.write(s.c_str(), s.size());
    if (output.fail())
        return false;

    return true;
}


bool Write(File &output, const std::wstring &s) {
    uint32_t size(s.size());
    size = htonl(size);
    output.write(reinterpret_cast<const char *>(&size), sizeof size);
    if (output.fail())
        return false;

    output.write(reinterpret_cast<const char *>(s.data()), s.size() * sizeof(wchar_t));
    if (output.fail())
        return false;

    return true;
}


bool Read(File &input, std::string * const s) {
    uint32_t size;
    input.read(reinterpret_cast<char *>(&size), sizeof size);
    if (input.fail())
        return false;

    size = ntohl(size);
    std::string buf(size, '\0');
    input.read(&buf.front(), size);
    if (input.fail())
        return false;

    s->swap(buf);

    return true;
}


bool Read(File &input, std::wstring * const s) {
    uint32_t size;
    input.read(reinterpret_cast<char *>(&size), sizeof size);
    if (input.fail())
        return false;

    size = ntohl(size);
    std::wstring buf(size, L'\0');
    input.read((char *)buf.data(), size * sizeof(wchar_t));
    if (input.fail())
        return false;

    s->swap(buf);

    return true;
}


bool Write(File &output, const bool b) {
    const int8_t i8(b);
    output.write(reinterpret_cast<const char *>(&i8), sizeof i8);
    return not output.fail();
}


bool Read(File &input, bool * const b) {
    int8_t i8;
    input.read(reinterpret_cast<char *>(&i8), sizeof i8);
    *b = i8;
    return not input.fail();
}


bool Write(File &output, const uint8_t u8) {
    output.write(reinterpret_cast<const char *>(&u8), sizeof u8);
    return not output.fail();
}


bool Read(File &input, uint8_t * const u8) {
    input.read(reinterpret_cast<char *>(u8), sizeof *u8);
    return not input.fail();
}


bool Write(File &output, const int8_t i8) {
    output.write(reinterpret_cast<const char *>(&i8), sizeof i8);
    return not output.fail();
}


bool Read(File &input, int8_t * const i8) {
    input.read(reinterpret_cast<char *>(i8), sizeof *i8);
    return not input.fail();
}


bool Write(File &output, const uint16_t u16) {
    uint16_t be_u16;
    be_u16 = htons(u16);
    output.write(reinterpret_cast<const char *>(&be_u16), sizeof be_u16);
    return not output.fail();
}


bool Read(File &input, uint16_t * const u16) {
    uint16_t be_u16;
    input.read(reinterpret_cast<char *>(&be_u16), sizeof be_u16);
    *u16 = ntohs(be_u16);
    return not input.fail();
}


bool Write(File &output, const int16_t i16) {
    uint16_t be_u16;
    be_u16 = htons(i16);
    output.write(reinterpret_cast<const char *>(&be_u16), sizeof be_u16);
    return not output.fail();
}


bool Read(File &input, int16_t * const i16) {
    uint16_t be_u16;
    input.read(reinterpret_cast<char *>(&be_u16), sizeof be_u16);
    *i16 = ntohs(be_u16);
    return not input.fail();
}


bool Write(File &output, const uint32_t u32) {
    uint32_t be_u32;
    be_u32 = htonl(u32);
    output.write(reinterpret_cast<const char *>(&be_u32), sizeof be_u32);
    return not output.fail();
}


bool Read(File &input, uint32_t * const u32) {
    uint32_t be_u32;
    input.read(reinterpret_cast<char *>(&be_u32), sizeof be_u32);
    *u32 = ntohl(be_u32);
    return not input.fail();
}


bool Write(File &output, const int32_t i32) {
    uint32_t be_u32;
    be_u32 = htonl(i32);
    output.write(reinterpret_cast<const char *>(&be_u32), sizeof be_u32);
    return not output.fail();
}


bool Read(File &input, int32_t * const i32) {
    uint32_t be_u32;
    input.read(reinterpret_cast<char *>(&be_u32), sizeof be_u32);
    *i32 = ntohl(be_u32);
    return not input.fail();
}


bool Write(File &output, const uint64_t u64) {
    const uint32_t lower(u64 & 0xFFFFFFFFu);
    if (not Write(output, lower))
        return false;

    const uint32_t upper(u64 >> 32u);
    return Write(output, upper);
}


bool Read(File &input, uint64_t * const u64) {
    uint32_t lower;
    if (not Read(input, &lower))
        return false;

    uint32_t upper;
    if (not Read(input, &upper))
        return false;

    *u64 = (static_cast<uint64_t>(upper) << 32u) | lower;
    return true;
}


bool Write(File &output, const int64_t i64) {
    const uint32_t lower(i64 & 0xFFFFFFFFu);
    if (not Write(output, lower))
        return false;

    const uint32_t upper(i64 >> 32u);
    return Write(output, upper);
}


bool Read(File &input, int64_t * const i64) {
    uint32_t lower;
    if (not Read(input, &lower))
        return false;

    uint32_t upper;
    if (not Read(input, &upper))
        return false;

    *i64 = (static_cast<uint64_t>(upper) << 32u) | lower;
    return true;
}


bool Write(File &output, const float f) {
    int exponent;
    int32_t mantissa;
    ExtractExponentAndMantissa(f, &exponent, &mantissa);
    if (not Write(output, static_cast<int32_t>(exponent)))
        return false;
    return Write(output, mantissa);
}


bool Read(File &input, float * const f) {
    int32_t exponent;
    if (not Read(input, &exponent))
        return false;

    int32_t mantissa;
    if (not Read(input, &mantissa))
        return false;

    ExponentAndMantissaToFloat(exponent, mantissa, f);
    return true;
}


bool Write(File &output, const double d) {
    int exponent;
    int64_t mantissa;
    ExtractExponentAndMantissa(d, &exponent, &mantissa);
    if (not Write(output, static_cast<int32_t>(exponent)))
        return false;
    return Write(output, mantissa);
}


bool Read(File &input, double * const d) {
    int32_t exponent;
    if (not Read(input, &exponent))
        return false;

    int64_t mantissa;
    if (not Read(input, &mantissa))
        return false;

    ExponentAndMantissaToDouble(exponent, mantissa, d);
    return true;
}


} // namespace BinaryIO

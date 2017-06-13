/** \file    File.h
 *  \brief   Declaration of class File.
 *  \author  Dr. Johannes Ruscheinski
 */

/*
 *  Copyright 2005-2008 Project iVia.
 *  Copyright 2005-2008 The Regents of The University of California.
 *  Copyright 2015-2017 Library of the University of Tübingen
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
#ifndef FILE_H
#define FILE_H


#include <stdexcept>
#include <cstdio>
#include "Compiler.h"


class File {
public:
    template<typename ArgType> struct SingleArgManipulator {
        File &(*func_)(File &file, ArgType arg);
        ArgType arg_;
    public:
        SingleArgManipulator(File &(*func)(File &file, ArgType arg), ArgType arg): func_(func), arg_(arg) { }
    };
    
    enum ThrowOnOpenBehaviour { THROW_ON_ERROR, DO_NOT_THROW_ON_ERROR };
private:
    enum OpenMode { READING, WRITING, READING_AND_WRITING };
private:
    std::string filename_;
    char buffer_[BUFSIZ];
    char *buffer_ptr_;
    size_t read_count_;
    FILE *file_;
    unsigned pushed_back_count_;
    char pushed_back_chars_[2];
    int precision_;
    OpenMode open_mode_;
public:
    /** \brief  Creates and initalises a File object.
     *  \param  path                      The pathname for the file (see fopen(3) for details).
     *  \param  mode                      The open mode (see fopen(3) for details).  An extension to the fopen modes
     *                                    are either "c" or "u".  "c" meaning "compress" can only be combined with "w"
     *                                    and "u" meaning "uncompress" with "r".  Using either flag makes seeking or
     *                                    rewinding impossible.
     *  \param  throw_on_error_behaviour  If true, any open failure will cause an exception to be thrown.  If not true
     *                                    you must use the fail() member function.
     */
    File(const std::string &filename, const std::string &mode,
         const ThrowOnOpenBehaviour throw_on_error_behaviour = DO_NOT_THROW_ON_ERROR);

    /** \brief  Create a File object from a file descriptor.
     *  \param  fd    A valid (open) file descriptor for the current process.
     *  \param  mode  The open mode (see fopen(3) for details).  Must be compatible, i.e. a subset of the mode for
     *                "fd."  If not specified the mode for "fd" will be used.
     */
    explicit File(const int fd, const std::string &mode = "");
    
    ~File() { if (file_ != nullptr) std::fclose(file_); }

    /** Closes this File.  If this fails you may consult the global "errno" for the reason. */
    bool close();

    inline int getFileDescriptor() const { return fileno(file_); }

    inline off_t tell() const {
        const off_t file_pos(::ftello(file_));
        if (open_mode_ == WRITING)
            return file_pos;
        return file_pos - read_count_ + (buffer_ptr_ - buffer_);
    }

    /** \brief  Set the file pointer for the next I/O operation.
     *  \param  offset  Signed offset relative to the reference point specified by "whence".
     *  \param  whence  SEEK_SET, SEEK_END, or SEEK_CUR.
     *  \return True upon success, else false.  If false, you can consult the global "errno" variable for the type
     *          of error encountered.
     */
    bool seek(const off_t offset, const int whence = SEEK_SET);
    
    inline int get() {
        if (unlikely(pushed_back_count_ > 0)) {
            const char pushed_back_char(pushed_back_chars_[0]);
            ++pushed_back_count_;
            for (unsigned i(0); i < pushed_back_count_; ++i)
                pushed_back_chars_[i] = pushed_back_chars_[i + 1];
            return pushed_back_char;
        }
        
        if (unlikely(buffer_ptr_ == buffer_ + read_count_))
            fillBuffer();
        if (unlikely(read_count_ == 0))
            return EOF;
        return *buffer_ptr_++;
    }

    /** \brief  Read some data from a file.
     *  \param  buf       The data to read.
     *  \param  buf_size  How much data to read.
     *  \return Returns a short count if an error occurred or EOF was encountered, otherwise returns "buf_size".
     *  \note   On returning a short count you need to call either eof() or anErrorOccurred() in order to
     *          determine whether the short count is due to an error condition or EOF.
     */
    size_t read(void * const buf, const size_t buf_size);

    /** \brief  Write some data to a file.
     *  \param  buf       The data to write.
     *  \param  buf_size  How much data to write.
     *  \return Returns a short count if an error occurred, otherwise returns "buf_size".
     */
    size_t write(const void * const buf, const size_t buf_size);

    /** \brief Write a character. */
    inline int put(const char ch) { return putc(ch, file_); }
    
    inline void putback(const char ch) {
        if (unlikely(pushed_back_count_ == sizeof(pushed_back_chars_)))
            throw std::runtime_error("in File::putback: can't push back " + std::to_string(sizeof(pushed_back_chars_))
                                     + " characters in a row!");
        for (unsigned i(pushed_back_count_); i > 0; --i)
            pushed_back_chars_[i] = pushed_back_chars_[i - 1];
        pushed_back_chars_[0] = ch;
        ++pushed_back_count_;
    }

    inline int peek() {
        if (unlikely(pushed_back_count_ > 0))
            return pushed_back_chars_[0];
        const int ch(get());
        if (likely(ch != EOF))
            putback(static_cast<char>(ch));
        return ch;
    }

    /** \brief    Extracts a "line" from an input stream.
     *  \param    line        The extracted "line" after the call.
     *  \param    terminator  The line terminator.  (Will not be included in "line".)
     *  \return   The number of extracted characters not including a possible terminator.
     *  \warning  The caller has to test for EOF separately, for example with the eof() member function!
     */
    size_t getline(std::string * const line, const char terminator = '\n');

    /** \brief    Extracts a "line" from an input stream.
     *  \param    terminator  The line terminator.  (Will not be included in "line".)
     *  \return   The line as an std::string
     *  \warning  The caller has to test for EOF separately, for example with the eof() member function!
     */
    inline std::string getline(const char terminator = '\n') {
        std::string line;
        getline(&line, terminator);
        return line;
    }
    
    const std::string &getPath() const { return filename_; }

    /** Returns a File's size in bytes. */
    off_t size() const;

    inline bool eof() const { return (buffer_ptr_ == buffer_ + read_count_) and std::feof(file_) != 0; }
    inline bool anErrorOccurred() const { return std::ferror(file_) != 0; }

    /** Will the next I/O operation fail? */
    inline bool fail() const { return file_ == nullptr or eof() or std::ferror(file_) != 0; }
    
    inline bool operator!() const { return fail(); }

    File &operator<<(const char * const s);
    File &operator<<(const std::string &s) { return operator<<(s.c_str()); }
    File &operator<<(const char ch);
    File &operator<<(const int i);
    File &operator<<(const unsigned u);
    File &operator<<(const long l);
    File &operator<<(const unsigned long ul);
    File &operator<<(const long long ll);
    File &operator<<(const unsigned long long ull);
    File &operator<<(const double d);
    File &operator<<(File &(*f)(File &)) { return f(*this); } // Supports I/O manipulators taking 0 arguments.

    /** Resets the file pointer to the beginning of the file. */
    inline void rewind() {
        if (unlikely(file_ == nullptr))
            throw std::runtime_error("in File::rewind: can't rewind a non-open file!");
        std::rewind(file_);
        if (open_mode_ != WRITING) {
            read_count_ = 0;
            buffer_ptr_ = buffer_;
        }
    }

    /** \brief  Flush all internal I/O buffers.
     *  \return True on success and false on failure.  Sets errno if there is a failure. */
    inline bool flush() const { return std::fflush(file_) == 0; }

    /** Appends the contents of the file corresponding to "fd" to the current File. (Maintains "fd"'s original
        offset.) */
    bool append(const int fd);

    bool append(const File &file);

    /** \brief  Change the length a file.
     *  \param  new_length  The length to which the file will be truncated if it was previously longer, or the length
     *                      to which the file will be extended if it was previously shorter.
     *  \return True if the operation succeeded, else false.  In case of an error, "errno" will be set to the
     *          approriate error code.
     */
    bool truncate(const off_t new_length = 0);

    static File &endl(File &f) { f.put('\n'), f.flush(); return f; }
    static SingleArgManipulator<int> setprecision(int new_precision) {
        return SingleArgManipulator<int>(SetPrecision, new_precision); }
private:
    void fillBuffer();
    static File &SetPrecision(File &f, int new_precision) { f.precision_ = new_precision; return f; }
};


#endif // ifndef FILE_H

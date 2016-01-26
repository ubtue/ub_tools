/** \file    File.h
 *  \brief   Declaration of class File.
 *  \author  Dr. Johannes Ruscheinski
 */

/*
 *  Copyright 2005-2008 Project iVia.
 *  Copyright 2005-2008 The Regents of The University of California.
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


#include <map>
#include <stdexcept>
#include <sstream>
#include <string>
#include <climits>
#include <cstdio>
#include <aio.h>
#include <sys/types.h>
#include "Compiler.h"
#include "ThreadUtil.h"


/** \class  File
 *  \brief  Implements a class representing a buffered file.
 *  \note   The initial reason that this class exists is that the C++ standard stream classes in libstdc++ cannot
 *          handle handle 64 bit offsets and therefore files larger than 2GB on 32 bit platforms.  After implementing
 *          this class we also discovered that it provides for significantly better I/O performance than the stream
 *          classes in libstdc++.
 */
class File {
    FILE *file_;
    std::string path_;
    std::string fifo_path_;
    std::string mode_;
    aiocb read_control_block_, write_control_block_;
    int precision_;
    typedef void (*Callback)(File &file, void * const user_data);
    std::map<Callback, void * const> at_close_function_to_userdata_map_;
    static ThreadUtil::ThreadSafeCounter<unsigned> next_unique_id_;
    unsigned unique_id_;
    bool delete_on_close_;
public:
    template<typename ArgType> struct SingleArgManipulator {
	File &(*func_)(File &file, ArgType arg);
	ArgType arg_;
    public:
	SingleArgManipulator(File &(*func)(File &file, ArgType arg), ArgType arg): func_(func), arg_(arg) { }
    };

    class const_iterator {
	friend class File;
	FILE * const file_;
	char current_char_, last_char_;
	bool eof_;
    public:
	const char *operator->() const { return &current_char_; }
	const char &operator*() const { return current_char_; }
	const char *operator++();
	const char *operator++(int);
	bool operator==(const const_iterator &rhs) const { return eof_ == true and eof_ == rhs.eof_; }
	bool operator!=(const const_iterator &rhs) const { return not operator==(rhs); }
    private:
	explicit const_iterator(FILE * const file);
    };
    enum ThrowOnOpenBehaviour { THROW_ON_ERROR, DO_NOT_THROW_ON_ERROR };
    enum DeleteOnCloseBehaviour { DO_NOT_DELETE_ON_CLOSE, DELETE_ON_CLOSE };
public:
    File(): file_(nullptr), precision_(6), delete_on_close_(false) { }

    /** \brief  Creates and initalises a File object.
     *  \param  path                       The pathname for the file (see fopen(3) for details).
     *  \param  mode                       The open mode (see fopen(3) for details).  An extension to the fopen modes
     *                                     are either "c" or "u".  "c" meaning "compress" can only be combined with "w"
     *                                     and "u" meaning "uncompress" with "r".  Using either flag makes seeking or
     *                                     rewinding impossible.
     *  \param  throw_on_error_behaviour   If true, any open failure will cause an exception to be thrown.  If not true
     *                                     you must use the fail() member function.
     *  \param  delete_on_close_behaviour  Determines if "path" will be deleted on close or not.
     *  \note   Any  created files will have mode S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH (0666), as modified
     *          by the process' umask value (see umask(2)).
     */
    File(const std::string &path, const std::string &mode,
	 const ThrowOnOpenBehaviour throw_on_error_behaviour = DO_NOT_THROW_ON_ERROR,
	 const DeleteOnCloseBehaviour delete_on_close_behaviour = DO_NOT_DELETE_ON_CLOSE);

    /** \brief  Create a File object from a file descriptor.
     *  \param  fd    A valid (open) file descriptor for the current process.
     *  \param  mode  The open mode (see fopen(3) for details).  Must be compatible, i.e. a subset of the mode for "fd."  If not
     *                specified the mode for "fd" will be used.
     */
    explicit File(const int fd, const std::string &mode = "");

    /** Destroy a File object. */
    virtual ~File();

    int getFileDescriptor() { return fileno(file_); }
    unsigned getUniqueId() const { return unique_id_; }

    FILE *getFilePointer() { return file_; }

    /** Returns the pathname of an open File or an empty string if the File is not open. */
    const std::string &getPath() const { return path_; }

    /** Returns a File's size in bytes. */
    off_t size() const;

    /** \brief  Closes the currently open file if any and reinitalises a File object.
     *  \param  filepath   The pathname for the file (see fopen(3) for details).
     *  \param  openmode   The open mode (see fopen(3) for details).
     *  \return False upon failure or true upon success.
     */
    bool reopen(const std::string &filepath = "", const std::string &openmode = "");

    /** Will the next I/O operation fail? */
    bool fail() const { return file_ == nullptr or anErrorOccurred() or eof(); }

    bool operator!() const { return fail(); }

    /** Returns true if we encountered EOF, otherwise false. */
    bool eof() const { return ::feof(file_); }

    bool operator()() const { return not eof(); }

    /** Returns true if an error occurred, otherwise false. */
    bool anErrorOccurred() const { return ::ferror(file_); }

    /** Clears the internal EOF and error indicators. */
    void clearError() { ::clearerr(file_); }

    /** Closes this File.  If this fails you may consult the global "errno" for the reason. */
    bool close();

    /** \brief  Flush all internal I/O buffers.
     *  \return True on success and false on failure.  Sets errno if there is a failure.
     */
    bool flush() const;

    /** See printf(3) for arguments and meaning of the return value. */
    int printf(const char * const format, ...) __attribute__((__format__(printf,2,3)));

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
    std::string getline(const char terminator = '\n');

    /** \brief  Write some data to a file.
     *  \param  buf       The data to write.
     *  \param  buf_size  How much data to write.
     *  \return Returns a short count if an error occurred, otherwise returns "buf_size".
     */
    size_t write(const void * const buf, const size_t buf_size);

    /** \brief  Serializes a std::string to a binary stream.
     *  \param  s       The string to serialize.
     *  \return True if the operation succeeded and false if there was some I/O error.
     */
    bool serialize(const std::string &s);

    /** \brief  Serializes a std::string to a binary stream.
     *  \param  s       The string to serialize.
     *  \return True if the operation succeeded and false if there was some I/O error.
     */
    bool serialize(const char * const s);

    /** \brief  Serializes a boolean variable to a binary stream.
     *  \param  b       The boolean variable to serialize.
     *  \return True if the operation succeeded and false if there was some I/O error.
     */
    bool serialize(const bool b);

    /** \brief  Serializes a number to a binary stream.
     *  \param  n       The number to serialize.
     *  \return True if the operation succeeded and false if there was some I/O error.
     */
    template <typename NumericType> bool serialize(const NumericType n) {
	if (unlikely(file_ == nullptr))
	    throw std::runtime_error("in File::serialize: can't serialize to a non-open file (4)!");
	return write(reinterpret_cast<const void *>(&n), sizeof n) == sizeof n;
    }

    /** Returns the next character from the input stream or EOF at the end of the input stream. */
    int get();

    /** \brief Write a character. */
    int put(const char ch) {
	return std::fputc(ch, file_);
    }

    void putback(const char ch);

    /** \brief  Read some data from a file.
     *  \param  buf       The data to read.
     *  \param  buf_size  How much data to read.
     *  \return Returns a short count if an error occurred or EOF was encountered, otherwise returns "buf_size".
     *  \note   On returning a short count you need to call either eof() or anErrorOccurred() in order to
     *          determine whether the short count is due to an error condition or EOF.
     */
    size_t read(void * const buf, const size_t buf_size);

    /** Looks at the next character in the input stream without actually removing from the stream. May return EOF. */
    int peek() const;

    /** Resets the file pointer to the beginning of the file. */
    void rewind() {
	if (unlikely(file_ == nullptr))
	    throw std::runtime_error("in File::rewind: can't rewind a non-open file!");
	std::rewind(file_);
    }

    /** \brief  Skips over input characters.
     *  \param  max_skip_count  Up to how many characters to skip.
     *  \param  delimiter       Last character to skip.
     *  \return The actual number of characters that were skipped.  May be zero if the stream is already at EOF.
     */
    uint64_t ignore(const uint64_t max_skip_count = 1, const int delimiter = EOF);

    /** \brief  Deserializes a std::string from a binary stream.
     *  \param  s      The string to deserialize.
     *  \return True if the operation succeeded and false if there was some I/O error.
     */
    bool deserialize(std::string * const s);

    /** \brief  Deserializes a boolean variable from a binary stream.
     *  \param  b      The boolean variable to deserialize.
     *  \return True if the operation succeeded and false if there was some I/O error.
     */
    bool deserialize(bool * const b);

    /** \brief  Deserializes a number from a binary stream.
     *  \param  n      The number to deserialize.
     *  \return True if the operation succeeded and false if there was some I/O error.
     */
    template <typename NumericType> bool deserialize(NumericType * const n) {
	if (unlikely(file_ == nullptr))
	    throw std::runtime_error("in File::deserialize: can't read from a non-open file (4)!");
	return read(reinterpret_cast<void *>(n), sizeof *n) == sizeof *n;
    }

    /** \brief  Set the file pointer for the next I/O operation.
     *  \param  offset  Signed offset relative to the reference point specified by "whence".
     *  \param  whence  SEEK_SET, SEEK_END, or SEEK_CUR.
     *  \return True upon success, else false.  If false, you can consult the global "errno" variable for the type
     *          of error encountered.
     */
    bool seek(const off_t offset, const int whence = SEEK_SET);

    /* See ftello(3) for parameter descriptions and behaviour. */
    off_t tell() const;

    /** \brief  Change the length a file.
     *  \param  new_length  The length to which the file will be truncated if it was previously longer, or the length to
     *                      which the file will be extended if it was previously shorter.
     *  \return True if the operation succeeded, else false.  In case of an error, "errno" will be set to the approriate
     *          error code.
     */
    bool truncate(const off_t new_length = 0);

    /** Appends the contents of the file corresponding to "fd" to the current File. (Maintains "fd"'s original offset.) */
    bool append(const int fd);

    bool append(const File &file);

    /** \brief  Registers a function to be called just before closing the current File object.
     *  \param  new_callback  The function to be called.
     *  \param  user_data     The (optional) data to passed into "new_callback" when it will be called.
     */
    void registerOnCloseCallback(void (*new_callback)(File &file, void * const user_data), void * const user_data = nullptr);

    /** \brief  Creates a read-only memory-map of a file.
     *  \return A pointer to the start of the memory map on success or nullptr on failure.
     */
    const char *getReadOnlyMmap() const;

    /** \brief  Requests an asynchronous I/O read request.
     *  \param  buf       The data to read.
     *  \param  buf_size  How much data to read.
     *  \param  offset    Where to start reading relative to the beginning of the file.
     *  \return If true, the request has been successfully enqueued, if false, please consult errno(3).  The possible
     *          error codes are described in aio_read(3).
     *  \note   After successfully calling this function you need to eventually follow up on it by exactly one call to
     *          waitOnAsyncReadCompletion() that returns true.  Any number of calls to waitOnAsyncReadCompletion() that
     *          return false before it returns true are valid.
     */
    bool startAsyncRead(void * const buf, const size_t buf_size, const off_t offset);

    /** \brief  Waits for a previously issued asynchronous I/O read request to finish.
     *  \param  timeout  How long to wait until we give up.  UINT_MAX stands for infinity.  0 means poll and return immediately.
     *                   Any other value is considered to be a timeout in milliseconds.
     *  \param  nbytes   If the function returns true, the actual number of bytes that was transferred.
     *  \return True if the request completed.  False if an error occurred, e.g. a timeout.  Check errno in the error case and see
     *          aio_suspend(3) for an explanation of the error codes.
     *  \note   Only one request may be outstanding per File.
     */
    bool waitOnAsyncReadCompletion(size_t * const nbytes, const unsigned timeout = UINT_MAX);

    /** \brief   Cancels an outstanding asynchronous read request.
     *  \return  True if the request has been cancelled and false if the request could not be cancelled.
     *  \warning You must only call this after a previous call to startAsyncRead().
     *  \warning If this function returns true you must not call waitOnAsyncReadCompletion().
     *  \note    If an error occurs an exception will be thrown.
     */
    bool cancelAsyncReadRequest();

    /** \brief  Requests an asynchronous I/O write request.
     *  \param  buf       The data to write.
     *  \param  buf_size  How much data to write.
     *  \param  offset    Where to start writing relative to the beginning of the file.
     *  \return If true, the request has been successfully enqueued, if false, please consult errno(3).  The possible
     *          error codes are described in aio_write(3).
     *  \note   Only one request may be outstanding per File.  If the file has been opened in append mode, the data
     *          will be written to the end of the file regardless of the provided offset!
     *  \note   After successfully calling this function you need to eventually follow up on it by exactly one call to
     *          waitOnAsyncReadCompletion() that returns true.  Any number of calls to waitOnAsyncReadCompletion()
     *          that return false before it returns true are valid.
     */
    bool startAsyncWrite(const void * const buf, const size_t buf_size, const off_t offset);

    /** \brief  Waits for a previously issued asynchronous I/O write request to finish.
     *  \param  timeout  How long to wait until we give up.  UINT_MAX stands for infinity.  0 means poll and return
     *                   immediately.  Any other value is considered to be a timeout in milliseconds.
     *  \param  nbytes   If the function returns true, the actual number of bytes that was transferred.
     *  \return True if the request completed.  False if an error occurred, e.g. a timeout.  Check errno in the error case and
     *          see aio_suspend(3) for an explanation of the error codes.
     */
    bool waitOnAsyncWriteCompletion(size_t * const nbytes, const unsigned timeout = UINT_MAX);

    /** \brief   Cancels an outstanding asynchronous write request.
     *  \return  True if the request has been cancelled and false if the request could not be cancelled.
     *  \warning You must only call this after a previous call to startAsyncWrite().
     *  \warning If this function returns true you must not call waitOnAsyncWriteCompletion().
     *  \note    If an error occurs an exception will be thrown.
     */
    bool cancelAsyncWriteRequest();

    enum LockType { SHARED_READ, EXCLUSIVE_WRITE };

    /** \brief  Specify a lock for a region of the current file.
     *  \param  lock_type         Either a shared lock (for reading) or an exclusive lock.
     *  \param  whence            One of SEEK_SET ("start" and "length" are relative to the beginning of the file),
     *                            SEEK_CUR ("start" and "length" are relative to the current position), or SEEK_END,
     *                            ("start" and "length" are relative to the end of the file).
     *  \param  start             Start of the range of bytes to be locked.
     *  \param  length            Length of the range of bytes to be locked.  The special value "0" indicates to lock
     *                            everything starting at "start" to the end of the file and beyond to wherever it may
     *                            grow.  This parameter may be negative thereby specifying a region ending at
     *                            "start."
     *  \note                     This call will block until we gain access and the lock succeeds.  An illegal operation
     *                            will result in an exception being thrown.  Multiple calls to this function lead to
     *                            coalescing of specified regions.
     *                            Any held locks will be released when the file is closed.
     */
    void lock(const LockType lock_type, const short whence = SEEK_SET, const off_t start = 0, const off_t length = 0);

    /** \brief  Attempt to specify a lock for a region of the current file.
     *  \param  lock_type         Either a shared lock (for reading) or an exclusive lock.
     *  \param  whence            One of SEEK_SET ("start" and "length" are relative to the beginning of the file),
     *                            SEEK_CUR ("start" and "length" are relative to the current position), or SEEK_END,
     *                            ("start" and "length" are relative to the end of the file).
     *  \param  start             Start of the range of bytes to be locked.
     *  \param  length            Length of the range of bytes to be locked.  The special value "0" indicates to lock
     *                            everything starting at "start" to the end of the file and beyond to wherever it may
     *                            grow.  This parameter may be negative thereby specifying a region ending at "start."
     *  \return                   True if we aquired the lock and false if we failed (meaning some other process is holding it).
     *  \note                     An illegal operation will result in an exception being thrown.  Any held locks will
     *                            be released when the file is closed.
     */
    bool tryLock(const LockType lock_type, const short whence = SEEK_SET, const off_t start = 0, const off_t length = 0);

    /** \brief  Unlocks a region of the current file.
     *  \param  whence  One of SEEK_SET ("start" and "length" are relative to the beginning of the file), SEEK_CUR
     *                  ("start" and "length" are relative to the current position), or SEEK_END, ("start" and
     *                  "length" are relative to the end of the file).
     *  \param  start   Start of the range of bytes to be unlocked.
     *  \param  length  Length of the range of bytes to be unlocked.  The special value "0" indicates to unlock
     *                  everything starting at "start" to the end of the file and beyond to wherever it may grow
     *                  This parameter may be negative thereby specifying a region ending at "start."
     *  \note           An illegal operation will result in an exception being thrown.  The default argument
     *                  values result in all regions of a file being unlocked.  So if you want to remove all
     *                  previously set locks just call file.unlock().
     */
    void unlock(const short whence = SEEK_SET, const off_t start = 0, const off_t length = 0);

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


    File &appendNewlineAndFlush();

    const_iterator begin() const;
    const_iterator end() const;

    static File &endl(File &f) { return f.appendNewlineAndFlush(); }
    static SingleArgManipulator<int> setprecision(int new_precision) {
	return SingleArgManipulator<int>(SetPrecision, new_precision); }
private:
    File(const File &rhs) = delete;
    const File operator=(const File &rhs) = delete;

    /** Helper function for both waitOnAsyncReadCompletion() and waitOnAsyncWriteCompletion(). */
    bool waitOnAsyncCompletion(aiocb * const control_block_ptr, const unsigned timeout, size_t * const nbytes);

    /** Helper function for both cancelAsyncReadRequest() and cancelAsyncWriteRequest(). */
    bool cancelAsyncRequest(aiocb * const control_block_ptr);

    static File &SetPrecision(File &f, int new_precision) { f.precision_ = new_precision; return f; }
};


template<typename ArgType> inline File &operator<<(File &file, const File::SingleArgManipulator<ArgType> &manipulator)
    { return manipulator.func_(file, manipulator.arg_); }


#endif // ifndef FILE_H

/** \file    File.cc
 *  \brief   Implementation of class File.
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

#include "File.h"
#include <stdexcept>
#include <cerrno>
#include <cstdarg>
#include <cstring>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include "FileUtil.h"
#include "ExecUtil.h"
#include "TimeUtil.h"


ThreadUtil::ThreadSafeCounter<unsigned> File::next_unique_id_;


const char *File::const_iterator::operator++() {
    const int ch(std::getc(file_));
    if (ch == EOF) {
	eof_ = true;
	current_char_ = '\0';
    } else
	current_char_ = static_cast<char>(ch);

    return &current_char_;
}


const char *File::const_iterator::operator++(int) {
    last_char_ = current_char_;
    const int ch(std::getc(file_));
    if (ch == EOF) {
	eof_ = true;
	current_char_ = '\0';
    } else
	current_char_ = static_cast<char>(ch);

    return &last_char_;
}


File::const_iterator::const_iterator(FILE *file)
    : file_(file)
{
    if (file == nullptr) {
	eof_ = true;
	current_char_ = EOF;
    } else {
	const int ch(std::getc(file_));
	eof_ = ch == EOF;
	current_char_ = static_cast<char>(ch);
    }
}


static const std::string GZIP_PATH("/bin/gzip");
static const std::string GUNZIP_PATH("/bin/gunzip");


File::File(const std::string &path, const std::string &mode, const ThrowOnOpenBehaviour throw_on_error_behaviour,
	   const DeleteOnCloseBehaviour delete_on_close_behaviour)
    : precision_(6), unique_id_(next_unique_id_++), delete_on_close_(delete_on_close_behaviour == DELETE_ON_CLOSE)
{
    file_ = nullptr;

    // Deal w/ decompression piping the output through a FIFO to a process?
    if (std::strchr(mode.c_str(), 'u') != nullptr) {
	if (unlikely(std::strchr(mode.c_str(), 'r') == nullptr)) {
	    if (throw_on_error_behaviour == THROW_ON_ERROR)
		throw std::runtime_error("in File::File: open mode contains a 'u' but no 'r'!");
	} else if (unlikely(std::strchr(mode.c_str(), 'm') != nullptr)) {
	    if (throw_on_error_behaviour != THROW_ON_ERROR)\
		return;
	    throw std::runtime_error("in File::File: open mode contains a 'u' and an 'm' which are incompatible!");
	} else {
	    // Remove the 'u'.
	    const size_t u_pos(mode.find('u'));
	    mode_ = mode.substr(0, u_pos) + mode.substr(u_pos + 1);

	    path_ = fifo_path_ = "/tmp/p" + std::to_string(::getpid()) + "t" + std::to_string(::pthread_self()) + "u"
		                 + std::to_string(unique_id_);
	    ::unlink(fifo_path_.c_str());
	    if (unlikely(::mkfifo(fifo_path_.c_str(), S_IRUSR | S_IWUSR) != 0)) {
		if (throw_on_error_behaviour != THROW_ON_ERROR)
		    return;
		throw std::runtime_error("in File::File: mkfifo(3) of \"" + fifo_path_ + "\" failed! ("
					 + std::string(::strerror(errno)) + ")");
	    }

	    // Create a process that reads the FIFO and decompresses the data:
	    if (ExecUtil::Spawn(GUNZIP_PATH, { }, /* new_stdin = */ path, /* new_stdout = */ fifo_path_) < 0) {
		if (throw_on_error_behaviour != THROW_ON_ERROR)\
		    return;
		throw std::runtime_error("in File::File: failed to spawn \"" + GUNZIP_PATH +"\"!");
	    }

	    file_ = ::fopen(fifo_path_.c_str(), mode_.c_str());
	}

    // Deal w/ compression piping the output through a FIFO to a process?
    } else if (std::strchr(mode.c_str(), 'c') != nullptr) {
	if (unlikely(std::strchr(mode.c_str(), 'w') == nullptr)) {
	    if (throw_on_error_behaviour == THROW_ON_ERROR)
		throw std::runtime_error("in File::File: open mode contains a 'c' but no 'w'!");
	} else if (unlikely(std::strchr(mode.c_str(), 'm') != nullptr)) {
	    if (throw_on_error_behaviour != THROW_ON_ERROR)\
		return;
	    throw std::runtime_error("in File::File: open mode contains a 'c' and an 'm' which are incompatible!");
	} else {
	    // Remove the 'c':
	    const size_t c_pos(mode.find('c'));
	    mode_ = mode.substr(0, c_pos) + mode.substr(c_pos + 1);

	    path_ = fifo_path_ = "/tmp/p" + std::to_string(::getpid()) + "t" + std::to_string(::pthread_self()) + "u"
		                 + std::to_string(unique_id_);
	    ::unlink(fifo_path_.c_str());
	    if (unlikely(::mkfifo(fifo_path_.c_str(), S_IRUSR | S_IWUSR) != 0)) {
		if (throw_on_error_behaviour != THROW_ON_ERROR)
		    return;
		throw std::runtime_error("in File::File: mkfifo(3) of \"" + fifo_path_ + "\" failed! ("
					 + std::string(::strerror(errno)) + ")");
	    }

	    // Create a process that reads the FIFO and compresses the data:
	    if (ExecUtil::Spawn(GZIP_PATH, { "-9" }, /* new_stdin = */ fifo_path_, /* new_stdout = */ path) < 1) {
		if (throw_on_error_behaviour != THROW_ON_ERROR)\
		    return;
		throw std::runtime_error("in File::File: failed to spawn \"" + GUNZIP_PATH +"\"!");
	    }

	    file_ = ::fopen(fifo_path_.c_str(), mode_.c_str());
	}

    // Neither compression nor decompression:
    } else {
	file_ = ::fopen(path.c_str(), mode.c_str());
	path_ = path;
	mode_ = mode;
    }

    if (unlikely(file_ == nullptr) and throw_on_error_behaviour == THROW_ON_ERROR)
	throw std::runtime_error("in File::File: fopen(3) on \"" + std::string(path) + "\" with mode \"" + mode
				 + "\" failed (" + std::string(::strerror(errno)) + ") (1)!");
}


File::File(const int fd, const std::string &mode)
	: path_("/proc/self/fd/" + std::to_string(fd)), precision_(6), unique_id_(next_unique_id_++), delete_on_close_(false)
{
    if (not mode.empty())
	mode_ = mode + "m";
    else {
	// Determine the mode from "fd":
	int flags;
	if (unlikely((flags = ::fcntl(fd, F_GETFL, &flags)) == -1))
	    throw std::runtime_error("in File::File: fcntl(2) failed (" + std::string(::strerror(errno)) + ")!");
	flags &= O_ACCMODE;
	if (flags == O_RDONLY)
	    mode_ = "rm";
	else if (flags == O_WRONLY)
	    mode_ = "wm";
	else // We assume flags == O_RDWR.
	    mode_ = "r+m";

    }

    file_ = ::fdopen(fd, mode_.c_str());
    if (unlikely(file_ == nullptr))
	throw std::runtime_error("in File::File: fdopen(3) on \"" + std::to_string(fd) + "\" with mode \""
				 + mode_ + "\" failed (" + std::string(::strerror(errno)) + ") (3)!");
}


File::~File() {
    close();
    if (not fifo_path_.empty())
	::unlink(fifo_path_.c_str());
}


off_t File::size() const {
    if (unlikely(file_ == nullptr))
	throw std::runtime_error("in File::size: can't obtain the size of non-open File \"" + path_
				 + "\"!");

    struct stat stat_buf;
    if (unlikely(::fstat(fileno(file_), &stat_buf) == -1))
	throw std::runtime_error("in File::size: fstat(2) failed on \"" + path_ + "\" ("
				 + std::string(::strerror(errno)) + ")!");

    return stat_buf.st_size;
}


bool File::reopen(const std::string &filepath, const std::string &openmode) {
    // need some non-const values
    std::string path(filepath);
    std::string mode(openmode + "m");

    // Allow a reopen without arguments. If "" specified, reuse the previous values
    if (path.empty())
	path = path_;

    if (mode.empty())
	mode = mode_;

    if (file_ == nullptr)
	file_ = ::fopen(path.c_str(), mode.c_str());
    else
	file_ = ::freopen(path.c_str(), mode.c_str(), file_);

    if (unlikely(file_ == nullptr)) {
	path_.clear();
	mode_.clear();
	return false;
    } else {
	path_ = path;
	mode_ = mode;
	return true;
    }
}


bool File::close() {
    if (file_ == nullptr)
	return false;
    else {
	// Call all registered on-close callbacks:
	for (std::map<Callback, void * const>::const_iterator function_and_user_data(at_close_function_to_userdata_map_.begin());
	     function_and_user_data != at_close_function_to_userdata_map_.end(); ++function_and_user_data)
	    (*function_and_user_data->first)(*this, function_and_user_data->second);

	const bool retval = ::fclose(file_) == 0;
	file_ = nullptr;

	if (delete_on_close_)
	    ::unlink(path_.c_str());

	path_.clear();
	mode_.clear();

	return retval;
    }
}


bool File::flush() const {
    if (unlikely(file_ == nullptr))
	throw std::runtime_error("in File::flush: can't flush non-open file \"" + path_ + "\"!");

    return ::fflush(file_) == 0;
}


int File::printf(const char * const format, ...) {
    va_list arg_ptr;
    va_start(arg_ptr, format);
    const int retcode = std::vfprintf(file_, format, arg_ptr);
    va_end(arg_ptr);

    return retcode;
}


size_t File::getline(std::string * const line, const char terminator) {
    line->clear();

    size_t count(0);
    for (;;) {
	const int ch(std::getc(file_));
	if (unlikely(ch == terminator or ch == EOF))
	    return count;
	*line += static_cast<char>(ch);
	++count;
    }
}


std::string File::getline(const char terminator) {
    std::string line;
    getline(&line, terminator);
    return line;
}


size_t File::write(const void * const buf, const size_t buf_size) {
    if (unlikely(file_ == nullptr))
	throw std::runtime_error("in File::write: can't write to non-open file (1) \"" + path_ + "\"!");

    return ::fwrite(buf, 1, buf_size, file_);
}


bool File::serialize(const std::string &s) {
    if (unlikely(file_ == nullptr))
	throw std::runtime_error("in File::serialize: can't serialize to non-open file (2) \"" + path_
				 + "\"!");

    const std::string::size_type length(s.size());
    if (write(reinterpret_cast<const char *>(&length), sizeof length) != sizeof length)
	return false;

    if (write(s.c_str(), length) != length)
	return false;

    return true;
}


bool File::serialize(const char * const s) {
    if (unlikely(file_ == nullptr))
	throw std::runtime_error("in File::serialize: can't serialize to non-open file (3) \"" + path_
				 + "\"!");

    const size_t length(std::strlen(s));
    if (write(reinterpret_cast<const char *>(&length), sizeof length) != sizeof length)
	return false;

    if (write(s, length) != length)
	return false;

    return true;
}


bool File::serialize(const bool b) {
    if (unlikely(file_ == nullptr))
	throw std::runtime_error("in File::serialize: can't serialize to non-open file (4) \"" + path_
				 + "\"!");

    return write(reinterpret_cast<const char *>(&b), sizeof b) == sizeof b;
}


int File::get() {
    if (unlikely(file_ == nullptr))
	throw std::runtime_error("in File::get: can't read from a non-open file \"" + path_ + "\"!");
    return std::fgetc(file_);
}


void File::putback(const char ch) {
    if (unlikely(std::ungetc(ch, file_) == EOF))
	throw std::runtime_error("in File::putback: failed to push back a character ("
				 + std::to_string(static_cast<unsigned>(ch)) + ") into an input stream!");
}


size_t File::read(void * const buf, const size_t buf_size) {
    if (unlikely(file_ == nullptr))
	throw std::runtime_error("in File::read: can't read from non-open file (1) \"" + path_ + "\"!");

    return ::fread(buf, 1, buf_size, file_);
}


int File::peek() const {
    const int ch(std::fgetc(file_));
    std::ungetc(ch, file_);

    return ch;
}


uint64_t File::ignore(const uint64_t max_skip_count, const int delimiter) {
    uint64_t skipped_count(0);
    for (;;) {
	const int ch(std::fgetc(file_));
	if (ch == EOF)
	    return skipped_count;
	++skipped_count;
	if (ch == delimiter or skipped_count == max_skip_count)
	    return skipped_count;
    }
}


bool File::deserialize(std::string * const s) {
    if (unlikely(file_ == nullptr))
	throw std::runtime_error("in File::deserialize: can't deserialize from non-open file (2) \"" + path_
				 + "\"!");

    std::string::size_type length;
    if (read(reinterpret_cast<void *>(&length), sizeof length) != sizeof length)
	return false;

    #pragma GCC diagnostic ignored "-Wvla"
    char buf[length];
    #pragma GCC diagnostic warning "-Wvla"
    if (read(buf, length) != length)
	return false;

    *s = std::string(buf, length);

    return true;
}


bool File::deserialize(bool * const b) {
    if (unlikely(file_ == nullptr))
	throw std::runtime_error("in File::deserialize: can't deserialize from non-open file (3) \"" + path_
				 + "\"!");

    return read(reinterpret_cast<char *>(b), sizeof *b) == sizeof *b;
}


bool File::seek(const off_t offset, const int whence) {
    if (unlikely(file_ == nullptr))
	throw std::runtime_error("in File::seek: can't seek on non-open file \"" + path_ + "\"!");
    if (unlikely(not fifo_path_.empty()))
	throw std::runtime_error("in File::seek: can't seek on a file that was opened with a 'u' or 'c' mode!");

    return ::fseeko(file_, offset, whence) == 0;
}


off_t File::tell() const {
    if (unlikely(file_ == nullptr))
	throw std::runtime_error("in File::tell: can't get a file offset on non-open file \"" + path_
				 + "\"!");

    return ::ftello(file_);
}


bool File::truncate(const off_t new_length) {
    if (unlikely(file_ == nullptr))
	throw std::runtime_error("in File::setNewSize: can't get non-open file's size \"" + path_ + "\"!");

    ::fflush(file_);
    return ::ftruncate(fileno(file_), new_length) == 0;
}


bool File::append(const int fd) {
    const off_t original_offset(::lseek(fd, 0, SEEK_CUR));
    if (unlikely(not FileUtil::Rewind(fd)))
	return false;

    flush();
    const int target_fd(fileno(file_));
    char buf[BUFSIZ];
    ssize_t read_count;
    errno = 0;
    while ((read_count = ::read(fd, buf, sizeof(buf))) > 0) {
	if (unlikely(::write(target_fd, buf, read_count) != read_count)) {
	    ::lseek(fd, original_offset, SEEK_SET);
	    return false;
	}
    }

    ::lseek(fd, original_offset, SEEK_SET);
    return errno == 0 ? true : false;
}


bool File::append(const File &file) {
    if (unlikely(not file.flush()))
	return false;
    return append(fileno(file.file_));
}


void File::registerOnCloseCallback(void (*new_callback)(File &file, void * const user_data), void * const user_data) {
    std::map<Callback, void * const>::iterator at_close_function_and_userdata(
        at_close_function_to_userdata_map_.find(new_callback));
    if (at_close_function_and_userdata == at_close_function_to_userdata_map_.end())
	at_close_function_to_userdata_map_.insert(std::make_pair(new_callback, user_data));
    else
	*const_cast<void **>(&at_close_function_and_userdata->second) = user_data;
}


const char *File::getReadOnlyMmap() const {
    struct stat stat_buf;
    if (unlikely(::fstat(fileno(file_), &stat_buf) == -1))
	return nullptr;
    const char *data = reinterpret_cast<const char *>(::mmap(0, stat_buf.st_size, PROT_READ, MAP_SHARED, fileno(file_), 0));
    return data == MAP_FAILED ? nullptr : data;
}


bool File::startAsyncRead(void * const buf, const size_t buf_size, const off_t offset) {
    flush(); // FILE* might have not put to disk yet
    std::memset(&read_control_block_, '\0', sizeof read_control_block_);
    read_control_block_.aio_fildes = fileno(file_);
    read_control_block_.aio_buf    = buf;
    read_control_block_.aio_nbytes = buf_size;
    read_control_block_.aio_offset = offset;

    return ::aio_read(&read_control_block_) == 0;
}


bool File::waitOnAsyncReadCompletion(size_t * const nbytes, const unsigned timeout) {
    aiocb * const control_block_ptr(&read_control_block_);
    return waitOnAsyncCompletion(control_block_ptr, timeout, nbytes);
}


bool File::cancelAsyncReadRequest() {
    aiocb * const control_block_ptr(&read_control_block_);
    return cancelAsyncRequest(control_block_ptr);
}


bool File::startAsyncWrite(const void * const buf, const size_t buf_size, const off_t offset) {
    flush(); // FILE* might have not put to disk yet
    std::memset(&write_control_block_, '\0', sizeof write_control_block_);
    write_control_block_.aio_fildes = fileno(file_);
    write_control_block_.aio_buf    = const_cast<void * const>(buf);
    write_control_block_.aio_nbytes = buf_size;
    write_control_block_.aio_offset = offset;

    return ::aio_write(&write_control_block_) == 0;
}


bool File::waitOnAsyncWriteCompletion(size_t * const nbytes, const unsigned timeout) {
    aiocb * const control_block_ptr(&write_control_block_);
    return waitOnAsyncCompletion(control_block_ptr, timeout, nbytes);
}


bool File::cancelAsyncWriteRequest() {
    aiocb * const control_block_ptr(&write_control_block_);
    return cancelAsyncRequest(control_block_ptr);
}


void File::lock(const LockType lock_type, const short whence, const off_t start, const off_t length) {
    struct flock lock_params;
    lock_params.l_type   = static_cast<short>((lock_type == SHARED_READ) ? F_RDLCK : F_WRLCK);
    lock_params.l_whence = whence;
    lock_params.l_start  = start;
    lock_params.l_len    = length;

    if (unlikely(::fcntl(fileno(file_), F_SETLKW, &lock_params) == -1))
	throw std::runtime_error("in File::lock: fcntl(2) failed (" + std::string(::strerror(errno)) + ")!");
}


bool File::tryLock(const LockType lock_type, const short whence, const off_t start, const off_t length) {
    struct flock lock_params;
    lock_params.l_type   = static_cast<short>((lock_type == SHARED_READ) ? F_RDLCK : F_WRLCK);
    lock_params.l_whence = whence;
    lock_params.l_start  = start;
    lock_params.l_len    = length;

    if (unlikely(::fcntl(fileno(file_), F_SETLK, &lock_params) != -1))
	return true;

    // Lock held by another process?
    if (errno == EACCES or errno == EAGAIN)
	return false;

    throw std::runtime_error("in File::tryLock: fcntl(2) failed (" + std::string(::strerror(errno)) + ")!");
}


void File::unlock(const short whence, const off_t start, const off_t length) {
    struct flock lock_params;
    lock_params.l_type   = F_UNLCK;
    lock_params.l_whence = whence;
    lock_params.l_start  = start;
    lock_params.l_len    = length;

    if (unlikely(::fcntl(fileno(file_), F_SETLK, &lock_params) == -1))
	throw std::runtime_error("in File::unlock: fcntl(2) failed (" + std::string(::strerror(errno)) + ")!");
}


File &File::operator<<(const char * const s) {
    std::fputs(s, file_);
    return *this;
}


File &File::operator<<(const char ch) {
    std::fputc(ch, file_);
    return *this;
}


File &File::operator<<(const int i) {
    std::fprintf(file_, "%1d", i);
    return *this;
}


File &File::operator<<(const unsigned u) {
    std::fprintf(file_, "%1u", u);
    return *this;
}


File &File::operator<<(const long l) {
    std::fprintf(file_, "%1ld", l);
    return *this;
}


File &File::operator<<(const unsigned long ul) {
    std::fprintf(file_, "%1lu", ul);
    return *this;
}


File &File::operator<<(const long long ll) {
    std::fprintf(file_, "%1lld", ll);
    return *this;
}


File &File::operator<<(const unsigned long long ull) {
    std::fprintf(file_, "%1llu", ull);
    return *this;
}


File &File::operator<<(const double d) {
    std::fprintf(file_, "%.*g", precision_, d);
    return *this;
}


File &File::appendNewlineAndFlush() {
    std::fputc('\n', file_);
    flush();
    return *this;
}


File::const_iterator File::begin() const {
    std::rewind(file_);
    return const_iterator(file_);
}


File::const_iterator File::end() const {
    return const_iterator(nullptr);
}


bool File::waitOnAsyncCompletion(aiocb * const control_block_ptr, const unsigned timeout, size_t * const nbytes) {
    bool success;

    // Wait forever?
    if (timeout == UINT_MAX)
	success = ::aio_suspend(&control_block_ptr, 1, nullptr) == 0;
    else {
	timespec time_spec;
	TimeUtil::MillisecondsToTimeSpec(timeout, &time_spec);
	success = ::aio_suspend(&control_block_ptr, 1, &time_spec) == 0;
    }

    // If we got the data, retrieve the actual number of bytes transferred:
    if (success) {
	errno = 0;
	const ssize_t retval(::aio_return(control_block_ptr));
	if (unlikely(errno != 0 or retval < 0))
	    throw std::runtime_error("in File::waitOnAsyncCompletion: aio_return(3) failed (return code: "
				     + std::to_string(retval) + ")!");
	*nbytes = static_cast<size_t>(retval);
    }

    return success;
}


bool File::cancelAsyncRequest(aiocb * const control_block_ptr) {
    switch (::aio_cancel(fileno(file_), control_block_ptr)) {
    case AIO_CANCELED:
	::aio_return(control_block_ptr);
	return true;
    case -1: // Some error occurred.
	throw std::runtime_error("in File::cancelAsyncRequest: aio_cancel(3) failed (" + std::string(::strerror(errno)) + ")!");
    default: // Request couldn't be cancelled.
	return false;
    }
}

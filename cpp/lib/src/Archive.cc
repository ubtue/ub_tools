/** \file   Archive.cc
 *  \brief  Implementations of the ArchiveReader and ArchiveWriter classes which are wrappers around libtar.
 *  \author Dr. Johannes Ruscheinski (johannes.ruscheinski@uni-tuebingen.de)
 *
 *  \copyright 2016-2017 Universitätsbibliothek Tübingen.  All rights reserved.
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU Affero General Public License as
 *  published by the Free Software Foundation, either version 3 of the
 *  License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Affero General Public License for more details.
 *
 *  You should have received a copy of the GNU Affero General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#include "Archive.h"
#include <stdexcept>
#include <cstring>
#include <archive_entry.h>
#include <fcntl.h>
#include "Compiler.h"
#include "File.h"
#include "StringUtil.h"
#include "util.h"


const std::string ArchiveReader::EntryInfo::getFilename() const {
    return ::archive_entry_pathname(archive_entry_);
}


bool ArchiveReader::EntryInfo::isRegularFile() const {
    return ::archive_entry_filetype(archive_entry_) == AE_IFREG;
}


bool ArchiveReader::EntryInfo::isDirectory() const {
    return ::archive_entry_filetype(archive_entry_) == AE_IFDIR;
}


const unsigned DEFAULT_BLOCKSIZE(10240);


ArchiveReader::ArchiveReader(const std::string &archive_file_name) {
    archive_handle_ = ::archive_read_new();
    ::archive_read_support_filter_all(archive_handle_);
    ::archive_read_support_format_all(archive_handle_);
    if (unlikely(::archive_read_open_filename(archive_handle_, archive_file_name.c_str(), DEFAULT_BLOCKSIZE)
                 != ARCHIVE_OK))
        throw std::runtime_error("in ArchiveReader::ArchiveReader: archive_read_open_filename(3) failed: "
                                 + std::string(::archive_error_string(archive_handle_)));
}


ArchiveReader::~ArchiveReader() {
    if (unlikely(::archive_read_free(archive_handle_) != ARCHIVE_OK))
        throw std::runtime_error("in ArchiveReader::~ArchiveReader: archive_read_free(3) failed!");
}


bool ArchiveReader::getNext(EntryInfo * const info) {
    int status;
    while ((status = ::archive_read_next_header(archive_handle_, &info->archive_entry_)) == ARCHIVE_RETRY)
        /* Intentionally empty! */;

    if (status == ARCHIVE_OK)
        return true;
    if (status == ARCHIVE_EOF)
        return false;

    throw std::runtime_error("in ArchiveReader::getNext: something went wrong! ("
                             + std::string(::archive_error_string(archive_handle_)) + ")");
}


ssize_t ArchiveReader::read(char * const buffer, const size_t size) {
    ssize_t retval;
    do
        retval = ::archive_read_data(archive_handle_, reinterpret_cast<void * const>(buffer), size);
    while (retval == ARCHIVE_RETRY);

    return (retval == ARCHIVE_FATAL or retval == ARCHIVE_WARN) ? -1 : retval;
}


bool ArchiveReader::extractEntry(const std::string &member_name, std::string output_filename) {
    if (output_filename.empty())
        output_filename = member_name;

    EntryInfo entry_info;
    while (getNext(&entry_info)) {
        if (entry_info.getFilename() != member_name)
            continue;

        if (entry_info.isDirectory())
            logger->error("in ArchiveReader::extractEntry: can't extract a directory!");

        const int to_fd(::open(output_filename.c_str(), O_WRONLY));
        if (unlikely(to_fd == -1))
            logger->error("in FileUtil::CopyOrDie: failed to open \"" + output_filename  + "\" for writing!");

        char buf[BUFSIZ];
        for (;;) {
            const ssize_t no_of_bytes(read(&buf[0], sizeof(buf)));
            if (no_of_bytes == 0) {
                ::close(to_fd);
                return true;
            }

            if (unlikely(no_of_bytes < 0))
                logger->error("in ArchiveReader::extractEntry: " + getLastErrorMessage());

            if (unlikely(::write(to_fd, &buf[0], no_of_bytes) != no_of_bytes))
                logger->error("in ArchiveReader::extractEntry: write(2) failed!");
        }

        ::close(to_fd);
    }
    
    return false;
}


ArchiveWriter::ArchiveWriter(const std::string &archive_file_name, const FileType file_type)
    : archive_entry_(nullptr)
{
    archive_handle_ = ::archive_write_new();

    switch (file_type) {
    case FileType::AUTO:
        if (StringUtil::EndsWith(archive_file_name, ".tar"))
            ::archive_write_set_format_pax_restricted(archive_handle_);
        else if (StringUtil::EndsWith(archive_file_name, ".tar.gz")) {
            ::archive_write_add_filter_gzip(archive_handle_);
            ::archive_write_set_format_pax_restricted(archive_handle_);
        } else
            throw std::runtime_error("in ArchiveWriter::ArchiveWriter: FileType::AUTO selected but,"
                                     " can't guess the file type from the given filename \"" + archive_file_name
                                     + "\"!");
        break;
    case FileType::TAR:
        ::archive_write_set_format_pax_restricted(archive_handle_);
        break;
    case FileType::GZIPPED_TAR:
        ::archive_write_add_filter_gzip(archive_handle_);
        ::archive_write_set_format_pax_restricted(archive_handle_);
        break;
    }

    if (unlikely(::archive_write_open_filename(archive_handle_, archive_file_name.c_str()) != ARCHIVE_OK))
        throw std::runtime_error("in ArchiveWriter::ArchiveWriter: archive_write_open_filename(3) failed: "
                                 + std::string(::archive_error_string(archive_handle_)));
}


ArchiveWriter::~ArchiveWriter() {
    if (archive_entry_ != nullptr)
        ::archive_entry_free(archive_entry_);

    if (unlikely(::archive_write_close(archive_handle_) != ARCHIVE_OK))
        throw std::runtime_error("in ArchiveWriter::~Archive!Writer: archive_write_close(3) failed: "
                                 + std::string(::archive_error_string(archive_handle_)));
    if (unlikely(::archive_write_free(archive_handle_) != ARCHIVE_OK))
        throw std::runtime_error("in ArchiveWriter::~Archive!Writer: archive_write_free(3) failed: "
                                 + std::string(::archive_error_string(archive_handle_)));
}


void ArchiveWriter::add(const std::string &filename, const std::string &archive_name) {
    struct stat stat_buf;
    if (unlikely(::stat(filename.c_str(), &stat_buf) != 0))
        throw std::runtime_error("in ArchiveWriter::add: stat(2) on \"" + filename + "\" failed: "
                                 + std::string(::strerror(errno)));

    if (archive_entry_ == nullptr)
        archive_entry_ = archive_entry_new();
    else
        ::archive_entry_clear(archive_entry_);
    ::archive_entry_set_pathname(archive_entry_, archive_name.empty() ? filename.c_str() : archive_name.c_str());
    ::archive_entry_copy_stat(archive_entry_, &stat_buf);

    int status;
    while ((status = ::archive_write_header(archive_handle_, archive_entry_)) == ARCHIVE_RETRY)
        /* Intentionally empty! */;
    if (unlikely(status != ARCHIVE_OK))
        throw std::runtime_error("in ArchiveWriter::add: archive_write_header(3) failed! ("
                                 + std::string(::archive_error_string(archive_handle_)));


    File input(filename, "r");
    char buffer[DEFAULT_BLOCKSIZE];
    size_t count;
    while ((count = input.read(buffer, DEFAULT_BLOCKSIZE)) > 0) {
        if (count < DEFAULT_BLOCKSIZE and input.anErrorOccurred())
            throw std::runtime_error("in ArchiveWriter::add: error reading \"" + filename + "\" !");
        if (unlikely(::archive_write_data(archive_handle_, buffer, count) != static_cast<const ssize_t>(count)))
            throw std::runtime_error("in ArchiveWriter::add: archive_write_data(3) failed: "
                                     + std::string(::archive_error_string(archive_handle_)));
    }
}

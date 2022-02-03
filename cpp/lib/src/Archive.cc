/** \file   Archive.cc
 *  \brief  Implementations of the archive processing functions and classes.
 *  \author Dr. Johannes Ruscheinski (johannes.ruscheinski@uni-tuebingen.de)
 *
 *  \copyright 2016-2020 Universitätsbibliothek Tübingen.  All rights reserved.
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
#include <cstring>
#include <fcntl.h>
#include "Compiler.h"
#include "File.h"
#include "FileUtil.h"
#include "StringUtil.h"
#include "util.h"


namespace Archive {


const std::string Reader::EntryInfo::getFilename() const {
    return ::archive_entry_pathname(archive_entry_);
}


bool Reader::EntryInfo::isRegularFile() const {
    return ::archive_entry_filetype(archive_entry_) == AE_IFREG;
}


bool Reader::EntryInfo::isDirectory() const {
    return ::archive_entry_filetype(archive_entry_) == AE_IFDIR;
}


const unsigned DEFAULT_BLOCKSIZE(10240);


Reader::Reader(const std::string &archive_file_name) {
    archive_handle_ = ::archive_read_new();
    ::archive_read_support_filter_all(archive_handle_);
    ::archive_read_support_format_all(archive_handle_);
    if (unlikely(::archive_read_open_filename(archive_handle_, archive_file_name.c_str(), DEFAULT_BLOCKSIZE) != ARCHIVE_OK))
        LOG_ERROR("archive_read_open_filename(3) failed: " + std::string(::archive_error_string(archive_handle_)));
}


Reader::~Reader() {
    if (unlikely(::archive_read_free(archive_handle_) != ARCHIVE_OK))
        LOG_ERROR("archive_read_free(3) failed!");
}


bool Reader::getNext(EntryInfo * const info) {
    int status;
    while ((status = ::archive_read_next_header(archive_handle_, &info->archive_entry_)) == ARCHIVE_RETRY)
        /* Intentionally empty! */;

    if (status == ARCHIVE_OK)
        return true;
    if (status == ARCHIVE_EOF)
        return false;

    LOG_ERROR("something went wrong! (" + std::string(::archive_error_string(archive_handle_)) + ")");
}


ssize_t Reader::read(char * const buffer, const size_t size) {
    ssize_t retval;
    do
        retval = ::archive_read_data(archive_handle_, reinterpret_cast<void *>(buffer), size);
    while (retval == ARCHIVE_RETRY);

    return (retval == ARCHIVE_FATAL or retval == ARCHIVE_WARN) ? -1 : retval;
}


bool Reader::extractEntry(const std::string &member_name, std::string output_filename) {
    if (output_filename.empty())
        output_filename = member_name;

    EntryInfo entry_info;
    while (getNext(&entry_info)) {
        if (entry_info.getFilename() != member_name)
            continue;

        if (entry_info.isDirectory())
            LOG_ERROR("an't extract a directory!");

        const int to_fd(::open(output_filename.c_str(), O_WRONLY | O_CREAT, 0600));
        if (unlikely(to_fd == -1))
            LOG_ERROR("failed to open \"" + output_filename + "\" for writing!");

        char buf[BUFSIZ];
        for (;;) {
            const ssize_t no_of_bytes(read(&buf[0], sizeof(buf)));
            if (no_of_bytes == 0) {
                ::close(to_fd);
                return true;
            }

            if (unlikely(no_of_bytes < 0))
                LOG_ERROR(getLastErrorMessage());

            if (unlikely(::write(to_fd, &buf[0], no_of_bytes) != no_of_bytes))
                LOG_ERROR("write(2) failed!");
        }

        ::close(to_fd);
    }

    return false;
}


Writer::Writer(const std::string &archive_file_name, const std::string &archive_write_options, const FileType file_type)
    : archive_entry_(nullptr), closed_(false) {
    archive_handle_ = ::archive_write_new();

    switch (file_type) {
    case FileType::AUTO:
        if (StringUtil::EndsWith(archive_file_name, ".tar")) {
            if (unlikely(not archive_write_options.empty()))
                LOG_ERROR("no write options are currently supported for the uncompressed tar format!");
            if (::archive_write_set_format_pax_restricted(archive_handle_) != ARCHIVE_OK)
                LOG_ERROR("failed to call archive_write_set_format_pax_restricted(3): "
                          + std::string(::archive_error_string(archive_handle_)));
        } else if (StringUtil::EndsWith(archive_file_name, ".tar.gz")) {
            if (::archive_write_add_filter_gzip(archive_handle_) != ARCHIVE_OK)
                LOG_ERROR("failed to call archive_write_add_filter_gzip(3): " + std::string(::archive_error_string(archive_handle_)));
            if (unlikely(::archive_write_set_options(archive_handle_, archive_write_options.c_str()) != ARCHIVE_OK))
                LOG_ERROR("failed to call archive_write_set_options(3) w/ \"" + archive_write_options + "\"! (1)");
            if (::archive_write_set_format_pax_restricted(archive_handle_) != ARCHIVE_OK)
                LOG_ERROR("failed to call archive_write_set_format_pax_restricted(3): "
                          + std::string(::archive_error_string(archive_handle_)) + " (2)");
        } else if (StringUtil::EndsWith(archive_file_name, ".zip")) {
            if (::archive_write_set_format_zip(archive_handle_) != ARCHIVE_OK)
                LOG_ERROR("failed to call archive_write_set_format_zip(3): " + std::string(::archive_error_string(archive_handle_)));
            if (unlikely(::archive_write_set_options(archive_handle_, archive_write_options.c_str()) != ARCHIVE_OK))
                LOG_ERROR("failed to call archive_write_set_options(3) w/ \"" + archive_write_options + "\"! (2)");
        } else
            LOG_ERROR(
                "FileType::AUTO selected but,"
                " can't guess the file type from the given filename \""
                + archive_file_name + "\"!");
        break;
    case FileType::TAR:
        if (unlikely(not archive_write_options.empty()))
            LOG_ERROR("no write options are currently supported for the uncompressed tar format! (2)");
        if (::archive_write_set_format_pax_restricted(archive_handle_) != ARCHIVE_OK)
            LOG_ERROR("failed to call archive_write_set_format_pax_restricted(3): " + std::string(::archive_error_string(archive_handle_))
                      + " (3)");
        break;
    case FileType::GZIPPED_TAR:
        if (::archive_write_add_filter_gzip(archive_handle_) != ARCHIVE_OK)
            LOG_ERROR("failed to call archive_write_add_filter_gzip(3): " + std::string(::archive_error_string(archive_handle_)) + " (2)");
        if (unlikely(::archive_write_set_options(archive_handle_, archive_write_options.c_str()) != ARCHIVE_OK))
            LOG_ERROR("failed to call archive_write_set_options(3) w/ \"" + archive_write_options + "\"! (3)");
        if (::archive_write_set_format_pax_restricted(archive_handle_) != ARCHIVE_OK)
            LOG_ERROR("failed to call archive_write_set_format_pax_restricted(3): " + std::string(::archive_error_string(archive_handle_))
                      + " (4)");
        break;
    case FileType::ZIP:
        if (::archive_write_set_format_zip(archive_handle_) != ARCHIVE_OK)
            LOG_ERROR("failed to call archive_write_set_format_zip(3): " + std::string(::archive_error_string(archive_handle_)) + " (2)");
        if (unlikely(::archive_write_set_options(archive_handle_, archive_write_options.c_str()) != ARCHIVE_OK))
            LOG_ERROR("failed to call archive_write_set_options(3) w/ \"" + archive_write_options + "\"! (4)");
        break;
    }

    if (unlikely(::archive_write_open_filename(archive_handle_, archive_file_name.c_str()) != ARCHIVE_OK))
        LOG_ERROR("archive_write_open_filename(3) failed: " + std::string(::archive_error_string(archive_handle_)));
}


void Writer::close() {
    if (closed_)
        return;

    if (archive_entry_ != nullptr)
        ::archive_entry_free(archive_entry_);

    if (unlikely(::archive_write_close(archive_handle_) != ARCHIVE_OK))
        LOG_ERROR("archive_write_close(3) failed: " + std::string(::archive_error_string(archive_handle_)));
    if (unlikely(::archive_write_free(archive_handle_) != ARCHIVE_OK))
        LOG_ERROR("archive_write_free(3) failed: " + std::string(::archive_error_string(archive_handle_)));

    closed_ = true;
}


void Writer::add(const std::string &filename, std::string archive_name) {
    if (archive_name.empty())
        archive_name = filename;
    if (unlikely(already_seen_archive_names_.find(archive_name) != already_seen_archive_names_.end()))
        LOG_ERROR("attempt to store a duplicate archive entry name \"" + archive_name + "\"!");
    else
        already_seen_archive_names_.emplace(archive_name);

    struct stat stat_buf;
    if (unlikely(::stat(filename.c_str(), &stat_buf) != 0))
        LOG_ERROR("stat(2) on \"" + filename + "\" failed: " + std::string(::strerror(errno)));

    if (archive_entry_ == nullptr)
        archive_entry_ = ::archive_entry_new();
    else
        ::archive_entry_clear(archive_entry_);
    ::archive_entry_set_pathname(archive_entry_, archive_name.c_str());
    ::archive_entry_copy_stat(archive_entry_, &stat_buf);

    int status;
    while ((status = ::archive_write_header(archive_handle_, archive_entry_)) == ARCHIVE_RETRY)
        /* Intentionally empty! */;
    if (unlikely(status != ARCHIVE_OK))
        LOG_ERROR("archive_write_header(3) failed! (" + std::string(::archive_error_string(archive_handle_)));

    File input(filename, "r", File::THROW_ON_ERROR);
    char buffer[DEFAULT_BLOCKSIZE];
    size_t count;
    while ((count = input.read(buffer, DEFAULT_BLOCKSIZE)) > 0) {
        if (count < DEFAULT_BLOCKSIZE and input.anErrorOccurred())
            LOG_ERROR("error reading \"" + filename + "\" !");
        write(buffer, count);
    }
}


void Writer::addEntry(const std::string &filename, const int64_t size, const mode_t mode, const EntryType entry_type) {
    if (archive_entry_ != nullptr)
        ::archive_entry_clear(archive_entry_);
    else
        archive_entry_ = ::archive_entry_new();

    ::archive_entry_set_pathname(archive_entry_, filename.c_str());
    ::archive_entry_set_size(archive_entry_, size);
    if (entry_type == EntryType::REGULAR_FILE)
        ::archive_entry_set_filetype(archive_entry_, AE_IFREG);
    else
        LOG_ERROR("unsupported entry type: " + std::to_string(static_cast<int>(entry_type)) + "!");
    ::archive_entry_set_perm(archive_entry_, mode);
    ::archive_write_header(archive_handle_, archive_entry_);
}


void Writer::write(const char * const buffer, const size_t size) {
    if (unlikely(::archive_write_data(archive_handle_, buffer, size) != static_cast<ssize_t>(size)))
        LOG_ERROR("archive_write_data(3) failed: " + std::string(::archive_error_string(archive_handle_)));
}


void UnpackArchive(const std::string &archive_name, const std::string &directory) {
    if (unlikely(not FileUtil::MakeDirectory(directory)))
        LOG_ERROR("failed to create directory \"" + directory + "\"!");

    Reader reader(archive_name);
    Reader::EntryInfo file_info;
    while (reader.getNext(&file_info)) {
        if (file_info.empty())
            continue;

        if (unlikely(not file_info.isRegularFile()))
            LOG_ERROR("unexpectedly, the entry \"" + file_info.getFilename() + "\" in \"" + archive_name + "\" is not a regular file!");

        const std::string output_filename(directory + "/" + file_info.getFilename());
        const auto output(FileUtil::OpenOutputFileOrDie(output_filename));

        char buf[8192];
        size_t read_count;
        while ((read_count = reader.read(buf, sizeof buf)) > 0) {
            if (unlikely(output->write(buf, read_count) != read_count))
                LOG_ERROR("failed to write data to \"" + output->getPath() + "\"! (No room?)");
        }
    }
}


} // namespace Archive

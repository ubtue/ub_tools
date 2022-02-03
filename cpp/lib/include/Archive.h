/** \file   Archive.h
 *  \brief  Declarations of the archive processing functions and classes.
 *  \author Dr. Johannes Ruscheinski (johannes.ruscheinski@uni-tuebingen.de)
 *
 *  \copyright 2016-2019 Universitätsbibliothek Tübingen.  All rights reserved.
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
#pragma once


#include <string>
#include <unordered_set>
#include <archive.h>
#include <archive_entry.h>


namespace Archive {


class Reader {
    archive *archive_handle_;

public:
    class EntryInfo {
        friend class Reader;
        archive_entry *archive_entry_;

    public:
        EntryInfo(): archive_entry_(nullptr) { }

        const std::string getFilename() const;
        inline int64_t size() const { return ::archive_entry_size(archive_entry_); }
        inline bool empty() const { return ::archive_entry_size(archive_entry_) == 0; }
        bool isRegularFile() const;
        bool isDirectory() const;
    };

public:
    explicit Reader(const std::string &archive_file_name);
    ~Reader();

    bool getNext(EntryInfo * const info);

    /** \brief Reads data from the current entry.
     *  \param buffer  Where to return the read data.
     *  \param size    How much to read.
     *  \return -1 is being returned if a failure occurred, 0 if there is no more data to read o/w the
     *          the number of bytes read will be returned.
     */
    ssize_t read(char * const buffer, const size_t size);

    std::string getLastErrorMessage() const { return ::archive_error_string(archive_handle_); }

    /** \brief Attempts to extract the archive member named "member_name".
     *  \param member_name      Which member to extract.
     *  \param output_filename  Where to write the extracted data.  If this is the empty string \"member_name\"
     *         will be used.
     *  \return True if "member_name" was found and successfully extracted, o/w false.
     */
    bool extractEntry(const std::string &member_name, std::string output_filename = "");
};


class Writer {
    archive *archive_handle_;
    archive_entry *archive_entry_;
    std::unordered_set<std::string> already_seen_archive_names_;
    bool closed_;

public:
    enum class FileType { AUTO, TAR, GZIPPED_TAR, ZIP };
    enum class EntryType { REGULAR_FILE };

public:
    // \param archive_write_options  Currently supported is only "compression-level" for gzipped archives!
    explicit Writer(const std::string &archive_file_name, const std::string &archive_write_options,
                    const FileType file_type = FileType::AUTO);

    explicit Writer(const std::string &archive_file_name, const FileType file_type = FileType::AUTO)
        : Writer(archive_file_name, "", file_type) { }
    ~Writer() { close(); }

    void close(); //\note idempotent!

    void add(const std::string &filename, std::string archive_name = "");

    //
    // First you need to call addEntry() and then write() at least once.  Make sure that the number of bytes written with write() is what
    // you first passed into addEntry()!
    //
    void addEntry(const std::string &filename, const int64_t size, const mode_t mode = 0644,
                  const EntryType entry_type = EntryType::REGULAR_FILE);
    void write(const char * const buffer, const size_t size);
    inline void write(const std::string &buffer) { write(buffer.c_str(), buffer.size()); }
};


/** \brief Extracts the members of "archive_name" into directory "directory".
 *  \note  We only support regular file members here.
 */
void UnpackArchive(const std::string &archive_name, const std::string &directory);


} // namespace Archive

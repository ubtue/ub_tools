/** \file   Archive.h
 *  \brief  Interfaces for the ArchiveReader and ArchiveWriter classes which are wrappers around libtar.
 *  \author Dr. Johannes Ruscheinski (johannes.ruscheinski@uni-tuebingen.de)
 *
 *  \copyright 2016-2017 Universitätsbiblothek Tübingen.  All rights reserved.
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
#ifndef ARCHIVE_H
#define ARCHIVE_H


#include <archive.h>
#include <string>


class ArchiveReader {
    archive *archive_handle_;
public:
    class EntryInfo {
        friend class ArchiveReader;
        archive_entry *archive_entry_;
    public:
        EntryInfo(): archive_entry_(nullptr) { }

        const std::string getFilename() const;
        bool isRegularFile() const;
        bool isDirectory() const;
    };
public:
    explicit ArchiveReader(const std::string &archive_file_name);
    ~ArchiveReader();

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


class ArchiveWriter {
    archive *archive_handle_;
    archive_entry *archive_entry_;
public:
    enum class FileType { AUTO, TAR, GZIPPED_TAR };
public:
    explicit ArchiveWriter(const std::string &archive_file_name, const FileType file_type = FileType::AUTO);
    ~ArchiveWriter();

    void add(const std::string &filename, const std::string &archive_name = "");
};


#endif // ifndef ARCHIVE_H

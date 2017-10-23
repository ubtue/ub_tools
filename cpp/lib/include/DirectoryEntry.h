/** \file   DirectoryEntry.h
 *  \brief  Interface for the DirectoryEntry class.
 *  \author Dr. Johannes Ruscheinski (johannes.ruscheinski@uni-tuebingen.de)
 *
 *  \copyright 2014,2017 Universitätsbibliothek Tübingen.  All rights reserved.
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
#ifndef DIRECTORY_ENTRY_H
#define DIRECTORY_ENTRY_H


#include <string>
#include <utility>
#include <vector>
#include "Compiler.h"
#include "StringUtil.h"
#include "MarcTag.h"


/** \class DirectoryEntry
 *  \brief Encapsulates a MARC-21 directory entry.
 */
class DirectoryEntry {
public:
    static const size_t DIRECTORY_ENTRY_LENGTH = 12; //< The fixed length of a directory entry according to the
                                                     //  standard.
    static const size_t TAG_LENGTH             =  3; //< The fixed length of a field tag according to the standard.
private:
    MarcTag tag_;
    unsigned field_length_;
    unsigned field_offset_;
public:
    /** Constructs a DirectoryEntry from the binary representation of a MARC-21 directory entry. */
    inline explicit DirectoryEntry(const std::string &raw_entry) {
        if (unlikely(raw_entry.size() != DIRECTORY_ENTRY_LENGTH))
            logger->error("DirectoryEntry::DirectoryEntry: incorrect raw directory entry size ("
                  + std::to_string(raw_entry.size()) + ").  Must be 12!");
        tag_ = raw_entry.substr(0, TAG_LENGTH);

        if (unlikely(std::sscanf(raw_entry.data() + TAG_LENGTH, "%4u", &field_length_) != 1))
            logger->error("DirectoryEntry::DirectoryEntry: can't scan field length ("
                          + raw_entry.substr(TAG_LENGTH, 4) + ") in directory entry! (Tag was " + tag_.to_string()
                          + ")");

        if (unlikely(std::sscanf(raw_entry.data() + 7, "%5u", &field_offset_) != 1))
            logger->error("DirectoryEntry::DirectoryEntry: can't scan field oddset in directory entry!");
    }

    /** Copy constructor. */
    DirectoryEntry(const DirectoryEntry &other)
        : tag_(other.tag_), field_length_(other.field_length_), field_offset_(other.field_offset_) { }

    /** Move constructor. */
    DirectoryEntry(DirectoryEntry &&other)
        : tag_(other.tag_), field_length_(other.field_length_), field_offset_(other.field_offset_) {}

    /** \brief Constructs a DirectoryEntry from its component parts.
     *
     *  \param tag           A field tag.  Must have a length of 3.
     *  \param field_length  Must be less than 10,000.
     *  \param field_offset  Must be less than 10,000.
     */
    DirectoryEntry(const MarcTag &tag, const unsigned field_length, const unsigned field_offset)
        : tag_(tag), field_length_(field_length), field_offset_(field_offset) {}

    inline DirectoryEntry &operator=(DirectoryEntry &&other) {
        tag_ = std::move(other.tag_);
        field_length_ = other.field_length_;
        field_offset_ = other.field_offset_;
        return *this;
    }

    /** Copy-assignment operator. */
    DirectoryEntry &operator=(const DirectoryEntry &rhs) {
        if (likely(&rhs != this)) {
            tag_          = rhs.tag_;
            field_length_ = rhs.field_length_;
            field_offset_ = rhs.field_offset_;
        }

        return *this;
    }

    inline void swap(DirectoryEntry &other) {
        std::swap(tag_, other.tag_);
        std::swap(field_length_, other.field_length_);
        std::swap(field_offset_, other.field_offset_);
    }

    const MarcTag &getTag() const { return tag_; }

    /** Includes the field terminator. */
    unsigned getFieldLength() const { return field_length_; }

    /** \param new_field_length  Must be less than 10,000 and must include the field terminator. */
    void setFieldLength(const unsigned new_field_length) { field_length_ = new_field_length; }

    unsigned getFieldOffset() const { return field_offset_; }

    /** \param new_field_offset  Must be less than 10,000. */
    void setFieldOffset(const unsigned new_field_offset) { field_offset_ = new_field_offset; }

    /** \return True if this DirectoryEntry corresponds to a control field, else false. */
    bool isControlFieldEntry() const { return tag_.isTagOfControlField(); }

    // Returns the string representation of a DirectoryEntry but w/o the trailing field terminator.
    inline std::string toString() const {
        std::string field_as_string;
        field_as_string.reserve(DirectoryEntry::DIRECTORY_ENTRY_LENGTH);

        field_as_string += tag_.to_string();
        field_as_string += StringUtil::PadLeading(std::to_string(field_length_), 4, '0');
        field_as_string += StringUtil::PadLeading(std::to_string(field_offset_), 5, '0');

        return field_as_string;
    }

    /** \brief Parses a binary MARC-21 directory blob.
     *
     *  \param entries_string A binary blob that represents the directory of a MARC-21 record.
     *  \param entries        Return value containing the parsed DirectoryEntry's.
     *  \param err_msg        If not nullptr, error messages will be returned here.
     *
     *  \return True if no parse errors occurred, else false.
     */
    static bool ParseDirEntries(const std::string &entries_string, std::vector<DirectoryEntry> * const entries,
                                std::string * const err_msg = nullptr);
};


#endif // ifndef DIRECTORY_ENTRY_H

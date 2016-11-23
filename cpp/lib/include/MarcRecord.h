/** \brief Interface for the MarcRecord class.
 *  \author Oliver Obenland (oliver.obenland@uni-tuebingen.de)
 *  \author Dr. Johannes Ruscheinski (johannes.ruscheinski@uni-tuebingen.de)
 *
 *  \copyright 2016 Universitätsbibliothek Tübingen.  All rights reserved.
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

#ifndef MARC_RECORD_H
#define MARC_RECORD_H


#include <iostream>
#include <limits>
#include <string>
#include <unordered_set>
#include <vector>
#include "DirectoryEntry.h"
#include "Leader.h"
#include "MarcReader.h"
#include "MarcWriter.h"
#include "Subfields.h"


class MarcRecord {
    friend class BinaryMarcReader;
    friend class XmlMarcReader;
    friend class BinaryMarcWriter;
    friend class XmlMarcWriter;
public:
    static const size_t FIELD_NOT_FOUND = std::numeric_limits<size_t>::max();
    static const size_t MAX_FIELD_LENGTH = 9998; // Substract size of trailing 0x1E
private:
    mutable Leader leader_;
    std::vector<DirectoryEntry> directory_entries_;
    std::string raw_data_;

    MarcRecord(Leader &leader, std::vector<DirectoryEntry> directory_entries, std::string &raw_data)
        : leader_(std::move(leader)), directory_entries_(std::move(directory_entries)),
          raw_data_(std::move(raw_data)) { }
public:
    MarcRecord() = default;
    MarcRecord(MarcRecord &&other) noexcept
        : leader_(std::move(other.leader_)), directory_entries_(std::move(other.directory_entries_)),
          raw_data_(std::move(other.raw_data_)) { }

    MarcRecord &operator=(const MarcRecord &rhs);
    operator bool () const { return not directory_entries_.empty(); }
    const Leader &getLeader() const { return leader_; }
    Leader &getLeader() { return leader_; }
    inline void setLeader(const Leader &new_leader) { leader_ = new_leader; }

    Leader::RecordType getRecordType() const { return leader_.getRecordType(); }
    std::string getControlNumber() const { return getFieldData("001"); }

    size_t getNumberOfFields() const { return directory_entries_.size(); }

    /** \brief Returns the content of the first field with given tag or an empty string if the tag is not present. **/
    inline std::string getFieldData(const MarcTag &tag) const { return getFieldData(getFieldIndex(tag)); }

    /** \brief Returns the content of the field at given index or an empty string if this index is not present. **/
    std::string getFieldData(const size_t tag_index) const;

    /** \brief Returns the subfields of the first field with given tag or an empty Subfields if the tag is not present. **/
    inline Subfields getSubfields(const MarcTag &tag) const { return getSubfields(getFieldIndex(tag)); }

    /** \brief Returns the subfields of the first field with given tag or an empty Subfield if this index is not present. **/
    Subfields getSubfields(const size_t field_index) const;

    /** \brief Deletes subfield for field index "field_index" and subfield code "subfield_code". */
    void deleteSubfield(const size_t field_index, const char subfield_code);

    /** \brief Returns the content of the first field with given tag or an empty string if the tag is not present. **/
    MarcTag getTag(const size_t index) const;

    /** \brief Returns the tag of the field at given index or MarcRecord::FIELD_NOT_FOUND if the tag is not present **/
    size_t getFieldIndex(const MarcTag &field_tag) const;

    /** \return The number of field indices for the tag "tag". */
    size_t getFieldIndices(const MarcTag &field_tag, std::vector<size_t> * const field_indices) const;

    /** \brief Updates the field at index "field_index" and adjusts various field and records lengths. */
    void updateField(const size_t field_index, const std::string &new_field_contents);

    bool insertSubfield(const MarcTag &new_field_tag, const char subfield_code,
                   const std::string &new_subfield_value, const char indicator1 = ' ', const char indicator2 = ' ');

    size_t insertField(const MarcTag &new_field_tag, const std::string &new_field_value);

    /** \brief Deletes the field at index "field_index" and adjusts various field and records lengths. */
    void deleteField(const size_t field_index);

    /** \brief Expecting a sorted, non-overlapping list of ranges, which should be deleted. */
    void deleteFields(const std::vector<std::pair<size_t, size_t>> &blocks);

    /** \brief Extracts the first occurrence of subfield "subfield_code" in field "tag".
     *  \return The value of the extracted subfield or the empty string if the tag or subfield were not found.
     */
    std::string extractFirstSubfield(const MarcTag &tag, const char subfield_code) const;

    /** \brief Extracts the first occurrence of subfield "subfield_code" from field w/ index "field_index".
     *  \return The value of the extracted subfield or the empty string if the tag or subfield were not found.
     */
    std::string extractFirstSubfield(const size_t field_index, const char subfield_code) const;

    /** \brief Extract values from all subfields from a list of fields.
     *  \param tags    A colon-separated list of field tags.
     *  \param values  Here the extracted subfield values will be returned.
     *  \param ignore_subfield_codes  Subfields whose codes are listed here will not be extracted.
     *  \return The number of values that have been extracted.
     */
    size_t extractAllSubfields(const std::string &tags, std::vector<std::string> * const values,
                               const std::string &ignore_subfield_codes = "") const;

    /** \brief Extract values from a, possibly repeated, subfield from a, possibly repeated, field.
     *  \param tag            A field tag.
     *  \param subfield_code  The subfield from which we'd like the values.
     *  \param values         Here the extracted subfield values will be returned.
     *  \return The number of values that have been extracted.
     */
    size_t extractSubfield(const MarcTag &tag, const char subfield_code,
                           std::vector<std::string> * const values) const;

    /** \brief Extract values from possibly repeated, subfields.
     *  \param tag             A field tag.
     *  \param subfield_codes  The "list" of subfield codes.
     *  \param values          The extracted subfield values will be returned here.
     *  \return The number of values that have been extracted.
     */
    size_t extractSubfields(const MarcTag &tag, const std::string &subfield_codes,
                            std::vector<std::string> * const values) const;

    /** \brief Finds local ("LOK") block boundaries.
     *  \param local_block_boundaries  Each entry contains the index of the first field of a local block in "first"
     *                                 and the index of the last field + 1 of a local block in "second".
     */
    size_t findAllLocalDataBlocks(std::vector<std::pair<size_t, size_t>> * const local_block_boundaries) const;

    /** \brief Locate a field in a local block.
     *  \param indicators           The two 1-character indicators that we're looking for. A question mark here
     *                              means don't care.  So, if you want to match any indicators you should pass "??"
     *                              here.
     *  \param field_tag            The 3 character tag that we're looking for.
     *  \param block_start_and_end  "first" must point to the first entry in "field_data" that belongs to the local
     *                              block that we're scanning and "second" one past the last entry.
     *  \return The number of times the field was found in the block.
     */
    size_t findFieldsInLocalBlock(const MarcTag &field_tag, const std::string &indicators,
                                  const std::pair<size_t, size_t> &block_start_and_end,
                                  std::vector<size_t> * const field_indices) const;


    /** \brief Remove matching tags and corresponding fields. */
    void filterTags(const std::unordered_set<MarcTag> &drop_tags);


    /** \brief Returns 3-letter language codes from field 041a. */
    std::string getLanguage(const std::string &default_language_code = "ger") const;

    /** \brief Extracts the optional language code from field 008.
     *  \return The extracted language code or the empty string if no language code was found.
     */
    std::string getLanguageCode() const;

    bool isElectronicResource() const;

    using RecordFunc = bool (&)(MarcRecord * const record, MarcWriter * const marc_writer,
                                std::string * const err_msg);

    // Returns false on error and EOF.  To distinguish between the two: on EOF "err_msg" is empty but not when an
    // error has been detected.
    // Each record read from "input" will be parsed and will be passed into "process_record". If "process_record"
    // returns false, ProcessRecords will be aborted and the error message will be passed up to the caller.
    static bool ProcessRecords(MarcReader * const marc_reader, RecordFunc process_record,
                               MarcWriter * const marc_writer, std::string * const err_msg);
private:
    // Copies all field data from record into this record and extends the directory_entries_ of this record
    // accordingly.
    void combine(const MarcRecord &record);

    static MarcRecord ReadSingleRecord(File * const input);
};


#endif /* MARC_RECORD_H */

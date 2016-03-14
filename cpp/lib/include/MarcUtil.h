/** \file   MarcUtil.h
 *  \brief  Various utility functions related to the processing of MARC-21 records.
 *  \author Dr. Johannes Ruscheinski (johannes.ruscheinski@uni-tuebingen.de)
 *
 *  \copyright 2014-2016 Universitätsbiblothek Tübingen.  All rights reserved.
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
#ifndef MARC_UTIL_H
#define MARC_UTIL_H


#include <memory>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>
#include <sys/types.h>
#include "DirectoryEntry.h"
#include "File.h"
#include "Leader.h"
#include "XmlWriter.h"


namespace MarcUtil {


class Record {
    mutable Leader leader_;
    mutable std::string raw_record_;
    mutable bool raw_record_is_out_of_date_;
    mutable std::vector<DirectoryEntry> dir_entries_;
    mutable std::vector<std::string> fields_;
    mutable off_t xml_file_start_offset_;
    mutable bool record_will_be_written_as_xml_;
private:
    Record() = default;
    Record(Leader &leader, std::vector<DirectoryEntry> &dir_entries, std::vector<std::string> &fields,
	   const off_t xml_file_start_offset) noexcept
        : leader_(std::move(leader)), raw_record_is_out_of_date_(true), dir_entries_(std::move(dir_entries)),
          fields_(std::move(fields)), xml_file_start_offset_(xml_file_start_offset) { }
public:
    Record(Record &&other) noexcept
        : leader_(std::move(other.leader_)), raw_record_(std::move(other.raw_record_)),
          raw_record_is_out_of_date_(other.raw_record_is_out_of_date_), dir_entries_(std::move(other.dir_entries_)),
	fields_(std::move(other.fields_)), xml_file_start_offset_(other.xml_file_start_offset_),
	record_will_be_written_as_xml_(other.record_will_be_written_as_xml_) { }

    operator bool () const { return not dir_entries_.empty(); }
    bool recordWillBeWrittenAsXml() const { return record_will_be_written_as_xml_; }
    void setRecordWillBeWrittenAsXml(const bool record_will_be_written_as_xml) 
        { record_will_be_written_as_xml_ = record_will_be_written_as_xml; }
    const Leader &getLeader() const { return leader_; }
    Leader &getLeader() { return leader_; }
    const std::vector<DirectoryEntry> &getDirEntries() const { return dir_entries_; }
    const std::vector<std::string> &getFields() const { return fields_; }

    std::string getControlNumber() const { return fields_.empty() ? "" : fields_[0]; }

    /** \brief Returns the index of "field_tag" or -1 if the tag is not present. */
    ssize_t getFieldIndex(const std::string &field_tag) const;

    /** \brief Returns 3-letter language codes from field 041a. */
    std::string getLanguage(const std::string &default_language_code = "ger") const;

    /** \brief Extracts the optional language code from field 008.
     *  \return The extracted language code or the empty string if no language code was found.
     */
    std::string getLanguageCode() const;

    off_t getXmlFileStartOffset() const { return xml_file_start_offset_; }

    /** \brief Updates the field at index "field_index" and adjusts various field and records lengths. */
    void updateField(const size_t field_index, const std::string &new_field_contents);

    bool insertField(const std::string &new_field_tag, const std::string &new_field_value);

    /** \brief Deletes the field at index "field_index" and adjusts various field and records lengths. */
    void deleteField(const size_t field_index);

    /** \brief Extract values from all subfields from a list of fields.
     *  \param tags    A colon-separated list of field tags.
     *  \param values  Here the extracted subfield values will be returned.
     *  \param ignore_subfield_codes  Subfields whose codes are listed here will not be extracted.
     *  \return The number of values that have been extracted.
     */
    size_t extractAllSubfields(const std::string &tags, std::vector<std::string> * const values,
			       const std::string &ignore_subfield_codes = "") const;

    /** \brief Extract values from all subfields from a list of fields.
     *  \param tags                       A colon-separated list of field tags.
     *  \param subfield_codes_and_values  Here the extracted subfield codes and values will be returned.
     *  \param ignore_subfield_codes      Subfields whose codes are listed here will not be extracted.
     *  \return The number of values that have been extracted.
     */
    size_t extractAllSubfields(const std::string &tags,
			       std::vector<std::pair<char, std::string>> * const subfield_codes_and_values,
			       const std::string &ignore_subfield_codes = "") const;

    /** \brief Extract values from a, possibly repeated, subfield from a, possibly repeated, field.
     *  \param tag            A field tag.
     *  \param subfield_code  The subfield from which we'd like the values.
     *  \param values         Here the extracted subfield values will be returned.
     *  \return The number of values that have been extracted.
     */
    size_t extractSubfield(const std::string &tag, const char subfield_code, std::vector<std::string> * const values) const;

    /** \brief Extract values from possibly repeated, subfields.
     *  \param tag             A field tag.
     *  \param subfield_codes  The "list" of subfield codes.
     *  \param values          The extracted subfield values will be returned here.
     *  \return The number of values that have been extracted.
     */
    size_t extractSubfields(const std::string &tag, const std::string &subfield_codes, std::vector<std::string> * const values)
	const;

    /** \brief Remove matching tags and corresponding fields. */
    void filterTags(const std::unordered_set<std::string> &drop_tags);

    /** \brief Extracts the first occurrence of subfield "subfield_code" in field "tag".
     *  \return The value of the extracted subfield or the empty string if the tag or subfield were not found.
     */
    std::string extractFirstSubfield(const std::string &tag, const char subfield_code) const;

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
    size_t findFieldsInLocalBlock(const std::string &field_tag, const std::string &indicators,
				  const std::pair<size_t, size_t> &block_start_and_end,
				  std::vector<size_t> * const field_indices) const;

    /** \brief Performs a few sanity checks. */
    bool recordSeemsCorrect(std::string * const err_msg) const;

    /** Write MARC-21-style data. */
    void write(File * const output) const;

    /* Write MARC-XML-style data. */
    void write(XmlWriter * const xml_writer) const;

    /** \brief Creates a Record instance by parsing a single <record> entry in a MARC-XML file.
     *  \param input                The file to read the XML data from.
     */
    static Record XmlFactory(File * const input);

    static Record BinaryFactory(File * const input);
private:
    void UpdateRawRecord() const;

    const std::string &getRawRecord() const {
	if (raw_record_is_out_of_date_)
	    UpdateRawRecord();
	return raw_record_;
    }
};


using RecordFunc = bool (&)(Record * const record, std::string * const err_msg);


// Returns false on error and EOF.  To distinguish between the two: on EOF "err_msg" is empty but not when an
// error has been detected.  For each entry in "dir_entries" there will be a corresponding entry in "field_data".
// Each record read from "input" will be parsed and the directory entries and field data will be passed into
// "process_record".  If "process_record" returns false, ProcessRecords will be aborted and the error message will
// be passed up to the caller.
bool ProcessRecords(File * const input, RecordFunc process_record, std::string * const err_msg);


using XmlRecordFunc = bool (&)(Record * const record, XmlWriter * const xml_writer, std::string * const err_msg);


// Returns false on error and EOF.  To distinguish between the two: on EOF "err_msg" is empty but not when an
// error has been detected.  For each entry in "dir_entries" there will be a corresponding entry in "field_data".
// Each record read from "input" will be parsed and the directory entries and field data will be passed into
// "process_record".  If "process_record" returns false, ProcessRecords will be aborted and the error message will
// be passed up to the caller.
bool ProcessRecords(File * const input, XmlRecordFunc process_record, XmlWriter * const xml_writer, std::string * const err_msg);


} // namespace MarcUtil


#endif // ifndef MARC_UTIL_H

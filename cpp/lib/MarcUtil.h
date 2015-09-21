/** \file   MarcUtil.h
 *  \brief  Various utility functions related to the processing of MARC-21 records.
 *  \author Dr. Johannes Ruscheinski (johannes.ruscheinski@uni-tuebingen.de)
 *
 *  \copyright 2014 Universitätsbiblothek Tübingen.  All rights reserved.
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
#include <utility>
#include <vector>
#include <cstdio>

#include "DirectoryEntry.h"
#include "Leader.h"

    
namespace MarcUtil {


bool ReadFields(const std::string &raw_fields, const std::vector<DirectoryEntry> &dir_entries,
                std::vector<std::string> * const fields, std::string * const err_msg);


/** \brief Returns the index of "field_tag" or -1 if the tag is not present. */
ssize_t GetFieldIndex(const std::vector<DirectoryEntry> &dir_entries, const std::string &field_tag);
    

// Returns false on error and EOF.  To distinguish between the two: on EOF "err_msg" is empty but not when an
// error has been detected.  For each entry in "dir_entries" there will be a corresponding entry in "field_data".
bool ReadNextRecord(FILE * const input, std::shared_ptr<Leader> &leader,
		    std::vector<DirectoryEntry> * const dir_entries, std::vector<std::string> * const field_data,
		    std::string * const err_msg, std::string * const entire_record = NULL);


// Inserts the new field with contents "new_contents" and tag "new_tag" in "*leader", "*dir_entries" and
// "*fields". N.B., only insertions into non-empty records, i.e. those w/ existing fields and a control number (001)
// field are supported!
void InsertField(const std::string &new_contents, const std::string &new_tag, const std::shared_ptr<Leader> &leader,
		 std::vector<DirectoryEntry> * const dir_entries, std::vector<std::string> * const fields);


// Creates a binary, a.k.a. "raw" representation of a MARC21 record.
std::string ComposeRecord(const std::vector<DirectoryEntry> &dir_entries, const std::vector<std::string> &fields,
                          const std::shared_ptr<Leader> &leader);


// Performs a few sanity checks.
bool RecordSeemsCorrect(const std::string &record, std::string * const err_msg);


/** \brief Constructs a MARC record and writes it to "output".
 */
void ComposeAndWriteRecord(FILE * const output, const std::vector<DirectoryEntry> &dir_entries,
                           const std::vector<std::string> &field_data, const std::shared_ptr<Leader> &leader);


/** \brief Updates the field at index "field_index" and adjusts various field and records lengths.
  */
void UpdateField(const size_t field_index, const std::string &new_field_contents,
		 const std::shared_ptr<Leader> &leader, std::vector<DirectoryEntry> * const dir_entries,
		 std::vector<std::string> * const field_data);


/** \brief Returns 3-letter language codes from field 041a. */
std::string GetLanguage(const std::vector<DirectoryEntry> &dir_entries, const std::vector<std::string> &fields,
                        const std::string &default_language_code = "ger");


/** \brief Extracts the optional language code from field 008.
 *  \return The extracted language code or the empty string if no language code was found.
 */
std::string GetLanguageCode(const std::vector<DirectoryEntry> &dir_entries, const std::vector<std::string> &fields);


/** \brief Extracts the first occurrence of subfield "subfield_code" in field "tag".
 *  \return The value of the extracted subfield or the empty string if the tag or subfield were not found.
 */
std::string ExtractFirstSubfield(const std::string &tag, const char subfield_code,
				 const std::vector<DirectoryEntry> &dir_entries,
				 const std::vector<std::string> &field_data);

/** \brief Extract values from all subfields from a list of fields.
 *  \param tags    A colon-separated list of field tags.
 *  \param values  Here the exracted subfield values will be returned.
 *  \param ignore_subfield_codes  Subfields whose codes are listed here will not be extracted.
 *  \return The number of values that have been extracted.
 */
size_t ExtractAllSubfields(const std::string &tags, const std::vector<DirectoryEntry> &dir_entries,
			   const std::vector<std::string> &field_data, std::vector<std::string> * const values,
			   const std::string &ignore_subfield_codes = "");


/** \brief Finds local ("LOK") block boundaries.
 *  \param local_block_boundaries  Each entry contains the index of the first field of a local block in "first"
 *                                 and the index of the last field + 1 of a local block in "second".
 */
size_t FindAllLocalDataBlocks(const std::vector<DirectoryEntry> &dir_entries,
			      const std::vector<std::string> &field_data,
			      std::vector<std::pair<size_t, size_t>> * const local_block_boundaries);

/** \brief Locate a field in a local block.
 *  \param indicators           The two 1-character indicators that we're looking for.
 *  \param field_tag            The 3 character tag that we're looking for.
 *  \param field_tag_and_indicators  The 3 character tag and the two 1 character indicators that we're looking for.
 *  \param block_start_and_end  "first" must point to the first entry in "field_data" that belongs to the local
 *                              block that we're scanning and "second" one past the last entry.
 *  \return The number of times the field was found in the block.
 */
size_t FindFieldsInLocalBlock(const std::string &field_tag, const std::string &indicators,
			      const std::pair<size_t, size_t> &block_start_and_end,
			      const std::vector<std::string> &field_data,
			      std::vector<size_t> * const field_indices);


} // namespace MarcUtil


#endif // ifndef MARC_UTIL_H

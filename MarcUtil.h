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


#include <string>
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
bool ReadNextRecord(FILE * const input, Leader ** const leader, std::vector<DirectoryEntry> * const dir_entries,
		    std::vector<std::string> * const field_data, std::string * const err_msg);


// Inserts the new field with contents "new_contents" and tag "new_tag" in "*leader", "*dir_entries" and
// "*fields". N.B., only insertions into non-empty records, i.e. those w/ existing fields and a control number (001)
// field are supported!
void InsertField(const std::string &new_contents, const std::string &new_tag, Leader * const leader,
		 std::vector<DirectoryEntry> * const dir_entries, std::vector<std::string> * const fields);


// Creates a binary, a.k.a. "raw" representation of a MARC21 record.
std::string ComposeRecord(const std::vector<DirectoryEntry> &dir_entries, const std::vector<std::string> &fields,
			  Leader * const leader);


// Performs a few sanity checks.
bool RecordSeemsCorrect(const std::string &record, std::string * const err_msg);


/** \brief Constructs a MARC record and writes it to "output".
 */
void ComposeAndWriteRecord(FILE * const output, const std::vector<DirectoryEntry> &dir_entries,
			   const std::vector<std::string> &field_data, Leader * const leader);


/** \brief Updates the field at index "field_index" and adjusts various field and records lengths.
  */
void UpdateField(const size_t field_index, const std::string &new_field_contents, Leader * const leader,
		 std::vector<DirectoryEntry> * const dir_entries, std::vector<std::string> * const field_data);


/** \brief Returns 3-letter language codes from field 041a. */
std::string GetLanguage(const std::vector<DirectoryEntry> &dir_entries, const std::vector<std::string> &fields,
			const std::string &default_language_code = "ger");


} // namespace MarcUtil


#endif // ifndef MARC_UTIL_H

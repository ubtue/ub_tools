/** \brief Utility for deleting partial or entire MARC records based on an input list.
 *  \author Dr. Johannes Ruscheinski (johannes.ruscheinski@uni-tuebingen.de)
 *
 *  \copyright 2015 Universitätsbiblothek Tübingen.  All rights reserved.
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

#include <algorithm>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <unordered_set>
#include <vector>
#include <cstdio>
#include <cstdlib>
#include "Compiler.h"
#include "MarcUtil.h"
#include "StringUtil.h"
#include "Subfields.h"
#include "util.h"


static void Usage() __attribute__((noreturn));


static void Usage() {
    std::cerr << "Usage: " << progname << " deletion_list input_marc output_marc\n";
    std::exit(EXIT_FAILURE);
}


static std::set<std::string> unknown_types;

// Use the following indicators to select whether to fully delete a record or remove its local data
// For a description of indicators
// c.f. https://wiki.bsz-bw.de/doku.php?id=v-team:daten:datendienste:sekkor (20160426)
const char FULL_RECORD_DELETE_INDICATORS[] = { 'A', 'B', 'C', 'D', 'E' };
const char LOCAL_DATA_DELETE_INDICATORS[] = { '3', '4', '5', '9' };
const size_t MIN_LINE_LENGTH = 21;

void ExtractDeletionIds(File * const deletion_list, std::unordered_set <std::string> * const delete_full_record_ids,
                        std::unordered_set <std::string> *const local_deletion_ids)
{

    const size_t PPN_LENGTH = 9;
    const size_t PPN_START_INDEX = 12;
    const size_t SEPARATOR_INDEX = PPN_START_INDEX - 1;

    unsigned line_no(0);
loop_top:
    while (not deletion_list->eof()) {
	const std::string line(StringUtil::Trim(deletion_list->getline()));
        ++line_no;
	if (unlikely(line.empty())) // Ignore empty lines.
	    continue;
        if (line.length() < PPN_START_INDEX)
            Error("short line " + std::to_string(line_no) + " in deletion list file \"" + deletion_list->getPath()
		  + "\": \"" + line + "\"!");
        for (char indicator : FULL_RECORD_DELETE_INDICATORS) {
            if (line[SEPARATOR_INDEX] == indicator) {
                delete_full_record_ids->insert(line.substr(PPN_START_INDEX)); // extract PPN
                goto loop_top;
            }
        }
        for (char indicator : LOCAL_DATA_DELETE_INDICATORS) {
            if (line[SEPARATOR_INDEX] == indicator) {
                if (line.length() < MIN_LINE_LENGTH)
                    Error("unexpected line length " + std::to_string(line.length()) + " for local entry on line "
                          + std::to_string(line_no) + " in deletion list file \"" + deletion_list->getPath() + "\"!");
                local_deletion_ids->insert(line.substr(PPN_START_INDEX, PPN_LENGTH)); // extract ELN
                goto loop_top;
            }
        }
        unknown_types.emplace(line.substr(SEPARATOR_INDEX, 1));
    }
}


int MatchLocalID(const std::unordered_set <std::string> &local_ids, const std::vector <DirectoryEntry> &dir_entries,
                 const std::vector <std::string> &field_data) {
    for (size_t i(0); i < dir_entries.size(); ++i) {
        if (dir_entries[i].getTag() != "LOK")
            continue;

        const Subfields subfields(field_data[i]);
        if (not subfields.hasSubfield('0'))
            continue;

        const std::string subfield_contents(subfields.getFirstSubfieldValue('0'));
        if (not StringUtil::StartsWith(subfield_contents, "001 ")
            or local_ids.find(subfield_contents.substr(4)) == local_ids.end())
            continue;

        return i;
    }

    return -1;
}


class MatchTag {
    const std::string tag_to_match_;
public:
    explicit MatchTag(const std::string &tag_to_match) : tag_to_match_(tag_to_match) { }

    bool operator()(const DirectoryEntry &dir_entry) const { return dir_entry.getTag() == tag_to_match_; }
};


/** \brief Deletes LOK sections if their pseudo tags are found in "local_deletion_ids"
 *  \return True if at least one local section has been deleted, else false.
 */
bool DeleteLocalSections(const std::vector<DirectoryEntry> &dir_entries, const std::vector<std::string> &fields,
			 const std::unordered_set <std::string> &local_deletion_ids, MarcUtil::Record * const record)
{
    bool modified(false);

    ssize_t start_local_match;
    while ((start_local_match = MatchLocalID(local_deletion_ids, dir_entries, fields)) != -1) {
	// We now expect a field "000" before the current "001" field.  (This is just a sanity check!):
	--start_local_match;
	if (start_local_match <= 0)
	    Error("weird data structure (1)!");
	const Subfields subfields1(fields[start_local_match]);
	if (not subfields1.hasSubfield('0')
	    or not StringUtil::StartsWith(subfields1.getFirstSubfieldValue('0'), "000 "))
	    Error("missing or empty local field \"000\"! (EPN: "
		  + fields[start_local_match + 1].substr(8) + ", PPN: " + fields[0] + ")");

	// Now we need to find the index one past the end of the local record.  This would
	// be either the "000" field of the next local record or one past the end of the overall
	// MARC record.
	size_t end_local_match(start_local_match + 2);
	while (end_local_match < fields.size()) {
	    const Subfields subfields2(fields[end_local_match]);
	    if (not subfields2.hasSubfield('0'))
		Error("weird data structure (2)!");
	    if (StringUtil::StartsWith(subfields2.getFirstSubfieldValue('0'), "000 "))
		break;

	    ++end_local_match;
	}
	
        for (ssize_t dir_entry_index(end_local_match - 1); dir_entry_index >= start_local_match; --dir_entry_index)
	    record->deleteField(dir_entry_index);

	modified = true;
    }

    return modified;
}


void ProcessRecords(const std::unordered_set <std::string> &title_deletion_ids,
                    const std::unordered_set <std::string> &local_deletion_ids, File *const input,
                    File *const output)
{
    unsigned total_record_count(0), deleted_record_count(0), modified_record_count(0);
    while (MarcUtil::Record record = MarcUtil::Record::BinaryFactory(input)) {
        ++total_record_count;

	const std::vector<DirectoryEntry> &dir_entries(record.getDirEntries());
        if (dir_entries[0].getTag() != "001")
            Error("First field is not \"001\"!");

	const std::vector<std::string> &fields(record.getFields());
        if (title_deletion_ids.find(fields[0]) != title_deletion_ids.end())
            ++deleted_record_count;
        else { // Look for local (LOK) data sets that may need to be deleted.
            if (not DeleteLocalSections(dir_entries, fields, local_deletion_ids, &record))
		record.write(output);
            else {
                // Only keep records that still have at least one "LOK" tag:
                if (std::find_if(dir_entries.cbegin(), dir_entries.cend(), MatchTag("LOK")) == dir_entries.cend())
                    ++deleted_record_count;
                else {
                    ++modified_record_count;
		    record.write(output);
                }
            }
        }
    }

    std::cerr << "Read " << total_record_count << " records.\n";
    std::cerr << "Deleted " << deleted_record_count << " records.\n";
    std::cerr << "Modified " << modified_record_count << " records.\n";
}


int main(int argc, char *argv[]) {
    progname = argv[0];

    if (argc != 4)
        Usage();

    const std::string deletion_list_filename(argv[1]);
    File deletion_list(deletion_list_filename, "r");
    if (not deletion_list)
        Error("can't open \"" + deletion_list_filename + "\" for reading!");

    std::unordered_set <std::string> title_deletion_ids, local_deletion_ids;
    ExtractDeletionIds(&deletion_list, &title_deletion_ids, &local_deletion_ids);

    const std::string marc_input_filename(argv[2]);
    File marc_input(marc_input_filename, "r");
    if (not marc_input)
        Error("can't open \"" + marc_input_filename + "\" for reading!");

    const std::string marc_output_filename(argv[3]);
    File marc_output(marc_output_filename, "w");
    if (not marc_output)
        Error("can't open \"" + marc_output_filename + "\" for writing!");

    try {
        ProcessRecords(title_deletion_ids, local_deletion_ids, &marc_input, &marc_output);
    } catch (const std::exception &e) {
        Error("Caught exception: " + std::string(e.what()));
    }

    if (not unknown_types.empty())
        Error("Unknown types: " + StringUtil::Join(unknown_types, ", "));
}

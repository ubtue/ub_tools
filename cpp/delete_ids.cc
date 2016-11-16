/** \brief Utility for deleting partial or entire MARC records based on an input list.
 *  \author Dr. Johannes Ruscheinski (johannes.ruscheinski@uni-tuebingen.de)
 *
 *  \copyright 2015,2016 Universitätsbiblothek Tübingen.  All rights reserved.
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
#include "MarcReader.h"
#include "MarcRecord.h"
#include "MarcWriter.h"
#include "StringUtil.h"
#include "Subfields.h"
#include "util.h"


static void Usage() __attribute__((noreturn));


static void Usage() {
    std::cerr << "Usage: " << ::progname << " deletion_list input_marc output_marc\n";
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
                        std::unordered_set <std::string> * const local_deletion_ids)
{
    const size_t PPN_LENGTH(9);
    const size_t PPN_START_INDEX(12);
    const size_t SEPARATOR_INDEX(PPN_START_INDEX - 1);

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
        for (const char indicator : FULL_RECORD_DELETE_INDICATORS) {
            if (line[SEPARATOR_INDEX] == indicator) {
                delete_full_record_ids->insert(line.substr(PPN_START_INDEX)); // extract PPN
                goto loop_top;
            }
        }
        for (const char indicator : LOCAL_DATA_DELETE_INDICATORS) {
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
bool DeleteLocalSections(const std::unordered_set <std::string> &local_deletion_ids, MarcRecord * const record)
{
    bool modified(false);

    std::vector<std::pair<size_t, size_t>> local_block_boundaries;
    record->findAllLocalDataBlocks(&local_block_boundaries);
    std::vector<std::pair<size_t, size_t>> local_block_boundaries_for_deletion;

    for (const auto local_block_boundary : local_block_boundaries) {
        std::vector<size_t> field_indices;
        record->findFieldsInLocalBlock("001", "??", local_block_boundary, &field_indices);
        if (field_indices.size() != 1)
            Error("Every local data block has to have exactly one 001 field. (Record: " + record->getControlNumber()
                  + ", Local data block: " + std::to_string(local_block_boundary.first) + " - "
                  + std::to_string(local_block_boundary.second) + ". Found " + std::to_string(field_indices.size())
                  + ")");
        const Subfields subfields(record->getSubfields(field_indices[0]));
        const std::string subfield_contents(subfields.getFirstSubfieldValue('0'));
        if (not StringUtil::StartsWith(subfield_contents, "001 ")
            or local_deletion_ids.find(subfield_contents.substr(4)) == local_deletion_ids.end())
            continue;

        local_block_boundaries_for_deletion.emplace_back(local_block_boundary);
        modified = true;
    }
    record->deleteFields(local_block_boundaries_for_deletion);

    return modified;
}


void ProcessRecords(const std::unordered_set <std::string> &title_deletion_ids,
                    const std::unordered_set <std::string> &local_deletion_ids, MarcReader * const marc_reader,
                    MarcWriter * const marc_writer)
{
    unsigned total_record_count(0), deleted_record_count(0), modified_record_count(0);
    while (MarcRecord record = marc_reader->read()) {
        ++total_record_count;

        if (title_deletion_ids.find(record.getControlNumber()) != title_deletion_ids.end())
            ++deleted_record_count;
        else { // Look for local (LOK) data sets that may need to be deleted.
            if (not DeleteLocalSections(local_deletion_ids, &record))
                marc_writer->write(record);
            else {
                // Unlike former versions we no longer delete records without local data
                ++modified_record_count;
                marc_writer->write(record);
            }
        }
    }

    std::cerr << "Read " << total_record_count << " records.\n";
    std::cerr << "Deleted " << deleted_record_count << " records.\n";
    std::cerr << "Modified " << modified_record_count << " records.\n";
}


int main(int argc, char *argv[]) {
    ::progname = argv[0];

    if (argc != 4)
        Usage();

    const std::string deletion_list_filename(argv[1]);
    File deletion_list(deletion_list_filename, "r");
    if (not deletion_list)
        Error("can't open \"" + deletion_list_filename + "\" for reading!");

    std::unordered_set <std::string> title_deletion_ids, local_deletion_ids;
    ExtractDeletionIds(&deletion_list, &title_deletion_ids, &local_deletion_ids);

    std::unique_ptr<MarcReader> marc_reader(MarcReader::Factory(argv[2], MarcReader::BINARY));
    std::unique_ptr<MarcWriter> marc_writer(MarcWriter::Factory(argv[3], MarcWriter::BINARY));

    try {
        ProcessRecords(title_deletion_ids, local_deletion_ids, marc_reader.get(), marc_writer.get());
    } catch (const std::exception &e) {
        Error("Caught exception: " + std::string(e.what()));
    }

    if (not unknown_types.empty())
        Error("Unknown types: " + StringUtil::Join(unknown_types, ", "));
}

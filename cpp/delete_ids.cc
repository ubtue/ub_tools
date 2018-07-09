/** \brief Utility for deleting partial or entire MARC records based on an input list.
 *  \author Dr. Johannes Ruscheinski (johannes.ruscheinski@uni-tuebingen.de)
 *
 *  \copyright 2015-2017 Universitätsbibliothek Tübingen.  All rights reserved.
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
#include <vector>
#include <cstdio>
#include <cstdlib>
#include "BSZUtil.h"
#include "MARC.h"
#include "StringUtil.h"
#include "util.h"


[[noreturn]]  void Usage() {
    std::cerr << "Usage: " << ::progname << " deletion_list input_marc output_marc\n";
    std::exit(EXIT_FAILURE);
}


/** \brief Deletes LOK sections if their pseudo tags are found in "local_deletion_ids"
 *  \return True if at least one local section has been deleted, else false.
 */
bool DeleteLocalSections(const std::unordered_set <std::string> &local_deletion_ids, MARC::Record * const record) {
    bool modified(false);

    std::vector<std::pair<MARC::Record::const_iterator, MARC::Record::const_iterator>> local_block_boundaries, local_block_boundaries_for_deletion;
    record->findAllLocalDataBlocks(&local_block_boundaries);

    for (const auto local_block_boundary : local_block_boundaries) {
        std::vector<MARC::Record::const_iterator> field_indices;
        record->findFieldsInLocalBlock("001", "??", local_block_boundary, &field_indices);
        if (field_indices.size() != 1)
            LOG_ERROR("Every local data block has to have exactly one 001 field. (Record: "
                          + record->getControlNumber() + ", Local data block: "
                          + std::to_string(record->getFieldIndex(local_block_boundary.first)) + " - "
                          + std::to_string(record->getFieldIndex(local_block_boundary.second)) + ". Found "
                          + std::to_string(field_indices.size()) + ")");
        const auto subfields(field_indices[0]->getSubfields());
        const std::string subfield_contents(subfields.getFirstSubfieldWithCode('0'));
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
                    const std::unordered_set <std::string> &local_deletion_ids, MARC::Reader * const marc_reader,
                    MARC::Writer * const marc_writer)
{
    unsigned total_record_count(0), deleted_record_count(0), modified_record_count(0);
    while (MARC::Record record = marc_reader->read()) {
        ++total_record_count;

        if (title_deletion_ids.find(record.getControlNumber()) != title_deletion_ids.end())
            ++deleted_record_count;
        else { // Look for local (LOK) data sets that may need to be deleted.
            if (DeleteLocalSections(local_deletion_ids, &record))
                ++modified_record_count;
            marc_writer->write(record);
        }
    }

    std::cerr << "Read " << total_record_count << " records.\n";
    std::cerr << "Deleted " << deleted_record_count << " records.\n";
    std::cerr << "Modified " << modified_record_count << " records.\n";
}


int Main(int argc, char *argv[]) {
    if (argc != 4)
        Usage();

    const std::string deletion_list_filename(argv[1]);
    File deletion_list(deletion_list_filename, "r");
    if (not deletion_list)
        LOG_ERROR("can't open \"" + deletion_list_filename + "\" for reading!");

    std::unordered_set <std::string> title_deletion_ids, local_deletion_ids;
    BSZUtil::ExtractDeletionIds(&deletion_list, &title_deletion_ids, &local_deletion_ids);

    std::unique_ptr<MARC::Reader> marc_reader(MARC::Reader::Factory(argv[2]));
    std::unique_ptr<MARC::Writer> marc_writer(MARC::Writer::Factory(argv[3]));

    try {
        ProcessRecords(title_deletion_ids, local_deletion_ids, marc_reader.get(), marc_writer.get());
    } catch (const std::exception &e) {
        LOG_ERROR("Caught exception: " + std::string(e.what()));
    }

    return EXIT_SUCCESS;
}

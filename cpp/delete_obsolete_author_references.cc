/** \brief Utility for deleting 100$0 fields if id is given in a LOEKXP list.
 *  \author Mario Trojan (mario.trojan@uni-tuebingen.de)
 *
 *  \copyright 2018,2019 Universitätsbibliothek Tübingen.  All rights reserved.
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

#include <iostream>
#include <memory>
#include <vector>
#include <cstdlib>
#include "BSZUtil.h"
#include "FileUtil.h"
#include "MARC.h"
#include "StringUtil.h"
#include "util.h"


namespace {


[[noreturn]] void Usage() {
    std::cerr << "Usage: " << ::progname << " deletion_list input_marc21 output_marc21\n";
    std::exit(EXIT_FAILURE);
}


void ProcessTag(MARC::Record * const record, const std::string &tag, const std::unordered_set<std::string> &title_deletion_ids,
                unsigned * const deleted_reference_count) {
    for (auto &field : record->getTagRange(tag)) {
        auto subfields(field.getSubfields());

        MARC::Subfields new_subfields;
        bool subfields_changed(false);
        for (auto &subfield : subfields) {
            bool remove_subfield(false);
            if (subfield.code_ == '0') {
                const std::string author_reference(subfield.value_);
                if (StringUtil::StartsWith(author_reference, "(DE-627)")) {
                    const std::string ppn(author_reference.substr(8));
                    if (title_deletion_ids.find(ppn) != title_deletion_ids.end()) {
                        LOG_INFO("deleting author " + ppn + " from title " + record->getControlNumber());
                        remove_subfield = true;
                    }
                }
            }

            if (remove_subfield) {
                ++*deleted_reference_count;
                subfields_changed = true;
            } else
                new_subfields.appendSubfield(subfield);
        }

        if (subfields_changed)
            field.setSubfields(new_subfields);
    }
}


void ProcessRecords(const std::unordered_set<std::string> &title_deletion_ids, MARC::Reader * const marc_reader,
                    MARC::Writer * const marc_writer) {
    const std::vector<std::string> tags{ "100", "110", "111", "700", "710", "711" };

    unsigned total_record_count(0), deleted_reference_count(0);
    while (MARC::Record record = marc_reader->read()) {
        ++total_record_count;

        for (const std::string &tag : tags)
            ProcessTag(&record, tag, title_deletion_ids, &deleted_reference_count);

        marc_writer->write(record);
    }

    std::cerr << "Read " << total_record_count << " records.\n";
    std::cerr << "Deleted " << deleted_reference_count << " references.\n";
}


} // unnamed namespace


int Main(int argc, char *argv[]) {
    if (argc != 4)
        Usage();

    const auto deletion_list(FileUtil::OpenInputFileOrDie(argv[1]));
    std::unordered_set<std::string> title_deletion_ids, local_deletion_ids;
    BSZUtil::ExtractDeletionIds(deletion_list.get(), &title_deletion_ids, &local_deletion_ids);

    const auto marc_reader(MARC::Reader::Factory(argv[2]));
    const auto marc_writer(MARC::Writer::Factory(argv[3]));

    ProcessRecords(title_deletion_ids, marc_reader.get(), marc_writer.get());

    return EXIT_SUCCESS;
}

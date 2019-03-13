/** \brief Utility for replacing old BSZ PPN's with new K10+ PPN's.
 *  \author Dr. Johannes Ruscheinski (johannes.ruscheinski@uni-tuebingen.de)
 *
 *  \copyright 2019 Universitätsbibliothek Tübingen.  All rights reserved.
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
#include <stdexcept>
#include <unordered_map>
#include <cstdlib>
#include <cstring>
#include <kchashdb.h>
#include "JSON.h"
#include "MARC.h"
#include "Solr.h"
#include "StringUtil.h"
#include "util.h"


namespace {


[[noreturn]] void Usage() {
    ::Usage("old_ppns_to_new_ppns_map_path marc_input marc_output field_and_subfield_code1 "
            "[field_and_subfield_code2 .. field_and_subfield_codeN]\n"
            "For field_and_subfield_code an example would be 773w.");
}


void ProcessRecords(MARC::Reader * const marc_reader, MARC::Writer * const marc_writer,
                    const std::vector<std::string> &tags_and_subfield_codes, kyotocabinet::HashDB * const db)
{
    unsigned total_record_count(0), patched_record_count(0);
    while (MARC::Record record = marc_reader->read()) {
            ++total_record_count;

            bool patched_record(false);
            for (const auto tag_and_subfield_code : tags_and_subfield_codes) {
                for (auto field : record.getTagRange(tag_and_subfield_code.substr(0, MARC::Record::TAG_LENGTH))) {
                    const char SUBFIELD_CODE(tag_and_subfield_code[MARC::Record::TAG_LENGTH]);
                    MARC::Subfields subfields(field.getSubfields());
                    bool patched_field(false);
                    for (auto &subfield : subfields) {
                        if (subfield.code_ != SUBFIELD_CODE)
                            continue;

                        std::string old_ppn_candidate;
                        if (StringUtil::StartsWith(subfield.value_, "(DE-576)"))
                            old_ppn_candidate = subfield.value_.substr(__builtin_strlen("(DE-576)"));
                        else
                            old_ppn_candidate = subfield.value_;


                        std::string new_ppn;
                        if (not db->get(old_ppn_candidate, &new_ppn))
                            continue;

                        subfield.value_ = new_ppn;
                        patched_field = true;
                    }

                    if (patched_field) {
                        field.setContents(subfields, field.getIndicator1(), field.getIndicator2());
                        patched_record = true;
                    }
                }
            }
            if (patched_record)
                ++patched_record_count;

            marc_writer->write(record);
    }

    LOG_INFO("Processed " + std::to_string(total_record_count) + " records and patched " + std::to_string(patched_record_count)
             + " of them.");
}


} // unnamed namespace


int Main(int argc, char *argv[]) {
    if (argc < 4)
        Usage();

    kyotocabinet::HashDB db;
    if (not db.open(argv[1], kyotocabinet::HashDB::OREADER))
        LOG_ERROR("Failed to open database \"" + std::string(argv[1]) + "\" for reading (" + std::string(db.error().message()) + ")!");

    std::vector<std::string> tags_and_subfield_codes;
    for (int arg_no(3); arg_no < argc; ++arg_no) {
        if (std::strlen(argv[arg_no]) != MARC::Record::TAG_LENGTH + 1)
            LOG_ERROR("bad tag + subfield code: \"" + std::string(argv[arg_no]) + "\"!");
        tags_and_subfield_codes.emplace_back(argv[arg_no]);
    }
    std::sort(tags_and_subfield_codes.begin(), tags_and_subfield_codes.end());

    const auto marc_reader(MARC::Reader::Factory(argv[1]));
    const auto marc_writer(MARC::Writer::Factory(argv[2]));
    ProcessRecords(marc_reader.get(), marc_writer.get(), tags_and_subfield_codes, &db);

    return EXIT_SUCCESS;
}

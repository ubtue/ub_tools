/** \brief Transfer 750 Translations from GND records to krim keywords authority file
 *  \author Johannes Riedl
 *
 *  \copyright 2021 Universitätsbibliothek Tübingen.  All rights reserved.
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


#include <vector>
#include <cstdlib>
#include "Compiler.h"
#include "MARC.h"
#include "RegexMatcher.h"
#include "util.h"


namespace {

typedef std::unordered_map<std::string, off_t> gndurl_to_offset_map;

[[noreturn]] void Usage() {
    ::Usage("krim_keywords_input krim_gnd_records_input krim_keywords_output");
}


void GetGNDRecordOffsets(MARC::Reader * const marc_reader, gndurl_to_offset_map * const gndurls_to_offsets) {
    unsigned total_record_count(0);
    off_t record_start(marc_reader->tell());
    while (const MARC::Record record = marc_reader->read()) {
        gndurls_to_offsets->emplace(record.getFirstSubfieldValue("024", 'a'), record_start);
        ++total_record_count;
        record_start = marc_reader->tell();
    }
}

void AugmentRecords(MARC::Reader * const krim_keywords_reader, MARC::Reader * const krim_gnd_reader,
                    MARC::Writer * const krim_keywords_writer, const gndurl_to_offset_map &gndurls_to_offsets) {
    while (MARC::Record record = krim_keywords_reader->read()) {
        const std::string gndurl(record.getFirstSubfieldValue("024", 'a'));
        if (not gndurl.empty()) {
            const auto gndurl_to_offset(gndurls_to_offsets.find(gndurl));
            if (gndurl_to_offset != gndurls_to_offsets.end()) {
                off_t gnd_record_start(gndurl_to_offset->second);
                krim_gnd_reader->seek(gnd_record_start);
                MARC::Record gnd_record(krim_gnd_reader->read());
                for (const auto &_750_fields : gnd_record.getTagRange("750")) {
                    record.insertField("750", _750_fields.getContents());
                }
            }
        }
        krim_keywords_writer->write(record);
    }
}


} // unnamed namespace


int Main(int argc, char *argv[]) {
    if (argc != 4)
        Usage();

    const auto krim_keywords_reader(MARC::Reader::Factory(argv[1]));
    const auto krim_gnd_reader(MARC::Reader::Factory(argv[2]));
    const auto krim_keywords_writer(MARC::Writer::Factory(argv[3]));

    gndurl_to_offset_map gndurls_to_offsets;
    GetGNDRecordOffsets(krim_gnd_reader.get(), &gndurls_to_offsets);
    AugmentRecords(krim_keywords_reader.get(), krim_gnd_reader.get(), krim_keywords_writer.get(), gndurls_to_offsets);
    return EXIT_SUCCESS;
}

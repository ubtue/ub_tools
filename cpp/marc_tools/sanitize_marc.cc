/** \brief Utility for cleaning up MARC records
 *  \author Dr. Johannes Ruscheinski (johannes.ruscheinski@uni-tuebingen.de)
 *
 *  \copyright 2020 Universitätsbibliothek Tübingen.  All rights reserved.
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

#include <stdexcept>
#include <vector>
#include <cstdio>
#include <cstdlib>
#include "MARC.h"
#include "util.h"


namespace {


void ProcessRecords(MARC::Reader * const marc_reader, MARC::Writer * const marc_writer) {
    unsigned record_count(0), modified_count(0);

    while (MARC::Record record = marc_reader->read()) {
        ++record_count;

        bool modified_or_deleted_at_least_one_field(false);
        for (auto field(record.begin()); field != record.end(); ++field) {
            if (field->isControlField())
                continue; // We will *not* modify or delete any control fields!

            const MARC::Subfields subfields(field->getSubfields());
            bool need_to_modify_or_delete_at_least_one_subfield(false);
            for (const auto &subfield : subfields) {
                if (unlikely(subfield.value_.empty() or subfield.value_.front() == ' ' or subfield.value_.back() == ' ')) {
                    need_to_modify_or_delete_at_least_one_subfield = true;
                    break;
                }
            }

            if (need_to_modify_or_delete_at_least_one_subfield) {
                MARC::Subfields new_subfields;
                new_subfields.reserve(subfields.size());
                for (auto subfield : subfields) {
                    if (subfield.value_.empty())
                        continue;
                    else if (subfield.value_[0] == ' ') {
                        StringUtil::Trim(&(subfield.value_));
                        if (likely(not subfield.value_.empty()))
                            new_subfields.appendSubfield(subfield.code_, subfield.value_);
                    } else
                        new_subfields.appendSubfield(subfield.code_, subfield.value_);
                }
                field->setContents(new_subfields, field->getIndicator1(), field->getIndicator2());
                modified_or_deleted_at_least_one_field = true;
            }

            if (unlikely(field->empty()))
                field = record.erase(field);
        }
        if (modified_or_deleted_at_least_one_field)
            ++modified_count;

        marc_writer->write(record);
    }

    LOG_INFO("Processed " + std::to_string(record_count) + " record(s) of which " + std::to_string(modified_count) + " were/was modified.");
}


} // unnamed namespace


int Main(int argc, char *argv[]) {
    if (argc != 3)
        ::Usage("marc_input marc_output");

    const auto marc_reader(MARC::Reader::Factory(argv[1]));
    const auto marc_writer(MARC::Writer::Factory(argv[2]));
    ProcessRecords(marc_reader.get(), marc_writer.get());

    return EXIT_SUCCESS;
}

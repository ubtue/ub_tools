/** \file    transfer_880_translations
 *  \brief   Transfer translations found in 880 to match with our ordinary 750 translations
 *  \author  Johannes Riedl
 */


/*
    Copyright (C) 2020 Library of the University of Tübingen

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU Affero General Public License as
    published by the Free Software Foundation, either version 3 of the
    License, or (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Affero General Public License for more details.

    You should have received a copy of the GNU Affero General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <iostream>
#include "MARC.h"
#include "util.h"


namespace {

const std::string UNDEFINED_LANGUAGE("Undefined");

[[noreturn]] void Usage() {
    ::Usage("authority_input autority_output");
}


std::string GetSubfieldAValueFromIxTheoTranslationField(const MARC::Record::Field &field) {
    return field.getSubfields().getFirstSubfieldWithCode('a');
}


std::string Get9ZValueFromIxTheoTranslationField(const MARC::Record &record, const MARC::Record::Field &field) {
    const auto _9Z_values(field.getSubfields().extractSubfieldsAndNumericSubfields("9Z"));
    if (_9Z_values.empty())
        LOG_ERROR("No valid 9Z content for record " + record.getControlNumber());
    return _9Z_values[0];
}


void ProcessRecords(MARC::Reader * const marc_reader, MARC::Writer * const marc_writer) {
    unsigned count(0), modified(0);
    std::vector<MARC::Subfields> new_750_entries;
    while (MARC::Record record = marc_reader->read()) {
        ++count;
        if (not record.hasFieldWithTag("880")) {
            marc_writer->write(record);
            continue;
        }
        new_750_entries.clear();
        for (const auto &field : record) {
            if (field.getTag() == "880" and field.hasSubfieldWithValue('2', "IxTheo")) {
                std::string language_code(UNDEFINED_LANGUAGE);
                if (field.hasSubfieldWithValue('6', "750-01/Hant"))
                    language_code = "hant";
                else if (field.hasSubfieldWithValue('6', "750-01/Hans"))
                    language_code = "hans";
                if (language_code != UNDEFINED_LANGUAGE) {
                    new_750_entries.emplace_back(MARC::Subfields({
                        { 'a', GetSubfieldAValueFromIxTheoTranslationField(field) },
                        { '2', "IxTheo" },
                        { '9', "L:" + language_code },
                        { '9', "Z:" + Get9ZValueFromIxTheoTranslationField(record, field) },
                    }));
                }
            }
        }
        if (not new_750_entries.empty()) {
            for (const auto &new_750_subfields : new_750_entries) {
                if (not record.insertFieldAtEnd("750", new_750_subfields, ' ', '7'))
                    LOG_ERROR("Could not insert field for record " + record.getControlNumber());
            }
            ++modified;
        }
        marc_writer->write(record);
    }
    LOG_INFO("Modified " + std::to_string(modified) + " records of " + std::to_string(count) + " altogether");
}


} // unnamed namespace


int Main(int argc, char **argv) {
    if (argc != 3)
        Usage();

    const std::unique_ptr<MARC::Reader> marc_reader(MARC::Reader::Factory(argv[1]));
    const std::unique_ptr<MARC::Writer> marc_writer(MARC::Writer::Factory(argv[2]));
    ProcessRecords(marc_reader.get(), marc_writer.get());
    return EXIT_SUCCESS;
}

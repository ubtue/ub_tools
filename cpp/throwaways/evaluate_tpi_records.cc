/** \file    evaluate_tpi_records
 *  \brief   Tool for doing som statistics for Tpi-Records
 *  \author  Johannes Riedl
 */


/*
    Copyright (C) 2023 Library of the University of Tübingen

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

[[noreturn]] void Usage() {
    ::Usage("tpi_records");
}

bool HasInformationIn548_550_551_667_687(const MARC::Record &record) {
    for (const std::string &tag : { "548", "550", "551", "667", "687" }) {
        // std::cout << "TAG: " << tag << '\n';
        if (record.hasFieldWithTag(tag))
            return true;
    }
    return false;
}


bool HasNo400Field(const MARC::Record &record) {
    return not record.hasFieldWithTag("400");
}


bool HasSeveral400Fields(const MARC::Record &record) {
    MARC::Record::ConstantRange _400_range(record.getTagRange("400"));
    if (std::distance(_400_range.begin(), _400_range.end()) > 1)
        return true;
    return false;
}


bool HasOne400Field(const MARC::Record &record) {
    MARC::Record::ConstantRange _400_range(record.getTagRange("400"));
    if (std::distance(_400_range.begin(), _400_range.end()) == 1)
        return true;
    return false;
}


void ProcessRecords(MARC::Reader * const marc_reader) {
    std::cout << "PPN"
              << ","
              << "No400"
              << ","
              << "One400"
              << ","
              << "Several400"
              << ","
              << "548_550_551_667_678" << '\n';
    while (MARC::Record record = marc_reader->read()) {
        if (not record.hasFieldWithTag("100"))
            continue;
        std::cout << record.getControlNumber() << "," << (HasNo400Field(record) and not HasInformationIn548_550_551_667_687(record)) << ","
                  << (HasOne400Field(record) and not HasInformationIn548_550_551_667_687(record)) << ","
                  << (HasSeveral400Fields(record) and not HasInformationIn548_550_551_667_687(record)) << ","
                  << HasInformationIn548_550_551_667_687(record) << '\n';
    }
}

} // namespace

int Main(int argc, char **argv) {
    if (argc != 2)
        Usage();

    const std::unique_ptr<MARC::Reader> marc_reader(MARC::Reader::Factory(argv[1]));
    ProcessRecords(marc_reader.get());
    return EXIT_SUCCESS;
}

/** \brief Utility for storing MARC records in our delivery history database.
 *  \author Dr. Johannes Ruscheinski (johannes.ruscheinski@uni-tuebingen.de)
 *
 *  \copyright 2018 Universitätsbibliothek Tübingen.  All rights reserved.
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
#include <unordered_set>
#include <cstdio>
#include <cstdlib>
#include "MARC.h"
#include "ZoteroHarvesterUtil.h"
#include "util.h"


namespace {


[[noreturn]] void Usage() {
    ::Usage("marc_data");
}


} // unnamed namespace


int Main(int argc, char *argv[]) {
    if (argc != 2)
        Usage();

    auto marc_reader(MARC::Reader::Factory(argv[1]));
    unsigned stored_record_count(0), skipped_record_count(0);
    ZoteroHarvester::Util::UploadTracker upload_tracker;

    while (const auto record = marc_reader->read()) {
        if (upload_tracker.archiveRecord(record, ZoteroHarvester::Util::UploadTracker::DeliveryState::AUTOMATIC))
            ++stored_record_count;
        else
            ++skipped_record_count;
    }

    LOG_INFO("Stored " + std::to_string(stored_record_count) + " MARC record(s).");
    LOG_INFO("Skipped " + std::to_string(skipped_record_count) + " MARC record(s).");

    return EXIT_SUCCESS;
}

/** \file    generate_complete_dumpfile.cc
 *  \brief   Generates a complete dump file with an internal structure as required by the MARC pipeline from a complete dump
 *           as delivered by the BSZ.
 *  \author  Dr. Johannes Ruscheinski
 */

/*
    Copyright (C) 2019 Library of the University of Tübingen

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

#include <set>
#include <cstdlib>
#include "Archive.h"
#include "BSZUtil.h"
#include "FileUtil.h"
#include "MARC.h"
#include "StringUtil.h"
#include "util.h"


namespace {


void CreateNewArchiveEntry(const std::string &input_filename, Archive::Writer * const archive_writer, const std::string &new_entry_name,
                           const size_t new_entry_size, const std::set<BSZUtil::ArchiveType> &desirable_archive_types) {
    Archive::Reader archive_reader(input_filename);
    archive_writer->addEntry(new_entry_name, new_entry_size);

    Archive::Reader::EntryInfo entry_info;
    char buffer[10000];
    while (archive_reader.getNext(&entry_info)) {
        const auto archive_entry_type(BSZUtil::GetArchiveType(entry_info.getFilename()));
        if (desirable_archive_types.find(archive_entry_type) == desirable_archive_types.cend())
            continue;

        for (;;) {
            const auto n_read(archive_reader.read(buffer, sizeof buffer));
            if (n_read < 0)
                LOG_ERROR("error while reading entry \"" + entry_info.getFilename() + "\" from \"" + input_filename + "\"!");
            else if (n_read > 0)
                archive_writer->write(buffer, n_read);
            else
                break;
        }
    }
}


} // unnamed namespace


int Main(int argc, char *argv[]) {
    if (argc != 3)
        ::Usage("BSZ_complete_dumpfile MARC_pipeline_complete_dumpfile");

    const std::string INPUT_FILENAME(argv[1]);
    const std::string OUTPUT_FILENAME(argv[2]);

    if (FileUtil::Exists(OUTPUT_FILENAME))
        LOG_ERROR("won't overwrite \"" + OUTPUT_FILENAME + "\"!");
    if (not StringUtil::EndsWith(OUTPUT_FILENAME, ".tar.gz"))
        LOG_ERROR("output filename \"" + OUTPUT_FILENAME + "\" must end with .tar.gz!");

    Archive::Reader archive_reader(INPUT_FILENAME);

    size_t cumulative_title_size(0), cumulative_authority_size(0);
    Archive::Reader::EntryInfo entry_info;
    while (archive_reader.getNext(&entry_info)) {
        const auto archive_entry_type(BSZUtil::GetArchiveType(entry_info.getFilename()));
        switch (archive_entry_type) {
        case BSZUtil::TITLE_RECORDS:
        case BSZUtil::SUPERIOR_TITLES:
            cumulative_title_size += entry_info.size();
            break;
        case BSZUtil::AUTHORITY_RECORDS:
            cumulative_authority_size += entry_info.size();
            break;
        default:
            LOG_ERROR("can't handle type " + std::to_string(archive_entry_type) + " of entry \"" + entry_info.getFilename() + "!");
        }
    }

    Archive::Writer archive_writer(OUTPUT_FILENAME, Archive::Writer::FileType::GZIPPED_TAR);
    CreateNewArchiveEntry(INPUT_FILENAME, &archive_writer, "tit", cumulative_title_size,
                          { BSZUtil::TITLE_RECORDS, BSZUtil::SUPERIOR_TITLES });
    CreateNewArchiveEntry(INPUT_FILENAME, &archive_writer, "aut", cumulative_authority_size, { BSZUtil::AUTHORITY_RECORDS });

    return EXIT_SUCCESS;
}

/** \file extract_local_krimdok_topics.cc
 *  \brief Extrakt topic from LOK689
 *  \author Johannes Riedl
 */

/*
    Copyright (C) 2017,2018 Library of the University of Tübingen

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
#include <unordered_map>
#include "Compiler.h"
#include "FileUtil.h"
#include "MARC.h"
#include "util.h"


[[noreturn]] void Usage() {
    std::cerr << "Usage: " << ::progname << " marc_title_data local_keyword_output\n";
    std::exit(EXIT_FAILURE);
}


void ExtractLocalKeywords(MARC::Reader * const marc_reader, std::set<std::string> * const all_local_keywords) {
    while (const auto &record = marc_reader->read()) {
        const auto local_block_starts(record.findStartOfAllLocalDataBlocks());
        for (const auto &local_block_start : local_block_starts) {
            for (const auto &local_689_field : record.getLocalTagRange("689", local_block_start)) {
                std::vector<std::string> local_keywords(local_689_field.getSubfields().extractSubfields('a'));
                std::copy(local_keywords.cbegin(), local_keywords.cend(), std::inserter(*all_local_keywords, all_local_keywords->end()));
            }
        }
    }
}


void WriteLocalKeywordsToFile(std::unique_ptr<File> &output, std::set<std::string> all_local_keywords) {
    for (const auto &topic : all_local_keywords)
        *output << topic << '\n';
}


int Main(int argc, char **argv) {
    ::progname = argv[0];

    if (argc != 3)
        Usage();

    const std::string marc_input_filename(argv[1]);
    const std::string local_keyword_output(argv[2]);

    if (unlikely(marc_input_filename == local_keyword_output))
        LOG_ERROR("Input file equals output file");

    std::unique_ptr<MARC::Reader> marc_reader(MARC::Reader::Factory(marc_input_filename));
    std::unique_ptr<File> output(FileUtil::OpenOutputFileOrDie(local_keyword_output));

    std::set<std::string> all_local_keywords;
    ExtractLocalKeywords(marc_reader.get(), &all_local_keywords);
    WriteLocalKeywordsToFile(output, all_local_keywords);

    return EXIT_SUCCESS;
}

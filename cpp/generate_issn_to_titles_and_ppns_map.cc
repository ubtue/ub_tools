/** \file    generate_issn_to_titles_and_ppns_map.cc
 *  \brief   Generates a file needed by convert_json_to_marc.
 *  \author  Dr. Johannes Ruscheinski
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

#include <set>
#include "FileUtil.h"
#include "MiscUtil.h"
#include "StringUtil.h"
#include "util.h"
#include "Zeder.h"


namespace {


std::string EscapeColons(const std::string &unescaped_string) {
    std::string escaped_string;
    escaped_string.reserve(unescaped_string.size());

    for (const char ch : unescaped_string) {
        if (ch == ':' or ch == '\\')
            escaped_string += '\\';
        escaped_string += ch;
    }

    return escaped_string;
}


auto SplitAndDedupeEntries(const std::string &entries) {
    std::vector<std::string> individual_entries;
    StringUtil::Split(entries, ' ', &individual_entries);
    std::set<std::string> deduplicated_entries;
    deduplicated_entries.insert(individual_entries.cbegin(), individual_entries.cend());
    return deduplicated_entries;
}


auto FilterOutInvalidISSNs(const std::set<std::string> &unvalidated_issns) {
    std::set<std::string> validated_issns;

    for (const auto &issn : unvalidated_issns) {
        std::string validated_issn;
        if (MiscUtil::IsPossibleISSN(issn) and MiscUtil::NormaliseISSN(issn, &validated_issn))
            validated_issns.emplace(validated_issn);
    }

    return validated_issns;
}


unsigned ProcessZederAndWriteMapFile(File * const output, const Zeder::SimpleZeder &zeder) {
    unsigned generated_count(0);
    for (const auto &journal : zeder) {
        if (journal.empty())
            continue;

        const auto print_issns(FilterOutInvalidISSNs(SplitAndDedupeEntries(journal.lookup("issn"))));
        if (print_issns.empty())
            continue;

        const auto electronic_issns(FilterOutInvalidISSNs(SplitAndDedupeEntries(journal.lookup("essn"))));
        if (electronic_issns.empty())
            continue;

        const auto title(journal.lookup("tit"));
        if (title.empty())
            continue;

        const auto electronic_ppns(SplitAndDedupeEntries(journal.lookup("eppns")));
        if (electronic_ppns.empty() or electronic_ppns.size() != electronic_issns.size())
            continue;

        for (const auto print_issn : print_issns) {
            auto electronic_issn(electronic_issns.cbegin());
            auto electronic_ppn(electronic_ppns.cbegin());
            while (electronic_issn != electronic_issns.cend()) {
                (*output) << print_issn << ':' << EscapeColons(title) << ':' << (*electronic_issn) << ':' << (*electronic_ppn) << '\n';
                ++generated_count;

                ++electronic_issn, ++electronic_ppn;
            }
        }
    }

    return generated_count;
}


unsigned ProcessZederFlavour(const Zeder::Flavour zeder_flavour, File * const map_output) {
    const Zeder::SimpleZeder zeder(zeder_flavour, { "eppns", "essn", "issn", "tit" });
    if (unlikely(zeder.empty()))
        LOG_ERROR("found no IxTheo Zeder entries matching any of our requested columns!");
    return ProcessZederAndWriteMapFile(map_output, zeder);
}


} // unnamed namespace


int Main(int argc, char **argv) {
    if (argc != 2)
        ::Usage("mapfile_output");

    const auto temp_file(FileUtil::OpenTempFileOrDie("/tmp/XXXXXX"));

    unsigned total_generated_count(0);
    total_generated_count += ProcessZederFlavour(Zeder::IXTHEO, temp_file.get());
    total_generated_count += ProcessZederFlavour(Zeder::KRIMDOK, temp_file.get());
    LOG_INFO("Generated " + std::to_string(total_generated_count) + " map entry/entries.");

    const std::string output_filename(argv[1]);
    FileUtil::RenameFileOrDie(temp_file->getPath(), output_filename, /* remove_target = */true);

    return EXIT_SUCCESS;
}

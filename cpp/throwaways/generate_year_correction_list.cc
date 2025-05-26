/** \brief Utility for displaying various bits of info about a collection of MARC records.
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

#include <algorithm>
#include <iostream>
#include <map>
#include <stdexcept>
#include <unordered_set>
#include <vector>
#include <cstdio>
#include <cstdlib>
#include "FileUtil.h"
#include "MARC.h"
#include "MiscUtil.h"
#include "RegexMatcher.h"
#include "util.h"


namespace {


[[noreturn]] void Usage() {
    ::Usage(
        "marc_data superior_ppn_list\n"
        "\tsuperior_ppn_list will contain the journal PPN's for which publication years have to be adjusted.\n");
}


void CollectMonoPPNs(MARC::Reader * const marc_reader, std::unordered_set<std::string> * const monograph_ppns) {
    std::unordered_set<std::string> superior_ppns;
    while (const MARC::Record record = marc_reader->read()) {

        if (record.isMonograph())
            monograph_ppns->emplace(record.getControlNumber());
    }

    LOG_INFO("Identified " + std::to_string(monograph_ppns->size()) + " monograph PPN's.");
}


void ProcessRecords(MARC::Reader * const marc_reader, File * const output, const std::unordered_set<std::string> &monograph_ppns) {
    unsigned record_count(0), matched_articles_count(0);
    std::unordered_set<std::string> superior_ppns;
    while (const MARC::Record record = marc_reader->read()) {
        ++record_count;

        if (not record.isArticle())
            continue;

        bool found_iSWA(false);
        for (const auto &field : record.getTagRange("LOK")) {
            if (field.hasSubfieldWithValue('0', "935") and field.hasSubfieldWithValue('a', "iSWA")) {
                found_iSWA = true;
                break;
            }
        }
        if (not found_iSWA)
            continue;

        const auto _773_field(record.findTag("773"));
        if (_773_field == record.end())
            continue;

        const auto _773_subfields(_773_field->getSubfields());
        std::string superior_ppn;
        for (const auto &subfield : _773_subfields) {
            if (subfield.code_ == 'w' and StringUtil::StartsWith(subfield.value_, "(DE-627)")) {
                superior_ppn = subfield.value_.substr(__builtin_strlen("(DE-627)"));
                break;
            }
        }
        if (superior_ppn.empty() or monograph_ppns.find(superior_ppn) == monograph_ppns.cend())
            continue;

        const auto subfield_d(_773_subfields.getFirstSubfieldWithCode('d'));
        static auto year_extractor(RegexMatcher::RegexMatcherFactoryOrDie("\\b(\\d{4})\\b"));
        std::string subfield_d_year;
        if (year_extractor->matched(subfield_d))
            subfield_d_year = (*year_extractor)[1];
        if (subfield_d_year.empty())
            continue;

        const auto subfield_g(_773_subfields.getFirstSubfieldWithCode('g'));
        std::string subfield_g_year;
        if (year_extractor->matched(subfield_g))
            subfield_g_year = (*year_extractor)[1];
        if (subfield_g_year.empty())
            continue;

        if (subfield_d_year != subfield_g_year) {
            superior_ppns.emplace(superior_ppn);
            ++matched_articles_count;
        }
    }

    LOG_INFO("found " + std::to_string(matched_articles_count) + " matching articles in " + std::to_string(record_count) + " records.");

    for (const auto &ppn : superior_ppns)
        (*output) << ppn << '\n';
    LOG_INFO("Found " + std::to_string(superior_ppns.size()) + " matching journals.");
}


} // unnamed namespace


int Main(int argc, char *argv[]) {
    if (argc != 3)
        Usage();

    const auto marc_reader(MARC::Reader::Factory(argv[1]));

    std::unordered_set<std::string> monograph_ppns;
    CollectMonoPPNs(marc_reader.get(), &monograph_ppns);
    marc_reader->rewind();

    const auto output(FileUtil::OpenOutputFileOrDie(argv[2]));
    ProcessRecords(marc_reader.get(), output.get(), monograph_ppns);

    return EXIT_SUCCESS;
}

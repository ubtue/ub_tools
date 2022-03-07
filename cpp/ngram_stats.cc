/** \brief Utility for estimating the accuracy of N-gram based language assignments.
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

#include <iostream>
#include <map>
#include <set>
#include <stdexcept>
#include <vector>
#include <cstdio>
#include <cstdlib>
#include "FileUtil.h"
#include "MARC.h"
#include "NGram.h"
#include "StringUtil.h"
#include "UBTools.h"
#include "util.h"


namespace {


[[noreturn]] void Usage() {
    ::Usage("marc_data language1 ... languageN");
}


void ProcessRecords(const std::set<std::string> &test_languages, MARC::Reader * const marc_reader) {
    unsigned correct_count(0), incorrect_count(0);
    while (const MARC::Record record = marc_reader->read()) {
        std::set<std::string> language_codes;
        if (MARC::GetLanguageCodes(record, &language_codes) != 1)
            continue;

        const auto actual_language(*language_codes.cbegin());
        if (test_languages.find(actual_language) == test_languages.cend())
            continue;

        std::vector<NGram::DetectedLanguage> top_languages;
        NGram::ClassifyLanguage(record.getCompleteTitle(), &top_languages, test_languages);
        if (top_languages.front().language_ == actual_language)
            ++correct_count;
        else
            ++incorrect_count;
    }

    LOG_INFO("Classified languages of " + std::to_string(correct_count + incorrect_count) + " record(s) of which "
             + StringUtil::ToString(100.0 * correct_count / double(correct_count + incorrect_count)) + " were classified correctly!");
}


bool IsKnownNGramLanguageCode(const std::string &code_candidate) {
    static std::unordered_set<std::string> known_language_codes;
    if (known_language_codes.empty()) {
        FileUtil::Directory directory(UBTools::GetTuelibPath() + "language_models", "...\\.lm");
        for (const auto &language_model : directory)
            known_language_codes.emplace(language_model.getName().substr(0, 3));
    }

    return known_language_codes.find(code_candidate) != known_language_codes.cend();
}


} // unnamed namespace


int Main(int argc, char *argv[]) {
    if (argc < 3)
        Usage();

    std::set<std::string> test_languages;
    for (int arg_no(2); arg_no < argc; ++arg_no) {
        const std::string language_code_candidate(argv[arg_no]);
        if (IsKnownNGramLanguageCode(language_code_candidate))
            test_languages.emplace(language_code_candidate);
        else
            LOG_ERROR("\"" + language_code_candidate + "\" is not a valid language code!");
    }

    const auto marc_reader(MARC::Reader::Factory(argv[1]));
    ProcessRecords(test_languages, marc_reader.get());

    return EXIT_SUCCESS;
}

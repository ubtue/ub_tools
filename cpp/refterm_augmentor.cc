/** \brief Utility for adding refterm aliases to a MARC collection.
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
#include <stdexcept>
#include <set>
#include <tuple>
#include <vector>
#include <cstdio>
#include <cstdlib>
#include "FileUtil.h"
#include "MARC.h"
#include "RegexMatcher.h"
#include "util.h"


namespace {


[[noreturn]] void Usage() {
    std::cerr << "Usage: " << ::progname << " refterms_file marc_data\n";
    std::exit(EXIT_FAILURE);
}


void LoadReftermMap(const std::string &refterm_filename,
                    std::vector<std::tuple<std::string, std::vector<std::string>>> * const combined_and_separated_terms)
{
    auto input(FileUtil::OpenInputFileOrDie(refterm_filename));
    unsigned line_no(0);
    while (not input->eof()) {
        ++line_no;
        std::string line;
        input->getline(&line);
        if (unlikely(StringUtil::StartsWith(line, "!!!GESPERRT!!!") or line.empty()))
            continue;

        auto pipe_pos(line.find('|'));
        if (unlikely(pipe_pos == std::string::npos))
            LOG_ERROR("missing | in \"" + refterm_filename + "\" on line #" + std::to_string(line_no) + "!");

        auto combined_term(line.substr(0, pipe_pos));
        std::vector<std::string> separated_terms;
        if (unlikely(StringUtil::SplitThenTrimWhite(line.substr(pipe_pos + 1), ',', &separated_terms) == 0))
            LOG_ERROR("missing separated terms in \"" + refterm_filename + "\" on line #" + std::to_string(line_no) + "!");

        combined_and_separated_terms->emplace_back(combined_term, separated_terms);
    }

    LOG_INFO("Loaded " + std::to_string(combined_and_separated_terms->size()) + " terms from \"" + refterm_filename + "\"!");
}


/**
 *  Remove single square bracket characters if they are the start and/or end
 *  chars (matched or unmatched) and are the only square bracket chars in the string.
 */
std::string RemoveOuterBrackets(const std::string &orig_str) {
    if (orig_str.empty())
        return orig_str;

    std::string result(StringUtil::TrimWhite(orig_str));

    if (not result.empty()) {
        const bool open_bracket_first(result[0] == '[');
        const bool close_bracket_last(StringUtil::EndsWith(result, ']'));
        if (open_bracket_first and close_bracket_last and result.find('[', 1) == std::string::npos
            and result.find_last_of(']', result.length() - 2) == std::string::npos)
            // only square brackets are at beginning and end
            result = result.substr(1, result.length() - 1);
        else if (open_bracket_first and result.find(']') == std::string::npos)
            // starts with '[' but no ']'; remove open bracket
            result = result.substr(1);
        else if (close_bracket_last and result.find('[') == std::string::npos)
            // ends with ']' but no '['; remove close bracket
            result = result.substr(0, result.length() - 1);
    }

    return StringUtil::TrimWhite(result);
}
    

// Trying to replicate https://github.com/solrmarc/solrmarc/blob/master/src/org/solrmarc/tools/DataUtil.java
std::string CleanData(std::string data) {
    static auto const trailing_junk_regex(RegexMatcher::RegexMatcherFactoryOrDie(" *([,/;:])$"));
    static auto const junior_senior_regex(RegexMatcher::RegexMatcherFactoryOrDie(".*[JS]r\\.$"));
    static auto const period_after_word_regex(RegexMatcher::RegexMatcherFactoryOrDie(".*\\w\\w\\.$"));
    static auto const period_after_uppercase_letters_regex(RegexMatcher::RegexMatcherFactoryOrDie(".*\\p{L}\\p{L}\\.$"));
    static auto const puctuation_followed_by_period_regex(RegexMatcher::RegexMatcherFactoryOrDie(".*\\p{P}\\.$"));
    
    std::string last_data;
    do {
        last_data = data;
        StringUtil::TrimWhite(&data);

        if (trailing_junk_regex->matched(data))
            data.resize(data.length() - (*trailing_junk_regex)[1].length());

        if (StringUtil::EndsWith(data, '.')) {
            if (junior_senior_regex->matched(data))
                ; // Don't strip period off a trailing "Jr." or "Sr.".
            else if (period_after_word_regex->matched(data) or period_after_uppercase_letters_regex->matched(data)
                     or puctuation_followed_by_period_regex->matched(data))
                data.resize(data.size() - 1); // Strip off the trailing period.
        }

        data = RemoveOuterBrackets(data);
        if (unlikely(data.empty()))
            return data;
    } while (data != last_data);
    return data;
}

    
void GetGermanTopics(const MARC::Record &record, std::set<std::string> * const extracted_topics) {
    extracted_topics->clear();
    
    static const std::set<std::string> TAGS{ "600", "610", "630", "648", "650", "651", "655" };
    for (const auto &field : record) {
        if (field.isControlField() or TAGS.find(field.getTag().toString()) == TAGS.cend())
            continue;

        for (const auto &subfield : field.getSubfields()) {
            if (StringUtil::IsDigit(subfield.code_))
                continue;

            std::string subfield_data(CleanData(subfield.value_));
            if (not subfield_data.empty())
                extracted_topics->emplace(StringUtil::ReplaceString("/", "\\/", &subfield_data));
        }
    }
}

    
void ProcessRecords(MARC::Reader * const marc_reader) {
    unsigned record_count(0);

    while (const MARC::Record record = marc_reader->read()) {
        ++record_count;

        std::set<std::string> extracted_topics;
        GetGermanTopics(record, &extracted_topics);
        if (not extracted_topics.empty())
            std::cout << StringUtil::Join(extracted_topics, ", ") << '\n';
    }

    LOG_INFO("Processed " + std::to_string(record_count) + " MARC record(s).");
}


} // unnamed namespace


int Main(int argc, char *argv[]) {
    if (argc != 3)
        Usage();

    std::vector<std::tuple<std::string, std::vector<std::string>>> combined_and_separated_terms;
    LoadReftermMap(argv[1], &combined_and_separated_terms);
    auto marc_reader(MARC::Reader::Factory(argv[2]));
    ProcessRecords(marc_reader.get());

    return EXIT_SUCCESS;
}

/** \file    populate_in_tuebingen_available.cc
 *  \author  Dr. Johannes Ruscheinski
 *
 *  A tool that adds a new "SIG" field to a MARC record if there are UB or IFK call numbers in a record.
 */

/*
    Copyright (C) 2015-2017, Library of the University of Tübingen

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
#include <limits>
#include <regex>
#include <set>
#include <utility>
#include <cstdlib>
#include "Compiler.h"
#include "DirectoryEntry.h"
#include "HtmlUtil.h"
#include "Leader.h"
#include "MarcRecord.h"
#include "MarcReader.h"
#include "MarcUtil.h"
#include "MarcWriter.h"
#include "MarcXmlWriter.h"
#include "RegexMatcher.h"
#include "StringUtil.h"
#include "Subfields.h"
#include "util.h"


void Usage() {
    std::cerr << "Usage: " << ::progname << " [--verbose] marc_input marc_output\n";
    std::exit(EXIT_FAILURE);
}


class Range {
public:
    static const unsigned ISSUE_WILDCARD  = std::numeric_limits<unsigned>::max();
    static const unsigned VOLUME_WILDCARD = std::numeric_limits<unsigned>::max();
    static const unsigned YEAR_WILDCARD   = std::numeric_limits<unsigned>::max();
public:
    unsigned start_issue_;
    unsigned start_volume_;
    unsigned start_year_;
    unsigned end_issue_;
    unsigned end_volume_;
    unsigned end_year_;
public:
    Range(const unsigned start_volume, const unsigned start_year, const unsigned end_volume, const unsigned end_year)
        : start_volume_(start_volume), start_year_(start_year), end_volume_(end_volume), end_year_(end_year) { }

    inline bool inRange(const unsigned volume, const unsigned year) const {
        return (start_volume_ < volume) and (volume < end_volume_) and (start_year_ < year) and (year < end_year_);
    }

    inline void setStartAndEndIssues(const unsigned start_issue, const unsigned end_issue) {
        start_issue_ = start_issue;
        end_issue_   = end_issue;
    }

    bool matched(const unsigned issue, const unsigned year, const unsigned volume) const;
};


bool Range::matched(const unsigned issue, const unsigned year, const unsigned volume) const {
    if (issue != ISSUE_WILDCARD) {
        if (start_issue_ != ISSUE_WILDCARD and issue < start_issue_)
            return false;
        if (end_issue_ != ISSUE_WILDCARD and issue > end_issue_)
            return false;
    }

    // We always need a matching year.
    if (start_year_ != YEAR_WILDCARD and year < start_year_)
        return false;
    if (end_year_ != YEAR_WILDCARD and year > end_year_)
        return false;

    if (volume != VOLUME_WILDCARD) {
        if (start_volume_ != VOLUME_WILDCARD and volume < start_volume_)
            return false;
        if (end_volume_ != VOLUME_WILDCARD and volume > end_volume_)
            return false;
    }

    return true;
}


static unsigned modified_record_count;
static unsigned add_sig_count;
static unsigned good_match_count;
static unsigned bad_match_count;


// Returns UB and criminology sigils or the empty string.
std::string FindSigil(MarcRecord * const record, const std::pair<size_t, size_t> &block_start_and_end) {
    std::vector<size_t> field_indices;
    record->findFieldsInLocalBlock("852", "  ", block_start_and_end, &field_indices);
    for (size_t field_index : field_indices) {
        const std::string _852a_contents(record->extractFirstSubfield(field_index, 'a'));
        if (StringUtil::StartsWith(_852a_contents, "DE-21"))
            return _852a_contents;
    }

    return "";
}


// Collects matching pairs of opening and closing symbols in "range".
inline void CleanupHelper(const char open_symbol, const char close_symbol, const std::string &range,
                          std::vector<std::pair<size_t, size_t>> * const matching_positions)
{
    size_t open_pos, seach_start_pos(0);
    while ((open_pos = range.find(open_symbol, seach_start_pos)) != std::string::npos) {
        const size_t close_pos(range.find(close_symbol, open_pos + 1));
        if (close_pos == std::string::npos)
            break;
        matching_positions->emplace_back(open_pos, close_pos);
        seach_start_pos = close_pos + 1;
    }
}


void CleanupRange(std::string * const range, unsigned * const trailing_issue1, unsigned * const trailing_issue2) {
    *trailing_issue1 = Range::ISSUE_WILDCARD;
    *trailing_issue2 = Range::ISSUE_WILDCARD;

    // Remove all spaces.
    std::string spaceless_range;
    for (const char ch : *range) {
        if (ch != ' ')
            spaceless_range += ch;
    }
    range->swap(spaceless_range);

    // Remove all characters between matching parentheses as well as the parentheses themselves.
    std::vector<std::pair<size_t, size_t>> matching_paren_positions;
    CleanupHelper('(', ')', *range, &matching_paren_positions);
    if (not matching_paren_positions.empty()) {
        std::string cleaned_up_range;
        size_t start_pos(0);
        for (const auto &start_end : matching_paren_positions) {
            cleaned_up_range += range->substr(start_pos, start_end.first - start_pos);
            start_pos = start_end.second + 1;
        }
        cleaned_up_range += range->substr(start_pos);

        range->swap(cleaned_up_range);
    }

    // Remove matched square brackets while retaining the characters between them.
    std::vector<std::pair<size_t, size_t>> matching_bracket_positions;
    CleanupHelper('[', ']', *range, &matching_bracket_positions);
    if (not matching_bracket_positions.empty()) {
        std::string cleaned_up_range;
        size_t start_pos(0);
        for (const auto &start_end : matching_bracket_positions) {
            cleaned_up_range += range->substr(start_pos, start_end.first - start_pos);
            cleaned_up_range += range->substr(start_end.first + 1, start_end.second - start_end.first - 1);
            start_pos = start_end.second + 1;
        }
        cleaned_up_range += range->substr(start_pos);

        range->swap(cleaned_up_range);
    }

    // Identify an optional trailing numeric issue.
    static RegexMatcher * const trailing_issue_matcher(RegexMatcher::RegexMatcherFactory(".*,(\\d+)(-\\d*)?$"));
    if (trailing_issue_matcher->matched(*range)) {
        size_t trailing_length(1 + (*trailing_issue_matcher)[1].length());
        if (unlikely(not StringUtil::ToUnsigned((*trailing_issue_matcher)[1], trailing_issue1)))
            logger->error("can't convert \"" + (*trailing_issue_matcher)[1] + "\" to an unsigned trailing issue!");
        if (trailing_issue_matcher->getNoOfGroups() == 2) {
            const std::string trailing_issue2_candidate((*trailing_issue_matcher)[2].substr(1));
            if (not trailing_issue2_candidate.empty()) {
                if (unlikely(not StringUtil::ToUnsigned(trailing_issue2_candidate, trailing_issue2)))
                    logger->error("can't convert \"" + (*trailing_issue_matcher)[2].substr(1)
                                  + "\" to an unsigned trailing issue!");
            }
            trailing_length += (*trailing_issue_matcher)[2].length();
        }
        range->resize(range->length() - trailing_length);
    }

    // Throw away everything after the last equal sign, including the equal sign but only if we have at least one
    // period, hyphen or comma before the equal sign.
    const size_t last_equal_pos(range->rfind('='));
    if (last_equal_pos != std::string::npos
        and range->substr(0, last_equal_pos).find_first_of(".-,") != std::string::npos)
        range->resize(last_equal_pos);

    // Handle year equivalencies that look like YYYY=yyyy:
    /*
    auto start_iter(range->cbegin()), end_iter(range->cend());
    std::smatch match_results;
    static const std::regex year_match_regex("[0-9]{4}=[0-9]{4}", std::regex::extended);
    while (std::regex_match(start_iter, end_iter, match_results, year_match_regex)) {
        std::string year1(range->substr(match_results.position(), 4));
        std::string year2(range->substr(match_results.position() + 4 + 1, 4));
        std::cout << "1:" << year1 << ", 2:" << year2 << '\n';
        start_iter += match_results.position() + 4 + 1 + 4;
    }
    */
}


bool ParseRanges1(const std::vector<std::string> &individual_ranges, std::vector<Range> * const ranges) {
    static RegexMatcher * const matcher(RegexMatcher::RegexMatcherFactory(
        "^(\\d+)\\.(\\d{4})(?:/\\d+)?-(?:\\d+/)?(\\d+)\\.(\\d{4})$"));
    bool found_at_least_one_match(false);
    for (const auto &individual_range : individual_ranges) {
        if (not matcher->matched(individual_range))
            continue;

        unsigned start_volume;
        if (not StringUtil::ToUnsigned((*matcher)[1], &start_volume)) {
            logger->warning("can't convert \"" + (*matcher)[1] + "\" to an unsigned start volume!");
            continue;
        }

        unsigned start_year;
        if (not StringUtil::ToUnsigned((*matcher)[2], &start_year)) {
            logger->warning("can't convert \"" + (*matcher)[2] + "\" to an unsigned start year!");
            continue;
        }

        unsigned end_volume;
        if (not StringUtil::ToUnsigned((*matcher)[3], &end_volume)) {
            logger->warning("can't convert \"" + (*matcher)[3] + "\" to an unsigned end volume!");
            continue;
        }

        unsigned end_year;
        if (not StringUtil::ToUnsigned((*matcher)[4], &end_year)) {
            logger->warning("can't convert \"" + (*matcher)[4] + "\" to an unsigned end year!");
            continue;
        }

        ranges->emplace_back(Range(start_volume, start_year, end_volume, end_year));
        found_at_least_one_match = true;
    }

    return found_at_least_one_match;
}


bool ParseRanges2(const std::vector<std::string> &individual_ranges, std::vector<Range> * const ranges) {
    static RegexMatcher * const matcher(RegexMatcher::RegexMatcherFactory("^(\\d+)\\.(\\d{4})-$"));
    bool found_at_least_one_match(false);
    for (const auto &individual_range : individual_ranges) {
        if (not matcher->matched(individual_range))
            continue;

        unsigned start_volume;
        if (not StringUtil::ToUnsigned((*matcher)[1], &start_volume)) {
            logger->warning("can't convert \"" + (*matcher)[1] + "\" to an unsigned start volume!");
            continue;
        }

        unsigned start_year;
        if (not StringUtil::ToUnsigned((*matcher)[2], &start_year)) {
            logger->warning("can't convert \"" + (*matcher)[2] + "\" to an unsigned start year!");
            continue;
        }

        ranges->emplace_back(Range(start_volume, start_year, Range::VOLUME_WILDCARD, Range::YEAR_WILDCARD));
        found_at_least_one_match = true;
    }

    return found_at_least_one_match;
}


bool ParseRanges3(const std::vector<std::string> &individual_ranges, std::vector<Range> * const ranges) {
    static RegexMatcher * const matcher(RegexMatcher::RegexMatcherFactory("^(\\d{4})(?:/\\d+)?-(\\d{4})$"));
    bool found_at_least_one_match(false);
    for (const auto &individual_range : individual_ranges) {
        if (not matcher->matched(individual_range))
            continue;

        unsigned start_year;
        if (not StringUtil::ToUnsigned((*matcher)[1], &start_year)) {
            logger->warning("can't convert \"" + (*matcher)[1] + "\" to an unsigned start year!");
            continue;
        }

        unsigned end_year;
        if (not StringUtil::ToUnsigned((*matcher)[2], &end_year)) {
            logger->warning("can't convert \"" + (*matcher)[2] + "\" to an unsigned end year!");
            continue;
        }

        ranges->emplace_back(Range(Range::VOLUME_WILDCARD, start_year, Range::VOLUME_WILDCARD, end_year));
        found_at_least_one_match = true;
    }

    return found_at_least_one_match;
}


bool ParseRanges4(const std::vector<std::string> &individual_ranges, std::vector<Range> * const ranges) {
    static RegexMatcher * const matcher(RegexMatcher::RegexMatcherFactory("^(\\d{4})$"));
    bool found_at_least_one_match(false);
    for (const auto &individual_range : individual_ranges) {
        if (not matcher->matched(individual_range))
            continue;

        unsigned start_year;
        if (not StringUtil::ToUnsigned((*matcher)[1], &start_year)) {
            logger->warning("can't convert \"" + (*matcher)[1] + "\" to an unsigned start year!");
            continue;
        }

        ranges->emplace_back(Range(Range::VOLUME_WILDCARD, start_year, Range::VOLUME_WILDCARD, Range::YEAR_WILDCARD));
        found_at_least_one_match = true;
    }

    return found_at_least_one_match;
}


bool ParseRanges5(const std::vector<std::string> &individual_ranges, std::vector<Range> * const ranges) {
    static RegexMatcher * const matcher(RegexMatcher::RegexMatcherFactory("^(\\d{4})/(\\d{2})$"));
    bool found_at_least_one_match(false);
    for (const auto &individual_range : individual_ranges) {
        if (not matcher->matched(individual_range))
            continue;

        unsigned start_year;
        if (not StringUtil::ToUnsigned((*matcher)[1], &start_year)) {
            logger->warning("can't convert \"" + (*matcher)[1] + "\" to an unsigned start year!");
            continue;
        }

        const std::string end_year_string((*matcher)[1].substr(0, 2) + (*matcher)[2]);
        unsigned end_year;
        if (not StringUtil::ToUnsigned(end_year_string, &end_year)) {
            logger->warning("can't convert \"" + end_year_string + "\" to an unsigned end year!");
            continue;
        }

        ranges->emplace_back(Range(Range::VOLUME_WILDCARD, start_year, Range::VOLUME_WILDCARD, end_year));
        found_at_least_one_match = true;
    }

    return found_at_least_one_match;
}


bool ParseRanges6(const std::vector<std::string> &individual_ranges, std::vector<Range> * const ranges) {
    static RegexMatcher * const matcher(RegexMatcher::RegexMatcherFactory("^(\\d+)\\.(\\d{4})$"));
    bool found_at_least_one_match(false);
    for (const auto &individual_range : individual_ranges) {
        if (not matcher->matched(individual_range))
            continue;

        unsigned start_volume;
        if (not StringUtil::ToUnsigned((*matcher)[1], &start_volume)) {
            logger->warning("can't convert \"" + (*matcher)[1] + "\" to an unsigned start volume!");
            continue;
        }

        unsigned start_year;
        if (not StringUtil::ToUnsigned((*matcher)[2], &start_year)) {
            logger->warning("can't convert \"" + (*matcher)[2] + "\" to an unsigned start year!");
            continue;
        }

        ranges->emplace_back(Range(start_volume, start_year, Range::VOLUME_WILDCARD, Range::YEAR_WILDCARD));
        found_at_least_one_match = true;
    }

    return found_at_least_one_match;
}


bool ParseRanges7(const std::vector<std::string> &individual_ranges, std::vector<Range> * const ranges) {
    static RegexMatcher * const matcher(RegexMatcher::RegexMatcherFactory("^(\\d+)\\.(\\d{4})/(\\d+)$"));
    bool found_at_least_one_match(false);
    for (const auto &individual_range : individual_ranges) {
        if (not matcher->matched(individual_range))
            continue;

        unsigned start_volume;
        if (not StringUtil::ToUnsigned((*matcher)[1], &start_volume)) {
            logger->warning("can't convert \"" + (*matcher)[1] + "\" to an unsigned start volume!");
            continue;
        }

        unsigned start_year;
        if (not StringUtil::ToUnsigned((*matcher)[2], &start_year)) {
            logger->warning("can't convert \"" + (*matcher)[2] + "\" to an unsigned start year!");
            continue;
        }

        unsigned end_year;
        const std::string end_year_candidate((*matcher)[2].substr(0, 2) + (*matcher)[3]);
        if (not StringUtil::ToUnsigned(end_year_candidate, &end_year)) {
            logger->warning("can't convert \"" + end_year_candidate + "\" to an unsigned end year!");
            continue;
        }

        ranges->emplace_back(Range(start_volume, start_year, Range::VOLUME_WILDCARD, end_year));
        found_at_least_one_match = true;
    }

    return found_at_least_one_match;
}


bool ParseRanges8(const std::vector<std::string> &individual_ranges, std::vector<Range> * const ranges) {
    static RegexMatcher * const matcher(RegexMatcher::RegexMatcherFactory("^(\\d+)\\.(\\d{4})(?:/\\d+)?-$"));
    bool found_at_least_one_match(false);
    for (const auto &individual_range : individual_ranges) {
        if (not matcher->matched(individual_range))
            continue;

        unsigned start_volume;
        if (not StringUtil::ToUnsigned((*matcher)[1], &start_volume)) {
            logger->warning("can't convert \"" + (*matcher)[1] + "\" to an unsigned start volume!");
            continue;
        }

        unsigned start_year;
        if (not StringUtil::ToUnsigned((*matcher)[2], &start_year)) {
            logger->warning("can't convert \"" + (*matcher)[2] + "\" to an unsigned start year!");
            continue;
        }

        ranges->emplace_back(Range(start_volume, start_year, Range::VOLUME_WILDCARD, Range::YEAR_WILDCARD));
        found_at_least_one_match = true;
    }

    return found_at_least_one_match;
}


bool ParseRanges9(const std::vector<std::string> &individual_ranges, std::vector<Range> * const ranges) {
    static RegexMatcher * const matcher(RegexMatcher::RegexMatcherFactory(
        "^(\\d+)\\.(\\d{4})(?:/\\d+)?-(\\d+)\\.(\\d{4})/(\\d{2})$"));
    bool found_at_least_one_match(false);
    for (const auto &individual_range : individual_ranges) {
        if (not matcher->matched(individual_range))
            continue;

        unsigned start_volume;
        if (not StringUtil::ToUnsigned((*matcher)[1], &start_volume)) {
            logger->warning("can't convert \"" + (*matcher)[1] + "\" to an unsigned start volume!");
            continue;
        }

        unsigned start_year;
        if (not StringUtil::ToUnsigned((*matcher)[2], &start_year)) {
            logger->warning("can't convert \"" + (*matcher)[2] + "\" to an unsigned start year!");
            continue;
        }

        unsigned end_volume;
        if (not StringUtil::ToUnsigned((*matcher)[3], &end_volume)) {
            logger->warning("can't convert \"" + (*matcher)[3] + "\" to an unsigned end volume!");
            continue;
        }

        unsigned end_year;
        const std::string end_year_candidate((*matcher)[4].substr(0, 2) + (*matcher)[5]);
        if (not StringUtil::ToUnsigned(end_year_candidate, &end_year)) {
            logger->warning("can't convert \"" + end_year_candidate + "\" to an unsigned end year!");
            continue;
        }

        ranges->emplace_back(Range(start_volume, start_year, end_volume, end_year));
        found_at_least_one_match = true;
    }

    return found_at_least_one_match;
}


bool ParseRanges10(const std::vector<std::string> &individual_ranges, std::vector<Range> * const ranges) {
    static RegexMatcher * const matcher(RegexMatcher::RegexMatcherFactory(
        "^(\\d{4})(?:/\\d+)?-(\\d{4})/(\\d{2})$"));
    bool found_at_least_one_match(false);
    for (const auto &individual_range : individual_ranges) {
        if (not matcher->matched(individual_range))
            continue;

        unsigned start_year;
        if (not StringUtil::ToUnsigned((*matcher)[1], &start_year)) {
            logger->warning("can't convert \"" + (*matcher)[1] + "\" to an unsigned start year!");
            continue;
        }

        const std::string end_year_candidate((*matcher)[2].substr(0, 2) + (*matcher)[3]);
        unsigned end_year;
        if (not StringUtil::ToUnsigned(end_year_candidate, &end_year)) {
            logger->warning("can't convert \"" + end_year_candidate + "\" to an unsigned end year!");
            continue;
        }

        ranges->emplace_back(Range(Range::VOLUME_WILDCARD, start_year, Range::VOLUME_WILDCARD, end_year));
        found_at_least_one_match = true;
    }

    return found_at_least_one_match;
}


bool ParseRanges11(const std::vector<std::string> &individual_ranges, std::vector<Range> * const ranges) {
    static RegexMatcher * const matcher(RegexMatcher::RegexMatcherFactory("^(\\d{4})(?:\\d+)?-$"));
    bool found_at_least_one_match(false);
    for (const auto &individual_range : individual_ranges) {
        if (not matcher->matched(individual_range))
            continue;

        unsigned start_year;
        if (not StringUtil::ToUnsigned((*matcher)[1], &start_year)) {
            logger->warning("can't convert \"" + (*matcher)[1] + "\" to an unsigned start year!");
            continue;
        }

        ranges->emplace_back(Range(Range::VOLUME_WILDCARD, start_year, Range::VOLUME_WILDCARD, Range::YEAR_WILDCARD));
        found_at_least_one_match = true;
    }

    return found_at_least_one_match;
}


bool ParseRanges12(const std::vector<std::string> &individual_ranges, std::vector<Range> * const ranges) {
    static RegexMatcher * const matcher(RegexMatcher::RegexMatcherFactory("^(\\d+)\\.(\\d{4})(?:/\\d+)?-(\\d{4})$"));
    bool found_at_least_one_match(false);
    for (const auto &individual_range : individual_ranges) {
        if (not matcher->matched(individual_range))
            continue;

        unsigned start_volume;
        if (not StringUtil::ToUnsigned((*matcher)[1], &start_volume)) {
            logger->warning("can't convert \"" + (*matcher)[1] + "\" to an unsigned start volume!");
            continue;
        }

        unsigned start_year;
        if (not StringUtil::ToUnsigned((*matcher)[2], &start_year)) {
            logger->warning("can't convert \"" + (*matcher)[2] + "\" to an unsigned start year!");
            continue;
        }

        unsigned end_year;
        if (not StringUtil::ToUnsigned((*matcher)[3], &end_year)) {
            logger->warning("can't convert \"" + (*matcher)[3] + "\" to an unsigned end year!");
            continue;
        }

        ranges->emplace_back(Range(start_volume, start_year, Range::VOLUME_WILDCARD, end_year));
        found_at_least_one_match = true;
    }

    return found_at_least_one_match;
}


bool ParseRanges13(const std::vector<std::string> &individual_ranges, std::vector<Range> * const ranges) {
    static RegexMatcher * const matcher(RegexMatcher::RegexMatcherFactory("^(\\d+)/(\\d+)\\.(\\d{4})/(\\d{2})$"));
    bool found_at_least_one_match(false);
    for (const auto &individual_range : individual_ranges) {
        if (not matcher->matched(individual_range))
            continue;

        unsigned start_volume;
        if (not StringUtil::ToUnsigned((*matcher)[1], &start_volume)) {
            logger->warning("can't convert \"" + (*matcher)[1] + "\" to an unsigned start volume!");
            continue;
        }

        unsigned end_volume;
        if (not StringUtil::ToUnsigned((*matcher)[2], &end_volume)) {
            logger->warning("can't convert \"" + (*matcher)[2] + "\" to an unsigned end volume!");
            continue;
        }

        unsigned start_year;
        if (not StringUtil::ToUnsigned((*matcher)[3], &start_year)) {
            logger->warning("can't convert \"" + (*matcher)[3] + "\" to an unsigned start year!");
            continue;
        }

        const std::string end_year_candidate((*matcher)[3].substr(0, 2) + (*matcher)[4]);
        unsigned end_year;
        if (not StringUtil::ToUnsigned(end_year_candidate, &end_year)) {
            logger->warning("can't convert \"" + end_year_candidate + "\" to an unsigned end year!");
            continue;
        }

        ranges->emplace_back(Range(start_volume, start_year, end_volume, end_year));
        found_at_least_one_match = true;
    }

    return found_at_least_one_match;
}


bool ParseRanges14(const std::vector<std::string> &individual_ranges, std::vector<Range> * const ranges) {
    static RegexMatcher * const matcher(RegexMatcher::RegexMatcherFactory("^(\\d+)/(\\d+)\\.(\\d{4})/(\\d{4})$"));
    bool found_at_least_one_match(false);
    for (const auto &individual_range : individual_ranges) {
        if (not matcher->matched(individual_range))
            continue;

        unsigned start_volume;
        if (not StringUtil::ToUnsigned((*matcher)[1], &start_volume)) {
            logger->warning("can't convert \"" + (*matcher)[1] + "\" to an unsigned start volume!");
            continue;
        }

        unsigned end_volume;
        if (not StringUtil::ToUnsigned((*matcher)[2], &end_volume)) {
            logger->warning("can't convert \"" + (*matcher)[2] + "\" to an unsigned end volume!");
            continue;
        }

        unsigned start_year;
        if (not StringUtil::ToUnsigned((*matcher)[3], &start_year)) {
            logger->warning("can't convert \"" + (*matcher)[3] + "\" to an unsigned start year!");
            continue;
        }

        unsigned end_year;
        if (not StringUtil::ToUnsigned((*matcher)[4], &end_year)) {
            logger->warning("can't convert \"" + (*matcher)[4] + "\" to an unsigned end year!");
            continue;
        }

        ranges->emplace_back(Range(start_volume, start_year, end_volume, end_year));
        found_at_least_one_match = true;
    }

    return found_at_least_one_match;
}


bool ParseRanges15(const std::vector<std::string> &individual_ranges, std::vector<Range> * const ranges) {
    static RegexMatcher * const matcher(RegexMatcher::RegexMatcherFactory("^(\\d{4})/(\\d{4})$"));
    bool found_at_least_one_match(false);
    for (const auto &individual_range : individual_ranges) {
        if (not matcher->matched(individual_range))
            continue;

        unsigned start_year;
        if (not StringUtil::ToUnsigned((*matcher)[1], &start_year)) {
            logger->warning("can't convert \"" + (*matcher)[1] + "\" to an unsigned start year!");
            continue;
        }

        unsigned end_year;
        if (not StringUtil::ToUnsigned((*matcher)[2], &end_year)) {
            logger->warning("can't convert \"" + (*matcher)[2] + "\" to an unsigned end year!");
            continue;
        }

        ranges->emplace_back(Range(Range::VOLUME_WILDCARD, start_year, Range::VOLUME_WILDCARD, end_year));
        found_at_least_one_match = true;
    }

    return found_at_least_one_match;
}


bool ParseRanges16(const std::vector<std::string> &individual_ranges, std::vector<Range> * const ranges) {
    static RegexMatcher * const matcher(RegexMatcher::RegexMatcherFactory(
        "^(\\d+).(\\d{4})-(?:\\d+/)(\\d+).(\\d{4})$"));
    bool found_at_least_one_match(false);
    for (const auto &individual_range : individual_ranges) {
        if (not matcher->matched(individual_range))
            continue;

        unsigned start_volume;
        if (not StringUtil::ToUnsigned((*matcher)[1], &start_volume)) {
            logger->warning("can't convert \"" + (*matcher)[1] + "\" to an unsigned start volume!");
            continue;
        }

        unsigned start_year;
        if (not StringUtil::ToUnsigned((*matcher)[2], &start_year)) {
            logger->warning("can't convert \"" + (*matcher)[2] + "\" to an unsigned start year!");
            continue;
        }

        unsigned end_volume;
        if (not StringUtil::ToUnsigned((*matcher)[3], &end_volume)) {
            logger->warning("can't convert \"" + (*matcher)[3] + "\" to an unsigned end volume!");
            continue;
        }

        unsigned end_year;
        if (not StringUtil::ToUnsigned((*matcher)[4], &end_year)) {
            logger->warning("can't convert \"" + (*matcher)[4] + "\" to an unsigned end year!");
            continue;
        }

        ranges->emplace_back(Range(start_volume, start_year, end_volume, end_year));
        found_at_least_one_match = true;
    }

    return found_at_least_one_match;
}


bool ParseRanges17(const std::vector<std::string> &individual_ranges, std::vector<Range> * const ranges) {
    static RegexMatcher * const matcher(RegexMatcher::RegexMatcherFactory("^(\\d{4})-(\\d+).(\\d{4})$"));
    bool found_at_least_one_match(false);
    for (const auto &individual_range : individual_ranges) {
        if (not matcher->matched(individual_range))
            continue;

        unsigned start_year;
        if (not StringUtil::ToUnsigned((*matcher)[1], &start_year)) {
            logger->warning("can't convert \"" + (*matcher)[1] + "\" to an unsigned start year!");
            continue;
        }

        unsigned end_volume;
        if (not StringUtil::ToUnsigned((*matcher)[2], &end_volume)) {
            logger->warning("can't convert \"" + (*matcher)[2] + "\" to an unsigned end volume!");
            continue;
        }

        unsigned end_year;
        if (not StringUtil::ToUnsigned((*matcher)[3], &end_year)) {
            logger->warning("can't convert \"" + (*matcher)[3] + "\" to an unsigned end year!");
            continue;
        }

        ranges->emplace_back(Range(Range::VOLUME_WILDCARD, start_year, end_volume, end_year));
        found_at_least_one_match = true;
    }

    return found_at_least_one_match;
}


// Match cases like 1.1972-1995/96
bool ParseRanges18(const std::vector<std::string> &individual_ranges, std::vector<Range> * const ranges) {
    static RegexMatcher * const matcher(RegexMatcher::RegexMatcherFactory(
        "^(\\d+)\\.(\\d{4})-(\\d{4})/(\\d{2})$"));
    bool found_at_least_one_match(false);
    for (const auto &individual_range : individual_ranges) {
        if (not matcher->matched(individual_range))
            continue;

        unsigned start_volume;
        if (not StringUtil::ToUnsigned((*matcher)[1], &start_volume)) {
            logger->warning("can't convert \"" + (*matcher)[1] + "\" to an unsigned start volume!");
            continue;
        }

        unsigned start_year;
        if (not StringUtil::ToUnsigned((*matcher)[2], &start_year)) {
            logger->warning("can't convert \"" + (*matcher)[2] + "\" to an unsigned start year!");
            continue;
        }

        unsigned end_year;
        const std::string end_year_candidate((*matcher)[3].substr(0, 2) + (*matcher)[4]);
        if (not StringUtil::ToUnsigned(end_year_candidate, &end_year)) {
            logger->warning("can't convert \"" + end_year_candidate + "\" to an unsigned end year!");
            continue;
        }

        ranges->emplace_back(Range(start_volume, start_year, Range::VOLUME_WILDCARD, end_year));
        found_at_least_one_match = true;
    }

    return found_at_least_one_match;
}


// Match cases like 1.1985-6/7.1990/91
bool ParseRanges19(const std::vector<std::string> &individual_ranges, std::vector<Range> * const ranges) {
    static RegexMatcher * const matcher(RegexMatcher::RegexMatcherFactory(
        "^(\\d+)\\.(\\d{4})-(?:\\d+/)(\\d+)(\\d{4})/(\\d{2})$"));
    bool found_at_least_one_match(false);
    for (const auto &individual_range : individual_ranges) {
        if (not matcher->matched(individual_range))
            continue;

        unsigned start_volume;
        if (not StringUtil::ToUnsigned((*matcher)[1], &start_volume)) {
            logger->warning("can't convert \"" + (*matcher)[1] + "\" to an unsigned start volume!");
            continue;
        }

        unsigned start_year;
        if (not StringUtil::ToUnsigned((*matcher)[2], &start_year)) {
            logger->warning("can't convert \"" + (*matcher)[2] + "\" to an unsigned start year!");
            continue;
        }

        unsigned end_volume;
        if (not StringUtil::ToUnsigned((*matcher)[3], &end_volume)) {
            logger->warning("can't convert \"" + (*matcher)[3] + "\" to an unsigned end volume!");
            continue;
        }

        unsigned end_year;
        const std::string end_year_candidate((*matcher)[4].substr(0, 2) + (*matcher)[5]);
        if (not StringUtil::ToUnsigned(end_year_candidate, &end_year)) {
            logger->warning("can't convert \"" + end_year_candidate + "\" to an unsigned end year!");
            continue;
        }

        ranges->emplace_back(Range(start_volume, start_year, end_volume, end_year));
        found_at_least_one_match = true;
    }

    return found_at_least_one_match;
}


// Match cases like 1890/95-1896/1900
bool ParseRanges20(const std::vector<std::string> &individual_ranges, std::vector<Range> * const ranges) {
    static RegexMatcher * const matcher(RegexMatcher::RegexMatcherFactory("^(\\d{4})(?:/\\d+)-(?:\\d{4})/(\\d{4})$"));
    bool found_at_least_one_match(false);
    for (const auto &individual_range : individual_ranges) {
        if (not matcher->matched(individual_range))
            continue;

        unsigned start_year;
        if (not StringUtil::ToUnsigned((*matcher)[1], &start_year)) {
            logger->warning("can't convert \"" + (*matcher)[1] + "\" to an unsigned start year!");
            continue;
        }

        unsigned end_year;
        if (not StringUtil::ToUnsigned((*matcher)[2], &end_year)) {
            logger->warning("can't convert \"" + (*matcher)[2] + "\" to an unsigned end year!");
            continue;
        }

        ranges->emplace_back(Range(Range::VOLUME_WILDCARD, start_year, Range::VOLUME_WILDCARD, end_year));
        found_at_least_one_match = true;
    }

    return found_at_least_one_match;
}


// Match cases like 1/8.1947/55-
bool ParseRanges21(const std::vector<std::string> &individual_ranges, std::vector<Range> * const ranges) {
    static RegexMatcher * const matcher(RegexMatcher::RegexMatcherFactory(
        "^(\\d+)(?:/\\d+)\\.(\\d{4})/(\\d{2})-$"));
    bool found_at_least_one_match(false);
    for (const auto &individual_range : individual_ranges) {
        if (not matcher->matched(individual_range))
            continue;

        unsigned start_volume;
        if (not StringUtil::ToUnsigned((*matcher)[1], &start_volume)) {
            logger->warning("can't convert \"" + (*matcher)[1] + "\" to an unsigned start volume!");
            continue;
        }

        unsigned start_year;
        if (not StringUtil::ToUnsigned((*matcher)[2], &start_year)) {
            logger->warning("can't convert \"" + (*matcher)[2] + "\" to an unsigned start year!");
            continue;
        }

        const std::string end_year_candidate((*matcher)[2].substr(0, 2) + (*matcher)[3]);
        unsigned end_year;
        if (not StringUtil::ToUnsigned(end_year_candidate, &end_year)) {
            logger->warning("can't convert \"" + end_year_candidate + "\" to an unsigned end year!");
            continue;
        }

        ranges->emplace_back(Range(start_volume, start_year, Range::VOLUME_WILDCARD, end_year));
        found_at_least_one_match = true;
    }

    return found_at_least_one_match;
}


// Match cases like 1.1953-70/71.1984/85
bool ParseRanges22(const std::vector<std::string> &individual_ranges, std::vector<Range> * const ranges) {
    static RegexMatcher * const matcher(RegexMatcher::RegexMatcherFactory(
        "^(\\d+)(?:/\\d+)?\\.(\\d{4})-(?:\\d+/)?(\\d+)\\.(\\d{4})/(\\d{2})$"));
    bool found_at_least_one_match(false);
    for (const auto &individual_range : individual_ranges) {
        if (not matcher->matched(individual_range))
            continue;

        unsigned start_volume;
        if (not StringUtil::ToUnsigned((*matcher)[1], &start_volume)) {
            logger->warning("can't convert \"" + (*matcher)[1] + "\" to an unsigned start volume!");
            continue;
        }

        unsigned start_year;
        if (not StringUtil::ToUnsigned((*matcher)[2], &start_year)) {
            logger->warning("can't convert \"" + (*matcher)[2] + "\" to an unsigned start year!");
            continue;
        }

        unsigned end_volume;
        if (not StringUtil::ToUnsigned((*matcher)[3], &end_volume)) {
            logger->warning("can't convert \"" + (*matcher)[3] + "\" to an unsigned end volume!");
            continue;
        }

        const std::string end_year_candidate((*matcher)[4].substr(0, 2) + (*matcher)[5]);
        unsigned end_year;
        if (not StringUtil::ToUnsigned(end_year_candidate, &end_year)) {
            logger->warning("can't convert \"" + end_year_candidate + "\" to an unsigned end year!");
            continue;
        }

        ranges->emplace_back(Range(start_volume, start_year, end_volume, end_year));
        found_at_least_one_match = true;
    }

    return found_at_least_one_match;
}


void ParseRanges(const std::string &_866a_contents, std::vector<Range> * const ranges) {
    ranges->clear();

    ++good_match_count;

    std::vector<std::string> individual_ranges;
    StringUtil::SplitThenTrimWhite(_866a_contents, ';', &individual_ranges);
    for (auto &individual_range : individual_ranges) {
        unsigned trailing_issue1, trailing_issue2;
        CleanupRange(&individual_range, &trailing_issue1, &trailing_issue2);
    }

    if (ParseRanges1(individual_ranges, ranges))
        return;
    if (ParseRanges2(individual_ranges, ranges))
        return;
    if (ParseRanges3(individual_ranges, ranges))
        return;
    if (ParseRanges4(individual_ranges, ranges))
        return;
    if (ParseRanges5(individual_ranges, ranges))
        return;
    if (ParseRanges6(individual_ranges, ranges))
        return;
    if (ParseRanges7(individual_ranges, ranges))
        return;
    if (ParseRanges8(individual_ranges, ranges))
        return;
    if (ParseRanges9(individual_ranges, ranges))
        return;
    if (ParseRanges10(individual_ranges, ranges))
        return;
    if (ParseRanges11(individual_ranges, ranges))
        return;
    if (ParseRanges12(individual_ranges, ranges))
        return;
    if (ParseRanges13(individual_ranges, ranges))
        return;
    if (ParseRanges14(individual_ranges, ranges))
        return;
    if (ParseRanges15(individual_ranges, ranges))
        return;
    if (ParseRanges16(individual_ranges, ranges))
        return;
    if (ParseRanges17(individual_ranges, ranges))
        return;
    if (ParseRanges18(individual_ranges, ranges))
        return;
    if (ParseRanges19(individual_ranges, ranges))
        return;
    if (ParseRanges20(individual_ranges, ranges))
        return;
    if (ParseRanges21(individual_ranges, ranges))
        return;
    if (ParseRanges22(individual_ranges, ranges))
        return;

    --good_match_count;
    ++bad_match_count;

    for (const auto &individual_range : individual_ranges)
        logger->warning("couldn't match range: \"" + individual_range + "\"!");
}


std::unordered_map<std::string, std::vector<Range>> parent_ppn_to_ranges_map;


bool ProcessSerialRecord(MarcRecord * const record, MarcWriter * const /*output*/, std::string * const /*err_msg*/) {
    if (not record->getLeader().isSerial())
        return true;

    std::vector<std::pair<size_t, size_t>> local_block_boundaries;
    record->findAllLocalDataBlocks(&local_block_boundaries);
    for (const auto &block_start_and_end : local_block_boundaries) {
        const std::string sigil(FindSigil(record, block_start_and_end));
        if (sigil != "DE-21" and sigil != "DE-21-110")
            continue;

        std::vector<size_t> field_indices;
        record->findFieldsInLocalBlock("866", "30", block_start_and_end, &field_indices);
        if (field_indices.empty())
            continue;

        for (size_t field_index : field_indices) {
            const std::string _866a_contents(record->extractFirstSubfield(field_index, 'a'));
            if (unlikely(_866a_contents.empty()))
                continue;

            std::vector<Range> ranges;
            ParseRanges(_866a_contents, &ranges);
            if (not ranges.empty())
                parent_ppn_to_ranges_map.emplace(record->getControlNumber(), ranges);
        }
    }

    return true;
}


bool ElectronicArticleIsAvailableInTuebingen(const MarcRecord &record) {
    if (MarcUtil::UBTueIsElectronicResource(record) and record.getLeader().isArticle()) {
        const std::string parent_ppn(MarcUtil::GetParentPPN(record));
        if (parent_ppn.empty())
            return false;

        const auto parent_ppn_and_ranges(parent_ppn_to_ranges_map.find(parent_ppn));
        if (parent_ppn_and_ranges == parent_ppn_to_ranges_map.end())
            return false;

        const Subfields _936_subfields(record.getSubfields("936"));
        const std::string issue_string(_936_subfields.getFirstSubfieldValue('e'));
        const std::string year_string(_936_subfields.getFirstSubfieldValue('j'));
        const std::string volume_string(_936_subfields.getFirstSubfieldValue('d'));
        if (issue_string.empty() and year_string.empty() and volume_string.empty())
            return false;
        unsigned issue;
        if (not StringUtil::ToUnsigned(issue_string, &issue))
            issue = Range::ISSUE_WILDCARD;
        unsigned year;
        if (not StringUtil::ToUnsigned(year_string, &year))
            return false; // Need at least the year!
        unsigned volume;
        if (not StringUtil::ToUnsigned(volume_string, &volume))
            volume = Range::VOLUME_WILDCARD;

        for (const auto &range : parent_ppn_and_ranges->second) {
            if (range.matched(issue, year, volume))
                return true;
        }
    }

    return false;
}


bool Get856URLAndAnchor(const std::string &_856_field_contents, std::string * const url, std::string * const anchor) {
    url->clear(), anchor->clear();

    const Subfields subfields(_856_field_contents);
    if (subfields.hasSubfield('u')) {
        *url = subfields.getFirstSubfieldValue('u');
        if (url->empty())
            return false;

        const std::string x_subfield(subfields.getFirstSubfieldValue('x'));
        if (x_subfield.empty())
            *anchor = "Tübingen Online Resource";
        else
            *anchor = HtmlUtil::HtmlEscape(x_subfield);

        return true;
    }

    return false;
}


bool ProcessRecord(MarcRecord * const record, MarcWriter * const marc_writer, std::string * const /*err_msg*/) {
    std::vector <std::pair<size_t, size_t>> local_block_boundaries;
    record->findAllLocalDataBlocks(&local_block_boundaries);

    bool modified_record(false);
    std::set<std::string> alread_seen_urls;
    for (const auto &block_start_and_end : local_block_boundaries) {
        std::vector <size_t> _852_field_indices;
        if (record->findFieldsInLocalBlock("852", "??", block_start_and_end, &_852_field_indices) == 0)
            continue;
        for (const size_t _852_index : _852_field_indices) {
            const Subfields subfields1(record->getSubfields(_852_index));
            const std::string not_available_subfield(subfields1.getFirstSubfieldValue('z'));
            if (not_available_subfield == "Kein Bestand am IfK; Nachweis für KrimDok")
                goto final_processing;

            // Only ordered but not actually available?
            if (subfields1.getFirstSubfieldValue('m') == "e")
                goto final_processing;

            const std::string isil_subfield(subfields1.getFirstSubfieldValue('a'));
            if (isil_subfield != "DE-21" and isil_subfield != "DE-21-110")
                continue;

            std::string detailed_availability;
            std::vector <size_t> _866_field_indices;
            if (record->findFieldsInLocalBlock("866", "30", block_start_and_end, &_866_field_indices) > 0) {
                for (const size_t _866_index : _866_field_indices) {
                    const Subfields subfields2(record->getSubfields(_866_index));
                    const std::string subfield_a(subfields2.getFirstSubfieldValue('a'));
                    if (not subfield_a.empty()) {
                        if (not detailed_availability.empty())
                            detailed_availability += "; ";
                        detailed_availability += subfield_a;
                        const std::string subfield_z(subfields2.getFirstSubfieldValue('z'));
                        if (not subfield_z.empty())
                            detailed_availability += " " + subfield_z;
                    }
                }
            }

            const std::string institution(isil_subfield == "DE-21" ? "UB: " : "IFK: ");
            if (_852_index + 1 < block_start_and_end.second) {
                const Subfields subfields2(record->getSubfields(_852_index + 1));
                const std::string call_number_subfield(subfields2.getFirstSubfieldValue('c'));
                if (not call_number_subfield.empty()) {
                    const std::string institution_and_call_number(institution + call_number_subfield);
                    ++add_sig_count;
                    modified_record = true;
                    record->insertSubfield("SIG", 'a', institution_and_call_number
                                           + (detailed_availability.empty() ? ""
                                                                            : "(" + detailed_availability + ")"));
                } else { // Look for a URL.
                    std::vector <size_t> _856_field_indices;
                    if (record->findFieldsInLocalBlock("856", "4 ", block_start_and_end, &_856_field_indices) > 0) {
                        std::string url, anchor;
                        if (not Get856URLAndAnchor(record->getFieldData(_856_field_indices.front()), &url, &anchor))
                            continue;
                        if (alread_seen_urls.find(url) == alread_seen_urls.cend()) {
                            alread_seen_urls.insert(url);
                            record->insertSubfield("SIG", 'a', "<a href=\"" + url + "\">" + anchor + "</a>");
                            modified_record = true;
                        }
                    }
                }
            }
            break;
        }
    }

final_processing:
    if (modified_record)
        ++modified_record_count;
    else if (ElectronicArticleIsAvailableInTuebingen(*record)) {
        std::string url, anchor;
        if (Get856URLAndAnchor(record->getFieldData("856"), &url, &anchor)) {
            if (alread_seen_urls.find(url) == alread_seen_urls.cend()) {
                alread_seen_urls.insert(url);
                record->insertSubfield("SIG", 'a', "<a href=\"" + url + "\">" + anchor + "</a>");
                ++modified_record_count;
            }
        }
    }

    marc_writer->write(*record);

    return true;
}


std::string Percentage(const float fraction, const float total) {
    if (total == 0.0f)
        return "NaN";

    char buffer[30];
    std::sprintf(buffer, "%g.2%%", fraction * 100.0f / total);
    return buffer;
}


void PopulateTheInTuebingenAvailableField(const bool verbose, MarcReader * const marc_reader,
                                          MarcWriter * const marc_writer)
{
    std::string err_msg;
    if (not MarcRecord::ProcessRecords(marc_reader, ProcessSerialRecord, marc_writer, &err_msg))
        logger->error("error while processing serial records: " + err_msg);

    if (verbose) {
        std::cout << "Successfully matched " << Percentage(good_match_count, good_match_count + bad_match_count)
                  << " (" << good_match_count << ") publication ranges.\n";
        std::cout << "Failed to match " << Percentage(bad_match_count, good_match_count + bad_match_count)
                  << " (" << bad_match_count << ") publication ranges.\n";
    }

    marc_reader->rewind();
    if (not MarcRecord::ProcessRecords(marc_reader, ProcessRecord, marc_writer, &err_msg))
        logger->error("error while processing records: " + err_msg);

    if (verbose) {
        std::cout << "Modified " << modified_record_count << " records.\n";
        std::cout << "Added " << add_sig_count << " signature fields.\n";
    }
}


int main(int argc, char **argv) {
    ::progname = argv[0];

    if (argc != 3 and argc != 4)
        Usage();

    bool verbose;
    if (argc == 3)
        verbose = false;
    else { // argc == 4
        if (std::strcmp(argv[1], "--verbose") != 0)
            Usage();
        verbose = true;
    }

    try {
        std::unique_ptr<MarcReader> marc_reader(MarcReader::Factory(argv[argc == 3 ? 1 : 2]));
        std::unique_ptr<MarcWriter> marc_writer(MarcWriter::Factory(argv[argc == 3 ? 2 : 3]));
        PopulateTheInTuebingenAvailableField(verbose, marc_reader.get(), marc_writer.get());
    } catch (const std::exception &x) {
        logger->error("caught exception: " + std::string(x.what()));
    }
}

/** \brief A MARC-21 filter utility filters a collection of MARC records based on values in a Zeder column.
 *  \author Dr. Johannes Ruscheinski (johannes.ruscheinski@uni-tuebingen.de)
 *
 *  \copyright 2021 Universitätsbibliothek Tübingen.  All rights reserved.
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

#include <unordered_set>
#include <cstdlib>
#include "MARC.h"
#include "RegexMatcher.h"
#include "StringUtil.h"
#include "util.h"
#include "Zeder.h"


namespace {


std::unordered_set<std::string> GetMatchingJournalPPNs(const Zeder::Flavour zeder_flavour, const std::string &match_column,
                                                       RegexMatcher * const regex_matcher,
                                                       std::unordered_set<std::string> * const zdb_numbers)
{
    std::unordered_set<std::string> matching_journal_ppns;

    Zeder::SimpleZeder zeder(zeder_flavour, { "eppn", "pppn", "pzdb", "ezdb", match_column });
    if (not zeder)
        LOG_ERROR("we can't connect to the Zeder MySQL database!");
    if (unlikely(zeder.empty()))
        LOG_ERROR("found no Zeder entries matching any of our requested columns!");

    unsigned journal_count(0), match_count(0);
    for (const auto &journal : zeder) {
        ++journal_count;
        if (not journal.hasAttribute(match_column) or not regex_matcher->matched(journal.lookup(match_column)))
            continue;

        std::set<std::string> print_ppns;
        StringUtil::SplitThenTrimWhite(journal.lookup("pppn"), ',', &print_ppns);
        std::set<std::string> online_ppns;
        StringUtil::SplitThenTrimWhite(journal.lookup("eppn"), ',', &online_ppns);
        if (print_ppns.empty() and online_ppns.empty()) {
            const auto zeder_id(std::to_string(journal.getId()));
            LOG_WARNING("Zeder entry #" + zeder_id + " is missing print and online PPN's!");
            continue;
        } else {
            matching_journal_ppns.insert(print_ppns.cbegin(), print_ppns.cend());
            matching_journal_ppns.insert(online_ppns.cbegin(), online_ppns.cend());
            ++match_count;
        }

        std::set<std::string> print_zdb_numbers;
        StringUtil::SplitThenTrimWhite(journal.lookup("pzdb"), ',', &print_zdb_numbers);
        zdb_numbers->insert(print_zdb_numbers.cbegin(), print_zdb_numbers.cend());
        std::set<std::string> online_zdb_numbers;
        StringUtil::SplitThenTrimWhite(journal.lookup("ezdb"), ',', &online_zdb_numbers);
        zdb_numbers->insert(online_zdb_numbers.cbegin(), online_zdb_numbers.cend());
    }

    LOG_INFO("Processed " + std::to_string(journal_count) + " Zeder journal(s) and found "
             + std::to_string(match_count) + " matching journal(s) from which "
             + std::to_string(matching_journal_ppns.size()) + " PPN's were extracted!");

    return matching_journal_ppns;
}


void ProcessRecords(const bool filter_on_zdb_numbers, const std::unordered_set<std::string> &journal_ppns,
                    const std::unordered_set<std::string> &journal_zdb_numbers,
                    MARC::Reader * const marc_reader, MARC::Writer * const marc_writer)
{
    unsigned total_record_count(0), matched_record_count(0);
    while (const auto record = marc_reader->read()) {
        ++total_record_count;

        if (journal_ppns.find(record.getSuperiorControlNumber()) != journal_ppns.cend()) {
            if (filter_on_zdb_numbers) {
                const auto zdb_number(record.getZDBNumber());
                if (not zdb_number.empty() and journal_zdb_numbers.find(zdb_number) == journal_zdb_numbers.cend())
                    continue; // Skip current record!
            }

            ++matched_record_count;
            marc_writer->write(record);
        }
    }

    LOG_INFO("Processed " + std::to_string(total_record_count) + " record(s) of which "
             + std::to_string(matched_record_count) + " record(s) matched.");
}


[[noreturn]] void Usage() {
    ::Usage("[--filter-on-zdb-numbers] zeder_flavour match_column column_regex marc_input marc_output\n\n"
            "Extracts all records from \"marc_input\" which have superior PPN's in Zeder columns pppn and eppn\n"
            "and Zeder column \"match_column\" matches the PCRE \"column_regex\".\n"
            "If --filter-on-zdb-numbers has been specified all inferior works that have their own ZDB number, which\n"
            "is not a ZDB number of any of the superior PPN's will be omitted.\n");
}


} // namespace


int Main(int argc, char *argv[]) {
    if (argc != 6 and argc != 7)
        Usage();

    bool filter_on_zdb_numbers(false);
    if (argc == 7) {
        if (__builtin_strcmp(argv[1], "--filter-on-zdb-numbers") != 0)
            Usage();
        filter_on_zdb_numbers = true;
        --argc, ++argv;
    }

    Zeder::Flavour zeder_flavour;
    if (__builtin_strcmp(argv[1], "ixtheo") == 0)
        zeder_flavour = Zeder::IXTHEO;
    else if (__builtin_strcmp(argv[1], "krimdok") == 0)
        zeder_flavour = Zeder::KRIMDOK;
    else
        LOG_ERROR("zeder_flavour must be one of (ixtheo,krimdok)!");

    const std::string match_column(argv[2]);

    const std::string column_regex(argv[3]);
    std::string err_msg;
    auto regex_matcher(RegexMatcher::RegexMatcherFactory(column_regex, &err_msg));
    if (regex_matcher == nullptr)
        LOG_ERROR("failed to compile column_regex \"" + column_regex +"\": " + err_msg);

    const auto marc_reader(MARC::Reader::Factory(argv[4]));
    const auto marc_writer(MARC::Writer::Factory(argv[5]));

    std::unordered_set<std::string> matching_journal_zdb_numbers;
    const auto matching_journal_ppns(GetMatchingJournalPPNs(zeder_flavour, match_column, regex_matcher,
                                                            &matching_journal_zdb_numbers));
    delete regex_matcher;
    ProcessRecords(filter_on_zdb_numbers, matching_journal_ppns, matching_journal_zdb_numbers,
                   marc_reader.get(), marc_writer.get());

    return EXIT_SUCCESS;
}

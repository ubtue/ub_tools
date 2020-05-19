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
#include <unordered_map>
#include <unordered_set>
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


struct JournalDescriptor {
    std::set<std::string> print_issn_;
    std::string online_issn_;
    std::string print_ppn_;
    std::string online_ppn_;
    std::string title_;
public:
    JournalDescriptor(const std::string &print_issn, const std::string &online_issn, const std::string &print_ppn,
                      const std::string &online_ppn, const std::string &title)
        : print_issn_(print_issn), online_issn_(online_issn), print_ppn_(print_ppn), online_ppn_(online_ppn),
          title_(title) { }
    void serialize(File * const output) const;
};


void JournalDescriptor::serialize(File * const output) const {
    if (not print_issn_.empty()) {
        (*output) << print_issn_ << ':' << EscapeColons(title_) << ':' << (online_issn_.empty() ? print_issn_ : online_issn_)
                  << (online_ppn_.empty() ? print_ppn_ : online_ppn_) << '\n';
    }

    if (not online_issn_.empty() and not online_ppn_.empty())
        (*output) << online_issn_ << ':' << EscapeColons(title_) << ':' << online_ppn_ << '\n';
}


unsigned ProcessZeder(const Zeder::SimpleZeder &zeder,
                      std::unordered_map<std::string, JournalDescriptor *> * const issns_to_journal_descs)
{
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


void WriteMapFile(const std::string &map_filename, const std::unordered_map<std::string, JournalDescriptor *> &issns_to_journal_descs) {
    const auto map_file(FileUtil::OpenOutputFileOrDie(map_filename));
    std::unordered_set<JournalDescriptor *> already_seen;
    for (const auto journal : issns_to_journal_descs) {
        if (already_seen.find(journal) == already_seen.end()) {
            already_seen.emplaced(journal);
            journal->serialize(map_file);
        }
    }
}


unsigned ProcessZederFlavour(const Zeder::Flavour zeder_flavour,
                             std::unordered_map<std::string, JournalDescriptor *> * const issns_to_journal_descs)
{
    const Zeder::SimpleZeder zeder(zeder_flavour, { "eppns", "essn", "issn", "tit" });
    if (unlikely(zeder.empty()))
        LOG_ERROR("found no IxTheo Zeder entries matching any of our requested columns!");
    return ProcessZeder(issns_to_journal_descs, zeder);
}


unsigned ProcessMARC(const std::string &filename, std::unordered_map<std::string, JournalDescriptor *> * const issns_to_journal_descs) {
    const auto marc_reader(MARC::Reader::Factory(filename));
    const unsigned journal_count(0);
    for (const auto record : marc_reader.read()) {
        if (not record.isSerial())
            continue;

        ++journal_count;
    }

    LOG_INFO("Found " + std::to_string(journal_count) + " journals in \"" + filename + "\".");
    return journal_count;
}


} // unnamed namespace


int Main(int argc, char **argv) {
    if (argc != 4)
        ::Usage("unmerged_ixtheo_marc_titles unmerged_krimdok_marc_titles mapfile_output");

    std::unordered_map<std::string, JournalDescriptor *> issns_to_journal_descs;

    unsigned total_generated_count(0);
    total_generated_count += ProcessZederFlavour(Zeder::IXTHEO, &issns_to_journal_descs);
    total_generated_count += ProcessZederFlavour(Zeder::KRIMDOK, &issns_to_journal_descs);
    total_generated_count += ProcessMARC(argv[2], &issns_to_journal_descs);
    total_generated_count += ProcessMARC(argv[3], &issns_to_journal_descs);
    LOG_INFO("Generated " + std::to_string(total_generated_count) + " map entry/entries.");

    const std::string output_filename(argv[1]);
    WriteMapFile(output_filename, issns_to_journal_descs);

    return EXIT_SUCCESS;
}

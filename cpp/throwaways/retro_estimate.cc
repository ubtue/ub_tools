/** \file   retro_estimate.cc
 *  \brief  Estimates the number of articles that need to be retrospetively
 *          digitized for a certain grant proposal.
 *  \author Dr. Johannes Ruscheinski (johannes.ruscheinski@uni-tuebingen.de)
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

#include <set>
#include <unordered_map>
#include <unordered_set>
#include "Compiler.h"
#include "FileUtil.h"
#include "Locale.h"
#include "MARC.h"
#include "StringUtil.h"
#include "TextUtil.h"
#include "Zeder.h"
#include "util.h"


namespace {


struct Journal {
    std::string title_;
    unsigned evaluation_range_start_year_;  // From the ausf column
    unsigned publication_range_start_year_; // From the eved column
    std::string pppn_;
    std::string eppn_;
    std::string ausfst_;
    std::string ber_;
    std::string liz_;
    unsigned pppn_article_count_;
    unsigned eppn_article_count_;

public:
    Journal(const std::string &title, const unsigned evaluation_range_start_year, const unsigned publication_range_start_year,
            const std::string &pppn, const std::string &eppn, const std::string &ausfst, const std::string &ber, const std::string &liz)
        : title_(title), evaluation_range_start_year_(evaluation_range_start_year),
          publication_range_start_year_(publication_range_start_year), pppn_(pppn), eppn_(eppn), ausfst_(ausfst), ber_(ber), liz_(liz),
          pppn_article_count_(0), eppn_article_count_(0) { }
    unsigned getNoOfRetroYears() const { return evaluation_range_start_year_ - publication_range_start_year_; }
    unsigned getTotalArticleCount() const { return pppn_article_count_ + eppn_article_count_; }
    void incrementArticleCount(const std::string &parent_ppn) {
        if (parent_ppn == pppn_)
            ++pppn_article_count_;
        else
            ++eppn_article_count_;
    }
    unsigned getThresholdYear() const { return (evaluation_range_start_year_ > 2010) ? 2019 : 2012; }
};


const std::unordered_set<std::string> ZEDER_COLUMNS{ "ausf", "ausfst", "ber", "eved", "eppn", "liz", "pppn", "retro2", "tit" };


bool StartsWithAPlausibleYear(const std::string &s) {
    if (s.length() < 4 or not StringUtil::ConsistsOfAllASCIIDigits(s.substr(0, 4)))
        return false;
    return s.substr(0, 4) >= "1000" and s.substr(0, 4) <= "2020";
}


void CollectJournalsFromZeder(const Zeder::SimpleZeder &zeder, std::unordered_map<std::string, Journal *> * const ppns_to_journals_map) {
    unsigned useable_journal_count(0);
    for (const auto &journal : zeder) {
        if (not journal.hasAttribute("retro2") or journal.getAttribute("retro2") != "fid2021" or not journal.hasAttribute("ausf")
            or not journal.hasAttribute("eved"))
            continue;

        const auto ausf(journal.lookup("ausf"));
        unsigned evaluation_range_start_year;
        if (StartsWithAPlausibleYear(ausf))
            evaluation_range_start_year = StringUtil::ToUnsigned(ausf.substr(0, 4));
        else
            evaluation_range_start_year = 2020;

        const auto eved(journal.lookup("eved"));
        unsigned publication_range_start_year;
        if (eved.length() < 4 or not StringUtil::ToUnsigned(eved.substr(0, 4), &publication_range_start_year))
            continue;

        const auto pppn(journal.lookup("pppn"));
        const auto eppn(journal.lookup("eppn"));
        if (pppn.empty() and eppn.empty())
            continue;

        const auto new_journal(new Journal(journal.lookup("tit"), evaluation_range_start_year, publication_range_start_year, pppn, eppn,
                                           journal.lookup("ausfst"), journal.lookup("ber"), journal.lookup("liz")));
        if (not pppn.empty())
            ppns_to_journals_map->emplace(pppn, new_journal);
        if (not eppn.empty())
            ppns_to_journals_map->emplace(eppn, new_journal);

        ++useable_journal_count;
    }

    LOG_INFO("Found " + std::to_string(useable_journal_count) + " useable journal entries in Zeder.");
}


void ProcessRecords(MARC::Reader * const marc_reader, std::unordered_map<std::string, Journal *> * const ppns_to_journals_map) {
    while (const auto record = marc_reader->read()) {
        const auto parent_ppn(record.getSuperiorControlNumber());
        const auto parent_ppn_and_journal(ppns_to_journals_map->find(parent_ppn));
        if (parent_ppn_and_journal == ppns_to_journals_map->end())
            continue;

        const auto publication_year_str(record.getMostRecentPublicationYear());
        if (not StartsWithAPlausibleYear(publication_year_str))
            continue;

        const auto publication_year(StringUtil::ToUnsigned(publication_year_str));
        if (publication_year <= parent_ppn_and_journal->second->getThresholdYear())
            parent_ppn_and_journal->second->incrementArticleCount(parent_ppn);
    }
}


void GenerateCSVReport(File * const output, const std::unordered_map<std::string, Journal *> ppns_to_journals_map) {
    const char SEPARATOR(',');

    (*output) << "\"pppn\"" << SEPARATOR << "\"eppn\"" << SEPARATOR << "\"tit\"" << SEPARATOR << "\"ausf\"" << SEPARATOR << "\"eved\""
              << SEPARATOR << "\"retro-jahre\"" << SEPARATOR << "\"retro-artikel\"" << SEPARATOR << "\"auswertungs-jahre\"" << SEPARATOR
              << "\"artikeldurchschnitt\"" << SEPARATOR << "\"artikelzahl-pppn\"" << SEPARATOR << "\"artikelzahl-eppn\"" << SEPARATOR
              << "\"artikelzahl-gesamt\"" << SEPARATOR << "\"ausfst\"" << SEPARATOR << "\"ber\"" << SEPARATOR << "\"liz\"\n";

    Locale locale("de_DE.utf8", LC_NUMERIC);
    std::unordered_set<Journal *> already_processed;
    for (const auto &[ppn, journal] : ppns_to_journals_map) {
        if (already_processed.find(journal) != already_processed.end())
            continue;

        const unsigned no_of_evaluation_years(journal->getThresholdYear() - journal->evaluation_range_start_year_ + 1);
        const double average_article_count_per_year(journal->getTotalArticleCount() / double(no_of_evaluation_years));
        const double no_of_retro_articles(journal->getNoOfRetroYears() * average_article_count_per_year);
        (*output) << TextUtil::CSVEscape(journal->pppn_) << SEPARATOR << TextUtil::CSVEscape(journal->eppn_) << SEPARATOR
                  << TextUtil::CSVEscape(journal->title_) << SEPARATOR << journal->evaluation_range_start_year_ << SEPARATOR
                  << journal->publication_range_start_year_ << SEPARATOR << journal->getNoOfRetroYears() << SEPARATOR
                  << no_of_retro_articles << SEPARATOR << no_of_evaluation_years << SEPARATOR << average_article_count_per_year << SEPARATOR
                  << journal->pppn_article_count_ << SEPARATOR << journal->eppn_article_count_ << SEPARATOR
                  << journal->getTotalArticleCount() << SEPARATOR << TextUtil::CSVEscape(journal->ausfst_) << SEPARATOR
                  << TextUtil::CSVEscape(journal->ber_) << SEPARATOR << TextUtil::CSVEscape(journal->liz_) << '\n';
        already_processed.emplace(journal);
    }
}


} // unnamed namespace


int Main(int argc, char *argv[]) {
    if (argc != 3)
        ::Usage("[--min-log-level=min_verbosity] marc_titles csv_output");

    const Zeder::SimpleZeder zeder(Zeder::IXTHEO, ZEDER_COLUMNS);
    if (unlikely(zeder.empty()))
        LOG_ERROR("found no Zeder entries matching any of our requested columns!");

    std::unordered_map<std::string, Journal *> ppns_to_journals_map;
    CollectJournalsFromZeder(zeder, &ppns_to_journals_map);

    auto marc_reader(MARC::Reader::Factory(argv[1]));
    ProcessRecords(marc_reader.get(), &ppns_to_journals_map);

    const auto csv_output(FileUtil::OpenOutputFileOrDie(argv[2]));
    GenerateCSVReport(csv_output.get(), ppns_to_journals_map);

    return EXIT_SUCCESS;
}

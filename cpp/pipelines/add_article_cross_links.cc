/** \brief Tool for cross linking articles that are likely to refer to the same work.
 *  \author Dr. Johannes Ruscheinski (johannes.ruscheinski@uni-tuebingen.de)
 *
 *  \copyright 2018-2019 Universitätsbibliothek Tübingen.  All rights reserved.
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
#include <unordered_map>
#include <unordered_set>
#include <cstdio>
#include <cstdlib>
#include "Compiler.h"
#include "ControlNumberGuesser.h"
#include "FileUtil.h"
#include "MARC.h"
#include "MiscUtil.h"
#include "StringUtil.h"
#include "util.h"


namespace {


[[noreturn]] void Usage() {
    std::cerr << "Usage: " << ::progname << " [--min-log-level=min_verbosity] marc_input marc_output possible_matches_list\n";
    std::exit(EXIT_FAILURE);
}


struct RecordInfo {
    std::set<std::string> dois_, isbns_, issns_;
    enum Type { MONOGRAPH, SERIAL, ARTICLE, OTHER } type_;
    std::string year_, volume_, issue_;
    bool may_be_a_review_, is_electronic_;
};


const std::string YEAR_WILDCARD("????"), VOLUME_WILDCARD("?"), ISSUE_WILDCARD("?");


void ExtractYearVolumeIssue(const MARC::Record &record, RecordInfo * const record_info) {
    record_info->year_ = YEAR_WILDCARD;
    record_info->volume_ = VOLUME_WILDCARD;
    record_info->issue_ = ISSUE_WILDCARD;

    const auto _773_field(record.findTag("773"));
    const auto g_773_contents(_773_field->getFirstSubfieldWithCode('g'));
    if (not g_773_contents.empty()) {
        std::vector<std::string> subfields;
        std::vector<std::string> filtered_dates;
        for (const auto &field : record.getTagRange("773")) {
            if (field.getIndicator1() == '1') {
                for (const auto &subfield : field.getSubfields()) {
                    StringUtil::Split(subfield.value_, ':', &subfields, true);
                    filtered_dates.emplace_back(subfields[1]);
                }
            }
        }
        record_info->volume_ = filtered_dates[0];
        record_info->year_ = filtered_dates[1];
        record_info->issue_ = filtered_dates[2];
    } else {
        for (const auto &field : record.getTagRange("936")) {
            if (field.getIndicator1() != 'u' or field.getIndicator2() != 'w')
                continue;

            const MARC::Subfields subfields(field.getSubfields());

            const auto year(subfields.getFirstSubfieldWithCode('j'));
            if (not year.empty())
                record_info->year_ = year;

            const auto volume(subfields.getFirstSubfieldWithCode('d'));
            if (not volume.empty())
                record_info->volume_ = volume;

            const auto issue(subfields.getFirstSubfieldWithCode('e'));
            if (not issue.empty())
                record_info->issue_ = issue;
        }
    }
}


void CollectInfos(MARC::Reader * const marc_reader, std::unordered_map<std::string, RecordInfo> * const ppns_to_infos_map) {
    while (const auto record = marc_reader->read()) {
        RecordInfo new_info;
        new_info.dois_ = record.getDOIs();
        new_info.isbns_ = record.getISBNs();
        new_info.issns_ = record.getISSNs();
        if (record.isMonograph())
            new_info.type_ = RecordInfo::MONOGRAPH;
        else if (record.isSerial())
            new_info.type_ = RecordInfo::SERIAL;
        else if (record.isArticle())
            new_info.type_ = RecordInfo::ARTICLE;
        else
            new_info.type_ = RecordInfo::OTHER;
        ExtractYearVolumeIssue(record, &new_info);
        new_info.may_be_a_review_ = record.isPossiblyReviewArticle();
        new_info.is_electronic_ = record.isElectronicResource();

        (*ppns_to_infos_map)[record.getControlNumber()] = new_info;
    }
}


bool SetContainsOnlyArticlePPNs(const std::set<std::string> &ppns, const std::unordered_map<std::string, RecordInfo> &ppns_to_infos_map) {
    for (const auto &ppn : ppns) {
        const auto ppn_and_record_info(ppns_to_infos_map.find(ppn));
        if (unlikely(ppn_and_record_info == ppns_to_infos_map.cend())) {
            LOG_WARNING("PPN " + ppn + " is missing in ppns_to_infos_map! (1)");
            continue;
        }
        if (ppn_and_record_info->second.type_ != RecordInfo::ARTICLE)
            return false;
    }

    return true;
}


bool ContainsAtLeastOnePossibleReview(const std::set<std::string> &ppns,
                                      const std::unordered_map<std::string, RecordInfo> &ppns_to_infos_map) {
    for (const auto &ppn : ppns) {
        const auto ppn_and_record_info(ppns_to_infos_map.find(ppn));
        if (unlikely(ppn_and_record_info == ppns_to_infos_map.cend()))
            LOG_ERROR("PPN " + ppn + " is missing in ppns_to_infos_map! (2)");
        if (ppn_and_record_info->second.may_be_a_review_)
            return true;
    }

    return false;
}


bool HasAtLeastOneCommonDOI(const std::set<std::string> &ppns, const std::unordered_map<std::string, RecordInfo> &ppns_to_infos_map) {
    if (unlikely(ppns.empty()))
        return false;

    auto ppn(ppns.cbegin());
    auto ppn_and_record_info(ppns_to_infos_map.find(*ppn));
    if (unlikely(ppn_and_record_info == ppns_to_infos_map.cend()))
        LOG_ERROR("PPN " + *ppn + " is missing in ppns_to_infos_map! (3)");
    std::set<std::string> shared_dois(ppn_and_record_info->second.dois_);

    for (++ppn; ppn != ppns.cend(); ++ppn) {
        ppn_and_record_info = ppns_to_infos_map.find(*ppn);
        if (unlikely(ppn_and_record_info == ppns_to_infos_map.cend()))
            LOG_ERROR("PPN " + *ppn + " is missing in ppns_to_infos_map! (4)");
        shared_dois = MiscUtil::Intersect(shared_dois, ppn_and_record_info->second.dois_);
    }

    return not shared_dois.empty();
}


bool IsConsistentSet(const std::set<std::string> &ppns, const std::unordered_map<std::string, RecordInfo> &ppns_to_infos_map) {
    if (unlikely(ppns.empty()))
        return false;

    auto ppn(ppns.cbegin());
    auto ppn_and_record_info(ppns_to_infos_map.find(*ppn));
    if (unlikely(ppn_and_record_info == ppns_to_infos_map.cend()))
        LOG_ERROR("PPN " + *ppn + " is missing in ppns_to_infos_map! (5)");
    std::string year(ppn_and_record_info->second.year_), volume(ppn_and_record_info->second.volume_),
        issue(ppn_and_record_info->second.issue_);

    for (++ppn; ppn != ppns.cend(); ++ppn) {
        ppn_and_record_info = ppns_to_infos_map.find(*ppn);
        if (unlikely(ppn_and_record_info == ppns_to_infos_map.cend()))
            LOG_ERROR("PPN " + *ppn + " is missing in ppns_to_infos_map! (6)");
        if (ppn_and_record_info->second.year_ != year or ppn_and_record_info->second.volume_ != volume
            or ppn_and_record_info->second.issue_ != issue)
            return false;
    }

    return true;
}


const std::string IXTHEO_PREFIX("https://ixtheo.de/Record/");


void InsertSingleSet(const std::set<std::string> &dups,
                     std::unordered_map<std::string, std::set<std::string> *> * const control_number_to_dups_set_map) {
    auto dups_set(new std::set<std::string>(dups));
    for (const auto &control_number : dups)
        (*control_number_to_dups_set_map)[control_number] = dups_set;
}


void FindDups(File * const matches_list_output, const std::unordered_map<std::string, std::set<std::string>> &title_to_control_numbers_map,
              const std::unordered_map<std::string, std::set<std::string>> &control_number_to_authors_map,
              const std::unordered_map<std::string, RecordInfo> &ppns_to_infos_map,
              std::unordered_map<std::string, std::set<std::string> *> * const control_number_to_dups_set_map) {
    unsigned doi_match_count(0), non_doi_match_count(0);
    for (const auto &title_and_control_numbers : title_to_control_numbers_map) {
        if (title_and_control_numbers.second.size() < 2
            or not SetContainsOnlyArticlePPNs(title_and_control_numbers.second, ppns_to_infos_map)
            or ContainsAtLeastOnePossibleReview(title_and_control_numbers.second, ppns_to_infos_map))
            continue;

        if (HasAtLeastOneCommonDOI(title_and_control_numbers.second, ppns_to_infos_map)) {
            InsertSingleSet(title_and_control_numbers.second, control_number_to_dups_set_map);
            for (const auto &control_number : title_and_control_numbers.second)
                (*matches_list_output) << IXTHEO_PREFIX << control_number << ' ';
            (*matches_list_output) << "\r\n";
            ++doi_match_count;
            continue;
        }

        if (not IsConsistentSet(title_and_control_numbers.second, ppns_to_infos_map))
            continue;

        // Collect all control numbers for all authors of the current title:
        std::map<std::string, std::set<std::string>> author_to_control_numbers_map;
        for (const auto &control_number : title_and_control_numbers.second) {
            const auto control_number_and_authors(control_number_to_authors_map.find(control_number));
            if (control_number_and_authors == control_number_to_authors_map.cend())
                continue;

            for (const auto &author : control_number_and_authors->second) {
                auto author_and_control_numbers(author_to_control_numbers_map.find(author));
                if (author_and_control_numbers == author_to_control_numbers_map.end())
                    author_to_control_numbers_map[author] = std::set<std::string>{ control_number };
                else
                    author_and_control_numbers->second.emplace(control_number);
            }
        }

        // Output those cases where we found multiple control numbers for the same author for a single title:
        std::unordered_set<std::string> already_processed_control_numbers;
        for (const auto &author_and_control_numbers : author_to_control_numbers_map) {
            if (author_and_control_numbers.second.size() >= 2) {
                // We may have multiple authors for the same work but only wish to report each duplicate work once:
                for (const auto &control_number : author_and_control_numbers.second) {
                    if (already_processed_control_numbers.find(control_number) != already_processed_control_numbers.cend())
                        goto skip_author;
                }

                InsertSingleSet(author_and_control_numbers.second, control_number_to_dups_set_map);
                for (const auto &control_number : author_and_control_numbers.second) {
                    already_processed_control_numbers.emplace(control_number);
                    (*matches_list_output) << IXTHEO_PREFIX << control_number << ' ';
                }
                (*matches_list_output) << "\r\n";
                ++non_doi_match_count;
skip_author:
    /* Intentionally empty! */;
            }
        }
    }

    LOG_INFO("found " + std::to_string(doi_match_count) + " DOI matches and " + std::to_string(non_doi_match_count) + " non-DOI matches.");
}


bool AugmentRecord(MARC::Record * const record, const std::set<std::string> &dups_set,
                   const std::unordered_map<std::string, RecordInfo> &ppns_to_infos_map) {
    const auto existing_cross_references(MARC::ExtractCrossReferencePPNs(*record));

    bool added_at_least_one_new_cross_link(false);
    for (const auto &cross_link_ppn : dups_set) {
        if (cross_link_ppn != record->getControlNumber()
            and existing_cross_references.find(cross_link_ppn) == existing_cross_references.cend()) {
            const auto ppn_and_record_info(ppns_to_infos_map.find(cross_link_ppn));
            if (unlikely(ppn_and_record_info == ppns_to_infos_map.cend()))
                LOG_ERROR("did not find a record info record for PPN \"" + cross_link_ppn + "\"!");
            const bool is_electronic(ppn_and_record_info->second.is_electronic_);
            record->insertField("776", { { 'i', "Erscheint auch als" },
                                         { 'n', (is_electronic ? "elektronische Ausgabe" : "Druckausgabe") },
                                         { 'w', "(DE-627)" + cross_link_ppn } });
            added_at_least_one_new_cross_link = true;
        }
    }

    return added_at_least_one_new_cross_link;
}


void AddCrossLinks(MARC::Reader * const marc_reader, MARC::Writer * const marc_writer,
                   const std::unordered_map<std::string, std::set<std::string> *> &control_number_to_dups_set_map,
                   const std::unordered_map<std::string, RecordInfo> &ppns_to_infos_map) {
    unsigned augmentation_count(0);
    while (auto record = marc_reader->read()) {
        const auto control_number_and_dups_set(control_number_to_dups_set_map.find(record.getControlNumber()));
        if (control_number_and_dups_set != control_number_to_dups_set_map.cend()
            and AugmentRecord(&record, *control_number_and_dups_set->second, ppns_to_infos_map))
            ++augmentation_count;
        marc_writer->write(record);
    }

    LOG_INFO("Added cross links to " + std::to_string(augmentation_count) + " record(s).");
}


} // unnamed namespace


int Main(int argc, char *argv[]) {
    if (argc != 4)
        Usage();

    auto marc_reader(MARC::Reader::Factory(argv[1]));
    auto marc_writer(MARC::Writer::Factory(argv[2]));

    std::unordered_map<std::string, RecordInfo> ppns_to_infos_map;
    CollectInfos(marc_reader.get(), &ppns_to_infos_map);

    ControlNumberGuesser control_number_guesser;

    std::unordered_map<std::string, std::set<std::string>> title_to_control_numbers_map;
    control_number_guesser.getTitlesAndControlNumbers(&title_to_control_numbers_map);
    LOG_INFO("loaded " + std::to_string(title_to_control_numbers_map.size()) + " mappings from titles to control numbers.");

    std::unordered_map<std::string, std::set<std::string>> control_number_to_authors_map;
    control_number_guesser.getControlNumbersAndAuthors(&control_number_to_authors_map);
    LOG_INFO("loaded " + std::to_string(control_number_to_authors_map.size()) + " mappings from control numbers to authors.");

    auto matches_list_output(FileUtil::OpenOutputFileOrDie(argv[3]));
    std::unordered_map<std::string, std::set<std::string> *> control_number_to_dups_set_map;
    FindDups(matches_list_output.get(), title_to_control_numbers_map, control_number_to_authors_map, ppns_to_infos_map,
             &control_number_to_dups_set_map);

    marc_reader->rewind();
    AddCrossLinks(marc_reader.get(), marc_writer.get(), control_number_to_dups_set_map, ppns_to_infos_map);

    return EXIT_SUCCESS;
}

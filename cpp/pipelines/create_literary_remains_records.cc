/** \file    create_literary_remains_records.cc
 *  \author  Dr. Johannes Ruscheinski
 *
 *  A tool for creating literary remains MARC records from Beacon files.
 */

/*
    Copyright (C) 2019-2020, Library of the University of Tübingen

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

#include <unordered_map>
#include <unordered_set>
#include "FileUtil.h"
#include "MARC.h"
#include "RegexMatcher.h"
#include "StringUtil.h"
#include "TimeUtil.h"
#include "util.h"


namespace {


struct TitleRecordCounter {
    unsigned total_count_;
    unsigned religious_studies_count_;
    unsigned bibstudies_count_;
    unsigned canonlaw_count_;
public:
    TitleRecordCounter(): total_count_(0), religious_studies_count_(0) { }
    TitleRecordCounter(const TitleRecordCounter &other) = default;
    inline bool exceedsReligiousStudiesThreshold() const
        { return 100.0 * static_cast<double>(religious_studies_count_) / static_cast<double>(total_count_) >= 10.0 /* percent */; }
    inline bool exceedsBibStudiesThreshold() const
        { return 100.0 * static_cast<double>(bibstudies_count_) / static_cast<double>(total_count_) >= 10.0 /* percent */; }
    inline bool exceedsCanonLawThreshold() const
        { return 100.0 * static_cast<double>(canonlaw_count_) / static_cast<double>(total_count_) >= 10.0 /* percent */; }
    inline bool hasReligiousStudies() const
        { return religious_studies_count_ > 0;}
    inline bool hasBibStudies() const
        { return bibstudies_count_ > 0; }
    inline bool hasCanonLaw() const
        { return canonlaw_count_ > 0; }
};


// Counts the number of subsystem title records for authors.
void CopyMarcAndCollectSubsystemFrequencies(
    MARC::Reader * const title_reader, MARC::Writer * const title_writer,
    std::unordered_map<std::string, TitleRecordCounter> * const author_ppn_to_subsystem_title_counters)
{
    while (auto record = title_reader->read()) {
        const bool rel_tag_found(record.findTag("REL") != record.end() /*remove after migration*/ or record.hasFieldWithSubfieldValue("SUB", 'a', "REL"));
        const bool bib_tag_found(record.findTag("BIB") != record.end() /*remove after migration*/ or record.hasFieldWithSubfieldValue("SUB", 'a', "BIB"));
        const bool can_tag_found(record.findTag("CAN") != record.end() /*remove after migration*/ or record.hasFieldWithSubfieldValue("SUB", 'a', "CAN"));

        for (const auto &author_name_and_author_ppn : record.getAllAuthorsAndPPNs()) {
            auto author_ppn_and_counter(author_ppn_to_subsystem_title_counters->find(author_name_and_author_ppn.second));
            if (author_ppn_and_counter == author_ppn_to_subsystem_title_counters->end())
                author_ppn_and_counter = author_ppn_to_subsystem_title_counters->emplace(author_name_and_author_ppn.second, TitleRecordCounter()).first;

            ++(author_ppn_and_counter->second.total_count_);
            if (rel_tag_found)
                ++(author_ppn_and_counter->second.religious_studies_count_);
            if (bib_tag_found)
                ++(author_ppn_and_counter->second.bibstudies_count_);
            if (can_tag_found)
                ++(author_ppn_and_counter->second.canonlaw_count_);
        }

        title_writer->write(record);
    }
}


struct LiteraryRemainsInfo {
    std::string author_id_;
    std::string author_name_;
    std::string url_;
    std::string source_name_;
    std::string dates_;
    std::vector<std::string> types_;
public:
    LiteraryRemainsInfo() = default;
    LiteraryRemainsInfo(const LiteraryRemainsInfo &other) = default;
    LiteraryRemainsInfo(const std::string &author_id, const std::string &author_name, const std::string &url, const std::string &source_name, const std::string &dates, const std::vector<std::string> &types = {})
        : author_id_(author_id), author_name_(author_name), url_(url), source_name_(source_name), dates_(dates), types_(types) { }

    LiteraryRemainsInfo &operator=(const LiteraryRemainsInfo &rhs) = default;
};


std::vector<std::pair<RegexMatcher *, std::string>> CompileMatchers() {
    // Please note that the order in the following vector matters.  The first successful match will be used.
    const std::vector<std::pair<std::string, std::string>> patterns_and_replacements {
        { "v([0-9]+) ?- ?v([0-9]+)", "\\1 v. Chr. - \\2 v. Chr." },
        { "v([0-9]+)"              , "\\1 v. Chr."               },
    };

    std::vector<std::pair<RegexMatcher *, std::string>> compiled_patterns_and_replacements;
    for (const auto &pattern_and_replacement : patterns_and_replacements)
        compiled_patterns_and_replacements.push_back(
            std::pair<RegexMatcher *, std::string>(RegexMatcher::RegexMatcherFactoryOrDie(pattern_and_replacement.first),
                                                   pattern_and_replacement.second));
    return compiled_patterns_and_replacements;
}


const std::vector<std::pair<RegexMatcher *, std::string>> matchers_and_replacements(CompileMatchers());


std::string ReplaceNonStandardBCEDates(const std::string &dates) {
    for (const auto &matcher_and_replacement : matchers_and_replacements) {
        RegexMatcher * const matcher(matcher_and_replacement.first);
        const std::string replaced_string(matcher->replaceWithBackreferences(dates, matcher_and_replacement.second));
        if (replaced_string != dates)
            return replaced_string;
    }

    return dates;
}

std::vector<std::string> GetBEATypes(const MARC::Record::Field &beacon_field) {
   return beacon_field.getSubfields().extractSubfields("v");
}


void SetSubsystemCounter(MARC::Record &record, std::string subsystem_code, unsigned int additional_value) {
    if (not record.hasFieldWithSubfieldValue("SUB", 'a', subsystem_code))
        record.insertField("SUB", {{'a', subsystem_code}, {'b', std::to_string(additional_value)}});
    else {
        for (auto &field : record.getTagRange("SUB")) {
            if (field.hasSubfieldWithValue('a', subsystem_code)) {
                MARC::Subfields subfields(field.getContents());
                if (not field.hasSubfield('b'))
                    subfields.addSubfield('b', std::to_string(additional_value));
                else {
                    std::string orig_count = subfields.getFirstSubfieldWithCode('b');
                    unsigned int new_count = std::stoi(orig_count) + additional_value;
                    subfields.replaceFirstSubfield('b', std::to_string(new_count));
                }
                field.setSubfields(subfields);
            }
        }
    }
}


void LoadAuthorGNDNumbersAndTagAuthors(
    MARC::Reader * const authority_reader, MARC::Writer * const authority_writer,
    const std::unordered_map<std::string, TitleRecordCounter> &author_ppn_to_subsystem_titles_counters,
    std::unordered_map<std::string, std::vector<LiteraryRemainsInfo>> * const gnd_numbers_to_literary_remains_infos_map,
    std::unordered_map<std::string, std::string> * const gnd_numbers_to_ppns_map)
{
    unsigned total_count(0), references_count(0), tagged_count(0);
    while (auto record = authority_reader->read()) {
        ++total_count;

        auto beacon_field(record.findTag("BEA"));
        if (beacon_field == record.end()) {
            authority_writer->write(record);
            continue;
        }

        const auto _100_field(record.findTag("100"));
        if (_100_field == record.end() or not _100_field->hasSubfield('a')) {
            authority_writer->write(record);
            continue;
        }

        std::string gnd_number;
        if (not MARC::GetGNDCode(record, &gnd_number)) {
            authority_writer->write(record);
            continue;
        }

        (*gnd_numbers_to_ppns_map)[gnd_number] = record.getControlNumber();

        const auto numeration(_100_field->getFirstSubfieldWithCode('b'));
        const auto titles_and_other_words_associated_with_a_name(_100_field->getFirstSubfieldWithCode('c'));
        const auto name_and_numeration(_100_field->getFirstSubfieldWithCode('a') + (numeration.empty() ? "" : " " + numeration));
        const auto author_name(not titles_and_other_words_associated_with_a_name.empty()
                                   ? name_and_numeration + " (" + titles_and_other_words_associated_with_a_name + ")"
                                   : name_and_numeration);

        const auto dates(ReplaceNonStandardBCEDates(_100_field->getFirstSubfieldWithCode('d')));

        std::vector<LiteraryRemainsInfo> literary_remains_infos;
        while (beacon_field != record.end() and beacon_field->getTag() == "BEA") {
            literary_remains_infos.emplace_back(record.getControlNumber(), author_name, beacon_field->getFirstSubfieldWithCode('u'),
                                                beacon_field->getFirstSubfieldWithCode('a'), dates, GetBEATypes(*beacon_field));
            ++beacon_field;
        }
        (*gnd_numbers_to_literary_remains_infos_map)[gnd_number] = literary_remains_infos;
        references_count += literary_remains_infos.size();

        if (author_ppn_to_subsystem_titles_counters.find(record.getControlNumber()) != author_ppn_to_subsystem_titles_counters.cend()) {
            auto subsystem_record_counter = author_ppn_to_subsystem_titles_counters.find(record.getControlNumber())->second;
            if (subsystem_record_counter.religious_studies_count_ > 0) {
                record.insertField("REL", { { 'a', "1" }, { 'o', FileUtil::GetBasename(::progname) } }); // remove after migration
                SetSubsystemCounter(record, "REL", 1);
                ++tagged_count;
            }
            if (subsystem_record_counter.bibstudies_count_ > 0) {
                record.insertField("BIB", { { 'a', "1" }, { 'o', FileUtil::GetBasename(::progname) } }); // remove after migration
                SetSubsystemCounter(record, "BIB", 1);
                ++tagged_count;
            }
            if (subsystem_record_counter.canonlaw_count_ > 0) {
                record.insertField("CAN", { { 'a', "1" }, { 'o', FileUtil::GetBasename(::progname) } }); // remove after migration
                SetSubsystemCounter(record, "CAN", 1);
                ++tagged_count;
            }
        }

        authority_writer->write(record);
    }

    LOG_INFO("Loaded " + std::to_string(references_count) + " literary remains references from \"" + authority_reader->getPath()
             + "\" which contained a total of " + std::to_string(total_count) + " records.");
    LOG_INFO("Tagged " + std::to_string(tagged_count) + " authority records as relevant to religious studies.");
}


std::string NormaliseAuthorName(std::string author_name) {
    const auto comma_pos(author_name.find(','));
    if (comma_pos == std::string::npos)
        return author_name;

    std::string auxillary_info;
    const auto open_paren_pos(author_name.find('('));
    if (open_paren_pos != std::string::npos) {
        if (comma_pos > open_paren_pos)
            return author_name;

        auxillary_info = " " + author_name.substr(open_paren_pos);
        author_name.resize(open_paren_pos);
    }

    return StringUtil::TrimWhite(author_name.substr(comma_pos + 1)) + " " + StringUtil::TrimWhite(author_name.substr(0, comma_pos))
           + auxillary_info;
}


std::string GetTitle(const std::string &author_name, const std::string &dates, const std::vector<std::string> &types) {
        std::string introductory_clause;
        if (std::find(types.begin(), types.end(), "Nachlass") != types.end())
            introductory_clause = "Nachlass von ";
        else if (std::find(types.begin(), types.end(), "Teilnachlass") != types.end())
            introductory_clause = "Teilnachlass von ";
        else
            introductory_clause = "Archivmaterialien zu ";
        std::string title(introductory_clause + author_name + ',' + dates);
        if (types.size() > 1)
            title += " ("  + StringUtil::Join(types, ", ") + ')';
        return title;
}


void AppendLiteraryRemainsRecords(
    MARC::Writer * const title_writer,
    const std::unordered_map<std::string, std::vector<LiteraryRemainsInfo>> &gnd_numbers_to_literary_remains_infos_map,
    const std::unordered_map<std::string, std::string> &gnd_numbers_to_ppns_map,
    const std::unordered_map<std::string, TitleRecordCounter> &author_ppn_to_subsystem_titles_counters)
{
    unsigned creation_count(0);
    for (const auto &gnd_numbers_and_literary_remains_infos : gnd_numbers_to_literary_remains_infos_map) {
        MARC::Record new_record(MARC::Record::TypeOfRecord::MIXED_MATERIALS, MARC::Record::BibliographicLevel::COLLECTION,
                                "LR" + gnd_numbers_and_literary_remains_infos.first);
        const std::string &author_name(gnd_numbers_and_literary_remains_infos.second.front().author_name_);
        std::string dates(gnd_numbers_and_literary_remains_infos.second.front().dates_.empty()
                              ? "" : " " + gnd_numbers_and_literary_remains_infos.second.front().dates_);
        new_record.insertField("003", "PipeLineGenerated");
        new_record.insertField("005", TimeUtil::GetCurrentDateAndTime("%Y%m%d%H%M%S") + ".0");
        new_record.insertField("008", "190606s2019    xx |||||      00| ||ger c");
        if (gnd_numbers_and_literary_remains_infos.second.front().dates_.empty())
            new_record.insertField("100", { { 'a', author_name }, { '0', "(DE-588)" + gnd_numbers_and_literary_remains_infos.first }, { '0', "(DE-627)" + gnd_numbers_and_literary_remains_infos.second.front().author_id_ } });
        else
            new_record.insertField("100",
                                   { { 'a', author_name }, { '0', "(DE-588)" + gnd_numbers_and_literary_remains_infos.first }, { '0', "(DE-627)" + gnd_numbers_and_literary_remains_infos.second.front().author_id_ },
                                     { 'd', gnd_numbers_and_literary_remains_infos.second.front().dates_ } });

        const auto &title(GetTitle(NormaliseAuthorName(author_name), dates, gnd_numbers_and_literary_remains_infos.second.front().types_));
        new_record.insertField("245", { { 'a', title } });

        for (const auto &literary_remains_info : gnd_numbers_and_literary_remains_infos.second)
            new_record.insertField("856",
                                   { { 'u', literary_remains_info.url_ },
                                       { '3', "Archivdatenbank (" + literary_remains_info.source_name_ + ")" } });

        // Do we have a religious studies author?
        const auto gnd_number_and_author_ppn(gnd_numbers_to_ppns_map.find(gnd_numbers_and_literary_remains_infos.first));
        if (unlikely(gnd_number_and_author_ppn == gnd_numbers_to_ppns_map.cend()))
            LOG_ERROR("we should *always* find the GND number in gnd_numbers_to_ppns_map!");
        const auto author_ppn_and_subsystem_titles_counter(
            author_ppn_to_subsystem_titles_counters.find(gnd_number_and_author_ppn->second));
        if (author_ppn_and_subsystem_titles_counter != author_ppn_to_subsystem_titles_counters.cend()) {
            if (author_ppn_and_subsystem_titles_counter->second.hasReligiousStudies()) {
                new_record.insertField("REL", { { 'a', "1" }, { 'o', FileUtil::GetBasename(::progname) } }); // remove after migration
                new_record.addSubfieldCreateFieldUnique("SUB", 'a', "REL");
            }
            if (author_ppn_and_subsystem_titles_counter->second.hasBibStudies()) {
                new_record.insertField("BIB", { { 'a', "1" }, { 'o', FileUtil::GetBasename(::progname) } }); // remove after migration
                new_record.addSubfieldCreateFieldUnique("SUB", 'a', "BIB");
            }
            if (author_ppn_and_subsystem_titles_counter->second.hasCanonLaw()) {
                new_record.insertField("CAN", { { 'a', "1" }, { 'o', FileUtil::GetBasename(::progname) } }); // remove after migration
                new_record.addSubfieldCreateFieldUnique("SUB", 'a', "CAN");
            }
        }

        title_writer->write(new_record);
        ++creation_count;
    }

    LOG_INFO("Appended a total of " + std::to_string(creation_count) + " record(s).");
}


} // unnamed namespace


int Main(int argc, char **argv) {
    if (argc != 5)
        ::Usage("marc_input marc_output authority_records_input authority_records_output");

    auto title_reader(MARC::Reader::Factory(argv[1]));
    auto title_writer(MARC::Writer::Factory(argv[2]));
    std::unordered_map<std::string, TitleRecordCounter> author_ppn_to_subsystem_titles_counters;
    CopyMarcAndCollectSubsystemFrequencies(title_reader.get(), title_writer.get(), &author_ppn_to_subsystem_titles_counters);
    if (author_ppn_to_subsystem_titles_counters.empty())
        LOG_ERROR("You must run this program on an input that contains some records!");

    auto authority_reader(MARC::Reader::Factory(argv[3]));
    auto authority_writer(MARC::Writer::Factory(argv[4]));
    std::unordered_map<std::string, std::vector<LiteraryRemainsInfo>> gnd_numbers_to_literary_remains_infos_map;
    std::unordered_map<std::string, std::string> gnd_numbers_to_ppns_map;
    LoadAuthorGNDNumbersAndTagAuthors(authority_reader.get(), authority_writer.get(), author_ppn_to_subsystem_titles_counters,
                                      &gnd_numbers_to_literary_remains_infos_map, &gnd_numbers_to_ppns_map);
    AppendLiteraryRemainsRecords(title_writer.get(), gnd_numbers_to_literary_remains_infos_map, gnd_numbers_to_ppns_map,
                                 author_ppn_to_subsystem_titles_counters);

    return EXIT_SUCCESS;
}

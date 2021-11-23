/** \file    convert_core_json_to_marc.cc
 *  \brief   Converts JSON to MARC.
 *  \author  Dr. Johannes Ruscheinski
 */

/*
    Copyright (C) 2021 Library of the University of Tübingen

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
#include <unordered_map>
#include "FileUtil.h"
#include "JSON.h"
#include "KeyValueDB.h"
#include "MARC.h"
#include "MiscUtil.h"
#include "TextUtil.h"
#include "TimeUtil.h"
#include "UBTools.h"
#include "util.h"


namespace {


[[noreturn]] void Usage() {
    ::Usage("[--create-unique-id-db|--ignore-unique-id-dups][--935-entry=entry] --sigil=project_sigil json_input [unmapped_issn_list marc_output]\n"
            "\t--create-unique-id-db: This flag has to be specified the first time this program will be executed only.\n"
            "\t--ignore-unique-id-dups: If specified MARC records will be created for unique ID's which we have encountered\n"
            "\t                         before.  The unique ID database will still be updated.\n"
            "\t--935-entry: The structure of this repeatable flag is \"(TIT|LOK):subfield_a_value\".  If TIT has been specified then no subfield 2\n"
            "\t             will be generated.  If LOK has been specified, subfield 2 will be set to LOK.\n"
            "\t--sigil: This is used to generate an 852 field which is needed by the K10+ to be able to assign records to the appropriate\n"
            "\t         project.  An example would be DE-2619 for criminology.\n"
            "\tunmapped_issn_list (output): Here we list the ISSN's for which we have no entry in issns_to_journaltitles_and_ppns.map,\n"
            "\t                             required unless --extract-and-count-issns-only was specified!\n"
            "\tmarc_outpt: required unless --extract-and-count-issns-only was specified!\n\n");
}


struct JournalTitlePPNAndOnlineISSN {
    std::string journal_title_;
    std::string ppn_;
    std::string online_issn_;
public:
    JournalTitlePPNAndOnlineISSN(const std::string &journal_title, const std::string &ppn, const std::string &online_issn)
        : journal_title_(journal_title), ppn_(ppn), online_issn_(online_issn) { }
};


// Parses an input file that has three (the last component may be empty) parts per line that are colon-separated.
// Embedded colons may be backslash escaped.
void LoadISSNsToJournalTitlesPPNsAndISSNsMap(
    std::unordered_map<std::string, JournalTitlePPNAndOnlineISSN> * const issns_to_journal_titles_ppns_and_issns_map)
{
    const std::string MAP_FILE_PATH(UBTools::GetTuelibPath() + "print_issns_titles_online_ppns_and_online_issns.csv");
    std::vector<std::vector<std::string>> lines;
    TextUtil::ParseCSVFileOrDie(MAP_FILE_PATH, &lines);

    for (const auto &line : lines) {
        if (lines[0].empty() or line[1].empty())
            continue; // ISSN and titles are required, PPN's and online ISSN's are optional.

        const std::string online_ppn(line.size() > 2 ? line[2] : "");
        const std::string online_issn(line.size() > 3 ? line[3] : "");
        issns_to_journal_titles_ppns_and_issns_map->emplace(line[0], JournalTitlePPNAndOnlineISSN(line[1], online_ppn, online_issn));
    }

    LOG_INFO("Loaded " + std::to_string(issns_to_journal_titles_ppns_and_issns_map->size())
             + " mappings from print ISSN's to online ISSN's, PPN's and journal titles.");
}


MARC::Record::BibliographicLevel MapTypeStringToBibliographicLevel(const std::string &item_type) {
    if (item_type == "Book item")
        return MARC::Record::MONOGRAPH_OR_ITEM;
    else if (item_type == "Book chapter")
        return MARC::Record::MONOGRAPHIC_COMPONENT_PART;
    else if (item_type == "Article")
        return MARC::Record::SERIAL_COMPONENT_PART;
    else {
        if (not item_type.empty())
            LOG_WARNING("unknown item type: " + item_type);
        return MARC::Record::SERIAL_COMPONENT_PART; // Yes, we're lying here! :-(
    }
}


// \return True if we found at least one author, else false.
bool ProcessAuthors(const JSON::ObjectNode &entry_object, MARC::Record * const record) {
    const auto authors(entry_object.getArrayNode("authors"));
    if (authors == nullptr or authors->empty())
        return false;

    bool first_author(true);
    for (const auto &author : *authors) {
        const auto author_object(JSON::JSONNode::CastToObjectNodeOrDie("author_object", author));
        record->insertField(first_author ? "100" : "700",
                            { { 'a', MiscUtil::NormalizeName(author_object->getStringNode("name")->getValue()) },
                              { '4', "aut" } }, /*indicator1=*/'1');
        if (first_author)
            first_author = false;
    }

    return not first_author;
}


void ProcessContributors(const JSON::ObjectNode &entry_object, MARC::Record * const record) {
    const auto contributors(entry_object.getOptionalArrayNode("contributors"));
    if (contributors == nullptr)
        return;
    for (const auto &contributor : *contributors) {
        const auto contributor_node(JSON::JSONNode::CastToStringNodeOrDie("contributor_node", contributor));
        record->insertField("700", { { 'a', contributor_node->getValue() }, { '4', "ctb" } });
    }
}


// \return True if a title was found, else false.
bool ProcessTitle(const JSON::ObjectNode &entry_object, MARC::Record * const record) {
    const auto title_node(entry_object.getOptionalStringNode("title"));
    if (title_node == nullptr)
        return false;
    record->insertField("245", 'a', title_node->getValue());
    return true;
}


void ProcessYear(const JSON::ObjectNode &entry_object, MARC::Record * const record) {
    if (entry_object.getNode("yearPublished")->getType() == JSON::JSONNode::NULL_NODE)
        return;
    const auto year_node(entry_object.getOptionalIntegerNode("yearPublished"));
    if (year_node == nullptr)
        return;
    record->insertField("936", 'j', year_node->toString(), 'u','w');
}


void ProcessDOI(const JSON::ObjectNode &entry_object, MARC::Record * const record) {
    if (entry_object.getNode("doi")->getType() == JSON::JSONNode::NULL_NODE)
        return;
    const auto doi_node(entry_object.getStringNode("doi"));
    record->insertField("856",
                        { { 'u', "https://doi.org/" + doi_node->getValue() }, { 'x', "Resolving System" },
                          { 'z', "Kostenfrei"}, { '3', "Volltext" } }, /*indicator1=*/'4', /*indicator2=*/'0');
    record->insertField("024", { { 'a', doi_node->getValue() }, { '2', "doi" } }, '0','7');
}


void ProcessDownloadURL(const JSON::ObjectNode &entry_object, MARC::Record * const record) {
    const auto download_url_node(entry_object.getStringNode("downloadUrl"));
    const auto download_url(download_url_node->getValue());
    if (not download_url.empty())
        record->insertField("856", 'u', download_url, '4', '0');
}


void ProcessLanguage(const JSON::ObjectNode &entry_object, MARC::Record * const record) {
    if (entry_object.getNode("language")->getType() == JSON::JSONNode::NULL_NODE)
        return;
    const auto language_object(entry_object.getObjectNode("language"));
    record->insertField("041", 'a', MARC::MapToMARCLanguageCode(language_object->getStringNode("code")->getValue()));
}


// \return True if an abstract was found, else false.
bool ProcessAbstract(const JSON::ObjectNode &entry_object, MARC::Record * const record) {
    if (not entry_object.hasNode("abstract") or entry_object.isNullNode("abstract"))
        return false;
    const auto abstract_node(entry_object.getStringNode("abstract"));
    record->insertField("520", 'a', abstract_node->getValue());
    return true;
}


// \return True if any uncontrolled terms were found, else false.
bool ProcessUncontrolledIndexTerms(const JSON::ObjectNode &entry_object, MARC::Record * const record) {
    bool found_at_least_one_index_term(false);

    const auto document_type_node(entry_object.getOptionalStringNode("documentType"));
    if (document_type_node != nullptr) {
        const auto document_type(document_type_node->getValue());
        if (not document_type.empty() and document_type != "unknown") {
            record->insertField("653", 'a', document_type);
            found_at_least_one_index_term = true;
        }
    }

    if (not entry_object.hasNode("fieldOfStudy") or entry_object.isNullNode("fieldOfStudy"))
        return found_at_least_one_index_term;
    const auto field_of_study(entry_object.getStringNode("fieldOfStudy")->getValue());
    if (not field_of_study.empty()) {
        record->insertField("653", 'a', field_of_study);
        found_at_least_one_index_term = true;
    }

    return found_at_least_one_index_term;
}


// \return True if any uncontrolled terms were found, else false.
bool ProcessYearPublished(const JSON::ObjectNode &entry_object, MARC::Record * const record) {
    if (not entry_object.hasNode("yearPublished") or entry_object.isNullNode("yearPublished"))
        return false;
    const auto year_published_node(entry_object.getIntegerNode("yearPublished"));
    record->insertField("264", 'c', year_published_node->toString(), /*indicator1=*/' ', /*indicator2=*/'1');
    return true;
}


bool ProcessJournal(const JSON::ObjectNode &entry_object,
                    const std::unordered_map<std::string, JournalTitlePPNAndOnlineISSN> &issns_to_journal_titles_ppns_and_issns_map,
                    std::unordered_map<std::string, unsigned> * const unmatched_issns_to_counts_map, MARC::Record * const record)
{
    if (not entry_object.hasNode("journals"))
        return false;
    const auto journals(entry_object.getArrayNode("journals"));
    for (size_t i(0); i < journals->size(); ++i) {
        const auto journal(journals->getObjectNode(i));
        if (not journal->hasNode("identifiers"))
            continue;

        const auto identifiers(journal->getArrayNode("identifiers"));
        for (size_t k(0); k < identifiers->size(); ++k) {
            const auto identifier(identifiers->getStringNode(k));
            const auto issn_candidate(identifier->getValue());
            if (MiscUtil::IsPossibleISSN(issn_candidate)) {
                std::string issn;
                if (unlikely(not MiscUtil::NormaliseISSN(issn_candidate, &issn)))
                    LOG_ERROR("Bad ISSN, we should *never* get here!");

                const auto issn_and_journal_title_ppn_and_online_ISSN(issns_to_journal_titles_ppns_and_issns_map.find(issn));
                if (issn_and_journal_title_ppn_and_online_ISSN == issns_to_journal_titles_ppns_and_issns_map.cend()) {
                    auto issn_and_count(unmatched_issns_to_counts_map->find(issn));
                    if (issn_and_count != unmatched_issns_to_counts_map->end())
                        ++(issn_and_count->second);
                    else
                        (*unmatched_issns_to_counts_map)[issn] = 1;
                    return false;
                }

                record->insertField("773",
                                    { { 'i', "In: " }, { 't', issn_and_journal_title_ppn_and_online_ISSN->second.journal_title_},
                                      { 'x', issn_and_journal_title_ppn_and_online_ISSN->second.online_issn_ }, },
                                    /*indicator1=*/'0', /*indicator2=*/'8');
                return true;
            }
        }
    }
    return false;
}


void Process935Entries(const std::vector<std::pair<std::string, std::string>> &_935_entries, MARC::Record * const record) {
    for (const auto &[subfield_a, subfield_2_selector] : _935_entries) {
        if (subfield_2_selector == "TIT")
            record->insertField("935", 'a', subfield_a);
        else
            record->insertField("935", { { 'a', subfield_a }, { '2', subfield_2_selector } });
    }
}


void GenerateMARCFromJSON(const JSON::ArrayNode &root_array,
                          const std::unordered_map<std::string, JournalTitlePPNAndOnlineISSN> &issns_to_journal_titles_ppns_and_issns_map,
                          MARC::Writer * const marc_writer, const std::string &project_sigil,
                          const std::vector<std::pair<std::string, std::string>> &_935_entries,
                          std::unordered_map<std::string, unsigned> * const unmatched_issns_to_counts_map,
                          const bool ignore_unique_id_dups, KeyValueDB * const unique_id_to_date_map)
{
    unsigned skipped_dupe_count(0), generated_count(0), skipped_incomplete_count(0);
    for (auto &entry : root_array) {
        const auto entry_object(JSON::JSONNode::CastToObjectNodeOrDie("entry", entry));
        const auto id(std::to_string(entry_object->getIntegerValue("id")));
        const auto control_number("CORE" + id);
        if (ignore_unique_id_dups and unique_id_to_date_map->keyIsPresent(control_number))
            ++skipped_dupe_count;
        else {
            MARC::Record new_record(MARC::Record::TypeOfRecord::LANGUAGE_MATERIAL, MapTypeStringToBibliographicLevel(""),
                                    control_number);
            if (not ProcessAuthors(*entry_object, &new_record)) {
                ++skipped_incomplete_count;
                continue;
            }
            ProcessContributors(*entry_object, &new_record);
            if (not ProcessTitle(*entry_object, &new_record)) {
                ++skipped_incomplete_count;
                continue;
            }
            new_record.insertControlField("007", "cr||||");
            new_record.insertField("591", 'a', "Metadaten maschinell erstellt (TUKRIM)");
            new_record.insertField("852", 'a', project_sigil);
            ProcessYear(*entry_object, &new_record);
            ProcessDOI(*entry_object, &new_record);
            ProcessDownloadURL(*entry_object, &new_record);
            ProcessLanguage(*entry_object, &new_record);
            ProcessAbstract(*entry_object, &new_record);
            ProcessUncontrolledIndexTerms(*entry_object, &new_record);
            ProcessYearPublished(*entry_object, &new_record);
            ProcessJournal(*entry_object, issns_to_journal_titles_ppns_and_issns_map, unmatched_issns_to_counts_map, &new_record);
            Process935Entries(_935_entries, &new_record);
            marc_writer->write(new_record);
            unique_id_to_date_map->addOrReplace(control_number, TimeUtil::GetCurrentDateAndTime());
            ++generated_count;
        }
    }

    std::cout << "Skipped " << skipped_dupe_count << " dupes and " << skipped_incomplete_count
              << " incomplete entry/entries and generated " << generated_count << " MARC record(s).\n";
}


void GenerateUnmappedISSNList(File * const unmatched_issns_file,
                              const std::unordered_map<std::string, unsigned> &unmatched_issns_to_counts_map)
{
    for (const auto &issn_and_count : unmatched_issns_to_counts_map)
        (*unmatched_issns_file) << issn_and_count.first << '\t' << issn_and_count.second << '\n';
    LOG_INFO("Wrote a list of " + std::to_string(unmatched_issns_to_counts_map.size()) + " unmapped ISSN's to \""
             + unmatched_issns_file->getPath() + "\".");
}


const std::string UNIQUE_ID_TO_DATE_MAP_PATH(UBTools::GetTuelibPath() + "convert_json_to_marc.db");


} // unnamed namespace


int Main(int argc, char **argv) {
    if (argc < 6)
        Usage();

    if (std::strcmp(argv[1], "--create-unique-id-db") == 0) {
        KeyValueDB::Create(UNIQUE_ID_TO_DATE_MAP_PATH);
        --argc, ++argv;
    }

    bool ignore_unique_id_dups(false);
    if (std::strcmp(argv[1], "--ignore-unique-id-dups") == 0) {
        ignore_unique_id_dups = true;
        --argc, ++argv;
    }

    std::vector<std::pair<std::string, std::string>> _935_entries;
    while (::strncmp(argv[1], "--935-entry=", 12) == 0) {
        const auto first_colon_pos(std::strchr(argv[1], ':'));
        if (first_colon_pos == nullptr)
            LOG_ERROR("value after --935-entry= must contain a colon!");
        *first_colon_pos = '\0';
        _935_entries.push_back(std::make_pair<std::string, std::string>(first_colon_pos + 1, argv[1] + 12));
        --argc, ++argv;
    }

    if (argc != 5)
        Usage();

    if (::strncmp(argv[1], "--sigil=", 8) != 0)
        Usage();
    const std::string project_sigil(argv[1] + 8); // "ISIL" in German.
    --argc, ++argv;

    std::unordered_map<std::string, JournalTitlePPNAndOnlineISSN> issns_to_journal_titles_ppns_and_issns_map;
    LoadISSNsToJournalTitlesPPNsAndISSNsMap(&issns_to_journal_titles_ppns_and_issns_map);
    const std::string json_file_path(argv[1]);
    const auto json_source(FileUtil::ReadStringOrDie(json_file_path));
    JSON::Parser parser(json_source);
    std::shared_ptr<JSON::JSONNode> tree_root;
    if (not parser.parse(&tree_root))
        LOG_ERROR("Failed to parse the JSON contents of \"" + json_file_path + "\": " + parser.getErrorMessage());

    const auto results_node(JSON::LookupNode("/results", tree_root));
    if (results_node == nullptr)
        LOG_ERROR("results node not found!");
    const auto array_root(JSON::JSONNode::CastToArrayNodeOrDie("results", results_node));

    const auto unmatched_issns_file(FileUtil::OpenOutputFileOrDie(argv[2]));

    KeyValueDB unique_id_to_date_map(UNIQUE_ID_TO_DATE_MAP_PATH);
    std::unordered_map<std::string, unsigned> unmatched_issns_to_counts_map;
    const std::unique_ptr<MARC::Writer> marc_writer(MARC::Writer::Factory(argv[3]));
    GenerateMARCFromJSON(*array_root.get(), issns_to_journal_titles_ppns_and_issns_map, marc_writer.get(), project_sigil,
                         _935_entries, &unmatched_issns_to_counts_map, ignore_unique_id_dups, &unique_id_to_date_map);
    GenerateUnmappedISSNList(unmatched_issns_file.get(), unmatched_issns_to_counts_map);

    return EXIT_SUCCESS;
}

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
#include <set>
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
    ::Usage(
        "[--create-unique-id-db|--ignore-unique-id-dups][--935-entry=entry] --sigil=project_sigil json_input\n"
        "\t--create-unique-id-db: This flag has to be specified the first time this program will be executed only.\n"
        "\t--ignore-unique-id-dups: If specified MARC records will be created for unique ID's which we have encountered\n"
        "\t                         before.  The unique ID database will still be updated.\n"
        "\t--935-entry: The structure of this repeatable flag is \"(TIT|LOK):subfield_a_value\".  If TIT has been specified then no "
        "subfield 2\n"
        "\t             will be generated.  If LOK has been specified, subfield 2 will be set to LOK.\n"
        "\t--sigil: This is used to generate an 852 field which is needed by the K10+ to be able to assign records to the appropriate\n"
        "\t         project.  An example would be DE-2619 for criminology.\n\n");
}


// \return True if we found at least one author, else false.
bool ProcessAuthors(const JSON::ObjectNode &entry_object, MARC::Record * const record, std::set<std::string> * const authors) {
    const auto authors_node(entry_object.getArrayNode("authors"));
    if (authors_node == nullptr or authors_node->empty())
        return false;

    authors->clear();
    bool first_author(true);
    for (const auto &author : *authors_node) {
        const auto author_object(JSON::JSONNode::CastToObjectNodeOrDie("author_object", author));
        const std::string author_name(author_object->getStringNode("name")->getValue());
        if (authors->find(author_name) != authors->end())
            continue; // Found a duplicate author!

        record->insertField(first_author ? "100" : "700", { { 'a', MiscUtil::NormalizeName(author_name) }, { '4', "aut" } },
                            /*indicator1=*/'1');
        authors->insert(author_name);
        if (first_author)
            first_author = false;
    }

    return not first_author;
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
    record->insertField("936", 'j', year_node->toString(), 'u', 'w');
}


void ProcessDownloadURL(const JSON::ObjectNode &entry_object, MARC::Record * const record) {
    const auto download_url_node(entry_object.getStringNode("downloadUrl"));
    const auto download_url(download_url_node->getValue());
    if (not download_url.empty())
        record->insertField("856", { { 'u', download_url }, { 'z', "LF" } }, '4', '0');
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
    std::string abstract_value = abstract_node->getValue().length() > 5 ? abstract_node->getValue() : "not available";
    record->insertField("520", 'a', StringUtil::Truncate(MARC::Record::MAX_VARIABLE_FIELD_DATA_LENGTH, abstract_value));
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


bool ProcessJournal(const JSON::ObjectNode &entry_object, MARC::Record * const record) {
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
                record->insertField("773",
                                    {
                                        { 'x', issn_candidate }
                                    },
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
                          MARC::Writer * const marc_writer, const std::string &project_sigil,
                          const std::vector<std::pair<std::string, std::string>> &_935_entries,
                          const bool ignore_unique_id_dups,
                          KeyValueDB * const unique_id_to_date_map) {
    unsigned skipped_dupe_count(0), generated_count(0), skipped_incomplete_count(0);
    for (auto &entry : root_array) {
        const auto entry_object(JSON::JSONNode::CastToObjectNodeOrDie("entry", entry));
        const auto id(std::to_string(entry_object->getIntegerValue("id")));
        const auto control_number("CORE" + id);
        if (ignore_unique_id_dups and unique_id_to_date_map->keyIsPresent(control_number))
            ++skipped_dupe_count;
        else {
            MARC::Record new_record(MARC::Record::TypeOfRecord::LANGUAGE_MATERIAL, MARC::Record::BibliographicLevel::MONOGRAPH_OR_ITEM,
                                    control_number);
            std::set<std::string> authors;
            if (not ProcessAuthors(*entry_object, &new_record, &authors)) {
                ++skipped_incomplete_count;
                continue;
            }

            //Do not use contributors anymore (team decission in video conf. on 09.02.2022)
            //ProcessContributors(*entry_object, &new_record, authors);

            if (not ProcessTitle(*entry_object, &new_record)) {
                ++skipped_incomplete_count;
                continue;
            }
            new_record.insertControlField("007", "cr||||");
            new_record.insertField("084", { { 'a', "2,1" }, { '2', "ssgn" } });
            new_record.insertField("035", 'a', "(core)" + id);
            new_record.insertField("591", 'a', "Metadaten maschinell erstellt (TUKRIM)");
            new_record.insertField("852", 'a', project_sigil);
            ProcessYear(*entry_object, &new_record);
            ProcessDownloadURL(*entry_object, &new_record);
            ProcessLanguage(*entry_object, &new_record);
            ProcessAbstract(*entry_object, &new_record);
            ProcessUncontrolledIndexTerms(*entry_object, &new_record);
            ProcessYearPublished(*entry_object, &new_record);
            ProcessJournal(*entry_object, &new_record);
            Process935Entries(_935_entries, &new_record);
            marc_writer->write(new_record);
            unique_id_to_date_map->addOrReplace(control_number, TimeUtil::GetCurrentDateAndTime());
            ++generated_count;
        }
    }

    std::cout << "Skipped " << skipped_dupe_count << " dupes and " << skipped_incomplete_count << " incomplete entry/entries and generated "
              << generated_count << " MARC record(s).\n";
}


const std::string UNIQUE_ID_TO_DATE_MAP_PATH(UBTools::GetTuelibPath() + "convert_core_json_to_marc.db");


} // unnamed namespace


int Main(int argc, char **argv) {
    if (argc < 4)
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

    if (argc != 3)
        Usage();

    if (::strncmp(argv[1], "--sigil=", 8) != 0)
        Usage();
    const std::string project_sigil(argv[1] + 8); // "ISIL" in German.
    --argc, ++argv;

    const std::string json_file_path(argv[1]);
    std::vector<std::string> json_filenames;
    if (FileUtil::GetFileNameList(json_file_path, &json_filenames) == 0) {
        LOG_ERROR("failed to get core-json file(s)");
    }
    for (const std::string &json_filename : json_filenames) {
        if (not json_filename.ends_with(".json"))
            continue;
        std::string json_source;
        FileUtil::ReadString(json_filename, &json_source);
        JSON::Parser parser(json_source);
        std::shared_ptr<JSON::JSONNode> tree_root;
        if (not parser.parse(&tree_root))
            LOG_ERROR("Failed to parse the JSON contents of \"" + json_file_path + "\": " + parser.getErrorMessage());

        const auto results_node(JSON::LookupNode("/results", tree_root));
        if (results_node == nullptr)
            LOG_ERROR("results node not found!");
        const auto array_root(JSON::JSONNode::CastToArrayNodeOrDie("results", results_node));

        KeyValueDB unique_id_to_date_map(UNIQUE_ID_TO_DATE_MAP_PATH);

        std::string marc_output_filename = json_filename;
        marc_output_filename = StringUtil::ReplaceString(".json", ".xml", marc_output_filename);
        const std::unique_ptr<MARC::Writer> marc_writer(MARC::Writer::Factory(marc_output_filename));

        GenerateMARCFromJSON(*array_root.get(), marc_writer.get(), project_sigil, _935_entries,
                              ignore_unique_id_dups, &unique_id_to_date_map);
    }

    return EXIT_SUCCESS;
}

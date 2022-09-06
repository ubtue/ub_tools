/** \brief Utility for downloading data from CORE.
 *  \author Mario Trojan (mario.trojan@uni-tuebingen.de)
 *
 *  \copyright 2022 Tübingen University Library.  All rights reserved.
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
#include <set>
#include <unordered_map>
#include "CORE.h"
#include "FileUtil.h"
#include "JSON.h"
#include "KeyValueDB.h"
#include "MARC.h"
#include "MiscUtil.h"
#include "StringUtil.h"
#include "TextUtil.h"
#include "TimeUtil.h"
#include "UBTools.h"
#include "util.h"


namespace {


[[noreturn]] void Usage() {
    ::Usage(
        "mode mode_params\n"
        "\n"
        "download id output_file\n"
        "\t- id: The CORE ID of the work to download.\n"
        "\t- output_file: The JSON result file.\n"
        "\n"
        "search query output_dir [limit]\n"
        "\t- query: The Query to use for CORE (like in the search field.)\n"
        "\t- output_dir: The directory to store the JSON result files (will be split due to API query limit restrictions).\n"
        "\t- limit (optional): The maximum amount of records that should be downloaded.\n"
        "\n"
        "merge input_dir output_file\n"
        "\t- input_dir: A dir with multiple JSON files to merge, typically from a search result.\n"
        "\t- output_file: The directory to store the merged JSON result file.\n"
        "\n"
        "filter input_file output_file\n"
        "\t- input_file: A single JSON input file.\n"
        "\t- output_file: The target JSON file without filtered datasets.\n"
        "\t- filtered_file_tuebingen: File to store datasets that have been filtered because they belong to tuebingen.\n"
        "\t- filtered_file_incomplete: File to store datasets that have been filtered because they are incomplete.\n"
        "\t- filtered_file_duplicates: File to store datasets that have been filtered because they are duplicates.\n"
        "\n"
        "count input_file\n"
        "\t- input_file: The JSON file to count the results from. Result will be written to stdout.\n"
        "\n"
        "statistics input_file\n"
        "\t- input_file: The JSON file to generate statistics from.\n"
        "\n"
        "convert [--create-unique-id-db|--ignore-unique-id-dups][--935-entry=entry] --sigil=project_sigil input_file output_file\n"
        "\t- --create-unique-id-db: This flag has to be specified the first time this program will be executed only.\n"
        "\t- --ignore-unique-id-dups: If specified MARC records will be created for unique ID's which we have encountered\n"
        "\t                           before.  The unique ID database will still be updated.\n"
        "\t- --935-entry: The structure of this repeatable flag is \"(TIT|LOK):subfield_a_value\".  If TIT has been specified then no "
        "subfield 2 will be generated. If LOK has been specified, subfield 2 will be set to LOK.\n"
        "\t- --sigil: This is used to generate an 852 field which is needed by the K10+ to be able to assign records to the appropriate "
        "project. An example would be DE-2619 for criminology.\n"
        "\t- input_file: The JSON file to convert.\n"
        "\t- output_file: The MARC or XML file to write to.\n"
        "\n"
        "data-providers\n"
        "\n");
}


// \return True if we found at least one author, else false.
void ConvertAuthors(const CORE::Work &work, MARC::Record * const record, std::set<std::string> * const authors) {
    authors->clear();
    bool first_author(true);
    for (const auto &author : work.getAuthors()) {
        if (authors->find(author.name_) != authors->end())
            continue; // Found a duplicate author!

        record->insertField(first_author ? "100" : "700", { { 'a', MiscUtil::NormalizeName(author.name_) }, { '4', "aut" } },
                            /*indicator1=*/'1');
        authors->insert(author.name_);
        if (first_author)
            first_author = false;
    }
}


// \return True if a title was found, else false.
void ConvertTitle(const CORE::Work &work, MARC::Record * const record) {
    record->insertField("245", 'a', work.getTitle());
}


void ConvertYear(const CORE::Work &work, MARC::Record * const record) {
    if (work.getYearPublished() == 0)
        return;
    record->insertField("936", 'j', std::to_string(work.getYearPublished()), 'u', 'w');
}


void ConvertDownloadURL(const CORE::Work &work, MARC::Record * const record) {
    if (not work.getDownloadUrl().empty())
        record->insertField("856", { { 'u', work.getDownloadUrl() }, { 'z', "LF" } }, '4', '0');
}


bool ConvertLanguage(const CORE::Work &work, MARC::Record * const record) {
    if (work.getLanguage().code_.empty())
        return false;
    std::string lang = MARC::MapToMARCLanguageCode(work.getLanguage().code_);
    if (lang != "eng" and lang != "ger" and lang != "spa" and lang != "baq" and lang != "cat" and lang != "por" and lang != "ita"
        and lang != "dut")
        return false;
    record->insertField("041", 'a', lang);
    return true;
}


void ConvertAbstract(const CORE::Work &work, MARC::Record * const record) {
    const std::string abstract(work.getAbstract());
    if (not abstract.empty() and abstract.length() > 5)
        record->insertField("520", 'a', StringUtil::Truncate(MARC::Record::MAX_VARIABLE_FIELD_DATA_LENGTH, abstract));
}


void ConvertUncontrolledIndexTerms(const CORE::Work &work, MARC::Record * const record) {
    if (not work.getDocumentType().empty() and work.getDocumentType() != "unknown")
        record->insertField("653", 'a', work.getDocumentType());
    if (not work.getFieldOfStudy().empty())
        record->insertField("653", 'a', work.getFieldOfStudy());
}


// \return True if any uncontrolled terms were found, else false.
bool ConvertYearPublished(const CORE::Work &work, MARC::Record * const record) {
    if (work.getYearPublished() == 0)
        return false;
    record->insertField("264", 'c', std::to_string(work.getYearPublished()), /*indicator1=*/' ', /*indicator2=*/'1');
    return true;
}


bool ConvertJournal(const CORE::Work &work, MARC::Record * const record) {
    for (const auto &journal : work.getJournals()) {
        for (const auto &identifier : journal.identifiers_) {
            if (MiscUtil::IsPossibleISSN(identifier)) {
                record->insertField("773", { { 'x', identifier } },
                                    /*indicator1=*/'0', /*indicator2=*/'8');
                return true;
            }
        }
    }
    return false;
}


void Convert935Entries(const std::vector<std::pair<std::string, std::string>> &_935_entries, MARC::Record * const record) {
    for (const auto &[subfield_a, subfield_2_selector] : _935_entries) {
        if (subfield_2_selector == "TIT")
            record->insertField("935", 'a', subfield_a);
        else
            record->insertField("935", { { 'a', subfield_a }, { '2', subfield_2_selector } });
    }
}


std::string ConvertId(const std::string &id) {
    return "CORE" + id;
}


void ConvertJSONToMARC(const std::vector<CORE::Work> &works, MARC::Writer * const marc_writer, const std::string &project_sigil,
                       const std::vector<std::pair<std::string, std::string>> &_935_entries, KeyValueDB * const unique_id_to_date_map) {
    unsigned generated_count(0);
    for (const auto &work : works) {
        const auto id(std::to_string(work.getId()));
        const auto control_number(ConvertId(id));

        MARC::Record new_record(MARC::Record::TypeOfRecord::LANGUAGE_MATERIAL, MARC::Record::BibliographicLevel::MONOGRAPH_OR_ITEM,
                                control_number);
        std::set<std::string> authors;
        ConvertAuthors(work, &new_record, &authors);

        // Do not use contributors anymore (team decision in video conf. on 09.02.2022)
        // ConvertContributors(*entry_object, &new_record, authors);

        ConvertTitle(work, &new_record);
        new_record.insertControlField("007", "cr||||");
        new_record.insertField("035", 'a', "(core)" + id);
        new_record.insertField("084", { { 'a', "2,1" }, { '2', "ssgn" } });
        new_record.insertField("591", 'a', "Metadaten maschinell erstellt (TUKRIM)");
        new_record.insertField("852", 'a', project_sigil);
        ConvertYear(work, &new_record);
        ConvertDownloadURL(work, &new_record);
        if (not ConvertLanguage(work, &new_record))
            continue;
        ConvertAbstract(work, &new_record);
        ConvertUncontrolledIndexTerms(work, &new_record);
        ConvertYearPublished(work, &new_record);
        ConvertJournal(work, &new_record);
        Convert935Entries(_935_entries, &new_record);
        marc_writer->write(new_record);
        unique_id_to_date_map->addOrReplace(control_number, TimeUtil::GetCurrentDateAndTime());
        ++generated_count;
    }

    std::cout << "Generated " << generated_count << " MARC record(s).\n";
}


const std::string UNIQUE_ID_TO_DATE_MAP_PATH(UBTools::GetTuelibPath() + "convert_core_json_to_marc.db");


void Convert(int argc, char **argv) {
    if (argc < 6)
        Usage();

    // ignore "mode" for further args processing
    --argc, ++argv;

    if (std::strcmp(argv[1], "--create-unique-id-db") == 0) {
        KeyValueDB::Create(UNIQUE_ID_TO_DATE_MAP_PATH);
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

    if (argc != 4)
        Usage();

    if (::strncmp(argv[1], "--sigil=", 8) != 0)
        Usage();
    const std::string project_sigil(argv[1] + 8); // "ISIL" in German.
    --argc, ++argv;

    const std::string json_file_path(argv[1]);
    const std::string marc_file_path(argv[2]);
    FileUtil::MakeParentDirectoryOrDie(marc_file_path, /*recursive=*/true);

    const auto works(CORE::GetWorksFromFile(json_file_path));
    KeyValueDB unique_id_to_date_map(UNIQUE_ID_TO_DATE_MAP_PATH);

    const std::unique_ptr<MARC::Writer> marc_writer(MARC::Writer::Factory(marc_file_path));
    ConvertJSONToMARC(works, marc_writer.get(), project_sigil, _935_entries, &unique_id_to_date_map);
}


void Download(int argc, char **argv) {
    // Parse args
    if (argc != 4)
        Usage();

    const unsigned id(StringUtil::ToUnsigned(argv[2]));
    const std::string output_file(argv[3]);
    FileUtil::MakeParentDirectoryOrDie(output_file, /*recursive=*/true);

    CORE::DownloadWork(id, output_file);
}


void Filter(int argc, char **argv) {
    if (argc != 7)
        Usage();

    bool ignore_unique_id_dups(false);
    if (std::strcmp(argv[1], "--ignore-unique-id-dups") == 0) {
        ignore_unique_id_dups = true;
        --argc, ++argv;
    }

    const std::string input_file(argv[2]);
    const std::string output_file(argv[3]);
    const std::string filter_file_tuebingen(argv[4]);
    const std::string filter_file_incomplete(argv[5]);
    const std::string filter_file_duplicate(argv[6]);

    const auto works(CORE::GetWorksFromFile(input_file));
    CORE::OutputFileStart(output_file);
    CORE::OutputFileStart(filter_file_tuebingen);
    CORE::OutputFileStart(filter_file_incomplete);
    CORE::OutputFileStart(filter_file_duplicate);
    bool first(true);
    unsigned skipped_uni_tue(0), skipped_dupe_count(0), skipped_incomplete_count(0);
    KeyValueDB unique_id_to_date_map(UNIQUE_ID_TO_DATE_MAP_PATH);
    for (const auto &work : works) {
        if (work.getPublisher() == "Universität Tübingen") {
            CORE::OutputFileAppend(filter_file_tuebingen, work, skipped_uni_tue == 0);
            ++skipped_uni_tue;
            continue;
        }
        if (work.getTitle() == "" or work.getAuthors().empty()) {
            CORE::OutputFileAppend(filter_file_incomplete, work, skipped_incomplete_count == 0);
            ++skipped_incomplete_count;
            continue;
        }
        const std::string control_number(ConvertId(std::to_string(work.getId())));
        if (ignore_unique_id_dups and unique_id_to_date_map.keyIsPresent(control_number)) {
            CORE::OutputFileAppend(filter_file_duplicate, work, skipped_dupe_count == 0);
            ++skipped_dupe_count;
            continue;
        }
        CORE::OutputFileAppend(output_file, work, first);
        first = false;
    }
    CORE::OutputFileEnd(output_file);
    CORE::OutputFileEnd(filter_file_tuebingen);
    CORE::OutputFileEnd(filter_file_incomplete);
    CORE::OutputFileEnd(filter_file_duplicate);

    LOG_INFO("Filtered " + std::to_string(skipped_uni_tue) + " Uni Tübingen records");
    LOG_INFO("Filtered " + std::to_string(skipped_incomplete_count) + " incomplete records");
    LOG_INFO("Filtered " + std::to_string(skipped_dupe_count) + " duplicate records");
}


void Merge(int argc, char **argv) {
    if (argc != 4)
        Usage();

    const std::string input_dir(argv[2]);
    const std::string output_file(argv[3]);

    // Reset target file
    if (FileUtil::Exists(output_file))
        throw std::runtime_error("target file already exists: " + output_file);
    CORE::OutputFileStart(output_file);
    bool first(true);

    // We want to sort the files alphabetically first
    std::vector<std::string> input_files_sort;
    FileUtil::Directory input_files_unsorted(input_dir, ".json$");
    for (const auto &input_file : input_files_unsorted) {
        input_files_sort.emplace_back(input_file.getFullName());
    }
    std::sort(input_files_sort.begin(), input_files_sort.end());


    // Merge into output file in sorted order
    for (const auto &input_file : input_files_sort) {
        LOG_INFO("merging " + input_file + " into " + output_file);
        const auto entities(CORE::GetEntitiesFromFile(input_file));
        for (const auto &entity : entities) {
            CORE::OutputFileAppend(output_file, entity, first);
            first = false;
        }
    }

    // Close target file
    CORE::OutputFileEnd(output_file);
}


void Search(int argc, char **argv) {
    // Parse args
    if (argc != 4 && argc != 5)
        Usage();

    const std::string query(argv[2]);
    const std::string output_dir(argv[3]);

    // Setup CORE instance & parameters
    CORE::SearchParams params;
    params.q_ = query;
    params.scroll_ = true;
    params.limit_ = 1000;
    params.exclude_ = { "fullText" };
    params.entity_type_ = CORE::EntityType::WORK;
    unsigned limit(0);
    if (argc == 5)
        limit = StringUtil::ToUnsigned(argv[4]);

    // Perform download
    CORE::SearchBatch(params, output_dir, limit);
}


void Count(int argc, char **argv) {
    // Parse args
    if (argc != 3)
        Usage();
    const std::string core_file(argv[2]);

    // Load file
    const auto works(CORE::GetWorksFromFile(core_file));
    std::cout << works.size();
}


void Statistics(int argc, char **argv) {
    // Parse args
    if (argc != 3)
        Usage();
    const std::string core_file(argv[2]);

    // Load file
    const auto works(CORE::GetWorksFromFile(core_file));

    unsigned count_works(works.size());
    unsigned count_articles(0);
    unsigned count_uni_tue(0);
    unsigned count_empty_title(0);
    unsigned count_empty_authors(0);

    std::map<std::string, unsigned> languages;
    for (const auto &work : works) {
        if (work.isArticle())
            ++count_articles;

        if (work.getTitle().empty())
            ++count_empty_title;

        if (work.getAuthors().empty())
            ++count_empty_authors;

        const auto language_iter(languages.find(work.getLanguage().code_));
        if (language_iter == languages.end())
            languages[work.getLanguage().code_] = 1;
        else
            ++languages[work.getLanguage().code_];

        if (work.getPublisher() == "Universität Tübingen")
            ++count_uni_tue;
    }

    LOG_INFO("Statistics for " + core_file + ":");
    LOG_INFO(std::to_string(count_works) + " datasets (" + std::to_string(count_articles) + " articles)");
    LOG_INFO(std::to_string(count_uni_tue) + " datasets from Universität Tübingen");
    LOG_INFO(std::to_string(count_empty_title) + " datasets with empty titles");
    LOG_INFO(std::to_string(count_empty_authors) + " datasets without authors");

    std::string languages_msg("languages: ");
    bool first(true);
    for (const auto &[language_code, language_count] : languages) {
        if (not first)
            languages_msg += ", ";
        languages_msg += "\"" + language_code + "\": " + std::to_string(language_count);
        first = false;
    }
    LOG_INFO(languages_msg);
}


void DataProviders(int argc, char ** /*argv*/) {
    // Parse args
    if (argc != 2)
        Usage();

    CORE::SearchParams params;
    params.q_ = "*";
    params.limit_ = 100;
    params.entity_type_ = CORE::EntityType::DATA_PROVIDER;
    const auto result(CORE::Search(params));
    LOG_INFO(std::to_string(result.total_hits_));
}


} // unnamed namespace


int Main(int argc, char **argv) {
    if (argc < 2)
        Usage();

    const std::string mode(argv[1]);
    if (mode == "download")
        Download(argc, argv);
    else if (mode == "search")
        Search(argc, argv);
    else if (mode == "merge")
        Merge(argc, argv);
    else if (mode == "filter")
        Filter(argc, argv);
    else if (mode == "convert")
        Convert(argc, argv);
    else if (mode == "count")
        Count(argc, argv);
    else if (mode == "statistics")
        Statistics(argc, argv);
    else if (mode == "data-providers")
        DataProviders(argc, argv);
    else
        Usage();

    return EXIT_SUCCESS;
}

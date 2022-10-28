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

#include <fstream>
#include <iostream>
#include <map>
#include <regex>
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
        "filter input_file output_file_keep output_file_skip [data_provider_filter_type] [data_provider_ids_file]\n"
        "\t- input_file: A single JSON input file.\n"
        "\t- output_file_keep: The target JSON file with dataset that should be kept.\n"
        "\t- output_file_skip: File to store datasets that have been removed when filtering. The reason will be stored in each JSON "
        "entry.\n"
        "\t- data_provider_filter_type: 'keep' or 'skip'.\n"
        "\t- data_provider_ids_file: File that contains the data provider ids to be used as a filter (1 by line).\n"
        "\n"
        "count input_file\n"
        "\t- input_file: The JSON file to count the results from. Result will be written to stdout.\n"
        "\n"
        "statistics [--extended] input_file\n"
        "\t- [--extended]: If given, print additional statistics (e.g. about data providers).\n"
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
        "data-providers output_file\n"
        "\t- output_file: The CSV file to write to.\n"
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


void ConvertLanguage(const CORE::Work &work, MARC::Record * const record) {
    const std::string lang(MARC::MapToMARCLanguageCode(work.getLanguage().code_));
    record->insertField("041", 'a', lang);
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


void ConvertYearPublished(const CORE::Work &work, MARC::Record * const record) {
    if (work.getYearPublished() != 0)
        record->insertField("264", 'c', std::to_string(work.getYearPublished()), /*indicator1=*/' ', /*indicator2=*/'1');
}


void ConvertJournal(const CORE::Work &work, MARC::Record * const record) {
    for (const auto &journal : work.getJournals()) {
        for (const auto &identifier : journal.identifiers_) {
            if (MiscUtil::IsPossibleISSN(identifier)) {
                record->insertField("773", { { 'x', identifier } },
                                    /*indicator1=*/'0', /*indicator2=*/'8');
            }
        }
    }
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
        ConvertLanguage(work, &new_record);
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
    if (argc != 5)
        if (argc != 7)
            Usage();

    const std::string input_file(argv[2]);
    const std::string output_file_keep(argv[3]);
    const std::string output_file_skip(argv[4]);

    std::string filter_operator;
    std::string keyword_file;

    std::string filter_key;
    std::vector<unsigned long> filter_data_provider_ids;
    bool filter;

    const auto works(CORE::GetWorksFromFile(input_file));
    CORE::OutputFileStart(output_file_keep);
    CORE::OutputFileStart(output_file_skip);
    bool first(true);

    unsigned skipped(0), skipped_uni_tue_count(0), skipped_dupe_count(0), skipped_incomplete_count(0), skipped_language_count(0),
        skipped_data_provider_count(0);

    if (argc == 7) {
        filter_operator = argv[5];
        if (filter_operator == "keep")
            filter = false;
        else if (filter_operator == "skip")
            filter = true;
        else
            Usage();

        std::vector<std::string> filter_data_provider_lines = FileUtil::ReadLines::ReadOrDie(argv[6]);
        for (const auto &line : filter_data_provider_lines) {
            filter_data_provider_ids.emplace_back(StringUtil::ToUnsignedLong(line));
        }
    }


    for (auto work : works) {
        if (not filter_data_provider_ids.empty()) {
            const bool is_in(MiscUtil::Intersect(work.getDataProviderIds(), filter_data_provider_ids).size() > 0);
            if (is_in == filter) {
                work.setFilteredReason("Data Provider");
                CORE::OutputFileAppend(output_file_skip, work, skipped == 0);
                ++skipped;
                ++skipped_data_provider_count;
                continue;
            }
        }
        if (work.getPublisher() == "Universität Tübingen") {
            work.setFilteredReason("Universität Tübingen");
            CORE::OutputFileAppend(output_file_skip, work, skipped == 0);
            ++skipped;
            ++skipped_uni_tue_count;
            continue;
        }
        if (work.getTitle().empty() or work.getAuthors().empty()) {
            work.setFilteredReason("Empty title or authors");
            CORE::OutputFileAppend(output_file_skip, work, skipped == 0);
            ++skipped;
            ++skipped_incomplete_count;
            continue;
        }

        static const std::unordered_set<std::string> allowed_languages({ "eng", "ger", "spa", "baq", "cat", "por", "ita", "dut" });
        if (work.getLanguage().code_.empty() or not allowed_languages.contains(MARC::MapToMARCLanguageCode(work.getLanguage().code_))) {
            work.setFilteredReason("Language empty or not allowed");
            CORE::OutputFileAppend(output_file_skip, work, skipped == 0);
            ++skipped;
            ++skipped_language_count;
            continue;
        }

        CORE::OutputFileAppend(output_file_keep, work, first);
        first = false;
    }
    CORE::OutputFileEnd(output_file_keep);
    CORE::OutputFileEnd(output_file_skip);

    LOG_INFO(
        "Filtered " + std::to_string(skipped) + " records, thereof:\n"
        "- " + std::to_string(skipped_data_provider_count) + " Data Provider\n"
        "- " + std::to_string(skipped_uni_tue_count) + " Uni Tübingen\n"
        "- " + std::to_string(skipped_incomplete_count) + " incomplete\n"
        "- " + std::to_string(skipped_dupe_count) + " duplicate\n"
        "- " + std::to_string(skipped_language_count) + " language"
    );
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
    CORE::SearchParamsWorks params;
    params.q_ = query;
    params.scroll_ = true;
    params.limit_ = 1000;
    params.exclude_ = { "fullText" };
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
    if (argc != 3 && argc != 4)
        Usage();
    else if (argc == 4 && std::strcmp(argv[2], "--extended") != 0)
        Usage();

    std::string core_file(argv[2]);
    if (argc == 4)
        core_file = argv[3];
    const bool extended = (argc == 4);

    // Load file
    const auto works(CORE::GetWorksFromFile(core_file));

    unsigned count_works(works.size());
    unsigned count_articles(0);
    unsigned count_uni_tue(0);
    unsigned count_empty_title(0);
    unsigned count_empty_authors(0);
    unsigned count_multiple_data_providers(0);

    std::map<unsigned long, unsigned> data_providers;
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
            ++language_iter->second;

        if (work.getPublisher() == "Universität Tübingen")
            ++count_uni_tue;

        const auto data_provider_ids(work.getDataProviderIds());
        if (data_provider_ids.size() > 1)
            ++count_multiple_data_providers;

        for (const auto &data_provider_id : data_provider_ids) {
            const auto data_providers_iter(data_providers.find(data_provider_id));
            if (data_providers_iter == data_providers.end())
                data_providers[data_provider_id] = 1;
            else
                ++data_providers_iter->second;
        }
    }

    LOG_INFO("Statistics for " + core_file + ":");
    LOG_INFO(std::to_string(count_works) + " datasets (" + std::to_string(count_articles) + " articles)");
    LOG_INFO(std::to_string(count_multiple_data_providers) + " datasets are associated with multiple data providers");
    LOG_INFO(std::to_string(count_uni_tue) + " datasets from publisher: \"Universität Tübingen\"");
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

    if (extended) {
        std::string data_providers_msg("data providers:\n");
        std::vector<std::pair<long unsigned, unsigned>> data_providers_sort;
        for (const auto &item : data_providers) {
            data_providers_sort.emplace_back(item);
        }
        std::sort(data_providers_sort.begin(), data_providers_sort.end(), [](const auto &x, const auto &y) { return x.second > y.second; });
        for (const auto &[data_provider_id, count] : data_providers_sort) {
            data_providers_msg += "ID: " + std::to_string(data_provider_id) + ", count: " + std::to_string(count) + "\n";
        }
        LOG_INFO(data_providers_msg);
    }
}


void DataProviders(int argc, char **argv) {
    // Parse args
    if (argc != 3)
        Usage();

    const std::string output_file(argv[2]);

    CORE::SearchParamsDataProviders params;
    params.q_ = "*";
    params.limit_ = 1000;
    const auto data_providers(CORE::SearchBatch(params));

    unsigned count(0);
    FileUtil::WriteStringOrDie(output_file, "ID;Name;Homepage URL;Type;Metadata Format;Created Date\n");
    for (const auto &data_provider : data_providers) {
        ++count;
        FileUtil::AppendStringOrDie(output_file, std::to_string(data_provider.getId()) + ";" + TextUtil::CSVEscape(data_provider.getName())
                                                     + ";" + TextUtil::CSVEscape(data_provider.getHomepageUrl()) + ";"
                                                     + TextUtil::CSVEscape(data_provider.getType()) + ";"
                                                     + TextUtil::CSVEscape(data_provider.getMetadataFormat()) + ";"
                                                     + TextUtil::CSVEscape(data_provider.getCreatedDate()) + "\n");
    }
    LOG_INFO("Generated " + output_file + " with " + std::to_string(count) + " entries.");
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

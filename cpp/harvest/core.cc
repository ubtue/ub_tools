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
        "[mode] [mode_params]\n"
        "download id output_file\n"
        "\t- id: The CORE ID of the work to download.\n"
        "\t- output_file: The JSON result file.\n"
        "\n"
        "search query output_dir\n"
        "\t- query: The Query to use for CORE (like in the search field.)\n"
        "\t- output_dir: The directory to store the JSON result files (will be split due to API query limit restrictions).\n"
        "\n"
        "merge input_dir output_file\n"
        "\t- input_dir: A dir with multiple JSON files to merge.\n"
        "\t- output_file: The directory to store the merged JSON result file.\n"
        "\n"
        "statistics input_file\n"
        "\t- input_file: The JSON file to generate statistics from.\n"
        "\n"
        "convert [--create-unique-id-db|--ignore-unique-id-dups][--935-entry=entry] --sigil=project_sigil input_file output_file\n"
        "\t- --create-unique-id-db: This flag has to be specified the first time this program will be executed only.\n"
        "\t- --ignore-unique-id-dups: If specified MARC records will be created for unique ID's which we have encountered\n"
        "\t                           before.  The unique ID database will still be updated.\n"
        "\t- --935-entry: The structure of this repeatable flag is \"(TIT|LOK):subfield_a_value\".  If TIT has been specified then no subfield 2 will be generated. If LOK has been specified, subfield 2 will be set to LOK.\n"
        "\t- --sigil: This is used to generate an 852 field which is needed by the K10+ to be able to assign records to the appropriate project. An example would be DE-2619 for criminology.\n"
        "\t- input_file: The JSON file to convert.\n"
        "\t- output_file: The MARC or XML file to write to.\n"
        "\n");
}


// \return True if we found at least one author, else false.
bool ConvertAuthors(const CORE::Work &work, MARC::Record * const record, std::set<std::string> * const authors) {
    if (work.authors_.empty())
        return false;

    authors->clear();
    bool first_author(true);
    for (const auto &author : work.authors_) {
        if (authors->find(author.name_) != authors->end())
            continue; // Found a duplicate author!

        record->insertField(first_author ? "100" : "700", { { 'a', MiscUtil::NormalizeName(author.name_) }, { '4', "aut" } },
                            /*indicator1=*/'1');
        authors->insert(author.name_);
        if (first_author)
            first_author = false;
    }

    return not first_author;
}


// \return True if a title was found, else false.
bool ConvertTitle(const CORE::Work &work, MARC::Record * const record) {
    if (work.title_.empty())
        return false;
    record->insertField("245", 'a', work.title_);
    return true;
}


void ConvertYear(const CORE::Work &work, MARC::Record * const record) {
    if (work.year_published_ == 0)
        return;
    record->insertField("936", 'j', std::to_string(work.year_published_), 'u', 'w');
}


void ConvertDownloadURL(const CORE::Work &work, MARC::Record * const record) {
    if (not work.download_url_.empty())
        record->insertField("856", { { 'u', work.download_url_ }, { 'z', "LF" } }, '4', '0');
}


bool ConvertLanguage(const CORE::Work &work, MARC::Record * const record) {
    if (work.language_.code_.empty())
        return false;
    std::string lang = MARC::MapToMARCLanguageCode(work.language_.code_);
    if (lang != "eng" and lang != "ger" and lang != "spa" and lang != "baq" and lang != "cat" and lang != "por" and lang != "ita" and lang != "dut")
        return false;
    record->insertField("041", 'a', lang);
    return true;
}


// \return True if an abstract was found, else false.
bool ConvertAbstract(const CORE::Work &work, MARC::Record * const record) {
    if (work.abstract_.empty())
        return false;
    std::string abstract_value = work.abstract_.length() > 5 ? work.abstract_ : "not available";
    record->insertField("520", 'a', StringUtil::Truncate(MARC::Record::MAX_VARIABLE_FIELD_DATA_LENGTH, abstract_value));
    return true;
}


// \return True if any uncontrolled terms were found, else false.
bool ConvertUncontrolledIndexTerms(const CORE::Work &work, MARC::Record * const record) {
    bool found_at_least_one_index_term(false);

    if (not work.document_type_.empty() and work.document_type_ != "unknown") {
        record->insertField("653", 'a', work.document_type_);
        found_at_least_one_index_term = true;
    }

    if (work.field_of_study_.empty())
        return found_at_least_one_index_term;

    record->insertField("653", 'a', work.field_of_study_);
    found_at_least_one_index_term = true;

    return found_at_least_one_index_term;
}


// \return True if any uncontrolled terms were found, else false.
bool ConvertYearPublished(const CORE::Work &work, MARC::Record * const record) {
    if (work.year_published_ == 0)
        return false;
    record->insertField("264", 'c', std::to_string(work.year_published_), /*indicator1=*/' ', /*indicator2=*/'1');
    return true;
}


bool ConvertJournal(const CORE::Work &work, MARC::Record * const record) {
    for (const auto &journal : work.journals_) {
        for (const auto &identifier : journal.identifiers_) {
            if (MiscUtil::IsPossibleISSN(identifier)) {
                record->insertField("773",
                                    {
                                        { 'x', identifier }
                                    },
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


void ConvertJSONToMARC(const std::vector<CORE::Work> &works,
                          MARC::Writer * const marc_writer, const std::string &project_sigil,
                          const std::vector<std::pair<std::string, std::string>> &_935_entries,
                          const bool ignore_unique_id_dups,
                          KeyValueDB * const unique_id_to_date_map) {
    unsigned skipped_dupe_count(0), generated_count(0), skipped_incomplete_count(0), skipped_uni_tue(0);
    for (const auto &work : works) {
        const auto id(std::to_string(work.id_));
        const auto control_number("CORE" + id);
        if (ignore_unique_id_dups and unique_id_to_date_map->keyIsPresent(control_number))
            ++skipped_dupe_count;
        else if (work.publisher_ == "Universität Tübingen")
            ++skipped_uni_tue;
        else {
            MARC::Record new_record(MARC::Record::TypeOfRecord::LANGUAGE_MATERIAL, MARC::Record::BibliographicLevel::MONOGRAPH_OR_ITEM,
                                    control_number);
            std::set<std::string> authors;
            if (not ConvertAuthors(work, &new_record, &authors)) {
                ++skipped_incomplete_count;
                continue;
            }

            //Do not use contributors anymore (team decision in video conf. on 09.02.2022)
            //ConvertContributors(*entry_object, &new_record, authors);

            if (not ConvertTitle(work, &new_record)) {
                ++skipped_incomplete_count;
                continue;
            }
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
    }

    std::cout << "Skipped " << skipped_dupe_count << " dupes and " << skipped_incomplete_count << " incomplete entries and "
              << skipped_uni_tue << " from UniTue and generated "  << generated_count << " MARC record(s).\n";
}


const std::string UNIQUE_ID_TO_DATE_MAP_PATH(UBTools::GetTuelibPath() + "convert_core_json_to_marc.db");


void Convert(int argc, char **argv) {
    if (argc < 5)
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

    if (argc != 4)
        Usage();

    if (::strncmp(argv[1], "--sigil=", 8) != 0)
        Usage();
    const std::string project_sigil(argv[1] + 8); // "ISIL" in German.
    --argc, ++argv;

    const std::string json_file_path(argv[1]);
    const std::string marc_file_path(argv[2]);
    std::vector<std::string> json_filenames;
    if (FileUtil::GetFileNameList(json_file_path, &json_filenames) == 0) {
        LOG_ERROR("failed to get core-json file(s) for: " + json_file_path);
    }
    for (const std::string &json_filename : json_filenames) {
        if (not StringUtil::EndsWith(json_filename, ".json"))
            continue;

        const auto works(CORE::GetWorksFromFile(json_filename));
        KeyValueDB unique_id_to_date_map(UNIQUE_ID_TO_DATE_MAP_PATH);

        const std::unique_ptr<MARC::Writer> marc_writer(MARC::Writer::Factory(marc_file_path));
        ConvertJSONToMARC(works, marc_writer.get(), project_sigil, _935_entries,
                              ignore_unique_id_dups, &unique_id_to_date_map);
    }
}


void Download(int argc, char **argv) {
    // Parse args
    if (argc != 4)
        Usage();

    const unsigned id(StringUtil::ToUnsigned(argv[2]));
    const std::string output_file(argv[3]);

    CORE core;
    core.downloadWork(id, output_file);
}


void Merge(int argc, char **argv) {
    if (argc != 4)
        Usage();

    const std::string input_dir(argv[2]);
    const std::string output_file(argv[3]);

    // Reset target file
    if (FileUtil::Exists(output_file))
        throw std::runtime_error("target file already exists: " + output_file);
    FileUtil::AppendString(output_file, "[\n");
    bool first(true);

    // Prepare reading input dir
    FileUtil::Directory input_files(input_dir, ".json$");
    for (const auto &input_file : input_files) {
        LOG_INFO("merging " + input_file.getFullName() + " into " + output_file);
        const CORE::SearchResponse response(FileUtil::ReadStringOrDie(input_file.getFullName()));
        for (const auto &result : response.results_) {
            if (not first)
                FileUtil::AppendString(output_file, ",\n");

            FileUtil::AppendString(output_file, result->toString());
            first = false;
        }
    }

    // Close target file
    FileUtil::AppendString(output_file, "\n]");
}


void Search(int argc, char **argv) {
    // Parse args
    if (argc != 4)
        Usage();

    const std::string query(argv[2]);
    const std::string output_dir(argv[3]);

    // Setup CORE instance & parameters
    CORE core;
    CORE::SearchParams params;
    params.q_ = query;
    params.exclude_ = { "fullText" }; // for performance reasons
    params.limit_ = 100; // default 10, max 100
    params.entity_type_ = CORE::EntityType::WORK;

    // Perform download
    core.searchBatch(params, output_dir);
}


void Statistics(int argc, char **argv) {
    // Parse args
    if (argc != 3)
        Usage();
    const std::string core_file(argv[2]);

    // Load file
    const auto works(CORE::GetWorksFromFile(core_file));

    unsigned count(0);
    unsigned articles(0);
    std::map<std::string, unsigned> languages;
    for (const auto &work : works) {
        ++count;
        if (work.isArticle())
            ++articles;
        const auto language_iter(languages.find(work.language_.code_));
        if (language_iter == languages.end())
            languages[work.language_.code_] = 1;
        else
            ++languages[work.language_.code_];

    }

    LOG_INFO("Statistics for " + core_file + ":");
    LOG_INFO(std::to_string(count) + " datasets (" + std::to_string(count) + " articles)");

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


} // unnamed namespace


int Main(int argc, char **argv) {
    if (argc < 2)
        Usage();

    const std::string mode(argv[1]);
    if (mode == "search")
        Search(argc, argv);
    else if (mode == "convert")
        Convert(argc, argv);
    else if (mode == "merge")
        Merge(argc, argv);
    else if (mode == "download")
        Download(argc, argv);
    else if (mode == "statistics")
        Statistics(argc, argv);
    else
        Usage();

    return EXIT_SUCCESS;
}

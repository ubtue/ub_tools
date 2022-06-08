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
#include "CORE.h"
#include "FileUtil.h"
#include "StringUtil.h"
#include "util.h"


namespace {


[[noreturn]] void Usage() {
    ::Usage(
        "[mode] [mode_params]\n"
        "batch query output_dir\n"
        "\t- query: The Query to use for CORE (like in the search field.)\n"
        "\t- output_dir: The directory to store the JSON result files (will be split due to API query limit restrictions).\n"
        "single id output_file\n"
        "\t- query: The CORE ID of the work to download.\n"
        "\t- output_dir: The directory to store the JSON result file.\n"
        "merge input_dir output_file\n"
        "\t- input_dir: A dir with multiple JSON files to merge.\n"
        "\t- output_file: The directory to store the merged JSON result file.\n"
        "\n");
}


} // unnamed namespace


void downloadWork(int argc, char **argv) {
    // Parse args
    if (argc != 4)
        Usage();

    const unsigned id(StringUtil::ToUnsigned(argv[2]));
    const std::string output_file(argv[3]);

    CORE core;
    core.downloadWork(id, output_file);
}


void merge(int argc, char **argv) {
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


void searchBatch(int argc, char **argv) {
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


void statistics(int argc, char **argv) {
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


int Main(int argc, char **argv) {
    if (argc < 2)
        Usage();

    const std::string mode(argv[1]);
    if (mode == "batch")
        searchBatch(argc, argv);
    if (mode == "merge")
        merge(argc, argv);
    else if (mode == "single")
        downloadWork(argc, argv);
    else if (mode == "statistics")
        statistics(argc, argv);
    else
        Usage();

    return EXIT_SUCCESS;
}

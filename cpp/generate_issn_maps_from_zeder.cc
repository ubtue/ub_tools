/** \brief Utility for generating ISSN mapping tables from Zeder.
 *  \author Madeeswaran Kannan (madeeswaran.kannan@uni-tuebingen.de)
 *
 *  \copyright 2019 Universitätsbibliothek Tübingen.  All rights reserved.
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
#include <functional>
#include <iostream>
#include <unordered_set>
#include <cstdio>
#include <cstdlib>
#include "BSZTransform.h"
#include "ExecUtil.h"
#include "File.h"
#include "FileUtil.h"
#include "MiscUtil.h"
#include "StringUtil.h"
#include "util.h"
#include "Zeder.h"


namespace {


const std::string FIND_DUPLICATE_ISSNS_FLAG("--find-duplicate-issns");
const std::string COMMIT_AND_PUSH_FLAG("--push-to-github");


[[noreturn]] void Usage() {
    ::Usage("[" + FIND_DUPLICATE_ISSNS_FLAG + "] [" + COMMIT_AND_PUSH_FLAG + "] issn_map_directory <map-type=filename>"
            " <map-type=filename>...\n\nValid map type(s): ssg");
}


enum MapType { SSG };


struct MapParams {
    std::string type_string_;
    std::string zeder_column_;
    std::function<std::string(const std::string &, const unsigned)> convert_zeder_value_to_map_value_;
};


std::string ZederToMapValueSSGN(const std::string &zeder_value, const unsigned /* zeder_id */) {
    // use the same string as in Zeder
    return zeder_value;
}


const std::map<MapType, MapParams> MAP_TYPE_TO_PARAMS {
    { MapType::SSG, { "ssg", "ber", ZederToMapValueSSGN } }
};


const std::map<std::string, MapType> STRING_TO_MAP_TYPE {
    { "ssg", MapType::SSG }
};


void ParseMapPairs(int argc, char * argv[], std::map<MapType, std::string> * const map_type_to_filename) {
    std::vector<std::string> splits;

    for (int i(1); i < argc; ++i) {
        StringUtil::Split(std::string(argv[i]), std::string("="), &splits);
        if (splits.size() != 2)
            Usage();

        const auto match(STRING_TO_MAP_TYPE.find(splits[0]));
        if (match == STRING_TO_MAP_TYPE.end())
            Usage();
        else if (map_type_to_filename->find(match->second) != map_type_to_filename->end())
            LOG_ERROR("Only one map file can be generated for each map type");

        map_type_to_filename->insert(std::make_pair(match->second, splits[1]));
    }
}

void DownloadFullDumpFromZeder(Zeder::Flavour flavour, Zeder::EntryCollection * const entries) {
    std::unordered_set<std::string> columns_to_download;
    std::unordered_set<unsigned> entries_to_download;
    std::unordered_map<std::string, std::string> filter_regexps;

    std::unique_ptr<Zeder::FullDumpDownloader::Params> params(
        new Zeder::FullDumpDownloader::Params(Zeder::GetFullDumpEndpointPath(flavour),
                                              entries_to_download, columns_to_download, filter_regexps));

    Zeder::EndpointDownloader::Factory(Zeder::EndpointDownloader::FULL_DUMP, std::move(params))->download(entries);
}


struct MapValue {
    std::string issn_;              // print or online
    std::string journal_title_;
    std::string zeder_instance_;
    unsigned zeder_id_;
    std::string value_;
public:
    MapValue(const std::string &issn, const std::string &journal_title, const std::string &zeder_instance,
             const unsigned zeder_id, const std::string &value)
     : issn_(issn), journal_title_(journal_title), zeder_instance_(zeder_instance), zeder_id_(zeder_id), value_(value) {}

    static bool Comparator(const MapValue &a, const MapValue &b) {
        if (a.zeder_instance_ == b.zeder_instance_)
            return a.zeder_id_ < b.zeder_id_;
        else
            return a.zeder_instance_ < b.zeder_instance_;
    }
};


void GenerateISSNMap(const std::string &zeder_instance, Zeder::EntryCollection &entries, const MapParams &params,
                     std::vector<MapValue> * const map_values)
{
    static const std::string ZEDER_TITLE_COLUMN("tit");
    static const std::string ZEDER_ISSN_COLUMN("issn");
    static const std::string ZEDER_ESSN_COLUMN("essn");

    for (const auto &entry : entries) {
        const auto zeder_value(entry.getAttribute(params.zeder_column_, ""));
        if (zeder_value.empty()) {
            LOG_DEBUG("Skipping zeder entry " + std::to_string(entry.getId()) + " with no value for '" + params.zeder_column_ + "'");
            continue;
        }

        std::set<std::string> issns, scratch;
        StringUtil::Split(entry.getAttribute(ZEDER_ISSN_COLUMN, ""), std::set<char>{ ' ', ','}, &scratch,
                          /* suppress_empty_tokens = */ true);
        for (const auto &split : scratch) {
            if (MiscUtil::IsPossibleISSN(split))
                issns.insert(split);
        }

        StringUtil::Split(entry.getAttribute(ZEDER_ESSN_COLUMN, ""), std::set<char>{ ' ', ','}, &scratch,
                          /* suppress_empty_tokens = */ true);
        for (const auto &split : scratch) {
            if (MiscUtil::IsPossibleISSN(split))
                issns.insert(split);
        }

        for (const auto &issn : issns) {
            const auto converted_value(params.convert_zeder_value_to_map_value_(zeder_value, entry.getId()));
            map_values->emplace_back(issn, entry.getAttribute(ZEDER_TITLE_COLUMN), zeder_instance,
                                     entry.getId(), converted_value);
        }
    }
}


void FindDuplicateISSNs(const std::vector<MapValue> &map_values) {
    std::unordered_set<std::string> processed_issns;
    std::unordered_multimap<std::string, std::pair<std::string, unsigned>> processed_values;

    for (const auto &value : map_values) {
        processed_values.insert(std::make_pair(value.issn_, std::make_pair(value.zeder_instance_, value.zeder_id_)));
        processed_issns.insert(value.issn_);
    }

    for (const auto &issn : processed_issns) {
        if (processed_values.count(issn) > 1) {
            const auto range(processed_values.equal_range(issn));

            std::string warning_message("ISSN '" + issn + "' found in multiple Zeder entries: ");
            for (auto itr(range.first); itr != range.second; ++itr)
                warning_message += std::to_string(itr->second.second) + " (" + itr->second.first + ") ";

            LOG_WARNING(warning_message);
        }
    }
}


void WriteMapValuesToFile(const std::vector<MapValue> &map_values, const MapParams &map_params, const std::string &file_path) {
    const auto output(FileUtil::OpenOutputFileOrDie(file_path));

    for (const auto &value : map_values) {
        output->writeln(value.issn_ + "=" + value.value_ + " # (" + std::to_string(value.zeder_id_) + " | "
                        + value.zeder_instance_ + ") " + value.journal_title_);
    }

    LOG_INFO("Wrote " + std::to_string(map_values.size()) + " entries to " + map_params.type_string_ + " map '"
                 + file_path + "'");
}


int ExecuteGitCommand(const std::vector<std::string> &command_and_args, const std::string &working_directory,
                      std::string * const stdout, std::string * const stderr)
{
    static const auto GIT_PATH(ExecUtil::Which("git"));

    std::unordered_map<std::string, std::string> env_vars;
    std::string std_out_buffer, std_err_buffer;
    FileUtil::AutoTempFile std_out, std_err;

    const auto ret_code(ExecUtil::Exec(GIT_PATH, { command_and_args }, /* new_stdin = */ "", std_out.getFilePath(),
                                       std_err.getFilePath(), /* timeout_in_seconds = */ 0, SIGKILL, env_vars,
                                       working_directory));

    FileUtil::ReadStringOrDie(std_out.getFilePath(), stdout);
    FileUtil::ReadStringOrDie(std_err.getFilePath(), stderr);

    return ret_code;
}


void PushToGitHub(const std::string &issn_directory, const std::vector<std::string> &files_to_push) {
    if (files_to_push.empty())
        return;

    std::string std_out_buffer, std_err_buffer;

    // check for actual changes
    if (ExecuteGitCommand({ "status", "-z" }, issn_directory, &std_out_buffer, &std_err_buffer) != EXIT_SUCCESS)
        LOG_ERROR("Couldn't execute git status!\n\nstdout:\n" + std_out_buffer + "\n\nstderr:\n" + std_err_buffer);

    if (std_out_buffer.empty()) {
        LOG_INFO("No changes to push to GitHub");
        return;
    }

    // stage files for commit
    for (const auto &file : files_to_push) {
        if (ExecuteGitCommand({ "add", file }, issn_directory, &std_out_buffer, &std_err_buffer) != EXIT_SUCCESS) {
            LOG_ERROR("Couldn't execute git add for file '" + file + "'!\n\nstdout:\n" + std_out_buffer
                      + "\n\nstderr:\n" + std_err_buffer);
        }
    }

    // commit
    if (ExecuteGitCommand({ "commit", "--author=\"ubtue_robot <>\"", "-mRegenerated files from Zeder" }, issn_directory,
                          &std_out_buffer, &std_err_buffer) != EXIT_SUCCESS)
    {
        LOG_ERROR("Couldn't execute git commit!\n\nstdout:\n" + std_out_buffer + "\n\nstderr:\n" + std_err_buffer);
    }

    // push to remote
    if (ExecuteGitCommand({ "push" }, issn_directory, &std_out_buffer, &std_err_buffer) != EXIT_SUCCESS)
        LOG_ERROR("Couldn't execute git push!\n\nstdout:\n" + std_out_buffer + "\n\nstderr:\n" + std_err_buffer);

    LOG_INFO("Pushed " + std::to_string(files_to_push.size()) + " files to GitHub");
}


} // unnamed namespace


int Main(int argc, char *argv[]) {
    if (argc < 3)
        Usage();

    bool find_duplicate_issns(false);
    if (StringUtil::StartsWith(argv[1], FIND_DUPLICATE_ISSNS_FLAG)) {
        find_duplicate_issns = true;
        --argc, ++argv;
    }

    bool push_to_github(false);
    if (StringUtil::StartsWith(argv[1], COMMIT_AND_PUSH_FLAG)) {
        push_to_github = true;
        --argc, ++argv;
    }

    if (argc < 3)
        Usage();

    const std::string issn_directory(argv[1]);
    --argc, ++argv;

    std::map<MapType, std::string> map_filename_pairs;
    ParseMapPairs(argc, argv, &map_filename_pairs);

    Zeder::EntryCollection entries_ixtheo, entries_krimdok;
    DownloadFullDumpFromZeder(Zeder::Flavour::IXTHEO, &entries_ixtheo);
    DownloadFullDumpFromZeder(Zeder::Flavour::KRIMDOK, &entries_krimdok);

    std::vector<MapValue> map_values;
    std::vector<std::string> files_to_push;

    for (const auto &map_filename_pair : map_filename_pairs) {
        const auto &map_params(MAP_TYPE_TO_PARAMS.at(map_filename_pair.first));
        const std::string output_file(issn_directory + "/" + map_filename_pair.second);

        map_values.clear();
        GenerateISSNMap("IxTheo", entries_ixtheo, map_params, &map_values);
        GenerateISSNMap("KrimDok", entries_krimdok, map_params, &map_values);
        std::sort(map_values.begin(), map_values.end(), MapValue::Comparator);

        if (find_duplicate_issns)
            FindDuplicateISSNs(map_values);

        WriteMapValuesToFile(map_values, map_params, output_file);

        if (push_to_github)
            files_to_push.emplace_back(output_file);
    }

   if (push_to_github)
       PushToGitHub(issn_directory, files_to_push);

    return EXIT_SUCCESS;
}

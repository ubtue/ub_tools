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


[[noreturn]] void Usage() {
    ::Usage("[--find-duplicate-issns] [--push-to-github] issn_map_directory <map-type=filename> <map-type=filename>...\n\n"
            "Valid map type(s): ssg");
}


enum MapType { SSG };


struct MapParams {
    std::string type_string_;
    std::string zeder_column_;
    std::function<std::string(const std::string &, const unsigned)> convert_zeder_value_to_map_value_;
};


std::string ZederToMapValueSSGN(const std::string &zeder_value, const unsigned zeder_id) {
    switch (BSZTransform::GetSSGNTypeFromString(zeder_value)) {
    case BSZTransform::SSGNType::FG_0:
        return "0";
    case BSZTransform::SSGNType::FG_1:
        return "1";
    case BSZTransform::SSGNType::FG_01:
        return "0/1";
    case BSZTransform::SSGNType::INVALID:
        LOG_ERROR("invalid Zeder value for SSGN '" + zeder_value + "' (Zeder ID: " + std::to_string(zeder_id) + ")");
    }
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
    unsigned zeder_id_;
    std::string value_;

    MapValue(const std::string issn, const std::string journal_title, const unsigned zeder_id, const std::string &value)
     : issn_(issn), journal_title_(journal_title), zeder_id_(zeder_id), value_(value) {}
};


void GenerateISSNMap(Zeder::EntryCollection &entries, const MapParams &params,
                     std::vector<MapValue> * const map_values)
{
    static const std::string ZEDER_TITLE_COLUMN("tit");
    static const std::string ZEDER_ISSN_COLUMN("issn");
    static const std::string ZEDER_ESSN_COLUMN("essn");

    map_values->clear();

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
            map_values->emplace_back(issn, entry.getAttribute(ZEDER_TITLE_COLUMN),
                                     entry.getId(), converted_value);
        }
    }
}


void FindDuplicateISSNs(const std::vector<MapValue> &map_values) {
    std::unordered_set<std::string> processed_issns;
    std::unordered_multimap<std::string, unsigned> processed_values;

    for (const auto &value : map_values) {
        processed_values.insert(std::make_pair(value.issn_, value.zeder_id_));
        processed_issns.insert(value.issn_);
    }

    for (const auto &issn : processed_issns) {
        if (processed_values.count(issn) > 1) {
            const auto range(processed_values.equal_range(issn));

            std::string warning_message("ISSN '" + issn + "' found in multiple Zeder entries: ");
            for (auto itr(range.first); itr != range.second; ++itr)
                warning_message += std::to_string(itr->second) + " ";

            LOG_WARNING(warning_message);
        }
    }
}


void WriteMapValuesToFile(const std::vector<MapValue> &map_values, const MapParams &map_params, const std::string &file_path) {
    File output(file_path, "w");

    for (const auto &value : map_values) {
        output.writeln(value.issn_ + "=" + value.value_ + " # (" + std::to_string(value.zeder_id_) + ") "
                       + value.journal_title_);
    }

    LOG_INFO("Wrote " + std::to_string(map_values.size()) + " entries to " + map_params.type_string_ + " map '"
                 + file_path + "'");
}


void PushToGitHub(const std::string &issn_directory, const std::vector<std::string> &files_to_push) {
    std::string command_line("cd " + issn_directory);

    for (const auto &file : files_to_push)
        command_line += " && git add " + file;

    command_line += " && git commit -m \"Update maps: ";

    for (const auto &file : files_to_push)
        command_line += file + " ";

    command_line += "\" && git pull && git push";

    std::string std_out, std_err;
    if (not ExecUtil::ExecSubcommandAndCaptureStdoutAndStderr(command_line, {}, &std_out, &std_err))
        LOG_WARNING("Couldn't push to GitHub! \n\nstdout:\n" + std_out + "\n\nstderr:\n" + std_err);
}


} // unnamed namespace


int Main(int argc, char *argv[]) {
    if (argc < 3)
        Usage();

    bool find_duplicate_issns(false);
    const std::string FIND_DUPLICATE_ISSNS_FLAG("--find-duplicate-issns");
    if (StringUtil::StartsWith(argv[1], FIND_DUPLICATE_ISSNS_FLAG)) {
        find_duplicate_issns = true;
        --argc, ++argv;
    }

    bool push_to_github(false);
    const std::string COMMIT_AND_PUSH_FLAG("--push-to-github");
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

    Zeder::EntryCollection entries;
    DownloadFullDumpFromZeder(Zeder::Flavour::IXTHEO, &entries);

    std::vector<MapValue> map_values;
    std::vector<std::string> files_to_push;

    for (const auto &map_filename_pair : map_filename_pairs) {
        const auto &map_params(MAP_TYPE_TO_PARAMS.at(map_filename_pair.first));
        const std::string output_file(issn_directory + "/" + map_filename_pair.second);

        GenerateISSNMap(entries, map_params, &map_values);
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

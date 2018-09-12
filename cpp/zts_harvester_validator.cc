/** \file   zts_harvester_validator.cc
 *  \brief  Tool to help validate ZTS Harvester entries
 *  \author Madeeswaran Kannan (madeeswaran.kannan@uni-tuebingen.de)
 *
 *  \copyright 2018 Universitätsbibliothek Tübingen.  All rights reserved.
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
#include <algorithm>
#include <iostream>
#include <map>
#include <unordered_map>
#include <unordered_set>
#include <cinttypes>
#include <csignal>
#include <cstring>
#include <unistd.h>
#include "ExecUtil.h"
#include "IniFile.h"
#include "MiscUtil.h"
#include "RegexMatcher.h"
#include "StringUtil.h"
#include "TimeUtil.h"
#include "util.h"
#include "Zeder.h"
#include "Zotero.h"


namespace {


[[noreturn]] void Usage() {
    std::cerr << "Usage: " << ::progname
              << " [--min-log-level=min_verbosity] [--ubtools-wd] output_file_path zts_harvester_args\n\n"
              << "        --ubtools-wd        Use the canonical ubtools directory as the working directory\n"
              << "    output_file_path        Generated report (.conf) file";
    std::exit(EXIT_FAILURE);
}


enum ErrorKind {
    UNKNOWN,
    ZTS_CONVERSION_FAILED,
    DOWNLOAD_MULTIPLE_FAILED,
    FAILED_TO_PARSE_JSON,
    ZTS_EMPTY_RESPONSE,
    BAD_STRPTIME_FORMAT
};


} // unnamed namespace


namespace std {
    template <>
    struct hash<::ErrorKind> {
        size_t operator()(const ::ErrorKind &error_kind) const {
            // hash method here.
            return hash<int>()(error_kind);
        }
    };
} // namespace std


namespace {


const std::unordered_map<ErrorKind, std::string> ERROR_KIND_TO_STRING_MAP{
    { UNKNOWN,                  "ERROR-UNKNOWN"  },
    { ZTS_CONVERSION_FAILED,    "ERROR-ZTS_CONVERSION_FAILED"  },
    { DOWNLOAD_MULTIPLE_FAILED, "ERROR-DOWNLOAD_MULTIPLE_FAILED"  },
    { FAILED_TO_PARSE_JSON,     "ERROR-FAILED_TO_PARSE_JSON"  },
    { ZTS_EMPTY_RESPONSE,       "ERROR-ZTS_EMPTY_RESPONSE"  },
    { BAD_STRPTIME_FORMAT,      "ERROR-BAD_STRPTIME_FORMAT"  },
};


struct HarvesterError {
    ErrorKind type;
    std::string message;
};


struct JournalErrors {
    std::unordered_map<std::string, HarvesterError> url_errors_;
};


bool ExecuteZtsHarvester(int argc, char *argv[], bool use_ubtools_folder, std::string * const stderr) {
    LOG_INFO("Executing ZTS Harvester. This will take a while...");

    std::string stdout_capture;
    std::vector<std::string> harvester_args{
        "--min-log-level=WARNING",
    };

    for (int i(1); i < argc; ++i)
        harvester_args.emplace_back(argv[i]);

    std::string working_directory("/usr/local/bin");
    if (use_ubtools_folder)
        working_directory = "/usr/local/ub_tools/cpp";

    return ExecUtil::ExecSubcommandAndCaptureStdoutAndStderr(working_directory + "/zts_harvester", harvester_args, &stdout_capture, stderr);
}


HarvesterError DetectHarvesterError(const std::string &message) {
    static const std::unordered_map<ErrorKind, RegexMatcher *> error_regexp_map{
        { ZTS_CONVERSION_FAILED,    RegexMatcher::RegexMatcherFactoryOrDie("^Zotero conversion failed: (.+?)$") },
        { DOWNLOAD_MULTIPLE_FAILED, RegexMatcher::RegexMatcherFactoryOrDie("^Download multiple results failed: (.+?)$") },
        { FAILED_TO_PARSE_JSON,     RegexMatcher::RegexMatcherFactoryOrDie("^failed to parse returned JSON: (.+?)$") },
        { ZTS_EMPTY_RESPONSE,       RegexMatcher::RegexMatcherFactoryOrDie("empty response\\!(.+?)$") },
        { BAD_STRPTIME_FORMAT,      RegexMatcher::RegexMatcherFactoryOrDie("StringToStructTm\\: don't know how to convert \\\"(.+?)\\\"") },
    };

    HarvesterError error{ UNKNOWN, "" };
    for (const auto &error_regexp : error_regexp_map) {
        if (error_regexp.second->matched(message)) {
            error.type = error_regexp.first;
            error.message = (*error_regexp.second)[1];
            break;
        }
    }

    return error;
}


bool ParseZtsHarvesterOutput(const std::string &harvester_output, std::unordered_map<std::string, JournalErrors> * const parsed_errors,
                             std::unordered_map<std::string, HarvesterError> * const unexpected_errors)
{
    std::vector<std::string> lines, line_splits;
    StringUtil::Split(harvester_output, '\n', &lines);
    for (const auto &line : lines) {
        if (StringUtil::Split(line, '\t', &line_splits) != 3) {
            unexpected_errors->insert(std::make_pair(line, DetectHarvesterError(line)));
            continue;
        }

        const auto &journal_name(line_splits.at(0)), &url(line_splits.at(1)), &message(line_splits.at(2));
        const auto url_error(DetectHarvesterError(message));
        auto match(parsed_errors->find(journal_name));
        if (match == parsed_errors->end()) {
            JournalErrors new_journal_errors;
            new_journal_errors.url_errors_[url] = url_error;
            parsed_errors->insert(std::make_pair(journal_name, new_journal_errors));
        } else
            match->second.url_errors_[url] = url_error;
    }

    return parsed_errors->empty() and unexpected_errors->empty();
}


void WriteReport(const std::string &report_file_path, const std::string &harvester_output,
                 const std::unordered_map<std::string, JournalErrors> &parsed_errors, const std::unordered_map<std::string, HarvesterError>
                 &unexpected_errors, bool successful_harvest)
{
    IniFile report("", true, true);
    report.appendSection("");
    report.getSection("")->insert("success", successful_harvest ? "true" : "false");

    std::string journal_names;
    for (const auto &parsed_error : parsed_errors) {
        const auto journal_name(parsed_error.first);
        if (journal_name.find('|') != std::string::npos)
            LOG_ERROR("Invalid character '|' in journal name '" + journal_name + "'");

        journal_names += journal_name + "|";
        report.appendSection(journal_name);

        for (const auto &url_error : parsed_error.second.url_errors_) {
            const auto error_string(ERROR_KIND_TO_STRING_MAP.at(url_error.second.type));
            // we cannot cache the section pointer as it can get invalidated after appending a new section
            report.getSection(journal_name)->insert(url_error.first, error_string);
            report.appendSection(error_string);
            report.getSection(error_string)->insert(url_error.first, url_error.second.message);
        }
    }

    const auto unknown_error_string(ERROR_KIND_TO_STRING_MAP.at(UNKNOWN));
    report.appendSection(unknown_error_string);
    for (const auto &unexpected_error : unexpected_errors) {
        const auto error_string(ERROR_KIND_TO_STRING_MAP.at(unexpected_error.second.type));
        report.getSection(unknown_error_string)->insert(unexpected_error.first, error_string);
        // if an error is both unexpected and unknown, don't report it
        if (unexpected_error.second.type != UNKNOWN) {
            report.appendSection(error_string);
            report.getSection(error_string)->insert(unexpected_error.first, unexpected_error.second.message);
        }
    }

    std::vector<std::string> lines;
    StringUtil::Split(harvester_output, '\n', &lines);

    const auto full_dump_section_name("HARVESTER-OUTPUT");
    report.appendSection(full_dump_section_name);
    auto full_dump_section(report.getSection(full_dump_section_name));
    for (const auto &line : lines)
        full_dump_section->insert("", "", "# " + line);

    report.getSection("")->insert("journal_names", journal_names);

    report.write(report_file_path);
}


} // unnamed namespace


int Main(int argc, char *argv[]) {
    if (argc < 2)
        Usage();

    bool use_ubtools_folder(false);
    if (std::strcmp(argv[1], "--ubtools-wd") == 0) {
        use_ubtools_folder = true;
        --argc, ++argv;
    }

    if (argc < 2)
        Usage();

    const std::string report_file_path(argv[1]);
    --argc, ++argv;

    std::string stderr_capture;
    std::unordered_map<std::string, JournalErrors> journal_errors;
    std::unordered_map<std::string, HarvesterError> unexpected_errors;

    bool success(ExecuteZtsHarvester(argc, argv, use_ubtools_folder, &stderr_capture));
    success = success & ParseZtsHarvesterOutput(stderr_capture, &journal_errors, &unexpected_errors);
    WriteReport(report_file_path, stderr_capture, journal_errors, unexpected_errors, success);

    LOG_INFO("Validation complete. Harvesting was " + std::string(success ? "" : "not ") + "successful");
    return success ? EXIT_SUCCESS : EXIT_FAILURE;
}

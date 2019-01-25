/** \file   zeder_to_zotero_pipeline.cc
 *  \brief  Tool to (semi-)automate the importing of data from Zeder
 *          into the Zotero Harvester pipeline
 *  \author Madeeswaran Kannan (madeeswaran.kannan@uni-tuebingen.de)
 *
 *  \copyright 2018,2019 Universitätsbibliothek Tübingen.  All rights reserved.
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
#include "FileUtil.h"
#include "IniFile.h"
#include "JournalConfig.h"
#include "MiscUtil.h"
#include "RegexMatcher.h"
#include "StringUtil.h"
#include "TextUtil.h"
#include "TimeUtil.h"
#include "util.h"
#include "Zeder.h"
#include "Zotero.h"


namespace {


[[noreturn]] void Usage() {
    std::cerr << "Usage: " << ::progname
              << " [--min-log-level=min_verbosity] [--ubtools-wd] config_file flavour\n\n"
              << "        --ubtools-wd        Use the canonical ubtools directory as the working directory\n"
              << "             flavour        Either 'ixtheo' or 'krimdok'.\n\n";
    std::exit(EXIT_FAILURE);
}


struct PipelineParams {
    Zeder::Flavour flavour_;
    bool use_ubtools_folder_;
    std::string working_directory_;
    std::string executable_directory_;
    std::string zeder_to_zoter_importer_config_file_;
    std::string harvester_config_file_;
    unsigned modified_time_cutoff_days_;
    std::unordered_set<std::string> columns_to_import_;
    std::unordered_map<std::string, std::string> filter_regexps_;
    std::string journal_name_column_;
    unsigned zts_harvester_validator_iterations_;

    PipelineParams(Zeder::Flavour flavour, bool use_ubtools_folder, const IniFile &config_file);
};


PipelineParams::PipelineParams(Zeder::Flavour flavour, bool use_ubtools_folder, const IniFile &config_file) {
    flavour_ = flavour;
    use_ubtools_folder_ = use_ubtools_folder;
    if (use_ubtools_folder_)
        executable_directory_ = "/usr/local/ub_tools/cpp";
    else
        executable_directory_ = "/usr/local/bin";

    switch (flavour_) {
    case Zeder::IXTHEO:
        working_directory_ = "/tmp/zeder_to_zotero_pipeline/ixtheo";
        break;
    case Zeder::KRIMDOK:
        working_directory_ = "/tmp/zeder_to_zotero_pipeline/krimdok";
        break;
    }

    zeder_to_zoter_importer_config_file_ = config_file.getString("", "zeder_to_zoter_importer_config_file");
    harvester_config_file_ = config_file.getString("", "zts_harvester_config_file");
    modified_time_cutoff_days_ = config_file.getUnsigned("", "skip_entries_older_than");
    zts_harvester_validator_iterations_ = config_file.getUnsigned("", "zts_harvester_validator_iterations");

    const auto flavour_section(config_file.getSection(Zeder::FLAVOUR_TO_STRING_MAP.at(flavour)));
    StringUtil::Split(flavour_section->getString("columns_to_import"), ',', &columns_to_import_);

    const auto filter_section_name(flavour_section->getString("column_filters"));
    const auto filter_section(config_file.getSection(filter_section_name));
    if (filter_section == config_file.end())
        LOG_ERROR("Couldn't find filter section '" + filter_section_name + "'");

    for (const auto &column_name : filter_section->getEntryNames())
        filter_regexps_[column_name] = filter_section->getString(column_name);

    journal_name_column_ = flavour_section->getString("journal_name_column");
    if (columns_to_import_.find(journal_name_column_) == columns_to_import_.end())
        LOG_ERROR("Journal column name '" + journal_name_column_ + "' not being imported");
}


void DownloadFullDump(const PipelineParams &params, Zeder::EntryCollection * const downloaded_entries) {
    const auto endpoint_url(Zeder::GetFullDumpEndpointPath(params.flavour_));
    std::unique_ptr<Zeder::FullDumpDownloader::Params> downloader_params(new Zeder::FullDumpDownloader::Params(endpoint_url,
                                                                         params.columns_to_import_, params.filter_regexps_));

    auto downloader(Zeder::FullDumpDownloader::Factory(Zeder::FullDumpDownloader::Type::FULL_DUMP, std::move(downloader_params)));
    if (not downloader->download(downloaded_entries))
        LOG_ERROR("Couldn't download full dump for " + Zeder::FLAVOUR_TO_STRING_MAP.at(params.flavour_));
}


void RemoveEntriesOlderThanCutoff(const PipelineParams &params, Zeder::EntryCollection * const downloaded_entries) {
    auto current_time_tm(TimeUtil::GetCurrentTimeGMT());
    auto current_time(::mktime(&current_time_tm)), cutoff_time(TimeUtil::AddDays(current_time, -params.modified_time_cutoff_days_));

    for (auto entry(downloaded_entries->begin()); entry != downloaded_entries->end();) {
        auto modified_time_buffer(entry->getLastModifiedTimestamp());
        const auto modified_time(::mktime(&modified_time_buffer));

        if (TimeUtil::IsDateInRange(cutoff_time, current_time, modified_time) != 0) {
            double days_diff = std::difftime(current_time, modified_time) / 3600 / 24;
            LOG_INFO("Skipping old entry " + std::to_string(entry->getId()) + " | Older by " + std::to_string(days_diff) + " day(s)");
            entry = downloaded_entries->erase(entry);
            continue;
        }

        ++entry;
    }
}


std::string GenerateConfigForNewAndUpdatedEntries(const PipelineParams &params, const Zeder::EntryCollection &new_and_updated_entries) {
    const std::string buffer_csv_file_path(params.working_directory_ + "/ztz_csv_buffer.csv"),
                      buffer_conf_file_path(params.working_directory_ + "/ztz_conf_buffer.conf");

    std::vector<std::string> attributes_to_export(params.columns_to_import_.begin(), params.columns_to_import_.end());
    std::unique_ptr<Zeder::CsvWriter::Params> exporter_params(new Zeder::CsvWriter::Params(buffer_csv_file_path, attributes_to_export));
    auto exporter(Zeder::Exporter::Factory(std::move(exporter_params)));

    exporter->write(new_and_updated_entries);

    std::string stderr_capture, stdout_capture;
    std::vector<std::string> importer_args{
        "--min-log-level=WARNING",
        "--mode=generate",
        TextUtil::UTF8ToLower(Zeder::FLAVOUR_TO_STRING_MAP.at(params.flavour_)),
        params.zeder_to_zoter_importer_config_file_,
        buffer_csv_file_path,
        buffer_conf_file_path
    };

    ExecUtil::ExecSubcommandAndCaptureStdoutAndStderr(params.executable_directory_ + "/zeder_to_zotero_importer", importer_args,
                                                      &stdout_capture, &stderr_capture);
    LOG_INFO(stderr_capture);
    return buffer_conf_file_path;
}


void DiffGeneratedConfigAgainstZtsHarvesterConfig(const PipelineParams &params, const std::string &generated_config_file_path,
                                                  std::unordered_set<unsigned> * const new_entry_ids,
                                                  std::unordered_set<unsigned> * const modified_entry_ids)
{
    static const std::unique_ptr<RegexMatcher> modified_entries_matcher(
        RegexMatcher::RegexMatcherFactoryOrDie("\\n+Modified entries\\:((?:\\s{1}\\d+)*)\\n*"));
    static const std::unique_ptr<RegexMatcher> new_entries_matcher(
        RegexMatcher::RegexMatcherFactoryOrDie("\\n+New entries\\:((?:\\s{1}\\d+)*)\\n*"));

    std::string stderr_capture, stdout_capture;
    std::vector<std::string> importer_args{
        "--min-log-level=INFO",
        "--mode=diff",
        TextUtil::UTF8ToLower(Zeder::FLAVOUR_TO_STRING_MAP.at(params.flavour_)),
        params.zeder_to_zoter_importer_config_file_,
        generated_config_file_path,
        params.harvester_config_file_
    };

    ExecUtil::ExecSubcommandAndCaptureStdoutAndStderr(params.executable_directory_ + "/zeder_to_zotero_importer", importer_args,
                                                      &stdout_capture, &stderr_capture);

    std::unordered_set<std::string> splits;
    auto string_to_unsigned([](const std::string &string) -> unsigned { return StringUtil::ToUnsigned(string);});

    if (modified_entries_matcher->matched(stderr_capture)) {
        StringUtil::Split((*modified_entries_matcher)[1], ',', &splits);
        std::transform(splits.begin(), splits.end(), std::inserter(*modified_entry_ids, modified_entry_ids->begin()), string_to_unsigned);
    }

    if (new_entries_matcher->matched(stderr_capture)) {
        StringUtil::Split((*new_entries_matcher)[1], ',', &splits);
        std::transform(splits.begin(), splits.end(), std::inserter(*new_entry_ids, new_entry_ids->begin()), string_to_unsigned);
    }

    LOG_INFO(stderr_capture);
}


std::string GenerateTempMergedZtsHarvesterConfig(const PipelineParams &params, const std::string &generated_config_file_path) {
    const std::string buffer_merged_conf_file_path(params.working_directory_ + "/ztz_merged_conf_buffer.conf");

    FileUtil::DeleteFile(buffer_merged_conf_file_path);
    FileUtil::CopyOrDie(params.harvester_config_file_, buffer_merged_conf_file_path);

    std::vector<std::string> importer_args{
        "--min-log-level=WARNING",
        "--mode=merge",
        TextUtil::UTF8ToLower(Zeder::FLAVOUR_TO_STRING_MAP.at(params.flavour_)),
        params.zeder_to_zoter_importer_config_file_,
        generated_config_file_path,
        buffer_merged_conf_file_path
    };

    ExecUtil::ExecOrDie(params.executable_directory_ + "/zeder_to_zotero_importer", importer_args);
    return buffer_merged_conf_file_path;
}


std::string ExecuteZtsHarvesterForValidation(const PipelineParams &params, const std::string &temp_harvester_merged_config_file_path,
                                             const Zeder::EntryCollection &downloaded_entries, const std::unordered_set<unsigned> &new_and_updated_entry_ids)
{
    const std::string buffer_validator_report_file_path(params.working_directory_ + "/ztz_validator_report_buffer.conf");

    FileUtil::DeleteFile(buffer_validator_report_file_path);
    const unsigned int timeout(15 * 3600);
    std::vector<std::string> validator_args{
        "--min-log-level=WARNING",
    };
    validator_args.emplace_back("--error-report-file=" + buffer_validator_report_file_path);
    validator_args.emplace_back(temp_harvester_merged_config_file_path);

    for (const auto &entry_id : new_and_updated_entry_ids)
        validator_args.emplace_back(downloaded_entries.find(entry_id)->getAttribute(params.journal_name_column_));

    std::string stdout_capture, stderr_capture;
    const auto ret_code(ExecUtil::Exec(params.executable_directory_ + "/zts_harvester", validator_args, "", "/tmp/tmp_stdout", "/tmp/tmp_stdout", timeout));
    if (ret_code == -1 and errno == ETIME)
        LOG_WARNING("ZTS Harvester Validation timed-out!");

    return buffer_validator_report_file_path;
}


struct ValidatorErrorHandlerParams {
    IniFile::iterator config_entry_;
    std::string url_;
    std::string error_message_;
};


void SplitFormatStringComponents(std::string format_string, std::string * const locale,
                                 std::unordered_set<std::string> * const merged)
{
    if (format_string[0] == '(') {
        const size_t closing_paren_pos(format_string.find(')', 1));
        if (unlikely(closing_paren_pos == std::string::npos or closing_paren_pos == 1))
            return;
        *locale = format_string.substr(1, closing_paren_pos - 1);
        format_string = format_string.substr(closing_paren_pos + 1);
    }

    StringUtil::SplitThenTrimWhite(format_string, '|', merged);
}


std::string MergeFormatStringComponents(const std::string &locale, const std::unordered_set<std::string> &format_string_splits) {
    std::string merged;
    if (not locale.empty())
        merged += "(" + locale + ")";

    for (const auto &format_string : format_string_splits)
        merged += format_string + "|";

    if (not merged.empty() and merged.back() =='|')
        merged = merged.substr(0, merged.length() - 1);

    return merged;
}


bool TryStringToStructTm(const std::string &date_string, const std::string &format_string) {
    try {
        TimeUtil::StringToStructTm(date_string, format_string);
        return true;
    } catch (...) {}

    return false;
}


bool SelectBestStrptimeFormatString(const ValidatorErrorHandlerParams &params,
                                    const std::unordered_set<std::string> &known_strptime_format_strings)
{
    const auto strptime_format_key(JournalConfig::ZoteroBundle::Key(JournalConfig::Zotero::STRPTIME_FORMAT));

    std::string current_format_string(params.config_entry_->getString(strptime_format_key));
    // preemptively check the date string with the current format string
    // we can skip further processing if a previously updated format string already works
    if (TryStringToStructTm(params.error_message_, current_format_string))
        return true;

    LOG_DEBUG("Selecting best strptime format string for '" + params.url_ + "'...");
    std::string current_format_string_locale;
    std::unordered_set<std::string> current_format_string_splits, attempted_format_strings;
    std::vector<std::string> test_format_strings;
    SplitFormatStringComponents(current_format_string, &current_format_string_locale, &current_format_string_splits);

    // parse the string using every known format and use the one the works, if any
    for (const auto &known_format_string : known_strptime_format_strings) {
        std::string known_format_string_locale;
        std::unordered_set<std::string> known_format_string_splits;
        SplitFormatStringComponents(known_format_string, &known_format_string_locale, &known_format_string_splits);

        for (const auto &individual_format_string : known_format_string_splits) {
            test_format_strings.clear();
            if (current_format_string_locale.empty() and known_format_string_locale.empty())
                test_format_strings.emplace_back(individual_format_string);
            if (not current_format_string_locale.empty())
                test_format_strings.emplace_back("(" + current_format_string_locale + ")" + individual_format_string);
            if (not known_format_string_locale.empty())
                test_format_strings.emplace_back("(" + known_format_string_locale + ")" + individual_format_string);

            for (const auto &test_format_string : test_format_strings) {
                if (attempted_format_strings.find(test_format_string) != attempted_format_strings.end())
                    continue;

                bool conversion_successful(TryStringToStructTm(params.error_message_, test_format_string));
                LOG_DEBUG("Format string '" + test_format_string + "': " + (conversion_successful ? "SUCCESS" : "FAILED"));

                if (conversion_successful) {
                    std::string working_locale;
                    std::unordered_set<std::string> working_format_string_splits;
                    SplitFormatStringComponents(test_format_string, &working_locale, &working_format_string_splits);

                    if (not current_format_string_locale.empty() and working_locale != current_format_string_locale) {
                        LOG_WARNING("Overriding locale of '" + params.config_entry_->getSectionName() + "' with '" +
                                    working_locale + "'");
                    }

                    current_format_string_splits.insert(*working_format_string_splits.begin());
                    params.config_entry_->replace(strptime_format_key, MergeFormatStringComponents(working_locale,
                                              working_format_string_splits));
                    return true;
                }

                attempted_format_strings.insert(test_format_string);
            }

            // the above combinations ought to account for the grand majority of the error cases
            // corner case: if a journal has date strings that use multiple locales, the above code will break
            // in such cases, it's advisable to maually resolve the issue
        }
    }

    return false;
}


bool EvaluateZtsHarvesterValidatorReport(const PipelineParams &params, const std::string &temp_harvester_merged_config_file_path,
                                         const std::string &temp_validator_report_file_path)
{
    if (not FileUtil::Exists(temp_validator_report_file_path)) {
        // the validator timed-out
        return false;
    }


    IniFile report(temp_validator_report_file_path);
    if (not report.getBool("", "has_errors")) {
        // skip the further processing if the last run was successful
        return true;
    }

    IniFile merged_config(temp_harvester_merged_config_file_path);

    std::unordered_set<std::string> validated_journal_names, updated_journal_names;
    StringUtil::Split(report.getString("", "journal_names"), '|', &validated_journal_names);
    if (validated_journal_names.empty()) {
        // true when all errors are unknown, which we can't automatically handle anyways
        LOG_WARNING("Validation was unsuccessful but no validated journals found in report!");
        return false;
    }

    // collect existing strptime format strings
    IniFile original_harvester_config(params.harvester_config_file_);
    std::unordered_set<std::string> known_strptime_format_strings;
    const auto strptime_format_key(JournalConfig::ZoteroBundle::Key(JournalConfig::Zotero::STRPTIME_FORMAT));
    for (const auto &section : original_harvester_config) {
        std::string format_string;
        if (section.lookup(strptime_format_key, &format_string) and not format_string.empty())
            known_strptime_format_strings.insert(format_string);
    }

    // non-static as we need to capture extra state at the time of the function call
    const std::unordered_map<std::string, std::function<bool(const ValidatorErrorHandlerParams &)>> default_error_type_handlers{
        // empty responses are rare and mostly benign
        { "ERROR-ZTS_EMPTY_RESPONSE",  [](const ValidatorErrorHandlerParams &) -> bool { return true; } },
        { "ERROR-BAD_STRPTIME_FORMAT", [&known_strptime_format_strings](const ValidatorErrorHandlerParams &validator_params) -> bool
                                            { return SelectBestStrptimeFormatString(validator_params, known_strptime_format_strings); } },

    };

    for (const auto &validated_journal_name : validated_journal_names) {
        LOG_DEBUG("Evaluating report for '" + validated_journal_name + "'...");
        for (const auto &entry : *report.getSection(validated_journal_name)) {
            const auto &url(entry.name_);
            const auto &error_type(entry.value_);
            const auto &error_message(report.getSection(error_type)->getString(url));

            ValidatorErrorHandlerParams handler_params{ merged_config.getSection(validated_journal_name), url, error_message };
            const auto default_handler(default_error_type_handlers.find(error_type));
            if (default_handler != default_error_type_handlers.end()) {
                if (default_handler->second(handler_params))
                    updated_journal_names.insert(validated_journal_name);
            }
        }
    }
    // unexpected errors will need to be resolved manually

    // save changes, if any
    merged_config.write(temp_harvester_merged_config_file_path);
    return false;
}


void SaveValidatorReportAndDiff(const PipelineParams &params, const std::string &temp_validator_report_file_path,
                                const std::string &temp_merged_config_file_path, const std::unordered_set<unsigned> &new_and_updated_entry_ids)
{
    char file_path_buffer[100]{};
    const auto time(TimeUtil::GetCurrentTimeGMT());
    ::strftime(file_path_buffer, sizeof(file_path_buffer), std::string(params.working_directory_ + "/%y%m%d_report").c_str(), &time);

    FileUtil::DeleteFile(file_path_buffer);
    FileUtil::Copy(temp_validator_report_file_path, file_path_buffer);

    ::strftime(file_path_buffer, sizeof(file_path_buffer), std::string(params.working_directory_ + "/%y%m%d_diff").c_str(), &time);
    IniFile merged_config(temp_merged_config_file_path), diff_config(file_path_buffer, true, true);

    for (const auto &section : merged_config) {
        const unsigned zeder_id(section.getUnsigned("zeder_id", 0));
        if (zeder_id == 0)
            continue;

        if (new_and_updated_entry_ids.find(zeder_id) != new_and_updated_entry_ids.end()) {
            diff_config.appendSection(section.getSectionName());
            auto diff_section(diff_config.getSection(section.getSectionName()));

            for (const auto &entry : section)
                diff_section->insert(entry.name_, entry.value_, entry.comment_);
        }
    }

    diff_config.write(file_path_buffer);
}


void AnnouncePhase(const std::string &message) {
    LOG_INFO("*** " + message + " ***");
}


class PipelineHandler {
public:
    PipelineHandler() {
        AnnouncePhase("Zeder to Zotero Pipeline BEGIN");
    }
    ~PipelineHandler() {
        AnnouncePhase("Zeder to Zotero Pipeline END");
    }
};


bool ProcessPipeline(const PipelineParams &pipeline_params) {
    Zeder::EntryCollection downloaded_entries;
    bool error(false);
    PipelineHandler handler;

    FileUtil::MakeDirectory(pipeline_params.working_directory_, true);

    AnnouncePhase("Downloading data from Zeder...");
    DownloadFullDump(pipeline_params, &downloaded_entries);

    AnnouncePhase("Filtering old entries...");
    RemoveEntriesOlderThanCutoff(pipeline_params, &downloaded_entries);

    if (downloaded_entries.empty()) {
        AnnouncePhase("SUCCESS! Current harvester config is up-to-date!");
        return true;
    }

    AnnouncePhase("Generating temporary harvester config...");
    const auto generated_config_file_path(GenerateConfigForNewAndUpdatedEntries(pipeline_params, downloaded_entries));
    std::unordered_set<unsigned> new_entry_ids, updated_entry_ids, new_and_updated_entry_ids;

    AnnouncePhase("Diff'ing against current harvester config...");
    DiffGeneratedConfigAgainstZtsHarvesterConfig(pipeline_params, generated_config_file_path, &new_entry_ids, &updated_entry_ids);
    new_and_updated_entry_ids.insert(new_entry_ids.begin(), new_entry_ids.end());
    new_and_updated_entry_ids.insert(updated_entry_ids.begin(), updated_entry_ids.end());
    if (new_and_updated_entry_ids.empty()) {
        AnnouncePhase("SUCCESS! Current harvester config is up-to-date!");
        return true;
    }

    const auto temp_merged_config_file_path(GenerateTempMergedZtsHarvesterConfig(pipeline_params, generated_config_file_path));

    AnnouncePhase("Validating new entries...");
    std::string validator_report_file_path;
    for (unsigned i(1); i <= pipeline_params.zts_harvester_validator_iterations_; ++i) {
        validator_report_file_path = ExecuteZtsHarvesterForValidation(pipeline_params, temp_merged_config_file_path,
                                                                      downloaded_entries, new_and_updated_entry_ids);

        if (EvaluateZtsHarvesterValidatorReport(pipeline_params, temp_merged_config_file_path, validator_report_file_path))
            error = true;

        LOG_INFO("Validation pass #" + std::to_string(i) + " = " + (error ? "PASSED" : "FAILED"));
        if (error)
            break;
    }

    if (error) {
        AnnouncePhase("SUCCESS! Updating current harvester config...");
        FileUtil::DeleteFile(pipeline_params.harvester_config_file_);
        FileUtil::CopyOrDie(temp_merged_config_file_path, pipeline_params.harvester_config_file_);
    } else {
        AnnouncePhase("FAILURE! Saving validator report and diff...");
        SaveValidatorReportAndDiff(pipeline_params, validator_report_file_path, temp_merged_config_file_path, new_and_updated_entry_ids);
    }

    return not error;
}


} // unnamed namespace


int Main(int argc, char *argv[]) {
    if (argc < 3)
        Usage();

    bool use_ubtools_folder(false);
    if (std::strcmp(argv[1], "--ubtools-wd") == 0) {
        use_ubtools_folder = true;
        --argc, ++argv;
    }

    if (argc < 3)
        Usage();

    IniFile config_file(argv[1]);

    Zeder::Flavour flavour;
    const auto flavour_string(argv[2]);
    if (std::strcmp(flavour_string, "ixtheo") == 0)
        flavour = Zeder::Flavour::IXTHEO;
    else if (std::strcmp(flavour_string, "krimdok") == 0)
        flavour = Zeder::Flavour::KRIMDOK;
    else
        Usage();

    PipelineParams pipeline_params(flavour, use_ubtools_folder, config_file);
    bool success(ProcessPipeline(pipeline_params));

    return success ? EXIT_SUCCESS : EXIT_FAILURE;
}

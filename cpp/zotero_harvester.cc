/** \brief Tool to automatically download metadata from online sources by leveraging Zotero
 *  \author Madeeswaran Kannan
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
#include <iostream>
#include <memory>
#include <unordered_map>
#include <cstdlib>
#include "FileUtil.h"
#include "IniFile.h"
#include "ZoteroHarvesterConfig.h"
#include "ZoteroHarvesterDownload.h"
#include "ZoteroHarvesterConversion.h"
#include "ZoteroHarvesterUtil.h"
#include "StringUtil.h"
#include "util.h"


namespace {


[[noreturn]] void Usage() {
    std::cerr << "Usage: " << ::progname << " [options] config_file_path selection_mode selection_args\n"
              << "\n"
              << "\tOptions:\n"
              << "\t[--min-log-level=log_level]         Possible log levels are ERROR, WARNING (default), INFO and DEBUG\n"
              << "\t[--force-downloads]                 All URLs are unconditionally downloaded\n"
              << "\t[--ignore-robots-dot-txt]           Ignore crawling parameters and restrictions specified in robots.txt files\n"
              << "\t[--output-directory=output_dir]     Generated files are saved to /tmp/zotero_harvester by default\n"
              << "\t[--output-filename=output_filename] Overrides the automatically-generated filename based on the current date/time. Output format is always MARC-XML\n"
              << "\n"
              << "\tSelection modes: UPLOAD, JOURNAL\n"
              << "\t\tUPLOAD - Only those journals that have the specified upload operation (either LIVE or TEST) set will be processed. When this parameter is not specified, tracking is automatically disabled.\n"
              << "\t\tJOURNAL - If no arguments are provided, all journals are processed. Otherwise, only those journals specified are processed.\n\n";
    std::exit(EXIT_FAILURE);
}


struct CommandLineArgs {
    enum SelectionMode { INVALID, UPLOAD, JOURNAL };

    bool force_downloads_;
    bool ignore_robots_dot_txt_;
    std::string output_directory_;
    std::string output_filename_;
    std::string config_path_;
    SelectionMode selection_mode_;
    std::set<std::string> selected_journals_;
    ZoteroHarvester::Config::UploadOperation selected_upload_operation_;
public:
    explicit CommandLineArgs() : force_downloads_(false), ignore_robots_dot_txt_(false),
        output_directory_("/tmp/zotero_harvester/"), selection_mode_(INVALID),
        selected_upload_operation_(ZoteroHarvester::Config::UploadOperation::NONE)
    {
        static const std::string TIME_FORMAT_STRING("%Y-%m-%d %T");

        char time_buffer[100]{};
        const auto current_time_gmt(TimeUtil::GetCurrentTimeGMT());
        std::strftime(time_buffer, sizeof(time_buffer), TIME_FORMAT_STRING.c_str(), &current_time_gmt);

        output_filename_ = "zotero_harvester_" + std::string(time_buffer) + ".xml";
    }
};


void ParseCommandLineArgs(int * const argc, char *** const argv, CommandLineArgs * const commandline_args) {


    while (StringUtil::StartsWith((*argv)[1], "--")) {
        if (std::strcmp((*argv)[1], "--force-downloads") == 0) {
            commandline_args->force_downloads_ = true;
            --*argc, ++*argv;
            continue;
        }

        if (std::strcmp((*argv)[1], "--ignore-robots-dot-txt") == 0) {
            commandline_args->ignore_robots_dot_txt_ = true;
            --*argc, ++*argv;
            continue;
        }

        const std::string OUTPUT_DIRECTORY_FLAG_PREFIX("--output-directory=");
        if (StringUtil::StartsWith((*argv)[1], OUTPUT_DIRECTORY_FLAG_PREFIX)) {
            commandline_args->output_directory_ = (*argv)[1] + OUTPUT_DIRECTORY_FLAG_PREFIX.length();
            --*argc, ++*argv;
            continue;
        }

        const std::string OUTPUT_FILENAME_FLAG_PREFIX("--output-filename=");
        if (StringUtil::StartsWith((*argv)[1], OUTPUT_FILENAME_FLAG_PREFIX)) {
            commandline_args->output_filename_ = (*argv)[1] + OUTPUT_FILENAME_FLAG_PREFIX.length();
            --*argc, ++*argv;
            continue;
        }

        Usage();
    }

    if (*argc < 3)
        Usage();

    commandline_args->config_path_ = (*argv)[1];
    --*argc, ++*argv;

    const std::string selection_mode((*argv)[1]);
    --*argc, ++*argv;

    if (::strcasecmp(selection_mode.c_str(), "UPLOAD") == 0)
        commandline_args->selection_mode_ = CommandLineArgs::SelectionMode::UPLOAD;
    else if (::strcasecmp(selection_mode.c_str(), "JOURNAL") == 0)
        commandline_args->selection_mode_ = CommandLineArgs::SelectionMode::JOURNAL;
    else
        Usage();

    for (int i(1); i < *argc; ++i) {
        const auto current_arg((*argv)[i]);

        switch (commandline_args->selection_mode_) {
        case CommandLineArgs::SelectionMode::UPLOAD:
        {
            auto upload_op(ZoteroHarvester::Config::STRING_TO_UPLOAD_OPERATION_MAP.find(current_arg));
            if (upload_op != ZoteroHarvester::Config::STRING_TO_UPLOAD_OPERATION_MAP.end())
                commandline_args->selected_upload_operation_ = static_cast<ZoteroHarvester::Config::UploadOperation>(upload_op->second);
            return; // intentional early return
        }
        case CommandLineArgs::SelectionMode::JOURNAL:
            commandline_args->selected_journals_.emplace_back(current_arg);
            break;
        default:
            LOG_ERROR("unknown selection mode");
        }
    }
}


struct HarvesterConfigData {
    std::unique_ptr<ZoteroHarvester::Config::GlobalParams> global_params_;
    std::vector<std::unique_ptr<ZoteroHarvester::Config::GroupParams>> group_params_;
    std::vector<std::unique_ptr<ZoteroHarvester::Config::JournalParams>> journal_params_;
    std::unique_ptr<ZoteroHarvester::Config::EnhancementMaps> enhancement_maps;
    std::map<std::string, const ZoteroHarvester::Config::GroupParams&> group_name_to_group_params_map_;

    inline const ZoteroHarvester::Config::GroupParams &
        lookupJournalGroup(const ZoteroHarvester::Config::JournalParams &journal_params) const
    {
        return group_name_to_group_params_map_.find(journal_params.group_)->second;
    }
};


void LoadHarvesterConfig(const std::string &config_path, HarvesterConfigData * const harvester_config) {
    const IniFile ini(config_path);

    harvester_config->global_params_.reset(new ZoteroHarvester::Config::GlobalParams(*ini.getSection("")));

    std::set<std::string> group_names;
    StringUtil::Split(harvester_config->global_params_->group_names_, ',', &group_names, /* suppress_empty_components = */ true);

    for (const auto &group_name : group_names) {
        const auto new_group(new ZoteroHarvester::Config::GroupParams(*ini.getSection(group_name)));
        harvester_config->group_params_.emplace_back(new_group);
        harvester_config->group_name_to_group_params_map_.emplace(group_name, *new_group);
    }

    for (const auto &section : ini) {
        if (section.getSectionName().empty())
            continue;
        else if (group_names.find(section.getSectionName()) != group_names.end())
            continue;

        harvester_config->journal_params_.emplace_back(new ZoteroHarvester::Config::JournalParams(section,
                                                       *harvester_config->global_params_));
    }

    harvester_config->enhancement_maps.reset(
        new ZoteroHarvester::Config::EnhancementMaps(harvester_config->global_params_->enhancement_maps_directory_));
}


const unsigned MAX_CONVERSION_TASKLETS(12);


struct JournalDataStore {

};


} // unnamed namespace


int Main(int argc, char *argv[]) {
    if (argc < 2)
        Usage();

    CommandLineArgs commandline_args;
    ParseCommandLineArgs(&argc, &argv, &commandline_args);

    HarvesterConfigData harvester_config;
    LoadHarvesterConfig(commandline_args.config_path_, &harvester_config);



    for (const auto &journal : harvester_config.journal_params_) {
        bool skip_journal(false);

        if (commandline_args.selection_mode_ == CommandLineArgs::SelectionMode::UPLOAD
            and commandline_args.selected_upload_operation_ != ZoteroHarvester::Config::UploadOperation::NONE
            and journal->upload_operation_ != commandline_args.selected_upload_operation_)
        {
            skip_journal = true;
        } else if (commandline_args.selection_mode_ == CommandLineArgs::SelectionMode::JOURNAL
                   and not commandline_args.selected_journals_.empty()
                   and commandline_args.selected_journals_.find(journal->name_) != commandline_args.selected_journals_.end())
        {
            skip_journal = true;
        }

        if (skip_journal)
            continue;

        const auto &group_params(harvester_config.lookupJournalGroup(*journal.get()));

    }


    return EXIT_SUCCESS;
}

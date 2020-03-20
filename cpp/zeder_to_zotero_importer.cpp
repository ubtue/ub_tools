/** \brief Utility to automatically update the Zotero Harvester configuration from Zeder.
 *  \author Madeeswaran Kannan
 *
 *  \copyright 2020 Universitätsbibliothek Tübingen.  All rights reserved.
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
#include <unordered_set>
#include <cstdio>
#include <cstdlib>
#include "FileUtil.h"
#include "IniFile.h"
#include "util.h"
#include "ZoteroHarvesterConfig.h"
#include "ZoteroHarvesterUtil.h"
#include "ZoteroHarvesterZederInterop.h"


namespace {


using namespace ZoteroHarvester;


[[noreturn]] void Usage() {
    std::cerr << "Usage: " << ::progname << " [options] config_file_path mode zeder_flavour zeder_ids fields_to_update\n"
              << "\n"
              << "\tOptions:\n"
              << "\t[--min-log-level=log_level]     Possible log levels are ERROR, WARNING (default), INFO and DEBUG\n"
              << "\t[--overwrite-on-import]         Overwrite existing entries/sections when performing an import\n"
              << "\n"
              << "\tconfig_file_path                Path to the Zotero Harvester config file\n"
              << "\tmode                            Either IMPORT or UPDATE\n"
              << "\tzeder_flavour                   Either IXTHEO or KRIMDOK\n"
              << "\tzeder_ids                       Comma-separated list of Zeder entry IDs to import/update.\n"
              << "\t                                Special-case for updating: Use '*' to update all entries found in the config that belong to the Zeder flavour\n"
              << "\fields_to_update                 Comma-separated list of the following fields to update: \n"
              << "\t                                \tONLINE_PPN, PRINT_PPN, ONLINE_ISSN, PRINT_ISSN, EXPECTED_LANGUAGES, ENTRY_POINT_URL, UPLOAD_OPERATION, UPDATE_WINDOW.\n"
              << "\t                                Ignored when importing entries (all importable fields will be imported).\n\n";
    std::exit(EXIT_FAILURE);
}


struct CommandLineArgs {
    enum Mode { INVALID, IMPORT, UPDATE };

    bool overwrite_on_import_;
    std::string config_path_;
    Mode mode_;
    Zeder::Flavour zeder_flavour_;
    std::set<unsigned> zeder_ids_;
    std::set<Config::JournalParams::IniKey> fields_to_update_;
public:
    explicit CommandLineArgs() : overwrite_on_import_(false), mode_(INVALID) {}
};


void ParseCommandLineArgs(int * const argc, char *** const argv, CommandLineArgs * const commandline_args) {
    while (StringUtil::StartsWith((*argv)[1], "--")) {
        if (std::strcmp((*argv)[1], "--overwrite-on-import") == 0) {
            commandline_args->overwrite_on_import_ = true;
            --*argc, ++*argv;
            continue;
        }
    }

    if (*argc < 5)
        Usage();

    commandline_args->config_path_ = (*argv)[1];
    --*argc, ++*argv;

    const std::string mode((*argv)[1]);
    --*argc, ++*argv;

    if (::strcasecmp(mode.c_str(), "IMPORT") == 0)
        commandline_args->mode_ = CommandLineArgs::Mode::IMPORT;
    else if (::strcasecmp(mode.c_str(), "UPDATE") == 0)
        commandline_args->mode_ = CommandLineArgs::Mode::UPDATE;
    else
        Usage();

    const std::string zeder_flavour((*argv)[1]);
    --*argc, ++*argv;

    commandline_args->zeder_flavour_ = Zeder::ParseFlavour(zeder_flavour);

    const std::string zeder_id_list((*argv)[1]);
    --*argc, ++*argv;

    std::set<std::string> buffer;
    if (zeder_id_list != "*") {
        StringUtil::SplitThenTrimWhite(zeder_id_list, ',', &buffer);
        for (const auto &id_str : buffer)
            commandline_args->zeder_ids_.emplace(StringUtil::ToUnsigned(id_str));
    } else if (commandline_args->mode_ == CommandLineArgs::Mode::IMPORT)
        LOG_ERROR("cannot import all Zeder entries at once");

    if (commandline_args->mode_ == CommandLineArgs::Mode::IMPORT)
        return;

    static const std::set<Config::JournalParams::IniKey> ALLOWED_INI_KEYS {
        Config::JournalParams::ENTRY_POINT_URL,
        Config::JournalParams::UPLOAD_OPERATION,
        Config::JournalParams::ONLINE_PPN,
        Config::JournalParams::PRINT_PPN,
        Config::JournalParams::ONLINE_ISSN,
        Config::JournalParams::PRINT_ISSN,
        Config::JournalParams::UPDATE_WINDOW,
        Config::JournalParams::EXPECTED_LANGUAGES,
    };

    const std::string update_fields_list((*argv)[1]);
    --*argc, ++*argv;

    buffer.clear();
    StringUtil::SplitThenTrimWhite(update_fields_list, ',', &buffer);
    for (const auto &update_field_str : buffer) {
        const auto ini_key(Config::JournalParams::GetIniKey(update_field_str));
        if (ALLOWED_INI_KEYS.find(ini_key) == ALLOWED_INI_KEYS.end())
            LOG_ERROR("update field '" + update_field_str + "' is invalid");

        commandline_args->fields_to_update_.emplace(ini_key);
    }

    if (commandline_args->fields_to_update_.empty())
        LOG_ERROR("no fields were provided to be updated");
}


void DownloadZederEntries(const Zeder::Flavour flavour, const std::unordered_set<unsigned> &entries_to_download,
                          Zeder::EntryCollection * const downloaded_entries)
{
    const auto endpoint_url(Zeder::GetFullDumpEndpointPath(flavour));
    const std::unordered_set<std::string> columns_to_download {};  // intentionally empty
    const std::unordered_map<std::string, std::string> filter_regexps {}; // intentionally empty
    std::unique_ptr<Zeder::FullDumpDownloader::Params> downloader_params(new Zeder::FullDumpDownloader::Params(endpoint_url,
                                                                         entries_to_download, columns_to_download, filter_regexps));

    auto downloader(Zeder::FullDumpDownloader::Factory(Zeder::FullDumpDownloader::Type::FULL_DUMP, std::move(downloader_params)));
    if (not downloader->download(downloaded_entries))
        LOG_ERROR("couldn't download full dump for " + Zeder::FLAVOUR_TO_STRING_MAP.at(flavour));
}


struct HarvesterConfig {
    std::unique_ptr<IniFile> config_file_;
    std::unique_ptr<Config::GlobalParams> global_params_;
    std::vector<std::unique_ptr<Config::GroupParams>> group_params_;
    std::vector<std::unique_ptr<Config::JournalParams>> journal_params_;
public:
    inline HarvesterConfig(const std::string &config_file_path) {
        Config::LoadHarvesterConfigFile(config_file_path, &global_params_, &group_params_, &journal_params_, &config_file_);
    }

    IniFile::Section *lookupConfig(const unsigned zeder_id, const Zeder::Flavour zeder_flavour) const;
    Config::JournalParams *lookupJournalParams(const unsigned zeder_id, const Zeder::Flavour zeder_flavour) const;
    IniFile::Section *addNewConfigSection(const std::string &section_name);
};


IniFile::Section *HarvesterConfig::lookupConfig(const unsigned zeder_id, const Zeder::Flavour zeder_flavour) const {
    const auto journal_param(lookupJournalParams(zeder_id, zeder_flavour));
    if (journal_param != nullptr)
        return &*config_file_->getSection(journal_param->name_);

    return nullptr;
}


Config::JournalParams *HarvesterConfig::lookupJournalParams(const unsigned zeder_id, const Zeder::Flavour zeder_flavour) const {
    for (const auto &journal_param : journal_params_) {
        if (journal_param->zeder_id_ == zeder_id and ZederInterop::GetZederInstanceForJournal(*journal_param) == zeder_flavour)
            return journal_param.get();
    }

    return nullptr;
}


IniFile::Section *HarvesterConfig::addNewConfigSection(const std::string &section_name) {
    if (config_file_->sectionIsDefined(section_name))
        LOG_ERROR("INI section '" + section_name + "' already exists");

    config_file_->appendSection(section_name);
    return &*config_file_->getSection(section_name);
}


std::vector<std::reference_wrapper<Config::JournalParams>> FetchJournalParamsForZederFlavour(const Zeder::Flavour zeder_flavour,
                                                                                             const HarvesterConfig &harvester_config)
{
    std::vector<std::reference_wrapper<Config::JournalParams>> journal_params;

    for (const auto &journal_param : harvester_config.journal_params_) {
        if (ZederInterop::GetZederInstanceForJournal(*journal_param) == zeder_flavour)
            journal_params.emplace_back(std::cref(*journal_param));
    }

    return journal_params;
}


void DetermineZederEntriesToBeDownloaded(const CommandLineArgs &commandline_args,
                                         const std::vector<std::reference_wrapper<Config::JournalParams>> &existing_journal_params,
                                         std::unordered_set<unsigned> * const entries_to_download)
{
    switch (commandline_args.mode_) {
    case CommandLineArgs::Mode::IMPORT:
        for (const auto id : commandline_args.zeder_ids_)
            entries_to_download->emplace(id);

        break;
    case CommandLineArgs::Mode::UPDATE:
        if (commandline_args.zeder_ids_.empty()) {
            // update all existing journals in the config
            for (const auto &journal_param : existing_journal_params)
               entries_to_download->emplace(journal_param.get().zeder_id_);
        }

        break;
    default:
        break;
    }

    if (entries_to_download->empty())
        LOG_ERROR("no entries to import/update");
}


void WriteIniEntry(IniFile::Section * const section, const std::string &name, const std::string &value) {
    // merge existing comments
    const auto existing_entry(section->find(name));
    const auto existing_entry_comment(existing_entry != section->end() ? existing_entry->comment_ : "");
    section->insert(name, value, existing_entry_comment, IniFile::Section::DupeInsertionBehaviour::OVERWRITE_EXISTING_VALUE);
}


unsigned ImportZederEntries(const Zeder::EntryCollection &zeder_entries, HarvesterConfig * const harvester_config,
                            const Zeder::Flavour zeder_flavour, const bool overwrite)
{
    unsigned num_entries_imported(0);
    for (const auto &zeder_entry : zeder_entries) {
        const auto zeder_id(StringUtil::ToUnsigned(ZederInterop::GetJournalParamsIniValueFromZederEntry(zeder_entry, zeder_flavour,
                            Config::JournalParams::IniKey::ZEDER_ID)));
        const auto title(ZederInterop::GetJournalParamsIniValueFromZederEntry(zeder_entry, zeder_flavour,
                         Config::JournalParams::IniKey::NAME));

        auto existing_journal_section(harvester_config->lookupConfig(zeder_id, zeder_flavour));
        if (existing_journal_section != nullptr and not overwrite) {
            LOG_WARNING("couldn't import Zeder entry " + std::to_string(zeder_id) + " (" title + "): already exists");
            continue;
        }

        if (existing_journal_section == nullptr)
            existing_journal_section = harvester_config->addNewConfigSection(title);

        const std::vector<Config::JournalParams::IniKey> ini_keys_to_import {
            Config::JournalParams::GROUP,
            Config::JournalParams::ENTRY_POINT_URL,
            Config::JournalParams::HARVESTER_OPERATION,
            Config::JournalParams::UPLOAD_OPERATION,
            Config::JournalParams::ONLINE_PPN,
            Config::JournalParams::PRINT_PPN,
            Config::JournalParams::ONLINE_ISSN,
            Config::JournalParams::PRINT_ISSN,
            Config::JournalParams::UPDATE_WINDOW,
            Config::JournalParams::EXPECTED_LANGUAGES
        };

        // spcial-case the Zeder ID and modified timestamp fields
        WriteIniEntry(existing_journal_section, Config::JournalParams::GetIniKeyString(Config::JournalParams::ZEDER_ID),
                      std::to_string(zeder_entry.getId()));
        char time_buffer[100]{};
        std::strftime(time_buffer, sizeof(time_buffer), Zeder::MODIFIED_TIMESTAMP_FORMAT_STRING,
                      &zeder_entry.getLastModifiedTimestamp());
        WriteIniEntry(existing_journal_section, Config::JournalParams::GetIniKeyString(Config::JournalParams::ZEDER_MODIFIED_TIME),
                      std::to_string(zeder_entry.getId()));

        // write out the rest
        for (const auto ini_key_to_import : ini_keys_to_import) {
            const auto ini_key_str(Config::JournalParams::GetIniKeyString(ini_key_to_import));
            const auto ini_val_str(ZederInterop::GetJournalParamsIniValueFromZederEntry(zeder_entry, zeder_flavour,
                                   ini_key_to_import));

            if (not ini_val_str.empty())
                WriteIniEntry(existing_journal_section, ini_key_str, ini_val_str);
        }

        LOG_INFO("imported => " + zeder_entry.prettyPrint());
        ++num_entries_imported;
    }

    return num_entries_imported;
}


unsigned UpdateZederEntries(const Zeder::EntryCollection &zeder_entries, HarvesterConfig * const harvester_config,
                            const std::set<Config::JournalParams::IniKey> &fields_to_update, const Zeder::Flavour zeder_flavour)
{
    unsigned num_entries_updated(0);
    for (const auto &zeder_entry : zeder_entries) {
        const auto zeder_id(StringUtil::ToUnsigned(ZederInterop::GetJournalParamsIniValueFromZederEntry(zeder_entry, zeder_flavour,
                            Config::JournalParams::IniKey::ZEDER_ID)));
        const auto title(ZederInterop::GetJournalParamsIniValueFromZederEntry(zeder_entry, zeder_flavour,
                         Config::JournalParams::IniKey::NAME));

        auto existing_journal_section(harvester_config->lookupConfig(zeder_id, zeder_flavour));
        if (existing_journal_section == nullptr) {
            LOG_WARNING("couldn't update Zeder entry " + std::to_string(zeder_id) + " (" title + "): must be imported first");
            continue;
        }


        LOG_DEBUG("updating Zeder entry " + std::to_string(zeder_id) + " (" title + ")...");
        for (const auto field_to_update : fields_to_update) {
            const auto ini_key_str(Config::JournalParams::GetIniKeyString(field_to_update));
            const auto ini_old_val_str(existing_journal_section->getString(ini_key_str, ""));
            const auto ini_new_val_str(ZederInterop::GetJournalParamsIniValueFromZederEntry(zeder_entry, zeder_flavour,
                                       field_to_update));
            if (not ini_new_val_str.empty()) {
                WriteIniEntry(existing_journal_section, ini_key_str, ini_new_val_str);
                LOG_INFO("\t" + ini_key_str + ": '" + ini_old_val_str + "' => '" + ini_new_val_str + "'");
            } else
                LOG_WARNING("\tinvalid new value for field '" + ini_key_str + "'. old value: " + ini_old_val_str);
        }

        ++num_entries_updated;
    }

    return num_entries_updated;
}


} // unnamed namespace


int Main(int argc, char *argv[]) {
    if (argc < 2)
        Usage();

    CommandLineArgs commandline_args;
    ParseCommandLineArgs(&argc, &argv, &commandline_args);

    HarvesterConfig harvester_config(commandline_args.config_path_);
    const auto existing_journal_params(FetchJournalParamsForZederFlavour(commandline_args.zeder_flavour_, harvester_config));

    Zeder::EntryCollection downloaded_entries;
    std::unordered_set<unsigned> entries_to_download;

    DetermineZederEntriesToBeDownloaded(commandline_args, existing_journal_params, &entries_to_download);
    DownloadZederEntries(commandline_args.zeder_flavour_, entries_to_download, &downloaded_entries);

    switch (commandline_args.mode_) {
    case CommandLineArgs::Mode::IMPORT: {
        const auto num_imported(ImportZederEntries(downloaded_entries, &harvester_config,
                                commandline_args.zeder_flavour_, commandline_args.overwrite_on_import_));
        LOG_INFO("Imported " + std::to_string(num_imported) + " Zeder entries");
        break;
    }
    case CommandLineArgs::Mode::UPDATE: {
        const auto num_updated(UpdateZederEntries(downloaded_entries, &harvester_config,
                               commandline_args.fields_to_update_, commandline_args.zeder_flavour_));
        LOG_INFO("Updated " + std::to_string(num_updated) + " Zeder entries");
        break;
    }
    default:
        break;
    }

    return EXIT_SUCCESS;
}

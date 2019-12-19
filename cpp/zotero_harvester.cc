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


using namespace ZoteroHarvester;


[[noreturn]] void Usage() {
    std::cerr << "Usage: " << ::progname << " [options] config_file_path selection_mode selection_args\n"
              << "\n"
              << "\tOptions:\n"
              << "\t[--min-log-level=log_level]         Possible log levels are ERROR, WARNING (default), INFO and DEBUG\n"
              << "\t[--force-downloads]                 All URLs are unconditionally downloaded and converted.\n"
              << "\t[--ignore-robots-dot-txt]           Ignore crawling/rate-limiting parameters specified in robots.txt files and disable download restrictions globally\n"
              << "\t[--output-directory=output_dir]     Generated files are saved to /tmp/zotero_harvester by default\n"
              << "\t[--output-filename=output_filename] Overrides the automatically-generated filename based on the current date/time. Output format is always MARC-XML\n"
              << "\n"
              << "\tSelection modes: UPLOAD, JOURNAL\n"
              << "\t\tUPLOAD - Only those journals that have the specified upload operation (either LIVE or TEST) set will be processed.\n"
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
    Config::UploadOperation selected_upload_operation_;
public:
    explicit CommandLineArgs();
};


CommandLineArgs::CommandLineArgs()
 : force_downloads_(false), ignore_robots_dot_txt_(false), output_directory_("/tmp/zotero_harvester/"),
   selection_mode_(INVALID), selected_upload_operation_(Config::UploadOperation::NONE)
{
    const std::string TIME_FORMAT_STRING("%Y-%m-%d %T");

    char time_buffer[100]{};
    const auto current_time_gmt(TimeUtil::GetCurrentTimeGMT());
    std::strftime(time_buffer, sizeof(time_buffer), TIME_FORMAT_STRING.c_str(), &current_time_gmt);

    output_filename_ = "zotero_harvester_" + std::string(time_buffer) + ".xml";
}


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
            auto upload_op(Config::STRING_TO_UPLOAD_OPERATION_MAP.find(current_arg));
            if (upload_op != Config::STRING_TO_UPLOAD_OPERATION_MAP.end())
                commandline_args->selected_upload_operation_ = static_cast<Config::UploadOperation>(upload_op->second);
            return; // intentional early return
        }
        case CommandLineArgs::SelectionMode::JOURNAL:
            commandline_args->selected_journals_.emplace(current_arg);
            break;
        default:
            LOG_ERROR("unknown selection mode");
        }
    }
}


struct HarvesterConfigData {
    std::unique_ptr<Config::GlobalParams> global_params_;
    std::vector<std::unique_ptr<Config::GroupParams>> group_params_;
    std::vector<std::unique_ptr<Config::JournalParams>> journal_params_;
    std::unique_ptr<Config::EnhancementMaps> enhancement_maps;
    std::map<std::string, const std::reference_wrapper<Config::GroupParams>> group_name_to_group_params_map_;

    inline const Config::GroupParams & lookupJournalGroup(const Config::JournalParams &journal_params) const {
        return group_name_to_group_params_map_.find(journal_params.group_)->second;
    }
};


void LoadHarvesterConfig(const std::string &config_path, HarvesterConfigData * const harvester_config) {
    const IniFile ini(config_path);

    harvester_config->global_params_.reset(new Config::GlobalParams(*ini.getSection("")));

    std::set<std::string> group_names;
    StringUtil::Split(harvester_config->global_params_->group_names_, ',', &group_names, /* suppress_empty_components = */ true);

    for (const auto &group_name : group_names) {
        const auto new_group(new Config::GroupParams(*ini.getSection(group_name)));
        harvester_config->group_params_.emplace_back(new_group);
        harvester_config->group_name_to_group_params_map_.emplace(group_name, *new_group);
    }

    for (const auto &section : ini) {
        if (section.getSectionName().empty())
            continue;
        else if (group_names.find(section.getSectionName()) != group_names.end())
            continue;

        harvester_config->journal_params_.emplace_back(new Config::JournalParams(section,
                                                       *harvester_config->global_params_));
    }

    harvester_config->enhancement_maps.reset(
        new Config::EnhancementMaps(harvester_config->global_params_->enhancement_maps_directory_));
}


// Represents active and queued operations of a specific journal.
struct JournalDatastore {
    const Config::JournalParams &journal_params_;
    std::deque<std::unique_ptr<Util::Future<Download::DirectDownload::Params, Download::DirectDownload::Result>>> queued_downloads_;
    std::unique_ptr<Util::Future<Download::Crawling::Params, Download::Crawling::Result>> current_crawl_;
    std::unique_ptr<Util::Future<Download::RSS::Params, Download::RSS::Result>> current_rss_feed_;
    std::deque<std::unique_ptr<Util::Future<Conversion::ConversionParams, Conversion::ConversionResult>>> queued_marc_records_;
public:
    JournalDatastore(const Config::JournalParams &journal_params) : journal_params_(journal_params) {}
};


struct Metrics {
    unsigned num_journals_with_harvest_operation_direct_;
    unsigned num_journals_with_harvest_operation_rss_;
    unsigned num_journals_with_harvest_operation_crawl_;
    unsigned num_downloads_crawled_successful_;
    unsigned num_downloads_crawled_unsuccessful_;
    unsigned num_downloads_harvested_successful_;
    unsigned num_downloads_harvested_unsuccessful_;
    unsigned num_downloads_skipped_since_already_delivered_;
    unsigned num_marc_conversions_successful_;
    unsigned num_marc_conversions_skipped_since_online_first_;
    unsigned num_marc_conversions_skipped_since_early_view_;
    unsigned num_marc_conversions_skipped_since_exclusion_filters_;
    unsigned num_marc_conversions_skipped_since_already_delivered_;
    std::unordered_map<std::string, unsigned> group_name_to_num_generated_marc_records_map_;
public:
    explicit Metrics();
public:
    std::string toString() const;
};


Metrics::Metrics()
 : num_journals_with_harvest_operation_direct_(0), num_journals_with_harvest_operation_rss_(0),
   num_journals_with_harvest_operation_crawl_(0), num_downloads_crawled_successful_(0), num_downloads_crawled_unsuccessful_(0),
   num_downloads_harvested_successful_(0), num_downloads_harvested_unsuccessful_(0),
   num_downloads_skipped_since_already_delivered_(0), num_marc_conversions_successful_(0),
   num_marc_conversions_skipped_since_online_first_(0), num_marc_conversions_skipped_since_early_view_(0),
   num_marc_conversions_skipped_since_exclusion_filters_(0), num_marc_conversions_skipped_since_already_delivered_(0) {}


std::string Metrics::toString() const {
    std::string out("\n\n\nZotero Harvester Metrics:\n");

    out += "\tJournals: " + std::to_string(num_journals_with_harvest_operation_direct_
                                           + num_journals_with_harvest_operation_rss_
                                           + num_journals_with_harvest_operation_crawl_) + "\n";
    out += "\t\tDirect: " + std::to_string(num_journals_with_harvest_operation_direct_) + "\n";
    out += "\t\tRSS: " + std::to_string(num_journals_with_harvest_operation_rss_) + "\n";
    out += "\t\tCrawl: " + std::to_string(num_journals_with_harvest_operation_crawl_) + "\n";

    out += "\tCrawls: " + std::to_string(num_downloads_crawled_successful_ + num_downloads_crawled_unsuccessful_) + "\n";
    out += "\t\tSuccessful: " + std::to_string(num_downloads_crawled_successful_) + "\n";
    out += "\t\tUnsuccessful : " + std::to_string(num_downloads_crawled_unsuccessful_) + "\n";

    out += "\tHarvests: " + std::to_string(num_downloads_harvested_successful_ + num_downloads_harvested_unsuccessful_
                                           + num_downloads_skipped_since_already_delivered_) + "\n";
    out += "\t\tSuccessful: " + std::to_string(num_downloads_harvested_successful_) + "\n";
    out += "\t\tUnsuccessful : " + std::to_string(num_downloads_harvested_unsuccessful_) + "\n";
    out += "\t\tSkipped (already delivered): " + std::to_string(num_downloads_skipped_since_already_delivered_) + "\n";

    out += "\tRecords: " + std::to_string(num_marc_conversions_successful_ + num_marc_conversions_skipped_since_online_first_
                                          + num_marc_conversions_skipped_since_early_view_
                                          + num_marc_conversions_skipped_since_exclusion_filters_
                                          + num_marc_conversions_skipped_since_already_delivered_) + "\n";
    out += "\t\tSuccessful: " + std::to_string(num_marc_conversions_successful_) + "\n";
    out += "\t\tSkipped (online-first): " + std::to_string(num_marc_conversions_skipped_since_online_first_) + "\n";
    out += "\t\tSkipped (early-view): " + std::to_string(num_marc_conversions_skipped_since_early_view_) + "\n";
    out += "\t\tSkipped (exclusion filter): " + std::to_string(num_marc_conversions_skipped_since_exclusion_filters_) + "\n";
    out += "\t\tSkipped (already delivered): " + std::to_string(num_marc_conversions_skipped_since_already_delivered_) + "\n";

    if (not group_name_to_num_generated_marc_records_map_.empty()) {
        out += "\n\tSuccessfully generated records per group:\n";
        for (const auto &entry : group_name_to_num_generated_marc_records_map_)
            out += "\t\t" + entry.first + ": " + std::to_string(entry.second) + "\n";
    }

    return out;
}


std::unique_ptr<JournalDatastore> QueueDownloadsForJournal(const Config::JournalParams &journal_params,
                                                           const HarvesterConfigData &harvester_config,
                                                           Util::HarvestableItemManager * const harvestable_manager,
                                                           Download::DownloadManager * const download_manager,
                                                           Metrics * const metrics)
{
    const auto &group_params(harvester_config.lookupJournalGroup(journal_params));
    std::unique_ptr<JournalDatastore> current_journal_datastore(new JournalDatastore(journal_params));
    const auto download_item(harvestable_manager->newHarvestableItem(journal_params.entry_point_url_, journal_params));

    switch (journal_params.harvester_operation_) {
    case Config::HarvesterOperation::DIRECT:
    {
        auto future(download_manager->directDownload(download_item, group_params.user_agent_,
                                                     Download::DirectDownload::Operation::USE_TRANSLATION_SERVER));
        current_journal_datastore->queued_downloads_.emplace_back(future.release());
        ++metrics->num_journals_with_harvest_operation_direct_;
        break;
    }
    case Config::HarvesterOperation::RSS:
    {
        auto future(download_manager->rss(download_item, group_params.user_agent_));
        current_journal_datastore->current_rss_feed_.reset(future.release());
        ++metrics->num_journals_with_harvest_operation_rss_;
        break;
    }
    case Config::HarvesterOperation::CRAWL:
    {
        auto future(download_manager->crawl(download_item, group_params.user_agent_));
        current_journal_datastore->current_crawl_.reset(future.release());
        ++metrics->num_journals_with_harvest_operation_crawl_;
        break;
    }
    }

    LOG_INFO("Queued journal '" + journal_params.name_ + "' | " + Config::HARVESTER_OPERATION_TO_STRING_MAP.at(journal_params.harvester_operation_) + " @ " + journal_params.entry_point_url_);
    return current_journal_datastore;
}


void EnqueueCrawlAndRssResults(JournalDatastore * const journal_datastore, bool * const jobs_in_progress, Metrics * const metrics) {
    if (journal_datastore->current_crawl_ != nullptr) {
        if (journal_datastore->current_crawl_->isComplete()) {
            for (auto &result : journal_datastore->current_crawl_->getResult().downloaded_items_)
                journal_datastore->queued_downloads_.emplace_back(result.release());

            metrics->num_downloads_crawled_successful_ += journal_datastore->current_crawl_->getResult().num_crawled_successful_;
            metrics->num_downloads_crawled_unsuccessful_ += journal_datastore->current_crawl_->getResult().num_crawled_unsuccessful_;

            journal_datastore->current_crawl_.reset();
        } else
            *jobs_in_progress = true;
    }

    if (journal_datastore->current_rss_feed_ != nullptr) {
        if (journal_datastore->current_rss_feed_->isComplete()) {
            for (auto &result : journal_datastore->current_rss_feed_->getResult().downloaded_items_)
                journal_datastore->queued_downloads_.emplace_back(result.release());

            journal_datastore->current_rss_feed_.reset();
        } else
            *jobs_in_progress = true;
    }
}


void EnqueueCompletedDownloadsForConversion(JournalDatastore * const journal_datastore, bool * const jobs_in_progress,
                                            Conversion::ConversionManager * const conversion_manager,
                                            const HarvesterConfigData &harvester_config,
                                            const std::unordered_set<std::string> &urls_harvested_during_current_session,
                                            Metrics * const metrics)
{
    for (auto iter(journal_datastore->queued_downloads_.begin()); iter != journal_datastore->queued_downloads_.end();) {
        if ((*iter)->isComplete()) {
            const auto &download_result((*iter)->getResult());

            if (urls_harvested_during_current_session.find(download_result.source_.url_.toString())
                != urls_harvested_during_current_session.end())
            {
                LOG_INFO("Item " + download_result.source_.toString() + " already harvested during this session");
            } else if (download_result.itemAlreadyDelivered()) {
                LOG_INFO("Item " + download_result.source_.toString() + " already delivered");
                ++metrics->num_downloads_skipped_since_already_delivered_;
            } else if (not download_result.downloadSuccessful()) {
                LOG_INFO("Item " + download_result.source_.toString() + " download failed! error: "
                         + download_result.error_message_ + " (response code = " + std::to_string(download_result.response_code_) + ")");
                ++metrics->num_downloads_harvested_unsuccessful_;
            } else {
                auto conversion_result(conversion_manager->convert(download_result.source_,
                                       download_result.response_body_,
                                       harvester_config.lookupJournalGroup(download_result.source_.journal_)));
                journal_datastore->queued_marc_records_.emplace_back(std::move(conversion_result));
                ++metrics->num_downloads_harvested_successful_;
            }

            iter = journal_datastore->queued_downloads_.erase(iter);
            continue;
        } else
            *jobs_in_progress = true;

        ++iter;
    }
}


bool ConversionResultsComparator(const std::unique_ptr<Util::Future<Conversion::ConversionParams, Conversion::ConversionResult>> &a,
                                 const std::unique_ptr<Util::Future<Conversion::ConversionParams, Conversion::ConversionResult>> &b)
{
    return a->getParameter().download_item_.id_ < b->getParameter().download_item_.id_;
}


// Tracks each group's MARC writer. Writers are instantiated on-demand.
class OutputFileCache {
    std::string output_filename_;
    std::string output_directory_;
    std::map<const Config::GroupParams *, std::unique_ptr<MARC::Writer>> output_marc_writers_;
public:
    OutputFileCache(const CommandLineArgs &commandline_args, const HarvesterConfigData &harvester_config);
public:
    const std::unique_ptr<MARC::Writer> &getWriter(const Config::GroupParams &group_params);
};


OutputFileCache::OutputFileCache(const CommandLineArgs &commandline_args, const HarvesterConfigData &harvester_config)
 : output_filename_(commandline_args.output_filename_), output_directory_(commandline_args.output_directory_)
{
    for (const auto &group_param : harvester_config.group_params_)
        output_marc_writers_.emplace(group_param.get(), nullptr);
}


const std::unique_ptr<MARC::Writer> &OutputFileCache::getWriter(const Config::GroupParams &group_params) {
    auto match(output_marc_writers_.find(&group_params));
    if (match == output_marc_writers_.end())
        LOG_ERROR("couldn't find output file writer for unknown group '" + group_params.name_ + "'");

    if (match->second != nullptr)
        return match->second;

    const auto output_file_directory(output_directory_ + "/" + group_params.bsz_upload_group_ + "/");
    FileUtil::MakeDirectory(output_file_directory, true);

    match->second.reset(MARC::Writer::Factory(output_file_directory + output_filename_).release());
    return match->second;
}


void WriteConversionResultsToDisk(JournalDatastore * const journal_datastore, OutputFileCache * const outputfile_cache,
                                  const CommandLineArgs &commandline_args, const Util::UploadTracker &upload_tracker,
                                  std::unordered_set<std::string> * const urls_harvested_during_current_session,
                                  Metrics * const metrics)
{
    // Sort the conversion results in the order in which they were queued.
    std::sort(journal_datastore->queued_marc_records_.begin(), journal_datastore->queued_marc_records_.end(),
              ConversionResultsComparator);

    // Iterate through the conversion results and write out consecutive successfully converted MARC records to disk.
    unsigned previous_converted_item_id(0);
    while (not journal_datastore->queued_marc_records_.empty()) {
        auto &current_conversion(journal_datastore->queued_marc_records_.front());
        const auto &current_download_item(current_conversion->getParameter().download_item_);
        const auto current_converted_item_id(current_download_item.id_);

        if (previous_converted_item_id == 0)
            previous_converted_item_id = current_converted_item_id;

        // Break early if the selected conversion task is not complete yet or if
        // it doesn't directly follow the previous task that completed successfully
        //
        // HarvestableItem IDs are almost always monotonic, so this condition will not
        // block the queue indefinitely. Under specific circumstances (e.g., when multiple
        // Futures are bound to the same source Tasklet), IDs can potentially repeat.
        // However, those cases are not problematic as a duplicate ID indicates a
        // duplicate download which is ignored when new conversion tasks are queued.
        if (not current_conversion->isComplete())
            break;
        else if (previous_converted_item_id != current_converted_item_id and
                 current_converted_item_id != previous_converted_item_id + 1)
            break;

        const auto &conversion_result(current_conversion->getResult());
        metrics->num_marc_conversions_skipped_since_online_first_ += conversion_result.num_skipped_since_online_first_;
        metrics->num_marc_conversions_skipped_since_early_view_ += conversion_result.num_skipped_since_early_view_;
        metrics->num_marc_conversions_skipped_since_exclusion_filters_ += conversion_result.num_skipped_since_exclusion_filters_;

        unsigned num_written_records(0);
        for (const auto &record : conversion_result.marc_records_) {
            // Check if the record was previously uploaded to the BSZ server
            // by comparing its hash and URLs with the ones stored in our database.
            const auto record_hash(Conversion::CalculateMarcRecordHash(*record));
            const auto record_urls(record->getSubfieldValues("856", 'u'));
            bool already_delivered(false);
            std::string tracker_message;

            if (not commandline_args.force_downloads_) {
                for (const auto &url : record_urls) {
                    if (upload_tracker.urlAlreadyDelivered(url)) {
                        already_delivered = true;
                        if (url != current_download_item.url_.toString())
                            tracker_message = "URL '" + url + "' match";
                        break;
                    }
                }

                if (not already_delivered) {
                    Util::UploadTracker::Entry previously_delivered_entry;
                    if (upload_tracker.hashAlreadyDelivered(record_hash, &previously_delivered_entry)) {
                        already_delivered = true;
                        tracker_message = "Hash '" + record_hash + "' match with URL '" + previously_delivered_entry.url_ + "'";
                    }
                }
            }

            if (already_delivered) {
                ++metrics->num_marc_conversions_skipped_since_already_delivered_;
                LOG_INFO("Item " + current_download_item.toString() + " already delivered"
                         + (not tracker_message.empty() ? "(" + tracker_message + ")" : ""));
                continue;
            }

            for (const auto &url : record_urls)
                urls_harvested_during_current_session->emplace(url);

            ++metrics->num_marc_conversions_successful_;
            ++num_written_records;

            const auto &group_params(current_conversion->getParameter().group_params_);
            ++metrics->group_name_to_num_generated_marc_records_map_[group_params.name_];

            const auto &writer(outputfile_cache->getWriter(group_params));
            writer->write(*record);
            writer->flush();
        }

        if (num_written_records > 0) {
            LOG_INFO("Generated " + std::to_string(num_written_records) + " record(s) for "
                     "item " + current_download_item.toString());
        }

        journal_datastore->queued_marc_records_.pop_front();
    }
}


} // unnamed namespace


int Main(int argc, char *argv[]) {
    if (argc < 2)
        Usage();

    Util::ZoteroLogger::Init();

    CommandLineArgs commandline_args;
    ParseCommandLineArgs(&argc, &argv, &commandline_args);

    HarvesterConfigData harvester_config;
    LoadHarvesterConfig(commandline_args.config_path_, &harvester_config);

    Util::HarvestableItemManager harvestable_manager(harvester_config.journal_params_);

    Download::DownloadManager::GlobalParams download_manager_params(*harvester_config.global_params_, &harvestable_manager);
    download_manager_params.force_downloads_ = commandline_args.force_downloads_;
    download_manager_params.ignore_robots_txt_ = commandline_args.ignore_robots_dot_txt_;
    Download::DownloadManager download_manager(download_manager_params);

    Conversion::ConversionManager::GlobalParams conversion_manager_params(commandline_args.force_downloads_,
                                                                          harvester_config.global_params_->skip_online_first_articles_unconditonally_,
                                                                          *harvester_config.enhancement_maps);
    Conversion::ConversionManager conversion_manager(conversion_manager_params);
    OutputFileCache output_file_cache(commandline_args, harvester_config);
    Util::UploadTracker upload_tracker;
    Metrics harvester_metrics;

    std::vector<std::unique_ptr<JournalDatastore>> journal_datastores;
    journal_datastores.reserve(harvester_config.journal_params_.size());
    std::unordered_set<std::string> urls_harvested_during_current_session;

    // Queue downloads for all selected journals.
    for (const auto &journal : harvester_config.journal_params_) {
        bool skip_journal(false);

        if (commandline_args.selection_mode_ == CommandLineArgs::SelectionMode::UPLOAD
            and commandline_args.selected_upload_operation_ != Config::UploadOperation::NONE
            and journal->upload_operation_ != commandline_args.selected_upload_operation_)
        {
            skip_journal = true;
        } else if (commandline_args.selection_mode_ == CommandLineArgs::SelectionMode::JOURNAL
                   and not commandline_args.selected_journals_.empty()
                   and commandline_args.selected_journals_.find(journal->name_) == commandline_args.selected_journals_.end())
        {
            skip_journal = true;
        }

        if (skip_journal)
            continue;

        auto current_journal_datastore(QueueDownloadsForJournal(*journal, harvester_config, &harvestable_manager,
                                                                &download_manager, &harvester_metrics));
        journal_datastores.emplace_back(std::move(current_journal_datastore));
    }

    static const unsigned BUSY_LOOP_THREAD_SLEEP_TIME(64 * 1000);   // ms -> us

    // Wait on completed downloads, initiate MARC conversions and write converted records to disk.
    while (true) {
        bool jobs_running(false);

        for (auto &journal_datastore : journal_datastores) {
            EnqueueCrawlAndRssResults(journal_datastore.get(), &jobs_running, &harvester_metrics);
            EnqueueCompletedDownloadsForConversion(journal_datastore.get(), &jobs_running, &conversion_manager, harvester_config,
                                                   urls_harvested_during_current_session, &harvester_metrics);
            WriteConversionResultsToDisk(journal_datastore.get(), &output_file_cache, commandline_args, upload_tracker,
                                         &urls_harvested_during_current_session, &harvester_metrics);

            if (not jobs_running)
                jobs_running = not journal_datastore->queued_downloads_.empty() or not journal_datastore->queued_marc_records_.empty();
        }

        if (not jobs_running)
            break;

        ::usleep(BUSY_LOOP_THREAD_SLEEP_TIME);
    }

    LOG_INFO(harvester_metrics.toString());

    return EXIT_SUCCESS;
}

/** \brief Classes related to the Zotero Harvester's JSON-to-MARC conversion API
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
#pragma once


#include <atomic>
#include <map>
#include <memory>
#include <set>
#include <unordered_map>
#include "JSON.h"
#include "MARC.h"
#include "RegexMatcher.h"
#include "ThreadUtil.h"
#include "ZoteroHarvesterConfig.h"
#include "ZoteroHarvesterUtil.h"


namespace ZoteroHarvester {


// This namespace contains classes and functions that used to convert JSON metadata
// returned by the Zotero Translation Server into a MARC-21 record. This is done by
// first converting the JSON response into an intermediate representation which is then
// enriched with additional information and then used to generate the final MARC record.
namespace Conversion {


// Represents a format-agnostic metadata record. Generated from a JSON response sent by
// the Zotero Translation Server.
struct MetadataRecord {
    enum class SSGType { INVALID, FG_0, FG_1, FG_01, FG_21 };


    enum class SuperiorType { INVALID, PRINT, ONLINE };


    struct Creator {
        std::string first_name_;
        std::string last_name_;
        std::string affix_;
        std::string title_;
        std::string type_;
        std::string ppn_;
        std::string gnd_number_;

    public:
        Creator(const std::string &first_name, const std::string &last_name, const std::string &type)
            : first_name_(first_name), last_name_(last_name), type_(type) { }
    };


    std::string item_type_;
    std::string title_;
    std::string short_title_;
    std::vector<Creator> creators_;
    std::string abstract_note_;
    std::string publication_title_;
    std::string volume_;
    std::string issue_;
    std::string pages_;
    std::string date_;
    std::string doi_;
    std::set<std::string> languages_;
    std::string url_;
    std::string issn_;
    std::string license_;
    std::string superior_ppn_;
    SuperiorType superior_type_;
    SSGType ssg_;
    std::vector<std::string> keywords_;
    std::multimap<std::string, std::string> custom_metadata_;
    bool pages_not_online_first_;

public:
    explicit MetadataRecord(): superior_type_(SuperiorType::INVALID), ssg_(SSGType::INVALID), pages_not_online_first_(false) { }

public:
    std::string toString() const;

    static SSGType GetSSGTypeFromString(const std::string &ssg_string);
};


void PostprocessTranslationServerResponse(const Util::HarvestableItem &download_item,
                                          std::shared_ptr<JSON::ArrayNode> * const response_json_array);


bool ZoteroItemMatchesExclusionFilters(const Util::HarvestableItem &download_item, const std::shared_ptr<JSON::ObjectNode> &zotero_item);


void ConvertZoteroItemToMetadataRecord(const std::shared_ptr<JSON::ObjectNode> &zotero_item, MetadataRecord * const metadata_record);


void AugmentMetadataRecord(MetadataRecord * const metadata_record, const Config::JournalParams &journal_params,
                           const Config::GroupParams &group_params);


void GenerateMarcRecordFromMetadataRecord(const Util::HarvestableItem &download_item, const MetadataRecord &metadata_record,
                                          const Config::GroupParams &group_params, MARC::Record * const marc_record,
                                          std::string * const marc_record_hash);


bool MarcRecordMatchesExclusionFilters(const Util::HarvestableItem &download_item, MARC::Record * const marc_record);


std::string CalculateMarcRecordHash(const MARC::Record &marc_record);


struct ConversionParams {
    Util::HarvestableItem download_item_;
    std::string json_metadata_;
    const Config::GlobalParams &global_params_;
    const Config::GroupParams &group_params_;
    const Config::GroupParams &subgroup_params_;

public:
    ConversionParams(const Util::HarvestableItem &download_item, const std::string &json_metadata,
                     const Config::GlobalParams &global_params, const Config::GroupParams &group_params,
                     const Config::SubgroupParams &subgroup_params)
        : download_item_(download_item), json_metadata_(json_metadata), global_params_(global_params), group_params_(group_params),
          subgroup_params_(subgroup_params) { }
};


struct ConversionResult {
    std::vector<std::unique_ptr<MARC::Record>> marc_records_;
    unsigned num_skipped_since_undesired_item_type_;
    unsigned num_skipped_since_online_first_;
    unsigned num_skipped_since_early_view_;
    unsigned num_skipped_since_exclusion_filters_;

public:
    explicit ConversionResult()
        : num_skipped_since_undesired_item_type_(0), num_skipped_since_online_first_(0), num_skipped_since_early_view_(0),
          num_skipped_since_exclusion_filters_(0) { }
    ConversionResult(const ConversionResult &rhs) = delete;
};


class ConversionTasklet : public Util::Tasklet<ConversionParams, ConversionResult> {
    void run(const ConversionParams &parameters, ConversionResult * const result);

public:
    ConversionTasklet(ThreadUtil::ThreadSafeCounter<unsigned> * const instance_counter, std::unique_ptr<ConversionParams> parameters);
    virtual ~ConversionTasklet() override = default;
};


// Tracks active and queued conversion tasks. Architecturally similar to
// DownloadManage. The public interface offers a non-blocking function to
// queue conversion tasks.
class ConversionManager {
private:
    static constexpr unsigned MAX_CONVERSION_TASKLETS = 15;

    const Config::GlobalParams &global_params_;
    pthread_t background_thread_;
    std::atomic_bool stop_background_thread_;
    ThreadUtil::ThreadSafeCounter<unsigned> conversion_tasklet_execution_counter_;
    std::deque<std::shared_ptr<ConversionTasklet>> active_conversions_;
    std::deque<std::shared_ptr<ConversionTasklet>> conversion_queue_;
    mutable std::mutex conversion_queue_mutex_;

    static void *BackgroundThreadRoutine(void *parameter);

    void processQueue();
    void cleanupCompletedTasklets();

public:
    ConversionManager(const Config::GlobalParams &global_params);
    ~ConversionManager();

public:
    std::unique_ptr<Util::Future<ConversionParams, ConversionResult>> convert(const Util::HarvestableItem &source,
                                                                              const std::string &json_metadata,
                                                                              const Config::GroupParams &group_params,
                                                                              const Config::SubgroupParams &subgroup_params);
    inline unsigned numActiveConversions() const { return conversion_tasklet_execution_counter_; }
    inline unsigned numQueuedConversions() const {
        std::lock_guard<decltype(conversion_queue_mutex_)> lock(conversion_queue_mutex_);
        return conversion_queue_.size();
    }
    inline bool conversionInProgress() const { return numActiveConversions() + numQueuedConversions() != 0; }
};


} // end namespace Conversion


} // end namespace ZoteroHarvester

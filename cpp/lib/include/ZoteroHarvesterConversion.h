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


namespace Conversion {


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
         : first_name_(first_name), last_name_(last_name), type_(type) {}
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
    std::string language_;
    std::string url_;
    std::string issn_;
    std::string license_;
    std::string superior_ppn_;
    SuperiorType superior_type_;
    SSGType ssg_;
    std::vector<std::string> keywords_;
    std::map<std::string, std::string> custom_metadata_;
public:
    explicit MetadataRecord() : superior_type_(SuperiorType::INVALID), ssg_(SSGType::INVALID) {}
};


void PostprocessTranslationServerResponse(const Util::Harvestable &download_item,
                                          std::shared_ptr<JSON::ArrayNode> * const response_json_array);


bool ZoteroItemMatchesExclusionFilters(const Util::Harvestable &download_item,
                                       const std::shared_ptr<JSON::ObjectNode> &zotero_item);


void ConvertZoteroItemToMetadataRecord(const std::shared_ptr<JSON::ObjectNode> &zotero_item,
                                       MetadataRecord * const metadata_record);


void AugmentMetadataRecord(MetadataRecord * const metadata_record, const Config::JournalParams &journal_params,
                           const Config::GroupParams &group_params, const Config::EnhancementMaps &enhancement_maps);


void GenerateMarcRecordFromMetadataRecord(const Util::Harvestable &download_item, const MetadataRecord &metadata_record,
                                          const Config::GroupParams &group_params, MARC::Record * const marc_record);


bool MarcRecordMatchesExclusionFilters(const Util::Harvestable &download_item, MARC::Record * const marc_record);


struct ConversionParams {
    Util::Harvestable download_item_;
    std::string json_metadata_;
    bool force_downloads_;
    bool skip_online_first_articles_unconditonally_;
    const Config::GroupParams &group_params_;
    const Config::EnhancementMaps &enhancement_maps_;
public:
    ConversionParams(const Util::Harvestable &download_item, const std::string &json_metadata, const bool force_downloads,
                     const bool skip_online_first_articles_unconditonally, const Config::GroupParams &group_params,
                     const Config::EnhancementMaps &enhancement_maps)
     : download_item_(download_item), json_metadata_(json_metadata), force_downloads_(force_downloads),
       skip_online_first_articles_unconditonally_(skip_online_first_articles_unconditonally),
       group_params_(group_params), enhancement_maps_(enhancement_maps) {}
};


struct ConversionResult {
    std::vector<std::unique_ptr<MARC::Record>> marc_records_;
};


class ConversionTasklet : public Util::Tasklet<ConversionParams, ConversionResult> {
    void run(const ConversionParams &parameters, ConversionResult * const result);
public:
    ConversionTasklet(std::unique_ptr<ConversionParams> parameters);
    virtual ~ConversionTasklet() override = default;

    static unsigned GetRunningInstanceCount();
};


} // end namespace Conversion


} // end namespace ZoteroHarvester

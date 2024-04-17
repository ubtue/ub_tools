/** \brief Utility for syncing legacy records from the FID stock to zotero delivered records
 *  \author Johannes Riedl (johannes.riedl@uni-tuebingen.de)
 *
 *  \copyright 2024 Universitätsbibliothek Tübingen.  All rights reserved.
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
#include <unordered_set>
#include <cstdio>
#include <cstdlib>
#include <nlohmann/json.hpp>
#include "Compiler.h"
#include "DbConnection.h"
#include "DbResultSet.h"
#include "DbRow.h"
#include "DnsUtil.h"
#include "EmailSender.h"
#include "IniFile.h"
#include "MARC.h"
#include "RegexMatcher.h"
#include "Solr.h"
#include "SolrJSON.h"
#include "StringUtil.h"
#include "UBTools.h"
#include "ZoteroHarvesterUtil.h"
#include "util.h"

// Needed because of a clang bug (c.f. https://github.com/llvm/llvm-project/issues/42943) that
// prevents properly detecting that LegacyEntryInformationHash and LegacyEntryInformationEqual below are needed
#pragma clang diagnostic ignored "-Wunneeded-internal-declaration"

namespace {


[[noreturn]] void Usage() {
    ::Usage("email_address");
}


struct LegacyRecordStock {
    std::string ixtheo_host_;
    std::string ixtheo_port_;
    std::string krim_host_;
    std::string krim_port_;
};


struct LegacyEntryInformation {
    std::vector<std::string> dois_;
    std::string main_title_;
    std::string record_id_;
};


auto LegacyEntryInformationHash([](const LegacyEntryInformation &A) { return std::hash<std::string>()(StringUtil::Join(A.dois_, "")); });
auto LegacyEntryInformationEqual([](const LegacyEntryInformation &A, const LegacyEntryInformation &B) { return A.dois_ == B.dois_; });
using LegacyEntryInformationSet =
    std::unordered_set<LegacyEntryInformation, decltype(LegacyEntryInformationHash), decltype(LegacyEntryInformationEqual)>;


const std::string CONF_FILE_PATH(UBTools::GetTuelibPath() + "zotero.conf");
const std::string LEGACY_RECORD_SECTION_NAME("LegacyRecordStock");


void GetLegacyStockConfig(const IniFile &zotero_conf, LegacyRecordStock * const legacy_record_stock) {
    for (const auto record_server : { "ixtheo_record_server", "krim_record_server" }) {
        if (StringUtil::StartsWith(record_server, "ixtheo")) {
            StringUtil::SplitOnString(zotero_conf.getString(LEGACY_RECORD_SECTION_NAME, record_server), ":",
                                      &(legacy_record_stock->ixtheo_host_), &(legacy_record_stock->ixtheo_port_));
        } else if (StringUtil::StartsWith(record_server, "krim")) {
            StringUtil::SplitOnString(zotero_conf.getString(LEGACY_RECORD_SECTION_NAME, record_server), ":",
                                      &(legacy_record_stock->krim_host_), &(legacy_record_stock->krim_port_));
        } else {
            // We should never come here
            LOG_ERROR("Invalid server type (" + std::string(record_server) + ")");
        }
    }
}


void GetLegacyEntriesInformationForJournal(const std::string &host_url, const std::string &host_port, const std::string &query,
                                           LegacyEntryInformationSet * const legacy_entries_information) {
    std::string result;
    std::string err_msg;
    if (not Solr::Query(query, "doi_str_mv, title_full, id", &result, &err_msg, host_url, std::stoi(host_port), 300 /*Timeout*/,
                        Solr::QueryResultFormat::JSON))
        LOG_ERROR("Error occurred for Solr Query \"" + query + "\": " + err_msg);

    std::cout << result << "\n\n\n";

    nlohmann::json parsed_result(nlohmann::json::parse(result));
    for (const auto &doc : parsed_result["response"]["docs"]) {
        LegacyEntryInformation legacy_entry_information;
        legacy_entry_information.record_id_ = doc["id"];
        // doc["doi_str_mv"].get_to(legacy_entry_information.dois_);
        if (doc.contains("doi_str_mv")) {
            for (const auto &doi : doc["doi_str_mv"])
                legacy_entry_information.dois_.emplace_back("https://doi.org/" + std::string(doi));
        }
        if (doc.contains("title_full"))
            legacy_entry_information.main_title_ = doc["title_full"];

        legacy_entries_information->emplace(legacy_entry_information);
    }
}


void InsertNonExistingLegacyEntriesToZoteroDB(const std::string &zeder_id, const std::string &zotero_group,
                                              const LegacyEntryInformationSet &legacy_entries_information) {
    ZoteroHarvester::Util::UploadTracker upload_tracker;
    for (const auto &legacy_entry_information : legacy_entries_information) {
        for (const auto &doi : legacy_entry_information.dois_)
            upload_tracker.archiveLegacyEntry(zeder_id, upload_tracker.GetZederInstanceString(zotero_group),
                                              legacy_entry_information.record_id_, legacy_entry_information.main_title_, doi);
    }
}


void GetQueryHostUrlAndPort(const LegacyRecordStock &legacy_record_stock, const std::string &zeder_group, std::string * const host_url,
                            std::string * const host_port) {
    if (not((zeder_group == "IxTheo") or (zeder_group == "KrimDok")))
        LOG_ERROR("Invalid zeder group \"" + zeder_group + "\"");

    if (zeder_group == "IxTheo") {
        *host_url = legacy_record_stock.ixtheo_host_;
        *host_port = legacy_record_stock.ixtheo_port_;
        return;
    }

    *host_url = legacy_record_stock.krim_host_;
    *host_port = legacy_record_stock.krim_port_;
}


void SyncLegacyRecordsForJournal(const LegacyRecordStock &legacy_record_stock, const auto &harvester_journal_section) {
    std::vector<std::string> journal_ppns;
    journal_ppns.emplace_back(harvester_journal_section.getString("print_ppn", ""));
    journal_ppns.emplace_back(harvester_journal_section.getString("online_ppn", ""));
    journal_ppns.erase(std::remove_if(journal_ppns.begin(), journal_ppns.end(), [](const std::string &ppn) { return ppn.empty(); }),
                       journal_ppns.end());
    std::transform(journal_ppns.begin(), journal_ppns.end(), journal_ppns.begin(),
                   [](const std::string ppn) { return "superior_ppn:" + ppn; });
    const std::string query(StringUtil::Join(journal_ppns, " OR "));

    std::string host_url, host_port;
    const std::string zotero_group(harvester_journal_section.getString("zotero_group", ""));
    GetQueryHostUrlAndPort(legacy_record_stock, zotero_group, &host_url, &host_port);
    LegacyEntryInformationSet legacy_entries_information;
    GetLegacyEntriesInformationForJournal(host_url, host_port, query, &legacy_entries_information);

    std::cout << "query: " << query << '\n';
    for (const auto &legacy_entry_information : legacy_entries_information) {
        std::cout << StringUtil::Join(legacy_entry_information.dois_, " ") << '\n';
        std::cout << legacy_entry_information.main_title_ << '\n';
        std::cout << legacy_entry_information.record_id_ << '\n';
    }
    // Update
    const std::string zeder_id(harvester_journal_section.getString("zeder_id", ""));
    InsertNonExistingLegacyEntriesToZoteroDB(zeder_id, zotero_group, legacy_entries_information);
}


void SyncLegacyRecords(const IniFile &zotero_conf, const IniFile &harvester_conf) {
    LegacyRecordStock legacy_record_stock;
    GetLegacyStockConfig(zotero_conf, &legacy_record_stock);

    for (const auto &section : harvester_conf) {
        if (section.getSectionName().empty())
            continue; // global section
        if (section.find("user_agent") != section.end() or section.find("author_swb_lookup_url") != section.end())
            continue; // Not a journal section.

        const auto delivery_mode(static_cast<ZoteroHarvester::Config::UploadOperation>(
            section.getEnum("zotero_delivery_mode", ZoteroHarvester::Config::STRING_TO_UPLOAD_OPERATION_MAP,
                            ZoteroHarvester::Config::UploadOperation::NONE)));
        if (delivery_mode == ZoteroHarvester::Config::UploadOperation::NONE)
            continue;
        if (section.getBool("zeder_newly_synced_entry", false))
            continue;
        SyncLegacyRecordsForJournal(legacy_record_stock, section);
    }
}

} // end unnamed namespace


int Main(int argc, __attribute__((unused)) char *argv[]) {
    if (argc != 1)
        Usage();

    const IniFile zotero_conf(CONF_FILE_PATH);
    const IniFile harvester_conf(UBTools::GetTuelibPath() + "zotero-enhancement-maps/zotero_harvester.conf");
    SyncLegacyRecords(zotero_conf, harvester_conf);

    return EXIT_SUCCESS;
}

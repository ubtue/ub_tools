/** \brief Utility for syncing legacy records from the FID stock or the K10Plus to zotero delivered records
 *  \author Johannes Riedl (johannes.riedl@uni-tuebingen.de)
 *
 *  \copyright 2024,2025 Universitätsbibliothek Tübingen.  All rights reserved.
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
#include "ExecUtil.h"
#include "FileUtil.h"
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
    ::Usage("[--include-k10plus] [--zotero-conf path] [--harvester-conf path] (--all (Beware - long runtime!!) | journal_name)");
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
const std::string GET_INFERIOR_K10PLUS_RECORDS_PATH("/usr/local/bin/get_inferior_k10plus_records_for_ppn.sh");


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


void GetLegacyEntriesInformationForJournalFromFIDStock(const std::string &host_url, const std::string &host_port, const std::string &query,
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


void GetLegacyEntriesInformationForJournalFromK10Plus(const std::vector<std::string> &journal_ppns,
                                                      LegacyEntryInformationSet * const legacy_entries_information) {
    for (const auto &journal_ppn : journal_ppns) {
        FileUtil::AutoTempFile k10plus_results;
        ExecUtil::ExecOrDie(GET_INFERIOR_K10PLUS_RECORDS_PATH, { journal_ppn, k10plus_results.getFilePath() });

        const std::unique_ptr<MARC::Reader> marc_reader(MARC::Reader::Factory(k10plus_results.getFilePath()));
        while (MARC::Record record = marc_reader.get()->read()) {
            LegacyEntryInformation legacy_entry_information;
            legacy_entry_information.record_id_ = record.getControlNumber();

            const auto dois_set(record.getDOIs());
            std::vector<std::string> dois(dois_set.begin(), dois_set.end());
            std::transform(dois.begin(), dois.end(), dois.begin(), [](const std::string &doi) { return "https://doi.org/" + doi; });
            std::copy(dois.begin(), dois.end(), std::back_inserter(legacy_entry_information.dois_));
            legacy_entry_information.main_title_ = record.getMainTitle();

            legacy_entries_information->emplace(legacy_entry_information);
        }
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


void SyncLegacyRecordsForJournal(const LegacyRecordStock &legacy_record_stock, const bool include_k10plus,
                                 const auto &harvester_journal_section) {
    std::vector<std::string> journal_ppns;
    journal_ppns.emplace_back(harvester_journal_section.getString("print_ppn", ""));
    journal_ppns.emplace_back(harvester_journal_section.getString("online_ppn", ""));
    journal_ppns.erase(std::remove_if(journal_ppns.begin(), journal_ppns.end(), [](const std::string &ppn) { return ppn.empty(); }),
                       journal_ppns.end());
    std::vector<std::string> query_journal_ppns;
    std::transform(journal_ppns.begin(), journal_ppns.end(), std::back_inserter(query_journal_ppns),
                   [](const std::string ppn) { return "superior_ppn:" + ppn; });
    const std::string query(StringUtil::Join(query_journal_ppns, " OR "));

    std::string host_url, host_port;
    const std::string zotero_group(harvester_journal_section.getString("zotero_group", ""));
    GetQueryHostUrlAndPort(legacy_record_stock, zotero_group, &host_url, &host_port);
    LegacyEntryInformationSet legacy_entries_information;
    GetLegacyEntriesInformationForJournalFromFIDStock(host_url, host_port, query, &legacy_entries_information);

    if (include_k10plus or harvester_journal_section.getBool("selective_evaluation"))
        GetLegacyEntriesInformationForJournalFromK10Plus(journal_ppns, &legacy_entries_information);

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


bool DeliveryModeIsNoneOrNewlySyncedEntry(const auto &section) {
    const auto delivery_mode(static_cast<ZoteroHarvester::Config::UploadOperation>(section.getEnum(
        "zotero_delivery_mode", ZoteroHarvester::Config::STRING_TO_UPLOAD_OPERATION_MAP, ZoteroHarvester::Config::UploadOperation::NONE)));
    if (delivery_mode == ZoteroHarvester::Config::UploadOperation::NONE)
        return true;
    if (section.getBool("zeder_newly_synced_entry", false))
        return true;
    return false;
}


void SyncLegacyRecords(const IniFile &zotero_conf, const IniFile &harvester_conf, const bool include_k10plus,
                       const std::string &journal_name = "") {
    LegacyRecordStock legacy_record_stock;
    GetLegacyStockConfig(zotero_conf, &legacy_record_stock);

    // Handle all
    if (journal_name.empty()) {
        for (const auto &section : harvester_conf) {
            if (section.getSectionName().empty())
                continue; // global section
            if (section.find("user_agent") != section.end() or section.find("author_swb_lookup_url") != section.end())
                continue; // Not a journal section.
            if (DeliveryModeIsNoneOrNewlySyncedEntry(section))
                continue;

            SyncLegacyRecordsForJournal(legacy_record_stock, include_k10plus, section);
        }
        return;
    }

    // Handle one journal
    for (const auto &section : harvester_conf) {
        if (section.getSectionName() == journal_name) {
            if (DeliveryModeIsNoneOrNewlySyncedEntry(section))
                LOG_WARNING("Configuration Entry for \"" + journal_name +"\" found, but delivery mode is none \
                            or it is a newly_synced entry - continuing anyway");
            SyncLegacyRecordsForJournal(legacy_record_stock, include_k10plus, section);
            return;
        }
    }

    LOG_ERROR("No section found for \"" + journal_name + "\" - Aborting");
}

} // end unnamed namespace


int Main(int argc, char *argv[]) {
    if (argc < 2 || argc > 7)
        Usage();

    bool all_journals(false);
    bool include_k10plus(false);
    std::string zotero_conf_path;
    std::string harvester_conf_path;
    std::string journal_name;

    while (argc > 1) {
        if (std::strcmp(argv[1], "--zotero-conf") == 0) {
            if (argc < 2)
                Usage();
            zotero_conf_path = argv[2];
            argv += 2;
            argc -= 2;
            continue;
        }

        if (std::strcmp(argv[1], "--harvester-conf") == 0) {
            if (argc < 2)
                Usage();
            harvester_conf_path = argv[2];
            argv += 2;
            argc -= 2;
            continue;
        }


        if (std::strcmp(argv[1], "--include-k10plus") == 0) {
            include_k10plus = true;
            ++argv;
            --argc;
            continue;
        }


        if (std::strcmp(argv[1], "--all") == 0) {
            all_journals = true;
            ++argv;
            --argc;
            break;
        }

        if (not all_journals and argc == 1)
            Usage();

        journal_name = argv[1];
        ++argv;
        --argc;

        if (argc != 1)
            Usage();
    }

    const IniFile zotero_conf(zotero_conf_path.empty() ? CONF_FILE_PATH : zotero_conf_path);
    const IniFile harvester_conf(harvester_conf_path.empty() ? UBTools::GetTuelibPath() + "zotero-enhancement-maps/zotero_harvester.conf"
                                                             : harvester_conf_path);

    SyncLegacyRecords(zotero_conf, harvester_conf, include_k10plus, all_journals ? "" : journal_name);

    return EXIT_SUCCESS;
}

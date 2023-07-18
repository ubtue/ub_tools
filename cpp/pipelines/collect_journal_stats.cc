/** \brief Updates Zeder (via Ingo's SQL database) w/ the last N issues of harvested articles for each journal.
 *  \author Dr. Johannes Ruscheinski (johannes.ruscheinski@uni-tuebingen.de)
 *
 *  \copyright 2018-2021 Universitätsbibliothek Tübingen.  All rights reserved.
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

#include <unordered_map>
#include <cstdlib>
#include "Compiler.h"
#include "DbConnection.h"
#include "DnsUtil.h"
#include "EmailSender.h"
#include "FileUtil.h"
#include "IniFile.h"
#include "JSON.h"
#include "MARC.h"
#include "SqlUtil.h"
#include "StringUtil.h"
#include "UBTools.h"
#include "Zeder.h"
#include "util.h"


namespace {


[[noreturn]] void Usage() {
    ::Usage(
        "[--min-log-level=log_level] [--debug] system_type marc_input json_output\n"
        "\twhere \"system_type\" must be one of ixtheo|krimdok");
}


// Please note that Zeder PPN entries are separated by spaces and, unlike what the column names "print_ppn" and
// "online_ppn" imply may in rare cases contain space-separated lists of PPN's.
inline auto SplitZederPPNs(const std::string &ppns) {
    std::vector<std::string> individual_ppns;
    StringUtil::Split(ppns, ' ', &individual_ppns);
    return individual_ppns;
}


struct ZederIdAndPPNType {
    unsigned zeder_id_;
    char type_; // 'p' or 'e' for "print" or "electronic"
public:
    ZederIdAndPPNType(const unsigned zeder_id, const char type): zeder_id_(zeder_id), type_(type) { }
};


std::unordered_map<std::string, ZederIdAndPPNType> GetPPNsToZederIdsAndTypesMap(const std::string &system_type) {
    std::unordered_map<std::string, ZederIdAndPPNType> ppns_to_zeder_ids_and_types_map;

    const Zeder::SimpleZeder zeder(system_type == "ixtheo" ? Zeder::IXTHEO : Zeder::KRIMDOK, { "eppn", "pppn" });
    if (not zeder) {
        EmailSender::SimplerSendEmail("no-reply@ub.uni-tuebingen.de", { system_type + "-team@ub.uni-tuebingen.de" },
                                      "Zeder Download Problems in collect_journal_stats", "We can't contact the Zeder MySQL server!",
                                      EmailSender::VERY_HIGH);
        return ppns_to_zeder_ids_and_types_map;
    }

    if (unlikely(zeder.empty()))
        LOG_ERROR(
            "found no Zeder entries matching any of our requested columns!"
            " (This *should* not happen as we included the column ID!)");

    unsigned included_journal_count(0);
    std::set<std::string> bundle_ppns; // We use a std::set because it is automatically being sorted for us.
    for (const auto &journal : zeder) {
        if (journal.empty())
            continue;
        ++included_journal_count;

        const auto print_ppns(SplitZederPPNs(journal.lookup("pppn")));
        const auto online_ppns(SplitZederPPNs(journal.lookup("eppn")));

        if (print_ppns.empty() and online_ppns.empty()) {
            --included_journal_count;
            LOG_WARNING("Zeder entry #" + std::to_string(journal.getId()) + " is missing print and online PPN's!");
            continue;
        }

        for (const auto &print_ppn : print_ppns)
            ppns_to_zeder_ids_and_types_map.emplace(print_ppn, ZederIdAndPPNType(journal.getId(), 'p'));

        for (const auto &online_ppn : online_ppns)
            ppns_to_zeder_ids_and_types_map.emplace(online_ppn, ZederIdAndPPNType(journal.getId(), 'e'));
    }

    LOG_INFO("downloaded information for " + std::to_string(included_journal_count) + " journal(s) from Zeder.");

    return ppns_to_zeder_ids_and_types_map;
}


struct Article {
    std::string id_;
    std::string jahr_;
    std::string band_;
    std::string heft_;
    std::string seitenbereich_;

public:
    Article(const std::string &id, const std::string &jahr, const std::string &band, const std::string &heft,
            const std::string &seitenbereich)
        : id_(id), jahr_(jahr), band_(band), heft_(heft), seitenbereich_(seitenbereich) {
        std::size_t pos = heft_.find(" ("); // Strip e.g. 17 (October 2019)
        if (pos != std::string::npos) {
            heft_ = heft_.substr(0, pos);
        }
    }
    Article() = default;
    Article(const Article &other) = default;

    /** \return True if the current entry represents a more recent article than "other".  If the entry is in the same issue
        we use the page numbers as an arbitrary tie breaker. */
    bool isNewerThan(const Article &other) const;
};


std::string GetLeadingDigits(const std::string &s) {
    std::string leading_digits;
    for (const char ch : s) {
        if (StringUtil::IsDigit(ch))
            leading_digits += ch;
        else
            return leading_digits;
    }
    return leading_digits;
}


inline bool Article::isNewerThan(const Article &other) const {
    if (jahr_ < other.jahr_)
        return false;
    if (jahr_ > other.jahr_)
        return true;
    if (band_ < other.band_)
        return false;
    if (band_ > other.band_)
        return true;
    if (heft_ < other.heft_)
        return false;
    if (heft_ > other.heft_)
        return true;

    const auto leading_digits(GetLeadingDigits(seitenbereich_));
    const auto other_leading_digits(GetLeadingDigits(other.seitenbereich_));
    if (not leading_digits.empty() and not other_leading_digits.empty())
        return StringUtil::ToUnsignedOrDie(leading_digits) > StringUtil::ToUnsignedOrDie(other_leading_digits);

    return seitenbereich_ > other.seitenbereich_; // Somewhat nonsensical, but useful nonetheless.
}


const size_t MAX_ISSUE_DATABASE_LENGTH(8); // maximum length of the issue column in the MySQL Zeder database


// Collects articles for whose superior PPN we have an entry in "ppns_to_zeder_ids_and_types_map".
void CollectZederArticles(MARC::Reader * const reader,
                          const std::unordered_map<std::string, ZederIdAndPPNType> &ppns_to_zeder_ids_and_types_map,
                          std::unordered_map<std::string, std::vector<Article>> * const zeder_ids_plus_ppns_to_articles_map) {
    LOG_INFO("Processing Zeder data...");
    unsigned total_count(0);
    while (const auto record = reader->read()) {
        ++total_count;

        const std::string superior_control_number(record.getSuperiorControlNumber());
        if (superior_control_number.empty())
            continue;

        const auto ppn_and_zeder_id_and_ppn_type(ppns_to_zeder_ids_and_types_map.find(superior_control_number));
        if (ppn_and_zeder_id_and_ppn_type == ppns_to_zeder_ids_and_types_map.cend())
            continue;

        const auto _936_field(record.findTag("936"));
        if (_936_field == record.end())
            continue;

        if (_936_field->getIndicator1() != 'u' or _936_field->getIndicator2() != 'w')
            continue;

        const std::string pages(_936_field->getFirstSubfieldWithCode('h'));
        std::string volume;
        std::string issue(_936_field->getFirstSubfieldWithCode('e'));
        if (issue.empty())
            issue = _936_field->getFirstSubfieldWithCode('d');
        else
            volume = _936_field->getFirstSubfieldWithCode('d');
        const std::string year(_936_field->getFirstSubfieldWithCode('j'));

        const std::string zeder_id(std::to_string(ppn_and_zeder_id_and_ppn_type->second.zeder_id_));
        const std::string ppn_type(1, ppn_and_zeder_id_and_ppn_type->second.type_);

        // Truncate in order to ensure that comparison with the database works:
        issue = issue.substr(0, MAX_ISSUE_DATABASE_LENGTH);
        const Article new_article(record.getControlNumber(), year, volume, issue, pages);

        const auto zeder_id_plus_ppn(zeder_id + "+" + superior_control_number);
        auto zeder_id_plus_ppn_and_articles(zeder_ids_plus_ppns_to_articles_map->find(zeder_id_plus_ppn));
        if (zeder_id_plus_ppn_and_articles == zeder_ids_plus_ppns_to_articles_map->end())
            (*zeder_ids_plus_ppns_to_articles_map)[zeder_id_plus_ppn] = std::vector<Article>{ new_article };
        else
            zeder_id_plus_ppn_and_articles->second.emplace_back(new_article);
    }

    LOG_INFO("Processed " + std::to_string(total_count) + " MARC record(s) and found "
             + std::to_string(zeder_ids_plus_ppns_to_articles_map->size()) + " Zeder article(s).");
}


std::string GetPPN(const std::string &zeder_id_plus_ppn) {
    const auto plus_pos(zeder_id_plus_ppn.find('+'));
    if (unlikely(plus_pos == std::string::npos))
        LOG_ERROR("missing + in \"" + zeder_id_plus_ppn + "\"!");
    return zeder_id_plus_ppn.substr(plus_pos + 1);
}


void GenerateJson(const std::unordered_map<std::string, ZederIdAndPPNType> &ppns_to_zeder_ids_and_types_map,
                  const std::unordered_map<std::string, std::vector<Article>> &zeder_ids_plus_ppns_to_articles_map,
                  const std::string &json_output_file) {
    LOG_INFO("Generate output file: " + json_output_file);
    const auto HOSTNAME(DnsUtil::GetHostname());
    auto json_file(FileUtil::OpenOutputFileOrDie(json_output_file));
    const auto JOB_START_TIME(std::to_string(std::time(nullptr)));
    unsigned long outer_size = zeder_ids_plus_ppns_to_articles_map.size();
    unsigned long outer_position = 0;
    std::vector<std::string> timestamps;
    std::vector<std::string> quellrechner;
    std::vector<std::string> zeder_ids;
    std::vector<std::string> ppn_typen;
    std::vector<std::string> ppns;
    std::vector<std::string> art_ppns;
    std::vector<std::string> jahre;
    std::vector<std::string> baende;
    std::vector<std::string> hefte;
    std::vector<std::string> seitenbereiche;
    for (const auto &[zeder_id_plus_ppn, articles] : zeder_ids_plus_ppns_to_articles_map) {
        ++outer_position;
        const auto ppn(GetPPN(zeder_id_plus_ppn));
        const auto ppn_and_zeder_id_and_ppn_type(ppns_to_zeder_ids_and_types_map.find(ppn));
        if (unlikely(ppn_and_zeder_id_and_ppn_type == ppns_to_zeder_ids_and_types_map.cend()))
            LOG_ERROR("no Zeder ID found for (Zeitschrift_)PPN \"" + ppn + "\"!");
        const auto zeder_id(std::to_string(ppn_and_zeder_id_and_ppn_type->second.zeder_id_));
        unsigned long inner_position = 0;
        for (auto &article : articles) {
            ++inner_position;
            const std::unordered_map<std::string, std::string> columns{
                { "timestamp", JOB_START_TIME }, { "Quellrechner", HOSTNAME },
                { "Zeder_ID", zeder_id },        { "Zeitschrift_PPN_Typ", std::string(1, ppn_and_zeder_id_and_ppn_type->second.type_) },
                { "Zeitschrift_PPN", ppn },      { "Artikel_PPN", article.id_ },
                { "Jahr", article.jahr_ },       { "Band", article.band_ },
                { "Heft", article.heft_ },       { "Seitenbereich", article.seitenbereich_ }
            };
            JSON::ObjectNode object_node(columns);

            timestamps.push_back(object_node.getStringValue("timestamp"));
            quellrechner.push_back("\"" + object_node.getStringValue("Quellrechner") + "\"");
            zeder_ids.push_back(object_node.getStringValue("Zeder_ID"));
            ppn_typen.push_back("\"" + object_node.getStringValue("Zeitschrift_PPN_Typ") + "\"");
            ppns.push_back("\"" + object_node.getStringValue("Zeitschrift_PPN") + "\"");
            art_ppns.push_back("\"" + object_node.getStringValue("Artikel_PPN") + "\"");
            jahre.push_back("\"" + object_node.getStringValue("Jahr") + "\"");
            baende.push_back("\"" + object_node.getStringValue("Band") + "\"");
            hefte.push_back("\"" + object_node.getStringValue("Heft") + "\"");
            seitenbereiche.push_back("\"" + object_node.getStringValue("Seitenbereich") + "\"");
        }
    }

    *json_file << "{";
    *json_file << "\"timestamp\":[" << StringUtil::Join(timestamps, ",") << "],";
    *json_file << "\"Quellrechner\":[" << StringUtil::Join(quellrechner, ",") << "],";
    *json_file << "\"Zeder_ID\":[" << StringUtil::Join(zeder_ids, ",") << "],";
    *json_file << "\"Zeitschrift_PPN_Typ\":[" << StringUtil::Join(ppn_typen, ",") << "],";
    *json_file << "\"Zeitschrift_PPN\":[" << StringUtil::Join(ppns, ",") << "],";
    *json_file << "\"Artikel_PPN\":[" << StringUtil::Join(art_ppns, ",") << "],";
    *json_file << "\"Jahr\":[" << StringUtil::Join(jahre, ",") << "],";
    *json_file << "\"Band\":[" << StringUtil::Join(baende, ",") << "],";
    *json_file << "\"Heft\":[" << StringUtil::Join(hefte, ",") << "],";
    *json_file << "\"Seitenbereich\":[" << StringUtil::Join(seitenbereiche, ",") << "]";
    *json_file << "}";

    LOG_INFO("Wrote " + std::to_string(outer_size) + " entries into JSON output file.");
}


const std::string TEXT_FILE_DIRECTORY(UBTools::GetFIDProjectsPath() + "Zeder_Supervision");


void UpdateTextFiles(const bool debug, const std::unordered_map<std::string, std::vector<Article>> &zeder_ids_plus_ppns_to_articles_map) {
    const auto DIRECTORY_PREFIX(debug ? "/tmp/collect_journal_stats/" : TEXT_FILE_DIRECTORY + "/" + DnsUtil::GetHostname() + "/");
    LOG_INFO("Writing output to " + DIRECTORY_PREFIX + "...");

    if (not FileUtil::Exists(DIRECTORY_PREFIX))
        FileUtil::MakeDirectoryOrDie(DIRECTORY_PREFIX);

    std::unordered_map<std::string, std::string> zeder_ids_plus_ppns_to_file_contents_map;
    for (const auto &[zeder_id_plus_ppn, articles] : zeder_ids_plus_ppns_to_articles_map) {
        auto zeder_id_plus_ppn_and_file_contents(zeder_ids_plus_ppns_to_file_contents_map.find(zeder_id_plus_ppn));
        if (zeder_id_plus_ppn_and_file_contents == zeder_ids_plus_ppns_to_file_contents_map.end())
            zeder_id_plus_ppn_and_file_contents =
                zeder_ids_plus_ppns_to_file_contents_map.insert(std::make_pair(zeder_id_plus_ppn, "")).first;

        for (const auto &article : articles) {
            zeder_id_plus_ppn_and_file_contents->second +=
                article.jahr_ + "," + article.band_ + "," + article.heft_ + ',' + article.seitenbereich_ + "\n";
        }
    }

    for (const auto &[zeder_id_plus_ppn, file_contents] : zeder_ids_plus_ppns_to_file_contents_map)
        FileUtil::WriteStringOrDie(DIRECTORY_PREFIX + zeder_id_plus_ppn + ".txt", file_contents);

    LOG_INFO("Wrote " + std::to_string(zeder_ids_plus_ppns_to_file_contents_map.size()) + " file(s) under " + DIRECTORY_PREFIX + ".");
}


// Sort Articles from newest to oldest for each journal.
void SortArticles(std::unordered_map<std::string, std::vector<Article>> * const zeder_ids_plus_ppns_to_articles_map) {
    for (auto &[_, articles] : *zeder_ids_plus_ppns_to_articles_map)
        std::sort(articles.begin(), articles.end(), [](const Article &a1, const Article &a2) -> bool { return a1.isNewerThan(a2); });
}


} // unnamed namespace


int Main(int argc, char *argv[]) {
    if (argc != 4 and argc != 5)
        Usage();

    bool debug(false);
    if (__builtin_strcmp(argv[1], "--debug") == 0) {
        debug = true;
        --argc, ++argv;
    }
    if (argc != 4)
        Usage();

    std::string system_type;
    if (__builtin_strcmp("ixtheo", argv[1]) == 0)
        system_type = "ixtheo";
    else if (__builtin_strcmp("krimdok", argv[1]) == 0)
        system_type = "krimdok";
    else
        LOG_ERROR("system_type must be one of ixtheo|krimdok!");

    const auto ppns_to_zeder_ids_and_types_map(GetPPNsToZederIdsAndTypesMap(system_type));

    const auto marc_reader(MARC::Reader::Factory(argv[2]));
    const std::string json_out_file(argv[3]);
    std::unordered_map<std::string, std::vector<Article>> zeder_ids_plus_ppns_to_articles_map;
    CollectZederArticles(marc_reader.get(), ppns_to_zeder_ids_and_types_map, &zeder_ids_plus_ppns_to_articles_map);

    if (not ppns_to_zeder_ids_and_types_map.empty()) {
        SortArticles(&zeder_ids_plus_ppns_to_articles_map);
        GenerateJson(ppns_to_zeder_ids_and_types_map, zeder_ids_plus_ppns_to_articles_map, json_out_file);

        if (not debug)
            UpdateTextFiles(debug, zeder_ids_plus_ppns_to_articles_map);
    }

    return EXIT_SUCCESS;
}

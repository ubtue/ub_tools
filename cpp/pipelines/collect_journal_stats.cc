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
#include "MARC.h"
#include "SqlUtil.h"
#include "StringUtil.h"
#include "UBTools.h"
#include "util.h"
#include "Zeder.h"


namespace {


[[noreturn]] void Usage() {
    ::Usage("[--min-log-level=log_level] [--debug] system_type marc_input marc_output\n"
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
    ZederIdAndPPNType(const unsigned zeder_id, const char type)
        : zeder_id_(zeder_id), type_(type) { }
};


std::unordered_map<std::string, ZederIdAndPPNType> GetPPNsToZederIdsAndTypesMap(const std::string &system_type) {
    std::unordered_map<std::string, ZederIdAndPPNType> ppns_to_zeder_ids_and_types_map;

    const Zeder::SimpleZeder zeder(system_type == "ixtheo" ? Zeder::IXTHEO : Zeder::KRIMDOK, { "eppn", "pppn" });
    if (not zeder) {
        EmailSender::SimplerSendEmail("no-reply@ub.uni-tuebingen.de", { system_type + "-team@ub.uni-tuebingen.de" },
                                      "Zeder Download Problems in collect_journal_stats",
                                      "We can't contact the Zeder MySQL server!",
                                      EmailSender::VERY_HIGH);
        return ppns_to_zeder_ids_and_types_map;
    }

    if (unlikely(zeder.empty()))
        LOG_ERROR("found no Zeder entries matching any of our requested columns!"
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


// \return the year as a small integer or 0 if we could not parse it.
unsigned short YearStringToShort(const std::string &year_as_string) {
    unsigned short year_as_unsigned_short;
    if (not StringUtil::ToUnsignedShort(year_as_string, &year_as_unsigned_short))
        return 0;
    return year_as_unsigned_short;
}


struct Article {
    std::string jahr_;
    std::string band_;
    std::string heft_;
    std::string seitenbereich_;
public:
    Article(const std::string &jahr, const std::string &band, const std::string &heft, const std::string &seitenbereich)
        : jahr_(jahr), band_(band), heft_(heft), seitenbereich_(seitenbereich) { }
    Article() = default;
    Article(const Article &other) = default;

    /** \return True if the current entry represents a more recent article than "other".  If the entry is in the same issue
        we use the page numbers as an arbitrary tie breaker. */
    bool isNewerThan(const Article &other) const;
};


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
    return seitenbereich_ > other.seitenbereich_; // Somewhat nonsensical, but useful nonetheless.
}


const size_t MAX_ISSUE_DATABASE_LENGTH(8); // maximum length of the issue column in the MySQL Zeder database


// Collects articles for whose superior PPN we have an entry in "ppns_to_zeder_ids_and_types_map".
void CollectZederArticles(MARC::Reader * const reader, MARC::Writer * const writer,
                          const std::unordered_map<std::string, ZederIdAndPPNType> &ppns_to_zeder_ids_and_types_map,
                          std::unordered_map<std::string, std::vector<Article>> * const zeder_ids_plus_ppns_to_articles_map)
{
    unsigned total_count(0);
    while (const auto record = reader->read()) {
        ++total_count;
        writer->write(record); // For the next pipeline phase.

        const std::string superior_control_number(record.getSuperiorControlNumber());
        if (superior_control_number.empty())
            continue;

        const auto ppn_and_zeder_id_and_ppn_type(ppns_to_zeder_ids_and_types_map.find(superior_control_number));
        if (ppn_and_zeder_id_and_ppn_type == ppns_to_zeder_ids_and_types_map.cend())
            continue;

        const auto _936_field(record.findTag("936"));
        if (_936_field == record.end())
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
        const std::string year_as_string(std::to_string(YearStringToShort(year)));

        // Truncate in order to ensure that comparison with the database works:
        issue = issue.substr(0, MAX_ISSUE_DATABASE_LENGTH);
        const Article new_article(year_as_string, volume, issue, pages);

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


size_t GetArticlesFromDatabase(const IniFile &ini_file, const std::string &system_type, const std::string &hostname,
                               std::unordered_map<std::string, Article> * const existing_articles)
{
    DbConnection db_connection_select(ini_file, "DatabaseSelect");
    db_connection_select.queryOrDie("SELECT MAX(timestamp),Zeder_ID,PPN,Jahr,Band,Heft,Seitenbereich"
                                    " FROM zeder.erschliessung WHERE Quellrechner='" + hostname + "' AND Systemtyp='"
                                    + system_type + "' GROUP BY Zeder_ID,PPN,Jahr,Band,Heft,Seitenbereich");
    auto result_set(db_connection_select.getLastResultSet());
    while (const auto row = result_set.getNextRow()) {
        const Article db_entry(row["Jahr"], row["Band"], row["Heft"], row["Seitenbereich"]);
        const auto key(row["Zeder_ID"] + "+" + row["PPN"]);
        auto key_and_article(existing_articles->find(key));
        if (key_and_article == existing_articles->end() or db_entry.isNewerThan(key_and_article->second))
            (*existing_articles)[key] = db_entry;
    }

    return existing_articles->size();
}


// \return True if "test_article" either does not exist in the database or is newer than the newest existing entry.
bool IsNewerThanWhatExistsInDB(const std::unordered_map<std::string, Article> &existing_articles,
                               const std::string &zeder_id, const std::string &ppn, const Article &test_article)
{
    const auto key_and_entry(existing_articles.find(zeder_id + "+" + ppn));
    if (key_and_entry == existing_articles.cend())
        return true;

    return test_article.isNewerThan(key_and_entry->second);
}


std::string GetPNN(const std::string &zeder_id_plus_ppn) {
    const auto plus_pos(zeder_id_plus_ppn.find('+'));
    if (unlikely(plus_pos == std::string::npos))
        LOG_ERROR("missing + in \"" + zeder_id_plus_ppn + "\"!");
    return zeder_id_plus_ppn.substr(plus_pos + 1);
}


void UpdateDatabase(const std::string &system_type,
                    const std::unordered_map<std::string, ZederIdAndPPNType> &ppns_to_zeder_ids_and_types_map,
                    const std::unordered_map<std::string, std::vector<Article>> &zeder_ids_plus_ppns_to_articles_map)
{
    std::unordered_map<std::string, Article> existing_articles;
    const IniFile ini_file;
    const auto HOSTNAME(DnsUtil::GetHostname());
    if (not ppns_to_zeder_ids_and_types_map.empty())
        LOG_INFO("Found " + std::to_string(GetArticlesFromDatabase(ini_file, system_type, HOSTNAME, &existing_articles))
                 + " existing database entries.");

    const auto JOB_START_TIME(std::to_string(std::time(nullptr)));

    DbConnection db_connection_insert(ini_file, "DatabaseInsert");
    const unsigned SQL_INSERT_BATCH_SIZE(50);
    const std::vector<std::string> COLUMN_NAMES{ "timestamp", "Quellrechner", "Systemtyp", "Zeder_ID", "PPN_Typ",
                                                 "PPN", "Jahr", "Band", "Heft", "Seitenbereich" };
    std::vector<std::vector<std::optional<std::string>>> column_values;

    for (const auto &[zeder_id_plus_ppn, articles] : zeder_ids_plus_ppns_to_articles_map) {
        const auto ppn(GetPNN(zeder_id_plus_ppn));
        const auto ppn_and_zeder_id_and_ppn_type(ppns_to_zeder_ids_and_types_map.find(ppn));
        if (unlikely(ppn_and_zeder_id_and_ppn_type == ppns_to_zeder_ids_and_types_map.cend()))
            LOG_ERROR("no Zeder ID found for PPN \"" + ppn + "\"!");

        const auto zeder_id(std::to_string(ppn_and_zeder_id_and_ppn_type->second.zeder_id_));
        for (auto article(articles.cbegin());
             article != articles.cend() and
                 IsNewerThanWhatExistsInDB(existing_articles, zeder_id, ppn, *article);
             ++article)
        {
            const std::vector<std::optional<std::string>> new_column_values{
                { /* timestamp */     JOB_START_TIME                                                  },
                { /* Quellrechner */  HOSTNAME                                                        },
                { /* Systemtyp */     system_type,                                                    },
                { /* Zeder_ID */      zeder_id                                                        },
                { /* PPN_Typ */       std::string(1, ppn_and_zeder_id_and_ppn_type->second.type_)     },
                { /* PPN */           ppn                                                             },
                { /* Jahr */          article->jahr_                                                  },
                { /* Band */          article->band_                                                  },
                { /* Heft */          article->heft_                                                  },
                { /* Seitenbereich */ article->seitenbereich_                                         },
            };
            column_values.emplace_back(new_column_values);

            if (column_values.size() == SQL_INSERT_BATCH_SIZE) {
                db_connection_insert.insertIntoTableOrDie("zeder.erschliessung", COLUMN_NAMES, column_values);
                column_values.clear();
            }
        }
    }
    if (not column_values.empty())
        db_connection_insert.insertIntoTableOrDie("zeder.erschliessung", COLUMN_NAMES, column_values);

    LOG_INFO("Inserted " + std::to_string(zeder_ids_plus_ppns_to_articles_map.size()) + " entries into Ingo's database.");
}


const std::string TEXT_FILE_DIRECTORY(UBTools::GetFIDProjectsPath() + "Zeder_Supervision");


void UpdateTextFiles(const bool debug,
                     const std::unordered_map<std::string, std::vector<Article>> &zeder_ids_plus_ppns_to_articles_map)
{
    const auto DIRECTORY_PREFIX(debug ? "/tmp/collect_journal_stats/"
                                      : TEXT_FILE_DIRECTORY + "/" + DnsUtil::GetHostname() + "/");
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

    LOG_INFO("Wrote " + std::to_string(zeder_ids_plus_ppns_to_file_contents_map.size()) + " file(s) under "
             + DIRECTORY_PREFIX + ".");
}


// Sort Articles from newest to oldest for each journal.
void SortArticles(std::unordered_map<std::string, std::vector<Article>> * const zeder_ids_plus_ppns_to_articles_map) {
    for (auto &[_, articles] : *zeder_ids_plus_ppns_to_articles_map)
        std::sort(articles.begin(), articles.end(),
                  [](const Article &a1, const Article &a2) -> bool { return a1.isNewerThan(a2); });
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
    const auto marc_writer(MARC::Writer::Factory(argv[3]));
    std::unordered_map<std::string, std::vector<Article>> zeder_ids_plus_ppns_to_articles_map;
    CollectZederArticles(marc_reader.get(), marc_writer.get(), ppns_to_zeder_ids_and_types_map,
                         &zeder_ids_plus_ppns_to_articles_map);

    if (not ppns_to_zeder_ids_and_types_map.empty()) {
        SortArticles(&zeder_ids_plus_ppns_to_articles_map);
        if (not debug)
            UpdateDatabase(system_type, ppns_to_zeder_ids_and_types_map, zeder_ids_plus_ppns_to_articles_map);
        UpdateTextFiles(debug, zeder_ids_plus_ppns_to_articles_map);
    }

    return EXIT_SUCCESS;
}

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
#include "FileUtil.h"
#include "IniFile.h"
#include "MARC.h"
#include "SqlUtil.h"
#include "StringUtil.h"
#include "util.h"
#include "Zeder.h"


namespace {


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
    const Zeder::SimpleZeder zeder(system_type == "ixtheo" ? Zeder::IXTHEO : Zeder::KRIMDOK, { "eppn", "pppn" });
    if (unlikely(zeder.empty()))
        LOG_ERROR("found no Zeder entries matching any of our requested columns!"
                  " (This *should* not happen as we included the column ID!)");

    std::unordered_map<std::string, ZederIdAndPPNType> ppns_to_zeder_ids_and_types_map;
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


struct DbEntry {
    std::string jahr_;
    std::string band_;
    std::string heft_;
    std::string seitenbereich_;
public:
    DbEntry(const std::string &jahr, const std::string &band, const std::string &heft, const std::string &seitenbereich)
        : jahr_(jahr), band_(band), heft_(heft), seitenbereich_(seitenbereich) { }
    DbEntry() = default;
    DbEntry(const DbEntry &other) = default;

    /** \return True if the current entry represents a more recent article than "other".  If the entry is in the same issue
        we use the page numbers as an arbitrary tie breaker. */
    bool isNewerThan(const DbEntry &other) const;
};


inline bool DbEntry::isNewerThan(const DbEntry &other) const {
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


size_t GetExistingDbEntries(const IniFile &ini_file, const std::string &hostname, const std::string &system_type,
                            std::unordered_map<std::string, DbEntry> * const existing_entries)
{
    DbConnection db_connection_select(ini_file, "DatabaseSelect");

    db_connection_select.queryOrDie("SELECT MAX(timestamp),Zeder_ID,PPN,Jahr,Band,Heft,Seitenbereich"
                                    " FROM zeder.erschliessung WHERE Quellrechner='" + hostname + "' AND Systemtyp='"
                                    + system_type + "' GROUP BY Zeder_ID,PPN,Jahr,Band,Heft,Seitenbereich");
    auto result_set(db_connection_select.getLastResultSet());
    while (const auto row = result_set.getNextRow()) {
        const DbEntry db_entry(row["Jahr"], row["Band"], row["Heft"], row["Seitenbereich"]);
        const auto key(row["Zeder_ID"] + "+" + row["PPN"]);
        auto key_and_entry(existing_entries->find(key));
        if (key_and_entry == existing_entries->end() or db_entry.isNewerThan(key_and_entry->second))
            (*existing_entries)[key] = db_entry;
    }

    return existing_entries->size();
}


// \return True if "test_entry" either does not exist in the databse or is newer than the newest existing entry.
bool NewerThanWhatExistsInDB(const std::unordered_map<std::string, DbEntry> &existing_entries,
                             const std::string &zeder_id, const std::string &ppn, const DbEntry &test_entry)
{
    const auto key_and_entry(existing_entries.find(zeder_id + "+" + ppn));
    if (key_and_entry == existing_entries.cend())
        return true;

    return test_entry.isNewerThan(key_and_entry->second);
}


const size_t MAX_ISSUE_DATABASE_LENGTH = 8; // maximum length of the issue field in the mySQL datafield


void CollectMostRecentEntries(const IniFile &ini_file, MARC::Reader * const reader, MARC::Writer * const writer,
                              const std::string &system_type, const std::string &hostname,
                              const std::unordered_map<std::string, ZederIdAndPPNType> &ppns_to_zeder_ids_and_types_map,
                              std::unordered_map<std::string, DbEntry> * const ppns_to_most_recent_entries_map)
{
    std::unordered_map<std::string, DbEntry> existing_entries;
    LOG_INFO("Found " + std::to_string(GetExistingDbEntries(ini_file, hostname, system_type, &existing_entries))
             + " existing database entries.");

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

        // truncate in order to ensure that comparison with the database works
        issue = issue.substr(0, MAX_ISSUE_DATABASE_LENGTH);
        const DbEntry new_db_entry(year_as_string, volume, issue, pages);
        if (NewerThanWhatExistsInDB(existing_entries, zeder_id, superior_control_number, new_db_entry)) {
            const auto superior_control_number_and_entry(ppns_to_most_recent_entries_map->find(superior_control_number));
            if (superior_control_number_and_entry == ppns_to_most_recent_entries_map->end() or new_db_entry.isNewerThan(superior_control_number_and_entry->second))
                (*ppns_to_most_recent_entries_map)[superior_control_number] = new_db_entry;
        }
    }

    LOG_INFO("Processed " + std::to_string(total_count) + " MARC records and found "
             + std::to_string(ppns_to_most_recent_entries_map->size()) + " new entries to insert .");
}


void UpdateDatabase(const IniFile &ini_file, const std::string &system_type, const std::string &hostname,
                    const std::unordered_map<std::string, ZederIdAndPPNType> &ppns_to_zeder_ids_and_types_map,
                    const std::unordered_map<std::string, DbEntry> &ppns_to_most_recent_entries_map)
{
    const auto JOB_START_TIME(std::to_string(std::time(nullptr)));

    DbConnection db_connection_insert(ini_file, "DatabaseInsert");
    const unsigned SQL_INSERT_BATCH_SIZE(20);
    const std::vector<std::string> COLUMN_NAMES{ "timestamp", "Quellrechner", "Systemtyp", "Zeder_ID", "PPN_Typ",
                                                 "PPN", "Jahr", "Band", "Heft", "Seitenbereich" };
    std::vector<std::vector<std::optional<std::string>>> column_values;

    for (const auto &[ppn, db_entry] : ppns_to_most_recent_entries_map) {
        const auto ppn_and_zeder_id_and_ppn_type(ppns_to_zeder_ids_and_types_map.find(ppn));
        if (unlikely(ppn_and_zeder_id_and_ppn_type == ppns_to_zeder_ids_and_types_map.cend()))
            LOG_ERROR("this should *never* happen!");

        const std::vector<std::optional<std::string>> new_column_values{
            { /* timestamp */     JOB_START_TIME                                                  },
            { /* Quellrechner */  hostname                                                        },
            { /* Systemtyp */     system_type,                                                    },
            { /* Zeder_ID */      std::to_string(ppn_and_zeder_id_and_ppn_type->second.zeder_id_) },
            { /* PPN_Typ */       std::string(1, ppn_and_zeder_id_and_ppn_type->second.type_)     },
            { /* PPN */           ppn                                                             },
            { /* Jahr */          db_entry.jahr_                                                  },
            { /* Band */          db_entry.band_                                                  },
            { /* Heft */          db_entry.heft_                                                  },
            { /* Seitenbereich */ db_entry.seitenbereich_                                         },
        };
        column_values.emplace_back(new_column_values);

        if (column_values.size() == SQL_INSERT_BATCH_SIZE) {
            db_connection_insert.insertIntoTableOrDie("zeder.erschliessung", COLUMN_NAMES, column_values);
            column_values.clear();
        }
    }
    if (not column_values.empty())
            db_connection_insert.insertIntoTableOrDie("zeder.erschliessung", COLUMN_NAMES, column_values);

    LOG_INFO("Inserted " + std::to_string(ppns_to_most_recent_entries_map.size()) + " entries into Ingo's database.");
}


const std::string TEXT_FILE_DIRECTORY("/mnt/ZE020150/FID-Entwicklung/Zeder_Supervision");


void UpdateTextFiles(const std::unordered_map<std::string, ZederIdAndPPNType> &ppns_to_zeder_ids_and_types_map,
                     const std::unordered_map<std::string, DbEntry> &ppns_to_most_recent_entries_map)
{
    const auto DIRECTORY_PREFIX(TEXT_FILE_DIRECTORY + "/" + DnsUtil::GetHostname() + "/");
    if (not FileUtil::Exists(DIRECTORY_PREFIX))
        FileUtil::MakeDirectoryOrDie(DIRECTORY_PREFIX);

    std::unordered_set<std::string> updated_files;
    for (const auto &[ppn, db_entry] : ppns_to_most_recent_entries_map) {
        const auto ppns_and_zeder_id_and_type(ppns_to_zeder_ids_and_types_map.find(ppn));
        if (unlikely(ppns_and_zeder_id_and_type == ppns_to_zeder_ids_and_types_map.cend()))
            LOG_ERROR("Map lookup failed for \"" + ppn + "\"!");
        const auto output(FileUtil::OpenForAppendingOrDie(DIRECTORY_PREFIX
                                                          + std::to_string(ppns_and_zeder_id_and_type->second.zeder_id_)
                                                          + ".txt"));
        (*output) << db_entry.jahr_ << ',' << db_entry.band_ << ',' << db_entry.heft_ << db_entry.seitenbereich_ << '\n';
    }

    LOG_INFO("Updated " + std::to_string(updated_files.size()) + " file(s) under " + DIRECTORY_PREFIX + ".");
}


} // unnamed namespace


int Main(int argc, char *argv[]) {
    if (argc != 4)
        ::Usage("[--min-log-level=log_level] system_type marc_input marc_output\n"
                "\twhere \"system_type\" must be one of ixtheo|krimdok");

    std::string system_type;
    if (__builtin_strcmp("ixtheo", argv[1]) == 0)
        system_type = "ixtheo";
    else if (__builtin_strcmp("krimdok", argv[1]) == 0)
        system_type = "krimdok";
    else
        LOG_ERROR("system_type must be one of ixtheo|krimdok!");

    const auto ppns_to_zeder_ids_and_types_map(GetPPNsToZederIdsAndTypesMap(system_type));

    const IniFile ini_file;
    const auto HOSTNAME(DnsUtil::GetHostname());
    const auto marc_reader(MARC::Reader::Factory(argv[2]));
    const auto marc_writer(MARC::Writer::Factory(argv[3]));
    std::unordered_map<std::string, DbEntry> ppns_to_most_recent_entries_map;
    CollectMostRecentEntries(ini_file, marc_reader.get(), marc_writer.get(), system_type, HOSTNAME,
                             ppns_to_zeder_ids_and_types_map, &ppns_to_most_recent_entries_map);
    UpdateDatabase(ini_file, system_type, HOSTNAME, ppns_to_zeder_ids_and_types_map,
                   ppns_to_most_recent_entries_map);
    UpdateTextFiles(ppns_to_zeder_ids_and_types_map, ppns_to_most_recent_entries_map);

    return EXIT_SUCCESS;
}

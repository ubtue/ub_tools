/** \brief Updates Zeder (via Ingo's SQL database) w/ the last N issues of harvested articles for each journal.
 *  \author Dr. Johannes Ruscheinski (johannes.ruscheinski@uni-tuebingen.de)
 *
 *  \copyright 2018-2020 Universitätsbibliothek Tübingen.  All rights reserved.
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
#include <unordered_map>
#include <cstdlib>
#include "Compiler.h"
#include "DbConnection.h"
#include "DnsUtil.h"
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

    bool operator==(const DbEntry &rhs) const;
};


bool DbEntry::operator==(const DbEntry &rhs) const {
    return jahr_ == rhs.jahr_ and band_ == rhs.band_ and heft_ == rhs.heft_ and seitenbereich_ == rhs.seitenbereich_;
}


size_t GetExistingDbEntries(const IniFile &ini_file, const std::string &hostname, const std::string &system_type,
                            std::unordered_map<std::string, DbEntry> * const existing_entries)
{
    DbConnection db_connection_select(ini_file, "DatabaseSelect");
    db_connection_select.queryOrDie("SELECT MAX(timestamp),Zeder_ID,PPN,Jahr,Band,Heft,Seitenbereich"
                                    " FROM zeder.erschliessung WHERE Quellrechner='" + hostname + "' AND Systemtyp='"
                                    + system_type + "' GROUP BY Zeder_ID,PPN");
    auto result_set(db_connection_select.getLastResultSet());
    while (const auto row = result_set.getNextRow())
        (*existing_entries)[row["Zeder_ID"] + "+" + row["PPN"]] =
            DbEntry(row["Jahr"], row["Band"], row["Heft"], row["Seitenbereich"]);

    return existing_entries->size();
}


bool AlreadyPresentInDB(const std::unordered_map<std::string, DbEntry> &existing_entries,
                        const std::string &zeder_id, const std::string &ppn, const DbEntry &test_entry)
{
    const auto key_and_entry(existing_entries.find(zeder_id + "+" + ppn));
    if (key_and_entry == existing_entries.cend())
        return false;

    return key_and_entry->second == test_entry;
}


void ProcessRecords(MARC::Reader * const reader, MARC::Writer * const writer, const std::string &system_type,
                    const std::unordered_map<std::string, ZederIdAndPPNType> &ppns_to_zeder_ids_and_types_map)
{
    const auto JOB_START_TIME(std::to_string(std::time(nullptr)));
    const auto HOSTNAME(DnsUtil::GetHostname());
    IniFile ini_file;
    std::unordered_map<std::string, DbEntry> existing_entries;
    LOG_INFO("Found " + std::to_string(GetExistingDbEntries(ini_file, HOSTNAME, system_type, &existing_entries))
             + " existing database entries.");

    const std::vector<std::string> COLUMN_NAMES{ "timestamp", "Quellrechner", "Systemtyp", "Zeder_ID", "PPN_Typ",
                                                 "PPN", "Jahr", "Band", "Heft", "Seitenbereich" };
    std::vector<std::vector<std::optional<std::string>>> column_values;

    DbConnection db_connection_insert(ini_file, "DatabaseInsert");
    const unsigned SQL_INSERT_BATCH_SIZE(20);
    unsigned total_count(0), inserted_count(0);
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
        if (AlreadyPresentInDB(existing_entries, zeder_id, superior_control_number,
                               DbEntry(year_as_string, volume, issue, pages)))
            continue;

        const std::vector<std::optional<std::string>> new_column_values{
            { /* timestamp */     JOB_START_TIME          },
            { /* Quellrechner */  HOSTNAME                },
            { /* Systemtyp */     system_type,            },
            { /* Zeder_ID */      zeder_id                },
            { /* PPN_Typ */       ppn_type                },
            { /* PPN */           superior_control_number },
            { /* Jahr */          year_as_string          },
            { /* Band */          volume                  },
            { /* Heft */          issue                   },
            { /* Seitenbereich */ pages                   },
        };
        column_values.emplace_back(new_column_values);

        if (column_values.size() == SQL_INSERT_BATCH_SIZE) {
            db_connection_insert.insertIntoTableOrDie("zeder.erschliessung", COLUMN_NAMES, column_values);
            column_values.clear();
        }

        ++inserted_count;
    }
    if (not column_values.empty())
            db_connection_insert.insertIntoTableOrDie("zeder.erschliessung", COLUMN_NAMES, column_values);

    LOG_INFO("Processed " + std::to_string(total_count) + " records and inserted " + std::to_string(inserted_count)
             + " into Ingo's database.");
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

    const auto marc_reader(MARC::Reader::Factory(argv[2]));
    const auto marc_writer(MARC::Writer::Factory(argv[3]));
    ProcessRecords(marc_reader.get(), marc_writer.get(), system_type, ppns_to_zeder_ids_and_types_map);

    return EXIT_SUCCESS;
}

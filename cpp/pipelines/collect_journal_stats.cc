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
#include "UBTools.h"
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


struct ZederIdAndType {
    unsigned zeder_id_;
    char type_; // 'p' or 'e' for "print" or "electronic"
public:
    ZederIdAndType(const unsigned zeder_id, const char type)
        : zeder_id_(zeder_id), type_(type) { }
};


std::unordered_map<std::string, ZederIdAndType> GetPPNsToZederIdsAndTypesMap(const std::string &system_type) {
    std::unordered_map<std::string, ZederIdAndType> ppns_to_zeder_ids_and_types_map;


    const Zeder::SimpleZeder zeder(system_type == "ixtheo" ? Zeder::IXTHEO : Zeder::KRIMDOK, { "eppn", "pppn" });
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
            ppns_to_zeder_ids_and_types_map.emplace(print_ppn, ZederIdAndType(journal.getId(), 'p'));

        for (const auto &online_ppn : online_ppns)
            ppns_to_zeder_ids_and_types_map.emplace(online_ppn, ZederIdAndType(journal.getId(), 'e'));
    }

    LOG_INFO("downloaded information for " + std::to_string(included_journal_count) + " journal(s) from Zeder.");

    return ppns_to_zeder_ids_and_types_map;
}


bool SplitPageNumbers(const std::string &possibly_combined_pages, unsigned * const start_page, unsigned * const end_page) {
    if (StringUtil::ToUnsigned(possibly_combined_pages, start_page)) {
        *end_page = *start_page;
        return true;
    } else if (possibly_combined_pages.length() >= 2
               and possibly_combined_pages[possibly_combined_pages.length() - 1] == 'f')
    {
        if (not StringUtil::ToUnsigned(possibly_combined_pages.substr(0, possibly_combined_pages.length() - 1), start_page))
            return false;
        *end_page = *start_page + 1;
        return true;
    }

    // Common case, page range with a hypen.
    const auto first_hyphen_pos(possibly_combined_pages.find('-'));
    if (first_hyphen_pos == std::string::npos)
        return false;

    return StringUtil::ToUnsigned(possibly_combined_pages.substr(0, first_hyphen_pos), start_page)
           and StringUtil::ToUnsigned(possibly_combined_pages.substr(first_hyphen_pos + 1), end_page);
}


// \return the year as a small integer or 0 if we could not parse it.
unsigned short YearStringToShort(const std::string &year_as_string) {
    unsigned short year_as_unsigned_short;
    if (not StringUtil::ToUnsignedShort(year_as_string, &year_as_unsigned_short))
        return 0;
    return year_as_unsigned_short;
}

void ProcessRecords(MARC::Reader * const reader, MARC::Writer * const writer, const std::string &system_type,
                    const std::unordered_map<std::string, ZederIdAndType> &ppns_to_zeder_ids_and_types_map,
                    DbConnection * const db_connection)
{
    const auto zeder_flavour(system_type == "krimdok" ? Zeder::KRIMDOK : Zeder::IXTHEO);
    const auto JOB_START_TIME(std::to_string(std::time(nullptr)));
    const auto HOSTNAME(DnsUtil::GetHostname());
    const std::string ZEDER_URL_PREFIX(Zeder::GetFullDumpEndpointPath(zeder_flavour) + "#suche=Z%3D");

    unsigned total_count(0), inserted_count(0);
    while (const auto record = reader->read()) {
        ++total_count;
        writer->write(record); // For the next pipeline phase.

        const std::string superior_control_number(record.getSuperiorControlNumber());
        if (superior_control_number.empty())
            continue;

        const auto zeder_id_and_type(ppns_to_zeder_ids_and_types_map.find(superior_control_number));
        if (zeder_id_and_type == ppns_to_zeder_ids_and_types_map.cend())
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

        std::map<std::string, std::string> column_names_to_values_map{
            { "timestamp",     JOB_START_TIME                                                         },
            { "Quellrechner",  HOSTNAME                                                               },
            { "Systemtyp",     system_type,                                                           },
            { "Zeder_ID",      std::to_string(zeder_id_and_type->second.zeder_id_)                    },
            { "Zeder_URL",     ZEDER_URL_PREFIX + std::to_string(zeder_id_and_type->second.zeder_id_) },
            { "PPN_Typ",       std::string(1, zeder_id_and_type->second.type_)                        },
            { "PPN",           superior_control_number                                                },
            { "Jahr",          std::to_string(YearStringToShort(year))                                },
            { "Band",          volume                                                                 },
            { "Heft",          issue                                                                  },
            { "Seitenbereich", pages                                                                  }
        };

        unsigned start_page, end_page;
        if (SplitPageNumbers(pages, &start_page, &end_page)) {
            column_names_to_values_map["Startseite"] = StringUtil::ToString(start_page);
            column_names_to_values_map["Endseite"]   = StringUtil::ToString(end_page);
        }

        db_connection->insertIntoTableOrDie("zeder.erschliessung", column_names_to_values_map,
                                            DbConnection::DKB_REPLACE, "PPN='" + superior_control_number + "'");
        ++inserted_count;
    }

    LOG_INFO("Processed " + std::to_string(total_count) + " records and inserted " + std::to_string(inserted_count)
             + " into Ingo's database.");
}


} // unnamed namespace


int Main(int argc, char *argv[]) {
    if (argc != 4)
        ::Usage("[--min-log-level=log_level] system_type marc_input marc_output\n"
                "\twhere \"system_type\" must be one of ixtheo|krimdok");

    std::string system_type;
    if (__builtin_strcmp("ixtheo", argv[1]))
        system_type = "ixtheo";
    else if (__builtin_strcmp("krimdok", argv[1]))
        system_type = "krimdok";
    else
        LOG_ERROR("system_type must be one of ixtheo|krimdok|relbib!");

    const auto ppns_to_zeder_ids_and_types_map(GetPPNsToZederIdsAndTypesMap(system_type));

    IniFile ini_file;
    DbConnection db_connection(ini_file);

    const auto marc_reader(MARC::Reader::Factory(argv[1]));
    const auto marc_writer(MARC::Writer::Factory(argv[2]));
    ProcessRecords(marc_reader.get(), marc_writer.get(), system_type, ppns_to_zeder_ids_and_types_map, &db_connection);

    return EXIT_SUCCESS;
}

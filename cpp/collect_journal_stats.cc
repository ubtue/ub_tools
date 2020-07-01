/** \brief Updates Zeder (via Ingo's SQL database) w/ the last N issues of harvested articles for each journal.
 *  \author Dr. Johannes Ruscheinski (johannes.ruscheinski@uni-tuebingen.de)
 *
 *  \copyright 2018-2019 Universitätsbibliothek Tübingen.  All rights reserved.
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
#include "IniFile.h"
#include "MapUtil.h"
#include "MARC.h"
#include "SqlUtil.h"
#include "StringUtil.h"
#include "UBTools.h"
#include "util.h"
#include "Zeder.h"


namespace {


const std::string ZEDER_URL_PREFIX_TEMPLATE("http://www-ub.ub.uni-tuebingen.de/zeder/?instanz=%ZEDER_FLAVOUR%#suche=Z%3D");


// We expect value to consist of 3 parts separated by colons: Zeder ID, PPN type ("print" or "online") and title.
void SplitValue(const std::string &value, std::string * const zeder_id, std::string * const type, std::string * const title) {
    const auto first_colon_pos(value.find(':'));
    if (unlikely(first_colon_pos == std::string::npos))
        LOG_ERROR("colons are missing in: " + value);
    *zeder_id = value.substr(0, first_colon_pos);

    const auto second_colon_pos(value.find(':', first_colon_pos + 1));
    if (unlikely(second_colon_pos == std::string::npos))
        LOG_ERROR("2nd colon is missing in: " + value);
    *type = value.substr(first_colon_pos + 1, second_colon_pos - first_colon_pos - 1);
    if (*type == "print")
        *type = "p";
    else if (*type == "online")
        *type = "e";
    else
        LOG_ERROR("invalid PPN type in \"" + value + "\"! (Must be \"print\" or \"online\".)");

    *title = value.substr(second_colon_pos + 1);
}


// \return the year as a small integer or 0 if we could not parse it.
unsigned short YearStringToShort(const std::string &year_as_string) {
    unsigned short year_as_unsigned_short;
    if (not StringUtil::ToUnsignedShort(year_as_string, &year_as_unsigned_short))
        return 0;
    return year_as_unsigned_short;
}


void ProcessRecords(const Zeder::Flavour zeder_flavour,  MARC::Reader * const reader,
                    const std::unordered_map<std::string, std::string> &journal_ppn_to_type_and_title_map,
                    DbConnection * const db_connection)
{
    const auto JOB_START_TIME(std::to_string(std::time(nullptr)));
    const auto HOSTNAME(DnsUtil::GetHostname());
    const auto ZEDER_URL_PREFIX(StringUtil::ReplaceString(ZEDER_URL_PREFIX_TEMPLATE, "%ZEDER_FLAVOUR%",
                                                          (zeder_flavour == Zeder::IXTHEO ? "ixtheo" : "krim")));

    unsigned total_count(0), inserted_count(0);
    while (const auto record = reader->read()) {
        ++total_count;

        const std::string superior_control_number(record.getSuperiorControlNumber());
        if (superior_control_number.empty())
            continue;

        const auto journal_ppn_and_type_and_title(journal_ppn_to_type_and_title_map.find(superior_control_number));
        if (journal_ppn_and_type_and_title == journal_ppn_to_type_and_title_map.cend())
            continue;

        const auto _936_field(record.findTag("936"));
        if (_936_field == record.end())
            continue;

        std::string zeder_id, type, title;
        SplitValue(journal_ppn_and_type_and_title->second, &zeder_id, &type, &title);

        const std::string pages(_936_field->getFirstSubfieldWithCode('h'));
        std::string volume;
        std::string issue(_936_field->getFirstSubfieldWithCode('e'));
        if (issue.empty())
            issue = _936_field->getFirstSubfieldWithCode('d');
        else
            volume = _936_field->getFirstSubfieldWithCode('d');
        const std::string year(_936_field->getFirstSubfieldWithCode('j'));

        db_connection->insertIntoTableOrDie("zeder.erschliessung",
                                            {
                                                { "timestamp=",    "JOB_START_TIME"                        },
                                                { "Quellrechner=", HOSTNAME                                },
                                                { "Systemtyp",     "ixtheo",                               },
                                                { "Zeder_ID",      zeder_id                                },
                                                { "Zeder_URL",     ZEDER_URL_PREFIX + zeder_id             },
                                                { "PPN_Typ",       type                                    },
                                                { "PPN",           journal_ppn_and_type_and_title->first   },
                                                { "Jahr",          std::to_string(YearStringToShort(year)) },
                                                { "Band",          volume                                  },
                                                { "Heft",          issue                                   },
                                                { "Seitenbereich", pages                                   },
                                                { "N_Aufsaetze",   "1"                                     }
                                            });

        ++inserted_count;
    }

    LOG_INFO("Processed " + std::to_string(total_count) + " records and inserted " + std::to_string(inserted_count)
             + " record(s) into Ingo's database.");
}


} // unnamed namespace


int Main(int argc, char *argv[]) {
    if (argc != 3)
        ::Usage("[--min-log-level=log_level] zeder_flavour marc_titles_records\n"
                "\twhere \"zeder_flavour\" must be either IXTHEO or KRIMDOK.\n");

    const auto zeder_flavour(Zeder::ParseFlavour(argv[1]));

    std::unordered_map<std::string, std::string> journal_ppn_to_type_and_title_map;
    MapUtil::DeserialiseMap(UBTools::GetTuelibPath() + "zeder_ppn_to_title.map", &journal_ppn_to_type_and_title_map);

    IniFile ini_file;
    DbConnection db_connection(ini_file);

    const auto marc_reader(MARC::Reader::Factory(argv[2]));
    ProcessRecords(zeder_flavour, marc_reader.get(), journal_ppn_to_type_and_title_map, &db_connection);

    return EXIT_SUCCESS;
}

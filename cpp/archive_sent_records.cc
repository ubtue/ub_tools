/** \brief Utility for storing MARC records in our delivery history database.
 *  \author Dr. Johannes Ruscheinski (johannes.ruscheinski@uni-tuebingen.de)
 *
 *  \copyright 2018 Universitätsbibliothek Tübingen.  All rights reserved.
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
#include <unordered_set>
#include <cstdio>
#include <cstdlib>
#include "DbConnection.h"
#include "DbResultSet.h"
#include "DbRow.h"
#include "GzStream.h"
#include "IniFile.h"
#include "MARC.h"
#include "SqlUtil.h"
#include "StringUtil.h"
#include "util.h"


namespace {


[[noreturn]] void Usage() {
    ::Usage("marc_data");
}


// \return one of "print", "online" or "unknown".
std::string GetISSNType(const std::string &issn) {
    static std::unordered_set<std::string> print_issns, online_issns;
    static std::unique_ptr<IniFile> zts_harvester_conf;
    if (zts_harvester_conf == nullptr) {
        zts_harvester_conf.reset(new IniFile("zts_harvester.conf"));
        for (const auto &section : *zts_harvester_conf) {
            const auto print_issn(section.getString("print_issn", ""));
            if (not print_issn.empty())
                print_issns.emplace(print_issn);

            const auto online_issn(section.getString("online_issn", ""));
            if (not online_issn.empty())
                online_issns.emplace(online_issn);
        }
    }

    if (print_issns.find(issn) != print_issns.cend())
        return "print";
    if (online_issns.find(issn) != online_issns.cend())
        return "online";
    return "unknown";
}


void StoreRecords(DbConnection * const db_connection, MARC::Reader * const marc_reader) {
    unsigned record_count(0);

    std::string record_blob;

    while (const MARC::Record record = marc_reader->read()) {
        record_blob = record.toBinaryString();

        const std::string hash(StringUtil::ToHexString(MARC::CalcChecksum(record)));
        const std::string url(record.getFirstSubfieldValue("URL", 'a'));
        const std::string zeder_id(record.getFirstSubfieldValue("ZID", 'a'));
        const std::string journal_name(record.getFirstSubfieldValue("JOU", 'a'));
        const std::string main_title(record.getMainTitle());

        db_connection->queryOrDie("SELECT * FROM delivered_marc_records WHERE hash="
                                  + db_connection->escapeAndQuoteString(hash));
        auto existing_records_with_hash(db_connection->getLastResultSet());
        bool already_delivered(false);
        if (not existing_records_with_hash.empty()) {
            while (auto row = existing_records_with_hash.getNextRow()) {
                const auto existing_title(row["title"]), existing_url(row["url"]);
                if (existing_url == url) {
                    LOG_WARNING("hash+url collision - record already delivered! title: '" + existing_title + "'\n"
                                "hash: '" + existing_hash + "'\nurl: '" + existing_url + "'");
                    already_delivered = true;
                } else {
                    LOG_WARNING("hash collision - record already delivered! title: '" + existing_title + "'\n"
                                "hash: '" + existing_hash + "'\nurl: '" + existing_url + "'");
                    already_delivered = true;
                }
            }
        }

        if (already_delivered)
            continue;

        ++record_count;

        std::string publication_year, volume, issue, pages;
        const auto _936_field(record.getFirstField("936"));
        if (_936_field != record.end()) {
            const MARC::Subfields subfields(_936_field->getSubfields());
            if (subfields.hasSubfield('j'))
                publication_year = ",publication_year=" + db_connection->escapeAndQuoteString(subfields.getFirstSubfieldWithCode('j'));
            if (subfields.hasSubfield('d'))
                volume = ",volume=" + db_connection->escapeAndQuoteString(subfields.getFirstSubfieldWithCode('d'));
            if (subfields.hasSubfield('e'))
                issue = ",issue=" + db_connection->escapeAndQuoteString(subfields.getFirstSubfieldWithCode('e'));
            if (subfields.hasSubfield('h'))
                pages = ",pages=" + db_connection->escapeAndQuoteString(subfields.getFirstSubfieldWithCode('h'));
        }

        std::string resource_type("unknown");
        const auto issns(record.getISSNs());
        for (const auto &issn : issns) {
            const auto issn_type(GetISSNType(issn));
            if (issn_type != "unknown") {
                resource_type = issn_type;
                break;
            }
        }

        db_connection->queryOrDie("INSERT INTO delivered_marc_records SET url=" + db_connection->escapeAndQuoteString(url)
                                  + ",zeder_id=" + db_connection->escapeAndQuoteString(zeder_id) + ",journal_name="
                                  + db_connection->escapeAndQuoteString(journal_name) +  ",hash="
                                  + db_connection->escapeAndQuoteString(hash) + ",main_title="
                                  + db_connection->escapeAndQuoteString(SqlUtil::TruncateToVarCharMaxLength(main_title))
                                  + publication_year + volume + issue + pages + ",resource_type='" + resource_type + "',record="
                                  + db_connection->escapeAndQuoteString(GzStream::CompressString(record_blob, GzStream::GZIP)));

        db_connection->queryOrDie("SELECT * FROM delivered_marc_records_superior_info WHERE zeder_id="
                                  + db_connection->escapeAndQuoteString(zeder_id));

        if (db_connection->getLastResultSet().empty()) {
            const std::string superior_title(record.getSuperiorTitle());
            const auto superior_control_number(record.getSuperiorControlNumber());
            const std::string superior_control_number_sql(superior_control_number.empty() ? "" : ",control_number="
                                                    + db_connection->escapeAndQuoteString(superior_control_number));

            db_connection->queryOrDie("INSERT INTO delivered_marc_records_superior_info SET zeder_id="
                                      + db_connection->escapeAndQuoteString(zeder_id) + ",title="
                                      + db_connection->escapeAndQuoteString(SqlUtil::TruncateToVarCharMaxLength(superior_title))
                                      + superior_control_number_sql);
        }

        record_blob.clear();
    }

    std::cout << "Stored " << record_count << " MARC record(s).\n";
}


} // unnamed namespace


int Main(int argc, char *argv[]) {
    if (argc != 2)
        Usage();

    DbConnection db_connection;
    auto marc_reader(MARC::Reader::Factory(argv[1]));
    StoreRecords(&db_connection, marc_reader.get());

    return EXIT_SUCCESS;
}

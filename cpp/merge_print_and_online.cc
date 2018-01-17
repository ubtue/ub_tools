/** \brief Utility for merging print and online editions into single records.
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
#include <set>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <cstdlib>
#include "DbConnection.h"
#include "DbResultSet.h"
#include "FileUtil.h"
#include "MARC.h"
#include "StringUtil.h"
#include "util.h"
#include "VuFind.h"


namespace {


void Usage() __attribute__((noreturn));


void Usage() {
    std::cerr << "Usage: " << ::progname << " marc_input marc_output missing_ppn_partners_list\n";
    std::exit(EXIT_FAILURE);
}


// Extracts mappings between print and electronic versions of records.  The order is arbitrary as we take the first record we
// find in the pair.  IOW, the first record becomes the new merged record.
void CollectMappings(MARC::Reader * const marc_reader, std::unordered_map<std::string, std::string> * const ppn_to_ppn_map,
                     std::unordered_set<std::string> * const merged_ppns)
{
    while (const MARC::Record record = marc_reader->read()) {
        for (const auto &field : record.getTagRange("776")) {
            const MARC::Subfields _776_subfields(field.getSubfields());
            if (_776_subfields.getFirstSubfieldWithCode('i') == "Erscheint auch als") {
                for (const auto &w_subfield : _776_subfields.extractSubfields('w')) {
                    if (StringUtil::StartsWith(w_subfield, "(DE-576)")) {
                        const std::string other_ppn(w_subfield.substr(__builtin_strlen("(DE-576)")));
                        if (merged_ppns->find(other_ppn) == merged_ppns->end()) {
                            if (unlikely(record.getBibliographicLevel() != 's'))
                                logger->warning("record with PPN " + record.getControlNumber() + " is not a serial!");
                            merged_ppns->emplace(record.getControlNumber());
                            (*ppn_to_ppn_map)[other_ppn] = record.getControlNumber();
                        }
                    }
                }
            }
        }
    }

    std::cout << "Found " << ppn_to_ppn_map->size() << " superior records that we may be able to merge.\n";
}


// Make inferior works point to the new merged superior parent found in "ppn_to_ppn_map".
bool PatchUplink(MARC::Record * const record, const std::unordered_map<std::string, std::string> &ppn_to_ppn_map) {
    static const std::set<std::string> UPLINK_TAGS{ "800", "810", "830", "773", "776" };

    bool patched(false);
    for (auto field : *record) {
        if (UPLINK_TAGS.find(field.getTag().to_string()) != UPLINK_TAGS.cend()) {
            MARC::Subfields subfields(field.getSubfields());
            auto subfield_w(std::find_if(subfields.begin(), subfields.end(),
                                         [](const MARC::Subfield &subfield) -> bool { return subfield.code_ == 'w'; }));
            if (subfield_w == subfields.end())
                continue;
            if (not StringUtil::StartsWith(subfield_w->value_, "(DE-576)"))
                continue;
            const std::string uplink_ppn(subfield_w->value_.substr(__builtin_strlen("(DE-576)")));
            const auto uplink_ppns(ppn_to_ppn_map.find(uplink_ppn));
            if (uplink_ppns == ppn_to_ppn_map.end())
                continue;

            // If we made it here, we need to replace the uplink PPN:
            subfield_w->value_ = "(DE-576)" + uplink_ppns->second;
            field.setContents(std::string(1, field.getIndicator1()) + std::string(1, field.getIndicator2())
                              + subfields.toString());
            patched = true;
        }
    }

    return patched;
}


void ProcessRecords(MARC::Reader * const marc_reader, MARC::Writer * const marc_writer, File * const missing_partners,
                    const std::unordered_map<std::string, std::string> &ppn_to_ppn_map,
                    const std::unordered_set<std::string> &merged_ppns)
{
    unsigned record_count(0), dropped_count(0), augmented_count(0);
    std::unordered_set<std::string> found_partners;
    while (MARC::Record record = marc_reader->read()) {
        ++record_count;
        if (ppn_to_ppn_map.find(record.getControlNumber()) != ppn_to_ppn_map.cend()) {
            ++dropped_count;
            found_partners.emplace(record.getControlNumber());
            continue;
        }

        if (merged_ppns.find(record.getControlNumber()) != merged_ppns.cend()) {
            // Mark the record as being both "print" as well as "electronic":
            record.insertField("ZWI", { { 'a', "1" } });
            ++augmented_count;
        } else if (PatchUplink(&record, ppn_to_ppn_map))
            ++augmented_count;

        marc_writer->write(record);
    }

    unsigned missing_partner_count(0);
    for (const auto &ppn_and_ppn : ppn_to_ppn_map) {
        if (found_partners.find(ppn_and_ppn.first) == found_partners.cend()) {
            missing_partners->write(ppn_and_ppn.first + "\n");
            ++missing_partner_count;
        }
    }

    std::cout << "Data set contained " << record_count << " MARC record(s).\n";
    std::cout << "Dropped " << dropped_count << " MARC record(s).\n";
    std::cout << "Augmented " << augmented_count << " MARC record(s).\n";
    std::cout << "Wrote " << missing_partner_count << " missing partner PPN('s) to \"" << missing_partners->getPath() << "\".\n";
}


// Here we update subscriptions.  There are 3 possible cases for each user and mapped PPN:
// 1. The trivial case where no subscriptions exist for a mapped PPN.
// 2. A subscription only exists for the mapped PPN.
//    In this case we only have to swap the PPN for the subscription.
// 3. Subscriptions exist for both, electronic and print PPNs.
//    Here we have to delete the subscription for the mapped PPN and ensure that the max_last_modification_time of the
//    remaining subscription is the minimum of the two previously existing subscriptions.
void PatchSerialSubscriptions(const std::unordered_map<std::string, std::string> &ppn_to_ppn_map) {
    std::string mysql_url;
    VuFind::GetMysqlURL(&mysql_url);
    DbConnection db_connection(mysql_url);

    std::unordered_set<std::string> replaced_ppns;
    for (const auto &ppn_and_ppn : ppn_to_ppn_map) {
        db_connection.queryOrDie("FROM ixtheo_journal_subscriptions SELECT id WHERE journal_control_number='" + ppn_and_ppn.first
                                 + "'");
        DbResultSet id_result_set(db_connection.getLastResultSet());
        if (id_result_set.empty())
            continue;

        while (const DbRow id_row = id_result_set.getNextRow()) {
            const std::string user_id(id_row["id"]);
            db_connection.queryOrDie("FROM ixtheo_journal_subscriptions SELECT journal_control_number,max_last_modification_time "
                                     "WHERE id='" + user_id + "'");
            DbResultSet control_number_result_set(db_connection.getLastResultSet());
            if (control_number_result_set.empty()) {
                db_connection.queryOrDie("UPDATE ixtheo_journal_subscriptions SET journal_control_number='"
                                         + ppn_and_ppn.second + "' WHERE id='" + user_id + "' AND journal_control_number='"
                                         + ppn_and_ppn.first + "'");
                continue;
            }

            //
            // If we get here we have subscriptions for both, the electronic and the print serial and need to merge them.
            //

            const DbRow control_number_row(control_number_result_set.getNextRow());
            db_connection.queryOrDie("FROM ixtheo_journal_subscriptions SELECT max_last_modification_time WHERE "
                                     "journal_control_number='" + ppn_and_ppn.second + "' AND id='" + user_id + "'");
            DbResultSet max_last_modification_time_result_set(db_connection.getLastResultSet());
            if (unlikely(max_last_modification_time_result_set.empty()))
                logger->error("this should *never* happen!");
            const DbRow max_last_modification_time_row(max_last_modification_time_result_set.getNextRow());
            const std::string min_max_last_modification_time(
                (max_last_modification_time_row["max_last_modification_time"] < control_number_row["max_last_modification_time"])
                    ? max_last_modification_time_row["max_last_modification_time"]
                    : control_number_row["max_last_modification_time"]);
            db_connection.queryOrDie("DELETE FROM ixtheo_journal_subscriptions WHERE journal_control_number='"
                                     + ppn_and_ppn.first + "' and id='" + user_id + "'");
            if (max_last_modification_time_row["max_last_modification_time"] > min_max_last_modification_time)
                db_connection.queryOrDie("UPDATE ixtheo_journal_subscriptions SET max_last_modification_time='"
                                         + min_max_last_modification_time + "' WHERE journal_control_number='"
                                         + ppn_and_ppn.second + "' and id='" + user_id + "'");
        }
    }
}


} // unnamed namespace


int main(int argc, char *argv[]) {
    ::progname = argv[0];

    if (argc != 4)
        Usage();

    std::unique_ptr<MARC::Reader> marc_reader(MARC::Reader::Factory(argv[1]));
    std::unique_ptr<MARC::Writer> marc_writer(MARC::Writer::Factory(argv[2]));
    std::unique_ptr<File> missing_partners(FileUtil::OpenOutputFileOrDie(argv[3]));

    try {
        std::unordered_map<std::string, std::string> ppn_to_ppn_map;
        std::unordered_set<std::string> merged_ppns;
        CollectMappings(marc_reader.get(), &ppn_to_ppn_map, &merged_ppns);
        marc_reader->rewind();
        ProcessRecords(marc_reader.get(), marc_writer.get(), missing_partners.get(), ppn_to_ppn_map, merged_ppns);
        PatchSerialSubscriptions(ppn_to_ppn_map);
    } catch (const std::exception &e) {
        logger->error("Caught exception: " + std::string(e.what()));
    }
}

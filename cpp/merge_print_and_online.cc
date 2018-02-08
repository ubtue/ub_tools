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


// Creates a map from the PPN "partners" to the offsets of the original records that had the links to the partners.
void CollectMappings(MARC::Reader * const marc_reader,
                     std::unordered_map<std::string, off_t> * const control_number_to_offset_map,
                     std::unordered_map<std::string, std::string> * const ppn_to_ppn_map)
{
    std::unordered_set<std::string> all_ppns;
    off_t last_offset(marc_reader->tell());
    while (const MARC::Record record = marc_reader->read()) {
        all_ppns.emplace(record.getControlNumber());
        for (const auto &field : record.getTagRange("776")) {
            const MARC::Subfields _776_subfields(field.getSubfields());
            if (_776_subfields.getFirstSubfieldWithCode('i') == "Erscheint auch als") {
                for (const auto &w_subfield : _776_subfields.extractSubfields('w')) {
                    if (StringUtil::StartsWith(w_subfield, "(DE-576)")) {
                        const std::string other_ppn(w_subfield.substr(__builtin_strlen("(DE-576)")));
                        if (unlikely(record.getBibliographicLevel() != 's'))
                            WARNING("record with PPN " + record.getControlNumber() + " is not a serial!");
                        else {
                            (*control_number_to_offset_map)[other_ppn] = last_offset;
                            (*ppn_to_ppn_map)[other_ppn] = record.getControlNumber();
                        }
                    }
                }
            }
        }

        last_offset = marc_reader->tell();
    }

    unsigned no_partner_count(0);
    for (auto &control_number_and_offset : *control_number_to_offset_map) {
        if (all_ppns.find(control_number_and_offset.first) == all_ppns.end()) {
            ++no_partner_count;
            control_number_to_offset_map->erase(control_number_and_offset.first);
        }
    }

    std::cout << "Found " << control_number_to_offset_map->size() << " superior record(s) that we may be able to merge.\n";
    std::cout << "Found " <<  no_partner_count << " superior record(s) that have missing \"partners\".\n";
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


// The strategy we emply here is that we just pick "contents1" unless we have an identical subfield structure.
std::string MergeFieldContents(const bool is_control_field, const std::string &contents1, const bool record1_is_electronic,
                               const std::string &contents2, const bool record2_is_electronic)
{
    if (is_control_field) // We don't really know what to do here!
        return contents1;

    const MARC::Subfields subfields1(contents1);
    std::string subfield_codes1;
    for (const auto &subfield : subfields1)
        subfield_codes1 += subfield.code_;

    const MARC::Subfields subfields2(contents2);
    std::string subfield_codes2;
    for (const auto &subfield : subfields2)
        subfield_codes2 += subfield.code_;

    if (subfield_codes1 != subfield_codes2) // We are up the creek!
        return contents1;

    MARC::Subfields merged_subfields;
    for (auto subfield1(subfields1.begin()), subfield2(subfields2.begin()); subfield1 != subfields1.end();
         ++subfield1, ++subfield2)
    {
        if (subfield1->value_ == subfield2->value_)
            merged_subfields.addSubfield(subfield1->code_, subfield1->value_);
        else {
            std::string merged_value(subfield1->value_);
            merged_value += " (";
            merged_value += record1_is_electronic ? "electronic" : "print";
            merged_value += "); ";
            merged_value += subfield2->value_;
            merged_value += " (";
            merged_value += record2_is_electronic ? "electronic" : "print";
            merged_value += ')';
            merged_subfields.addSubfield(subfield1->code_, merged_value);
        }
    }

    return merged_subfields.toString();
}


MARC::Record MergeRecords(MARC::Record &record1, MARC::Record &record2) {
    MARC::Record merged_record(record1.getLeader());

    const auto record1_end_or_lok_start(record1.getFirstField("LOK"));
    record1.sortFields(record1.begin(), record1_end_or_lok_start);
    auto record1_field(record1.begin());

    const auto record2_end_or_lok_start(record2.getFirstField("LOK"));
    record2.sortFields(record1.begin(), record2_end_or_lok_start);
    auto record2_field(record2.begin());

    while (record1_field != record1_end_or_lok_start and record2_field != record2_end_or_lok_start) {
        if (record1_field->getTag() == record2_field->getTag() and not MARC::IsRepeatableField(record1_field->getTag())) {
            merged_record.appendField(record1_field->getTag(),
                                      MergeFieldContents(record1_field->getTag().isTagOfControlField(),
                                                         record1_field->getContents(), record1.isElectronicResource(),
                                                         record2_field->getContents(), record2.isElectronicResource()),
                                      record1_field->getIndicator1(), record1_field->getIndicator2());
            ++record1_field, ++record2_field;
        } else if (*record1_field < *record2_field) {
            merged_record.appendField(*record1_field);
            ++record1_field;
        } else if (*record2_field < *record1_field) {
            merged_record.appendField(*record2_field);
            ++record2_field;
        } else { // Both fields are identical => just take any one of them.
            merged_record.appendField(*record1_field);
            ++record1_field, ++record2_field;
        }
    }

    // Append local data, if we have any:
    if (record1_end_or_lok_start != record1.end()) {
        for (record1_field = record1_end_or_lok_start; record1_field != record1.end(); ++record1_field)
            merged_record.appendField(*record1_field);
    } else if (record2_end_or_lok_start != record2.end()) {
        for (record2_field = record2_end_or_lok_start; record2_field != record2.end(); ++record2_field)
            merged_record.appendField(*record2_field);
    }

    // Mark the record as being both "print" as well as "electronic":
    merged_record.insertField("ZWI", { { 'a', "1" } });

    return merged_record;
}


MARC::Record ReadRecordFromOffsetOrDie(MARC::Reader * const marc_reader, const off_t offset) {
    if (unlikely(not marc_reader->seek(offset)))
        ERROR("can't seek to offset " + std::to_string(offset) + "!");
    MARC::Record record(marc_reader->read());
    if (unlikely(not record))
        ERROR("failed to read a record from offset " + std::to_string(offset) + "!");

    return record;
}


void ProcessRecords(MARC::Reader * const marc_reader, MARC::Writer * const marc_writer, File * const missing_partners,
                    const std::unordered_map<std::string, off_t> &control_number_to_offset_map,
                    const std::unordered_map<std::string, std::string> &ppn_to_ppn_map)
{
    unsigned record_count(0), merged_count(0), augmented_count(0);
    while (MARC::Record record = marc_reader->read()) {
        ++record_count;

        const auto control_number_and_offset(control_number_to_offset_map.find(record.getControlNumber()));
        if (control_number_and_offset != control_number_to_offset_map.end()) {
            MARC::Record record2(ReadRecordFromOffsetOrDie(marc_reader, control_number_and_offset->second));
            record = MergeRecords(record, record2);
            ++merged_count;
        } else if (PatchUplink(&record, ppn_to_ppn_map))
            ++augmented_count;

        marc_writer->write(record);
    }

    // Process records for which we are missing the partner:
    const unsigned missing_partner_count(control_number_to_offset_map.size());
    for (const auto &control_number_and_offset : control_number_to_offset_map) {
        missing_partners->write(control_number_and_offset.first + "\n");
        const MARC::Record record(ReadRecordFromOffsetOrDie(marc_reader, control_number_and_offset.second));
        marc_writer->write(record);
    }

    std::cout << "Data set contained " << record_count << " MARC record(s).\n";
    std::cout << "Merged " << merged_count << " MARC record(s).\n";
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
void PatchSerialSubscriptions(DbConnection * connection, const std::unordered_map<std::string, std::string> &ppn_to_ppn_map) {
    for (const auto &ppn_and_ppn : ppn_to_ppn_map) {
        connection->queryOrDie("SELECT id,max_last_modification_time FROM ixtheo_journal_subscriptions WHERE "
                               "journal_control_number='" + ppn_and_ppn.first + "'");
        DbResultSet ppn_first_result_set(connection->getLastResultSet());
        while (const DbRow ppn_first_row = ppn_first_result_set.getNextRow()) {
            const std::string user_id(ppn_first_row["id"]);
            connection->queryOrDie("SELECT max_last_modification_time FROM ixtheo_journal_subscriptions "
                                   "WHERE id='" + user_id + "' AND journal_control_number='" + ppn_and_ppn.second + "'");
            DbResultSet ppn_second_result_set(connection->getLastResultSet());
            if (ppn_second_result_set.empty()) {
                connection->queryOrDie("UPDATE ixtheo_journal_subscriptions SET journal_control_number='"
                                       + ppn_and_ppn.second + "' WHERE id='" + user_id + "' AND journal_control_number='"
                                       + ppn_and_ppn.first + "'");
                continue;
            }

            //
            // If we get here we have subscriptions for both, the electronic and the print serial and need to merge them.
            //

            const DbRow ppn_second_row(ppn_second_result_set.getNextRow());
            const std::string min_max_last_modification_time(
                (ppn_second_row["max_last_modification_time"] < ppn_first_row["max_last_modification_time"])
                    ? ppn_second_row["max_last_modification_time"]
                    : ppn_first_row["max_last_modification_time"]);
            connection->queryOrDie("DELETE FROM ixtheo_journal_subscriptions WHERE journal_control_number='"
                                   + ppn_and_ppn.first + "' and id='" + user_id + "'");
            if (ppn_first_row["max_last_modification_time"] > min_max_last_modification_time)
                connection->queryOrDie("UPDATE ixtheo_journal_subscriptions SET max_last_modification_time='"
                                       + min_max_last_modification_time + "' WHERE journal_control_number='"
                                       + ppn_and_ppn.second + "' and id='" + user_id + "'");
        }
    }
}


void PatchPDASubscriptions(DbConnection * connection, const std::unordered_map<std::string, std::string> &ppn_to_ppn_map) {
    for (const auto &ppn_and_ppn : ppn_to_ppn_map) {
        connection->queryOrDie("SELECT id FROM ixtheo_pda_subscriptions WHERE book_ppn='" + ppn_and_ppn.first + "'");
        DbResultSet result_set(connection->getLastResultSet());
        while (const DbRow row = result_set.getNextRow())
            connection->queryOrDie("UPDATE ixtheo_pda_subscriptions SET book_ppn='" + ppn_and_ppn.first + "' WHERE id='"
                                   + row["id"] + "' AND book_ppn='" + ppn_and_ppn.second + "'");
    }
}


void PatchResourceTable(DbConnection * connection, const std::unordered_map<std::string, std::string> &ppn_to_ppn_map) {
    for (const auto &ppn_and_ppn : ppn_to_ppn_map) {
        connection->queryOrDie("SELECT id FROM resource WHERE record_id='" + ppn_and_ppn.first + "'");
        DbResultSet result_set(connection->getLastResultSet());
        while (const DbRow row = result_set.getNextRow())
            connection->queryOrDie("UPDATE resource SET record_id='" + ppn_and_ppn.second + "' WHERE id=" + row["id"]);
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
        std::unordered_map<std::string, off_t> control_number_to_offset_map;
        std::unordered_map<std::string, std::string> ppn_to_ppn_map;
        CollectMappings(marc_reader.get(), &control_number_to_offset_map, &ppn_to_ppn_map);
        marc_reader->rewind();
        ProcessRecords(marc_reader.get(), marc_writer.get(), missing_partners.get(), control_number_to_offset_map,
                       ppn_to_ppn_map);

        std::string mysql_url;
        VuFind::GetMysqlURL(&mysql_url);
        DbConnection db_connection(mysql_url);
        PatchSerialSubscriptions(&db_connection, ppn_to_ppn_map);
        PatchPDASubscriptions(&db_connection, ppn_to_ppn_map);
        PatchResourceTable(&db_connection, ppn_to_ppn_map);
    } catch (const std::exception &e) {
        ERROR("Caught exception: " + std::string(e.what()));
    }
}

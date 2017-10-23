/** \file    find_tue_dups.cc
 *  \brief   Find duplicates amongst the libraries of institutions associated w/ the University of Tübingen.
 *  \author  Dr. Johannes Ruscheinski
 */

/*
    Copyright (C) 2017, Library of the University of Tübingen

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU Affero General Public License as
    published by the Free Software Foundation, either version 3 of the
    License, or (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Affero General Public License for more details.

    You should have received a copy of the GNU Affero General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <algorithm>
#include <iostream>
#include <set>
#include <unordered_set>
#include <cstring>
#include "Compiler.h"
#include "FileUtil.h"
#include "JSON.h"
#include "TextUtil.h"
#include "MarcReader.h"
#include "MarcRecord.h"
#include "RegexMatcher.h"
#include "util.h"


void Usage() {
    std::cerr << "Usage: " << ::progname << " marc_input\n";
    std::cerr << "       Please note that this program requires an input MARC format as provided by\n";
    std::cerr << "       the team at the University of Freiburg!\n\n";
    std::exit(EXIT_FAILURE);
}


void ExtractSubfield(const MarcRecord &record, const std::string &tag, const char subfield_code,
                     std::set<std::string> * const extracted_values)
{
    std::vector<size_t> field_indices;
    record.getFieldIndices(tag, &field_indices);
    for (const size_t field_index : field_indices) {
        const std::string subfield_a(record.extractFirstSubfield(field_index, subfield_code));
        if (likely(not subfield_a.empty()))
            extracted_values->emplace(subfield_a);
    }
}


std::string ExtractZDBNumber(const MarcRecord &record) {
    // First we try our luck w/ 016...
    std::vector<size_t> field_indices;
    record.getFieldIndices("016", &field_indices);
    for (const size_t field_index : field_indices) {
        const std::string field_contents(record.getFieldData(field_index));
        const Subfields subfields(field_contents);
        if (subfields.getFirstSubfieldValue('2') == "DE-600")
            return subfields.getFirstSubfieldValue('a');
    }

    // ...and then we try our luck w/ 035:
    record.getFieldIndices("035", &field_indices);
    for (const size_t field_index : field_indices) {
        const std::string field_contents(record.getFieldData(field_index));
        const Subfields subfields(field_contents);
        const std::string subfield_a(subfields.getFirstSubfieldValue('a'));
        if (StringUtil::StartsWith(subfield_a, "(DE-599)ZDB"))
            return subfield_a.substr(11);
    }

    return "";
}


void ExtractISSNs(const MarcRecord &record, std::set<std::string> * const issns_and_isbns) {
    ExtractSubfield(record, "022", 'a', issns_and_isbns);
    ExtractSubfield(record, "440", 'x', issns_and_isbns);
    ExtractSubfield(record, "490", 'x', issns_and_isbns);
    ExtractSubfield(record, "730", 'x', issns_and_isbns);
    ExtractSubfield(record, "773", 'x', issns_and_isbns);
    ExtractSubfield(record, "776", 'x', issns_and_isbns);
    ExtractSubfield(record, "780", 'x', issns_and_isbns);
    ExtractSubfield(record, "785", 'x', issns_and_isbns);
}


void ExtractISBNs(const MarcRecord &record, std::set<std::string> * const issns_and_isbns) {
    ExtractSubfield(record, "020", 'a', issns_and_isbns);
    ExtractSubfield(record, "773", 'a', issns_and_isbns);
}


std::string ExtractInventory(const std::string &_910_subfield_a) {
    if (_910_subfield_a.empty())
        return "";

    JSON::JSONNode *tree_root(nullptr);
    try {
        JSON::Parser json_parser(_910_subfield_a);
        if (not (json_parser.parse(&tree_root)))
            logger->error("in ExtractInventory: failed to parse returned JSON: " + json_parser.getErrorMessage()
                  + "(input was: " + StringUtil::CStyleEscape(_910_subfield_a) + ")");

        if (tree_root->getType() != JSON::JSONNode::OBJECT_NODE)
            logger->error("in ExtractInventory: expected an object node!");
        const JSON::ObjectNode * const object(reinterpret_cast<const JSON::ObjectNode * const>(tree_root));

        const JSON::JSONNode * const inventory_node(object->getValue("bestand8032"));
        if (inventory_node == nullptr)
            return "";
        if (inventory_node->getType() != JSON::JSONNode::STRING_NODE)
            logger->error("in ExtractInventory: expected a string node!");
        return reinterpret_cast<const JSON::StringNode * const>(inventory_node)->getValue();

        delete tree_root;
    } catch (...) {
        delete tree_root;
        throw;
    }
}


enum SupplementaryInfoType { SIGNATURE, INVENTORY };
const std::set<std::string> SIGILS_OF_INTEREST{ "21", "21-24", "21-108", "21-31", "21-35" };


// \return The number of occurrences of an object in Tübingen.
unsigned FindTueSigilsAndSignaturesOrInventory(const MarcRecord &record,
                                               const SupplementaryInfoType supplementary_info_type,
                                               std::string * const ub_signatures_or_inventory,
                                               std::string * const non_ub_sigils_and_inventory)
{
    unsigned occurrence_count(0);
    std::vector<size_t> _910_indices;
    record.getFieldIndices("910", &_910_indices);
    for (const size_t _910_index : _910_indices) {
        const std::string _910_field_contents(record.getFieldData(_910_index));
        if (_910_field_contents.empty())
            continue;
        const Subfields subfields(_910_field_contents);
        const std::string sigil(subfields.getFirstSubfieldValue('c'));

        std::vector<std::string> signatures;
        std::string inventory;
        if (supplementary_info_type == SIGNATURE)
            subfields.extractSubfields('d', &signatures);
        else
            inventory = ExtractInventory(subfields.getFirstSubfieldValue('a'));

        const bool is_monograph(record.getLeader().isMonograph());
        if (is_monograph and sigil != "21")
            continue;
        if (SIGILS_OF_INTEREST.find(sigil) == SIGILS_OF_INTEREST.cend())
            continue;

        ++occurrence_count;
        if (sigil == "21") { // UB
            if (supplementary_info_type == SIGNATURE) {
                if (not ub_signatures_or_inventory->empty())
                    ub_signatures_or_inventory->append(",");
                ub_signatures_or_inventory->append(StringUtil::Join(signatures, ", "));
            } else if (not inventory.empty()) { // Assume supplementary_info_type == INVENTORY.
                if (not ub_signatures_or_inventory->empty())
                    ub_signatures_or_inventory->append(",");
                ub_signatures_or_inventory->append(inventory);
            }
        } else if (not inventory.empty()) { // Assume supplementary_info_type == INVENTORY.
            if (not non_ub_sigils_and_inventory->empty())
                non_ub_sigils_and_inventory->append(", ");
            non_ub_sigils_and_inventory->append(sigil + ':' + inventory);
        } else { // Non-UB and we don't have inventory information.
            if (not non_ub_sigils_and_inventory->empty())
                non_ub_sigils_and_inventory->append(", ");
            non_ub_sigils_and_inventory->append(sigil);
        }
    }

    return occurrence_count;
}


// Extracts 084$a entries but only if 084$2 contains "zdbs".
std::string ExtractDDCGroups(const MarcRecord &record) {
    std::string ddc_groups;
    std::vector<size_t> _084_indices;
    record.getFieldIndices("084", &_084_indices);
    for (const size_t _084_index : _084_indices) {
        const std::string _084_field_contents(record.getFieldData(_084_index));
        if (_084_field_contents.empty())
            continue;
        const Subfields subfields(_084_field_contents);
        if (not subfields.hasSubfieldWithValue('2', "zdbs"))
            continue;
        const std::string a_contents(subfields.getFirstSubfieldValue('a'));
        if (not a_contents.empty()) {
            if (not ddc_groups.empty())
                ddc_groups += ';';
            ddc_groups += a_contents;
        }
    }

    return ddc_groups;
}


void WriteSerialEntry(File * const output, const std::string &target_sigil, const std::string &ppn,
                      const std::string &main_title, const std::set<std::string> &issns_and_isbns,
                      const std::string &area_or_zdb_number, const std::string &ub_signatures_or_inventory,
                      const std::string &non_ub_sigils_and_inventory, const std::string &ddc_groups)
{
    std::set<std::string> sigils_and_inventory_info;
    StringUtil::SplitThenTrimWhite(non_ub_sigils_and_inventory, ',', &sigils_and_inventory_info);
    std::set<std::string> target_sigils_and_inventory_info;
    for (const auto &sigil_and_inventory_info : sigils_and_inventory_info) {
        if (StringUtil::StartsWith(sigil_and_inventory_info, target_sigil))
            target_sigils_and_inventory_info.emplace(sigil_and_inventory_info);
    }
    if (target_sigils_and_inventory_info.empty())
        return;
    
    (*output) << '"' << ppn << "\",\"" << TextUtil::CSVEscape(main_title)
              << "\",\"" << TextUtil::CSVEscape(StringUtil::Join(issns_and_isbns, ','))
              << "\",\"" << TextUtil::CSVEscape(area_or_zdb_number)
              << "\",\"" << TextUtil::CSVEscape(ub_signatures_or_inventory)
              << "\",\"" << TextUtil::CSVEscape(StringUtil::Join(target_sigils_and_inventory_info, ','))
              << "\",\"" << TextUtil::CSVEscape(ddc_groups) << "\"\n";
}


bool IsProbablyAYear(const std::string &year_candidate) {
    if (year_candidate.length() != 4)
        return false;
    return StringUtil::IsDigit(year_candidate[0]) and StringUtil::IsDigit(year_candidate[1])
           and StringUtil::IsDigit(year_candidate[2]) and StringUtil::IsDigit(year_candidate[3]);
}


bool FindTueDups(const MarcRecord &record, File * const monos_csv, File * const juristisches_seminar_csv,
                 File * const brechtbau_bibliothek_csv, File * const evangelische_theologie_csv,
                 File * const katholische_theologie_csv)
{
    const bool is_monograph(record.getLeader().isMonograph());
    std::string ub_signatures_or_inventory, non_ub_sigils_and_inventory;
    if (FindTueSigilsAndSignaturesOrInventory(record, (is_monograph ? SIGNATURE : INVENTORY),
                                              &ub_signatures_or_inventory, &non_ub_sigils_and_inventory) < 2)
        return false; // Not a dupe.

    if (is_monograph) {
        if (ub_signatures_or_inventory.empty())
            return false;
    } else if (ub_signatures_or_inventory.empty() or non_ub_sigils_and_inventory.empty())
        return false;

    const std::string _008_contents(record.getFieldData("008"));
    std::string publication_year;
    if (likely(_008_contents.length() >= 11))
        publication_year = _008_contents.substr(7, 4);

    std::string area_or_zdb_number;
    if (is_monograph) {
        if (not IsProbablyAYear(publication_year) or (publication_year < "1960" or publication_year > "2010"))
            return false;

        const std::string _910_contents(record.getFieldData("910"));
        if (not _910_contents.empty()) {
            const Subfields subfields(_910_contents);
            area_or_zdb_number = subfields.getFirstSubfieldValue('j');
        }
    } else // SERIALS
        area_or_zdb_number = ExtractZDBNumber(record);

    const std::string _245_contents(record.getFieldData("245"));
    std::string main_title;
    if (not _245_contents.empty()) {
        const Subfields subfields(_245_contents);
        main_title = subfields.getFirstSubfieldValue('a');
    }

    std::set<std::string> issns_and_isbns;
    if (is_monograph)
        ExtractISBNs(record, &issns_and_isbns);
    else
        ExtractISSNs(record, &issns_and_isbns);

    const std::string ddc_groups(ExtractDDCGroups(record));

    if (is_monograph) {
        (*monos_csv) << '"' << record.getControlNumber() << "\",\"" << TextUtil::CSVEscape(main_title)
                     << "\",\"" << TextUtil::CSVEscape(StringUtil::Join(issns_and_isbns, ','))
                     << "\",\"" << TextUtil::CSVEscape(publication_year)
                     << "\",\"" << TextUtil::CSVEscape(area_or_zdb_number)
                     << "\",\"" << TextUtil::CSVEscape(ub_signatures_or_inventory)
                     << "\",\"" << TextUtil::CSVEscape(non_ub_sigils_and_inventory) << "\"\n";
    } else {
        WriteSerialEntry(juristisches_seminar_csv, "21-24", record.getControlNumber(), main_title, issns_and_isbns,
                         area_or_zdb_number, ub_signatures_or_inventory, non_ub_sigils_and_inventory, ddc_groups);
        WriteSerialEntry(brechtbau_bibliothek_csv, "21-108", record.getControlNumber(), main_title, issns_and_isbns,
                         area_or_zdb_number, ub_signatures_or_inventory, non_ub_sigils_and_inventory, ddc_groups);
        WriteSerialEntry(evangelische_theologie_csv, "21-31", record.getControlNumber(), main_title, issns_and_isbns,
                         area_or_zdb_number, ub_signatures_or_inventory, non_ub_sigils_and_inventory, ddc_groups);
        WriteSerialEntry(katholische_theologie_csv, "21-35", record.getControlNumber(), main_title, issns_and_isbns,
                         area_or_zdb_number, ub_signatures_or_inventory, non_ub_sigils_and_inventory, ddc_groups);
    }

    return true;
}


enum OutputSet { MONOGRAPHS, SERIALS };


void WriteHeader(File * const output, const OutputSet output_set) {
    (*output) << "\"PPN\"" << ",\"Titel\"" << (output_set == MONOGRAPHS ? ",\"ISBN\"" : ",\"ISSN\"")
              << (output_set == MONOGRAPHS ? ",\"Erscheinungsjahr\"" : "")
              << (output_set == MONOGRAPHS ? ",\"Fachgebiet\"" : ",\"ZDB-ID-Nummer\"")
              << (output_set == MONOGRAPHS ? ",\"UB - Signatur\"" : ",\"UB - Bestandsangabe\"")
              << (output_set == MONOGRAPHS ? ",\"Siegel der anderen besitzenden Bibliotheken" : ",\"Siegel+Bestand\"")
              << (output_set == SERIALS ? ",\"DDC-Sachgruppe\"" : "") << '\n';
}


void FindTueDups(MarcReader * const marc_reader) {
    std::unique_ptr<File> monos_csv(FileUtil::OpenOutputFileOrDie("monos.csv"));
    std::unique_ptr<File> juristisches_seminar_csv(FileUtil::OpenOutputFileOrDie("juristisches_seminar.csv"));
    std::unique_ptr<File> brechtbau_bibliothek_csv(FileUtil::OpenOutputFileOrDie("brechtbau_bibliothek.csv"));
    std::unique_ptr<File> evangelische_theologie_csv(FileUtil::OpenOutputFileOrDie("evangelische_theologie.csv"));
    std::unique_ptr<File> katholische_theologie_csv(FileUtil::OpenOutputFileOrDie("katholische_theologie.csv"));

    // Write the headers:
    WriteHeader(monos_csv.get(), MONOGRAPHS);
    WriteHeader(juristisches_seminar_csv.get(), SERIALS);
    WriteHeader(brechtbau_bibliothek_csv.get(), SERIALS);
    WriteHeader(evangelische_theologie_csv.get(), SERIALS);
    WriteHeader(katholische_theologie_csv.get(), SERIALS);

    unsigned count(0), control_number_dups_count(0), dups_count(0), monograph_count(0), serial_count(0),
             linked_ppn_count(0);
    std::unordered_set<std::string> previously_seen_ppns;
    while (const MarcRecord record = marc_reader->read()) {
        ++count;
        if (record.isElectronicResource())
            continue;

        const std::string ppn(record.getControlNumber());
        if (previously_seen_ppns.find(ppn) != previously_seen_ppns.cend()) {
            ++control_number_dups_count;
            logger->warning("found a duplicate control number: " + ppn);
            continue;
        } else
            previously_seen_ppns.insert(ppn);

        if (ppn.find('_') != std::string::npos) {
            ++linked_ppn_count;
            continue;
        }

        // Only consider monographs and serials:
        const Leader &leader(record.getLeader());
        if (not (leader.isMonograph() or leader.isSerial()))
            continue;

        if (FindTueDups(record, monos_csv.get(), juristisches_seminar_csv.get(), brechtbau_bibliothek_csv.get(),
                        evangelische_theologie_csv.get(), katholische_theologie_csv.get()))
        {
            ++dups_count;
            if (leader.isMonograph())
                ++monograph_count;
            else
                ++serial_count;
        }
    }
    std::cerr << "Processed " << count << " records and found " << dups_count << " dups (" << monograph_count
              << " monographs and " << serial_count << " serials).\n";
    std::cerr << "Found " << control_number_dups_count << " records w/ duplicate control numbers!\n";
    std::cerr << "Ignored " << linked_ppn_count << " records that have a control numbers w/ an underscore.\n";
}


int main(int argc, char **argv) {
    ::progname = argv[0];

    if (argc != 2)
        Usage();

    std::unique_ptr<MarcReader> marc_reader(MarcReader::Factory(argv[1], MarcReader::BINARY));

    try {
        FindTueDups(marc_reader.get());
    } catch (const std::exception &x) {
        logger->error("caught exception: " + std::string(x.what()));
    }
}

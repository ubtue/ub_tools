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
#include <cstring>
#include "Compiler.h"
#include "JSON.h"
#include "TextUtil.h"
#include "MarcRecord.h"
#include "RegexMatcher.h"
#include "util.h"
#include "XmlUtil.h"


void Usage() {
    std::cerr << "Usage: " << ::progname << " --output-set=(MONOGRAPHS|SERIALS) marc_input\n";
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
        JSON::Parser json_parser(XmlUtil::DecodeEntities(_910_subfield_a));
        if (not (json_parser.parse(&tree_root)))
            Error("in ExtractInventory: failed to parse returned JSON: " + json_parser.getErrorMessage());

        if (tree_root->getType() != JSON::JSONNode::OBJECT_NODE)
            Error("in ExtractInventory: expected an object node!");
        const JSON::ObjectNode * const object(reinterpret_cast<const JSON::ObjectNode * const>(tree_root));

        const JSON::JSONNode * const inventory_node(object->getValue("bestand8032"));
        if (inventory_node == nullptr)
            return "";
        if (inventory_node->getType() != JSON::JSONNode::STRING_NODE)
            Error("in ExtractInventory: expected a string node!");
        return reinterpret_cast<const JSON::StringNode * const>(inventory_node)->getValue();

        delete tree_root;
    } catch (...) {
        delete tree_root;
        throw;
    }
}


enum SupplementaryInfoType { SIGNATURE, INVENTORY };


// \return The number of occurrences of an object in Tübingen.
unsigned FindTueSigilsAndSignaturesOrInventory(const MarcRecord &record,
                                               const SupplementaryInfoType supplementary_info_type,
                                               std::string * const ub_signatures_or_inventory,
                                               std::string * const non_ub_sigils_and_signatures_or_inventory)
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

        if (not sigil.empty()) {
            ++occurrence_count;
            if (sigil == "21") { // UB
                if (supplementary_info_type == SIGNATURE) {
                    if (not ub_signatures_or_inventory->empty())
                        ub_signatures_or_inventory->append(",");
                    ub_signatures_or_inventory->append(StringUtil::Join(signatures, ','));
                } else if (not inventory.empty()) { // Assume supplementary_info_type == INVENTORY.
                    if (not ub_signatures_or_inventory->empty())
                        ub_signatures_or_inventory->append(",");
                    ub_signatures_or_inventory->append(inventory);
                }
            } else if (supplementary_info_type == SIGNATURE) {
                if (not non_ub_sigils_and_signatures_or_inventory->empty())
                    non_ub_sigils_and_signatures_or_inventory->append(",");
                non_ub_sigils_and_signatures_or_inventory->append(sigil + ':' + StringUtil::Join(signatures, ','));
            } else if (not inventory.empty()) { // Assume supplementary_info_type == INVENTORY.
                if (not non_ub_sigils_and_signatures_or_inventory->empty())
                    non_ub_sigils_and_signatures_or_inventory->append(",");
                non_ub_sigils_and_signatures_or_inventory->append(sigil + ':' + inventory);
            }
        }
    }

    return occurrence_count;
}


enum OutputSet { MONOGRAPHS, SERIALS };


bool FindTueDups(const OutputSet output_set, const MarcRecord &record) {
    std::string ub_signatures_or_inventory, non_ub_sigils_and_signatures_or_inventory;
    const unsigned occurrence_count(
        FindTueSigilsAndSignaturesOrInventory(record, (output_set == MONOGRAPHS ? SIGNATURE : INVENTORY),
                                              &ub_signatures_or_inventory,
                                              &non_ub_sigils_and_signatures_or_inventory));

    // We only keep dups and only those that occur at least once in the Tübingen University's main library:
    if (occurrence_count < 2 or ub_signatures_or_inventory.empty())
        return false;

    const std::string _008_contents(record.getFieldData("008"));
    std::string publication_year;
    if (likely(_008_contents.length() >= 11))
        publication_year = _008_contents.substr(7, 4);

    std::string area_or_zdb_number;
    if (output_set == MONOGRAPHS) {
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
    if (output_set == MONOGRAPHS)
        ExtractISBNs(record, &issns_and_isbns);
    else
        ExtractISSNs(record, &issns_and_isbns);

    std::cout << '"' << record.getControlNumber() << "\",\"" << TextUtil::CSVEscape(main_title) << "\",\""
              << TextUtil::CSVEscape(StringUtil::Join(issns_and_isbns, ','))
              << TextUtil::CSVEscape(publication_year) <<"\",\"" << TextUtil::CSVEscape(area_or_zdb_number)
              <<"\",\"" << TextUtil::CSVEscape(ub_signatures_or_inventory) <<  "\",\""
              << TextUtil::CSVEscape(non_ub_sigils_and_signatures_or_inventory) << "\"\n";

    return true;
}


void FindTueDups(const OutputSet output_set, MarcReader * const marc_reader) {
    // Write a header:
    std::cout << "\"PPN\"" << ",\"Titel\"" << (output_set == MONOGRAPHS ? ",\"ISBN\"" : ",\"ISSN\"")
              << ",\"Erscheinungsjahr\"" << (output_set == MONOGRAPHS ?",\"Fachgebiet\"" : "ZDB-ID-Nummer")
              << (output_set == MONOGRAPHS ? ",\"UB - Signatur\"" : ",\"UB - Bestandsangabe\"")
              << ",\"Sigel der anderen besitzenden Bibliotheken"
              << (output_set == SERIALS ? " mit Bestandsangaben\"" : "\"") << '\n';

    unsigned count(0), dups_count(0), monograph_count(0), serial_count(0);
    while (MarcRecord record = marc_reader->read()) {
        ++count;

        // Only consider monographs and serials:
        const Leader &leader(record.getLeader());
        if (not (leader.isMonograph() or leader.isSerial())
            or (leader.isMonograph() and output_set == SERIALS)
            or (leader.isSerial() and output_set == MONOGRAPHS))
            continue;

        if (FindTueDups(output_set, record)) {
            ++dups_count;
            if (leader.isMonograph())
                ++monograph_count;
            else
                ++serial_count;
        }
    }
    std::cerr << "Processed " << count << " records and found " << dups_count << " dups (" << monograph_count
              << " monographs and " << serial_count << " serials).\n";
}


int main(int argc, char **argv) {
    ::progname = argv[0];

    if (argc != 3)
        Usage();

    OutputSet output_set;
    if (std::strcmp(argv[1], "--output-set=MONOGRAPHS") == 0)
        output_set = MONOGRAPHS;
    else if (std::strcmp(argv[1], "--output-set=SERIALS") == 0)
        output_set = SERIALS;
    else
        Error("invalid input format \"" + std::string(argv[1]) + "\"!  (Must be MONOGRAPHS or SERIALS)");

    std::unique_ptr<MarcReader> marc_reader(MarcReader::Factory(argv[2], MarcReader::BINARY));

    try {
        FindTueDups(output_set, marc_reader.get());
    } catch (const std::exception &x) {
        Error("caught exception: " + std::string(x.what()));
    }
}

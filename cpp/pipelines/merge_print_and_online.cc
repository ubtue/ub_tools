/** \brief Utility for merging print and online editions into single records.
 *  \author Dr. Johannes Ruscheinski (johannes.ruscheinski@uni-tuebingen.de)
 *
 *  \copyright 2018,2019 Universitätsbibliothek Tübingen.  All rights reserved.
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
#include <memory>
#include <queue>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <cstdlib>
#include "DbConnection.h"
#include "DbResultSet.h"
#include "FileUtil.h"
#include "MARC.h"
#include "StlHelpers.h"
#include "StringUtil.h"
#include "TextUtil.h"
#include "util.h"
#include "VuFind.h"


namespace {


[[noreturn]] void Usage() {
    std::cerr << "Usage: " << ::progname
              << " [--min-log-level=min_log_level] [--debug] marc_input marc_output missing_ppn_partners_list\n"
              << "       missing_ppn_partners_list will be generated by this program and will contain the PPN's\n"
              << "       of superior works with cross links between print and online edition with one of\n"
              << "       the partners missing.  N.B. the input MARC file *must* be in the MARC-21 format!\n\n";
    std::exit(EXIT_FAILURE);
}


const std::set<MARC::Tag> UPLINK_TAGS{ "800", "810", "830", "773", "776" };


std::string ExtractUplinkPPN(const MARC::Record::Field &field) {
    const MARC::Subfields subfields(field.getSubfields());
    auto subfield_w(std::find_if(subfields.begin(), subfields.end(),
                                 [](const MARC::Subfield &subfield) -> bool { return subfield.code_ == 'w'; }));
    if (subfield_w == subfields.end())
        return "";

    if (not StringUtil::StartsWith(subfield_w->value_, "(DE-627)"))
        return "";
    return subfield_w->value_.substr(__builtin_strlen("(DE-627)"));
}


template<typename ValueType>void SerializeMap(const std::string &output_filename, const std::unordered_map<std::string, ValueType> &map) {
    const auto map_file(FileUtil::OpenOutputFileOrDie(output_filename));
    for (const auto &key_and_value : map)
        *map_file << key_and_value.first << " -> " << key_and_value.second << '\n';
}


void SerializeMultimap(const std::string &output_filename, const std::unordered_multimap<std::string, std::string> &multimap) {
    const auto map_file(FileUtil::OpenOutputFileOrDie(output_filename));
    if (multimap.empty())
        return;

    auto key_and_value(multimap.cbegin());
    std::string last_key(key_and_value->first);
    *map_file << key_and_value->first << " -> " << key_and_value->second;
    for (/* Intentionally empty! */; key_and_value != multimap.cend(); ++key_and_value) {
        if (key_and_value->first == last_key)
            *map_file << ',' << key_and_value->second;
        else {
            last_key = key_and_value->first;
            *map_file << '\n' << key_and_value->first << " -> " << key_and_value->second;
        }
    }
    *map_file << '\n';
}


// In this function we get all cross referenced PPN's and check the maps for their references as well.
// We then determine the new superior PPN for all cross refences and overwrite all existing mapping entries.
std::set<std::string> GetCrossLinkPPNs(const MARC::Record &record,
                                       const std::unordered_map<std::string, std::string> &ppn_to_canonical_ppn_map,
                                       const std::unordered_multimap<std::string, std::string> &canonical_ppn_to_ppn_map)
{
    auto cross_link_ppns(MARC::ExtractCrossReferencePPNs(record));
    if (cross_link_ppns.empty())
        return { };
    cross_link_ppns.emplace(record.getControlNumber());

    // Find the transitive hull of referenced PPN's that we have already encountered in the input stream:
    std::queue<std::string> queue;
    for (const auto &cross_link_ppn : cross_link_ppns)
        queue.push(cross_link_ppn);
    while (not queue.empty()) {
        const auto it(ppn_to_canonical_ppn_map.find(queue.front()));
        if (it != ppn_to_canonical_ppn_map.end() and cross_link_ppns.find(it->second) == cross_link_ppns.cend()) {
            queue.push(it->second);
            cross_link_ppns.emplace(it->second);
        }

        auto canonical_ppn_and_ppn_range(canonical_ppn_to_ppn_map.equal_range(queue.front()));
        for (auto it2(canonical_ppn_and_ppn_range.first); it2 != canonical_ppn_and_ppn_range.second; ++it2) {
            if (cross_link_ppns.find(it2->second) == cross_link_ppns.cend()) {
                queue.push(it2->second);
                cross_link_ppns.emplace(it2->second);
            }
        }

        queue.pop();
    }
    cross_link_ppns.erase(record.getControlNumber());
    return cross_link_ppns;
}


/* 3 maps are populated in this function:
 *
 * ppn_to_offset_map        - this is simply a map from each record's PPN to the byte offset in the input file.
 * ppn_to_canonical_ppn_map - This is a map for cross-linked PPN's pointing to the PPN that will become the PPN of the merged records.
 *                            To keep things simple we simply choose the alphanumerically largest PPN as the canonical PPN.
 * canonical_ppn_to_ppn_map - This is the reverse of the previous map except for that we may have many values for a single key.
 */
void CollectRecordOffsetsAndCrosslinks(const bool debug,
    MARC::Reader * const marc_reader, std::unordered_map<std::string, off_t> * const ppn_to_offset_map,
    std::unordered_map<std::string, std::string> * const ppn_to_canonical_ppn_map,
    std::unordered_multimap<std::string, std::string> * const canonical_ppn_to_ppn_map)
{
    off_t last_offset(0);
    unsigned record_count(0);
    while (const auto record = marc_reader->read()) {
        ++record_count;

        if (unlikely(ppn_to_offset_map->find(record.getControlNumber()) != ppn_to_offset_map->end()))
            LOG_ERROR("duplicate PPN \"" + record.getControlNumber() + "\" in input file \"" + marc_reader->getPath() + "\"!");

        (*ppn_to_offset_map)[record.getControlNumber()] = last_offset;

        last_offset = marc_reader->tell();

        // We only want to merge serials!
        if (not record.isSerial())
            continue;

        auto equivalent_ppns(GetCrossLinkPPNs(record, *ppn_to_canonical_ppn_map, *canonical_ppn_to_ppn_map));
        if (equivalent_ppns.empty())
            continue;
        equivalent_ppns.emplace(record.getControlNumber());

        // The max PPN, will be the winner for merging, IOW, it will be the PPN of the merged record.
        const std::string new_canonical_ppn(*std::max_element(equivalent_ppns.begin(), equivalent_ppns.end(),
                                                              [](std::string a, std::string b) -> bool { return a < b; }));

        // Remove old references:
        for (const auto &ppn : equivalent_ppns) {
            ppn_to_canonical_ppn_map->erase(ppn);
            canonical_ppn_to_ppn_map->erase(ppn);
        }

        // Add new/updated references:
        for (const auto &ppn : equivalent_ppns) {
            if (ppn == new_canonical_ppn) // Avoid self reference:
                continue;

            ppn_to_canonical_ppn_map->emplace(ppn, new_canonical_ppn);
            canonical_ppn_to_ppn_map->emplace(new_canonical_ppn, ppn);
        }
    }

    if (debug) {
        std::string map_filename("ppn_to_canonical_ppn.map");
        SerializeMap(map_filename, *ppn_to_canonical_ppn_map);
        std::cerr << "Wrote the mapping from non-canonical PPN's to canonical PPN's to \"" + map_filename + "\"!";

        map_filename = "canonical_ppn_to_ppn.map";
        SerializeMultimap(map_filename, *canonical_ppn_to_ppn_map);
        std::cerr << "Wrote the mapping from canonical PPN's to non-canonical PPN's to \"" + map_filename + "\"!";

        map_filename = "ppn_to_offset.map";
        SerializeMap(map_filename, *ppn_to_offset_map);
        std::cerr << "Wrote the mapping from canonical PPN's to non-canonical PPN's to \"" + map_filename + "\"!";
    }

    LOG_INFO("Found " + std::to_string(record_count) + " record(s).");
    LOG_INFO("Found " + std::to_string(ppn_to_canonical_ppn_map->size()) + " cross link(s).");
}


void EliminateDanglingOrUnreferencedCrossLinks(const bool debug, const std::unordered_map<std::string, off_t> &ppn_to_offset_map,
                                               std::unordered_map<std::string, std::string> * const ppn_to_canonical_ppn_map,
                                               std::unordered_multimap<std::string, std::string> * const canonical_ppn_to_ppn_map)
{
    unsigned dropped_count(0);
    auto canonical_ppn_and_ppn(canonical_ppn_to_ppn_map->begin());
    std::set<std::string> group_ppns;
    while (canonical_ppn_and_ppn != canonical_ppn_to_ppn_map->end()) {
        const std::string canonical_ppn(canonical_ppn_and_ppn->first);
        group_ppns.emplace(canonical_ppn_and_ppn->second);
        const auto next_canonical_ppn_and_ppn(std::next(canonical_ppn_and_ppn, 1));
        bool drop_group(false);

        if (next_canonical_ppn_and_ppn == canonical_ppn_to_ppn_map->end()
            or next_canonical_ppn_and_ppn->first != canonical_ppn)
        {
            // Decide to drop group either if PPN for merging is not a superior PPN or doesn't exist...
            drop_group = ppn_to_offset_map.find(canonical_ppn) == ppn_to_offset_map.end();

            // ... or at least one of the PPN's doesn't exist
            if (not drop_group) {
                for (const auto &ppn : group_ppns) {
                    if (ppn_to_offset_map.find(ppn) == ppn_to_offset_map.end()) {
                        LOG_INFO("Don't merge group around PPN " + ppn + " because the PPN is missing in our data! All PPNs in group: "
                                 + StringUtil::Join(group_ppns, ','));
                        drop_group = true;
                        break;
                    }
                }
            }

            // Do drop
            if (drop_group) {
                for (const auto &ppn : group_ppns)
                    ppn_to_canonical_ppn_map->erase(ppn);

                dropped_count += group_ppns.size() + 1;
                while (canonical_ppn_to_ppn_map->find(canonical_ppn) != canonical_ppn_to_ppn_map->end())
                    canonical_ppn_and_ppn = canonical_ppn_to_ppn_map->erase(canonical_ppn_to_ppn_map->find(canonical_ppn));

            }

            group_ppns.clear();
        }

        if (not drop_group)
            ++canonical_ppn_and_ppn;
    }

    if (debug) {
        std::string map_filename("ppn_to_canonical_ppn2.map");
        SerializeMap(map_filename, *ppn_to_canonical_ppn_map);
        std::cerr << "Wrote the mapping from non-canonical PPN's to canonical PPN's to \"" + map_filename + "\"!";

        map_filename = "canonical_ppn_to_ppn2.map";
        SerializeMultimap(map_filename, *canonical_ppn_to_ppn_map);
        std::cerr << "Wrote the mapping from canonical PPN's to non-canonical PPN's to \"" + map_filename + "\"!";
    }

    LOG_INFO("Dropped " + std::to_string(dropped_count) + " cross link(s) because at least one end was not a superior work or is missing.");
}


// Make inferior works point to the new merged superior parent found in "ppn_to_canonical_ppn_map".  Links referencing a key in
// "ppn_to_canonical_ppn_map" will be patched with the corresponding value.
// only 1 uplink of the same tag type will be kept.
unsigned PatchUplinks(MARC::Record * const record, const std::unordered_map<std::string, std::string> &ppn_to_canonical_ppn_map) {
    unsigned patched_uplinks(0);

    std::vector<size_t> uplink_indices_for_deletion;
    std::set<std::string> uplink_tags_done;
    for (auto field(record->begin()); field != record->end(); ++field) {
        const std::string field_tag(field->getTag().toString());
        if (UPLINK_TAGS.find(field_tag) != UPLINK_TAGS.cend()) {
            const std::string uplink_ppn(ExtractUplinkPPN(*field));
            if (uplink_ppn.empty())
                continue;

            if (uplink_tags_done.find(field_tag) != uplink_tags_done.end()) {
                uplink_indices_for_deletion.emplace_back(field - record->begin());
                continue;
            }

            const auto ppn_and_ppn(ppn_to_canonical_ppn_map.find(uplink_ppn));
            if (ppn_and_ppn == ppn_to_canonical_ppn_map.end())
                continue;

            // If we made it here, we need to replace the uplink PPN:
            field->insertOrReplaceSubfield('w', "(DE-627)" + ppn_and_ppn->second);
            uplink_tags_done.emplace(field_tag);
            ++patched_uplinks;
        }
    }

    record->deleteFields(uplink_indices_for_deletion);
    return patched_uplinks;
}


enum ElectronicOrPrint { ELECTRONIC, PRINT };


const std::map<std::string, std::set<ElectronicOrPrint>> suffix_to_set_map {
    { "(electronic)",       { ELECTRONIC        } },
    { "(print)",            { PRINT             } },
    { "(electronic/print)", { ELECTRONIC, PRINT } },
};


std::string StripElectronicAndPrintSuffixes(const std::string &subfield, std::set<ElectronicOrPrint> * const electronic_or_print,
                                            std::string * const non_canonized_contents_without_suffix)
{
    const auto open_paren_pos(subfield.find('('));
    if (open_paren_pos == std::string::npos)
        *non_canonized_contents_without_suffix = subfield;
    else
        *non_canonized_contents_without_suffix = StringUtil::TrimWhite(subfield.substr(0, open_paren_pos));

    electronic_or_print->clear();

    for (const auto &suffix_and_set : suffix_to_set_map) {
        if (StringUtil::EndsWith(subfield, suffix_and_set.first)) {
            *electronic_or_print = suffix_and_set.second;
            return StringUtil::TrimWhite(subfield.substr(0, subfield.length() - suffix_and_set.first.length()));
        }
    }

    return subfield;
}


std::string SubfieldContentsAndElectronicOrPrintToString(const std::string &contents_without_suffix,
                                                         const std::set<ElectronicOrPrint> &electronic_or_print)
{
    for (const auto &suffix_and_set : suffix_to_set_map) {
        if (electronic_or_print == suffix_and_set.second)
            return contents_without_suffix + " " + suffix_and_set.first;
    }

    return contents_without_suffix;
}


std::string MergeSubfieldContents(const std::string &subfield1, const std::string &subfield2, const MARC::Record &record1,
                                  const MARC::Record &record2)
{
    std::set<ElectronicOrPrint> electronic_or_print1;
    std::string non_canonised_contents_without_suffix1;
    const auto canonised_contents_without_suffix1(StripElectronicAndPrintSuffixes(subfield1, &electronic_or_print1,
                                                                                  &non_canonised_contents_without_suffix1));

    std::set<ElectronicOrPrint> electronic_or_print2;
    std::string non_canonised_contents_without_suffix2;
    const auto canonised_contents_without_suffix2(StripElectronicAndPrintSuffixes(subfield2, &electronic_or_print2,
                                                                                  &non_canonised_contents_without_suffix2));

    if (record1.isElectronicResource())
        electronic_or_print1.emplace(ELECTRONIC);
    if (record1.isPrintResource())
        electronic_or_print1.emplace(PRINT);

    if (record2.isElectronicResource())
        electronic_or_print2.emplace(ELECTRONIC);
    if (record2.isPrintResource())
        electronic_or_print2.emplace(PRINT);

    if (canonised_contents_without_suffix1 != canonised_contents_without_suffix2)
        return SubfieldContentsAndElectronicOrPrintToString(non_canonised_contents_without_suffix1, electronic_or_print1) + "; "
               + SubfieldContentsAndElectronicOrPrintToString(non_canonised_contents_without_suffix2, electronic_or_print2);

    const std::set<ElectronicOrPrint> combined_properties(StlHelpers::SetUnion(electronic_or_print1, electronic_or_print2));
    return SubfieldContentsAndElectronicOrPrintToString(canonised_contents_without_suffix1, combined_properties);
}


// The strategy we employ here is that we just pick "contents1" unless we have an identical subfield structure.
MARC::Subfields MergeFieldContents(const MARC::Subfields &subfields1, const MARC::Record &record1,
                                   const MARC::Subfields &subfields2, const MARC::Record &record2)
{
    std::string subfield_codes1;
    for (const auto &subfield : subfields1)
        subfield_codes1 += subfield.code_;

    std::string subfield_codes2;
    for (const auto &subfield : subfields2)
        subfield_codes2 += subfield.code_;

    if (subfield_codes1 != subfield_codes2) // We are up the creek!
        return subfields1;

    MARC::Subfields merged_subfields;
    for (auto subfield1(subfields1.begin()), subfield2(subfields2.begin()); subfield1 != subfields1.end(); ++subfield1, ++subfield2) {
        if (subfield1->value_ == subfield2->value_)
            merged_subfields.addSubfield(subfield1->code_, subfield1->value_);
        else {
            const std::string merged_value(MergeSubfieldContents(subfield1->value_, subfield2->value_, record1, record2));
            merged_subfields.addSubfield(subfield1->code_, merged_value);
        }
    }

    return merged_subfields;
}


std::string CanoniseText(const std::string &s) {
    std::vector<uint32_t> utf32_chars;
    if (unlikely(not TextUtil::UTF8ToUTF32(s, &utf32_chars)))
        return s;

    static const uint32_t SPACE(0x20u);

    std::vector<uint32_t> clean_utf32_chars;

    // Remove leading whitespace and replace multiple occurrences of whitespace characters with a single space and
    // convert any other characters to lowercase:
    bool whitespace_seen(true);
    for (const uint32_t ch : utf32_chars) {
        if (TextUtil::IsWhitespace(ch)) {
            if (not whitespace_seen) {
                whitespace_seen = true;
                clean_utf32_chars.emplace_back(SPACE);
            }
        } else {
            clean_utf32_chars.emplace_back(TextUtil::UTF32ToLower(ch));
            whitespace_seen = false;
        }
    }

    // Remove any trailing commas and whitespace:
    static const uint32_t COMMA(0x2Cu);
    size_t trim_count(0);
    for (auto ch_iter(clean_utf32_chars.rbegin()); ch_iter != clean_utf32_chars.rend(); ++ch_iter) {
        if (TextUtil::IsWhitespace(*ch_iter) or *ch_iter == COMMA)
            ++trim_count;
    }
    clean_utf32_chars.resize(clean_utf32_chars.size() - trim_count);

    std::string clean_s;
    for (const uint32_t utf32_ch : clean_utf32_chars) {
        if (unlikely(TextUtil::IsSomeKindOfDash(utf32_ch)))
            clean_s += '-'; // ordinary minus
        else
            clean_s += TextUtil::UTF32ToUTF8(utf32_ch);
    }

    return clean_s;
}


// Returns true if the contents of the leading subfields with subfield codes "subfield_codes" in field1 and field2 are
// identical, else returns false. Please note that the code specified in "subfield_codes" must exist.
bool SubfieldPrefixIsIdentical(const MARC::Record::Field &field1, const MARC::Record::Field &field2, const std::vector<char> &subfield_codes)
{
    const MARC::Subfields subfields1(field1.getSubfields());
    const auto subfield1(subfields1.begin());

    const MARC::Subfields subfields2(field2.getSubfields());
    const auto subfield2(subfields2.begin());

    for (const char subfield_code : subfield_codes) {
        if (subfield1 == subfields1.end() or subfield2 == subfields2.end())
            return false;
        if (subfield1->code_ != subfield_code or subfield2->code_ != subfield_code)
            return false;
        if (CanoniseText(subfield1->value_) != CanoniseText(subfield2->value_))
            return false;
    }

    return true;
}


void UpdateMergedPPNs(MARC::Record * const record, const std::set<std::string> &merged_ppns) {
    MARC::Subfields zwi_subfields;
    zwi_subfields.addSubfield('a', "1");
    for (const auto &merged_ppn : merged_ppns)
        zwi_subfields.addSubfield('b', merged_ppn);

    record->replaceField("ZWI", zwi_subfields);
}


bool FuzzyEqual(const MARC::Record::Field &field1, const MARC::Record::Field &field2, const bool compare_indicators) {
    if (field1.getTag() != field2.getTag()) {
        if (not compare_indicators or field1.getIndicator1() != field2.getIndicator1()
                                or field1.getIndicator2() != field2.getIndicator2())
            return false;
    }

    const MARC::Subfields subfields1(field1.getSubfields());
    auto subfield1(subfields1.begin());

    const MARC::Subfields subfields2(field2.getSubfields());
    auto subfield2(subfields2.begin());

    if (subfields1.size() != subfields2.size())
        return false;

    while (subfield1 != subfields1.end() and subfield2 != subfields2.end()) {
        if (subfield1->code_ != subfield2->code_ or CanoniseText(subfield1->value_) != CanoniseText(subfield2->value_))
            return false;
        ++subfield1, ++subfield2;
    }

    return subfield1 == subfields1.end() and subfield2 == subfields2.end();
}


static const std::vector<std::pair<std::string,std::string>> non_repeatable_to_repeatable_tag_map({ { "100", "700" },
                                                                                                    { "110", "710" },
                                                                                                    { "111", "711" }
                                                                                                  });


std::string GetTargetRepeatableTag(const MARC::Tag &non_repeatable_tag) {
    for (const auto &non_repeatable_and_repeatable_tag : non_repeatable_to_repeatable_tag_map) {
        if (non_repeatable_and_repeatable_tag.first == non_repeatable_tag.toString())
            return non_repeatable_and_repeatable_tag.second;
    }
    return non_repeatable_tag.toString();
}


bool MergeFieldPairWithControlFields(MARC::Record::Field * const merge_field, MARC::Record::Field &import_field)
{
    if (not merge_field->isControlField() or !import_field.isControlField())
        return false;

    std::string merged_contents;
    if (merge_field->getTag() == "005") // Date and Time of Latest Transaction
        merged_contents = std::max(merge_field->getContents(), import_field.getContents());
    else
        merged_contents = merge_field->getContents();

    merge_field->setContents(merged_contents);

    return true;
}


bool MergeFieldPairWithNonRepeatableFields(MARC::Record::Field * const merge_field, const MARC::Record::Field &import_field,
                                           MARC::Record * const merge_record, const MARC::Record &import_record)
{
    if (merge_field->isControlField() or import_field.isControlField()
        or merge_field->isRepeatableField() or import_field.isRepeatableField())
    {
        return false;
    }

    merge_field->setSubfields(MergeFieldContents(merge_field->getSubfields(), *merge_record,
                                                 import_field.getSubfields(), import_record));

    return true;
}


// Special handling for the ISSN's.
bool MergeFieldPair022(MARC::Record::Field * const merge_field, const MARC::Record::Field &import_field,
                       MARC::Record * const merge_record, const MARC::Record &import_record)
{
    if (merge_field->getTag() != "022" or import_field.getTag() != "022")
        return false;

    if (merge_record->isElectronicResource())
        merge_field->insertOrReplaceSubfield('2', "electronic");
    else
        merge_field->insertOrReplaceSubfield('2', "print");
    merge_field->insertOrReplaceSubfield('9', merge_record->getMainTitle());

    MARC::Record::Field record2_022_field(import_field);
    if (import_record.isElectronicResource())
        record2_022_field.insertOrReplaceSubfield('2', "electronic");
    else
        record2_022_field.insertOrReplaceSubfield('2', "print");
    record2_022_field.insertOrReplaceSubfield('9', import_record.getMainTitle());
    merge_record->insertFieldAtEnd(record2_022_field);

    return true;
}


bool MergeFieldPair264(MARC::Record::Field * const merge_field, const MARC::Record::Field &import_field,
                       MARC::Record * const merge_record, const MARC::Record import_record)
{
    if (merge_field->getTag() != "264" or import_field.getTag() != "264"
        or not SubfieldPrefixIsIdentical(*merge_field, import_field, {'a', 'b'}))
    {
        return false;
    }

    std::string merged_c_subfield;
    const MARC::Subfields subfields1(merge_field->getSubfields());
    const std::string subfield_c1(subfields1.getFirstSubfieldWithCode('c'));
    const MARC::Subfields subfields2(import_field.getSubfields());
    const std::string subfield_c2(subfields2.getFirstSubfieldWithCode('c'));
    if (subfield_c1 == subfield_c2)
        merged_c_subfield = subfield_c1;
    else {
        if (not subfield_c1.empty())
            merged_c_subfield = subfield_c1 + " (" + (merge_record->isElectronicResource() ? "electronic" : "print") + ")";
        if (not subfield_c2.empty()) {
            if (not merged_c_subfield.empty())
                merged_c_subfield += "; ";
            merged_c_subfield = subfield_c2 + " (" + (import_record.isElectronicResource() ? "electronic" : "print") + ")";
        }
    }

    if (not merged_c_subfield.empty()) {
        MARC::Record::Field merged_field(import_field);
        merged_field.insertOrReplaceSubfield('c', merged_c_subfield);
        merge_field->setContents(merged_field.getSubfields(), merged_field.getIndicator1(), merged_field.getIndicator2());
    }

    return true;
}


bool MergeFieldPair936(MARC::Record::Field * const merge_field, const MARC::Record::Field import_field)
{
    if (merge_field->getTag() != "936" or import_field.getTag() != "936")
        return false;

    if (not FuzzyEqual(*merge_field, import_field, true)) {
        LOG_WARNING("don't know how to merge 936 fields! (field1=\"" + merge_field->getContents() + "\",field2=\""
                    + import_field.getContents() + "\"), arbitrarily keeping field1");
    }
    return true;
}


// tag is only used for performance reasons
bool RecordHasField(const MARC::Record &record, const MARC::Record::Field &field, const bool compare_indicators, MARC::Record::Field * found_field) {
    for (auto &record_field : record) {
        if (FuzzyEqual(field, record_field, compare_indicators)) {
            *found_field = record_field;
            return true;
        }
    }
    return false;
}


/**
 * Merge import_record into merge_record
 *
 * - non-found fields can always be added
 * - different repeatable fields can always be added
 *   - except for some special cases like 022, 264, 936
 * - non-repeatable fields:
 * - check if field with same tag + indicators exist
 *    - if not, we can add it directly
 *    - else look for a matching repeatable field
 *        - if exists, simply add it there
 *        - else do hard merge into the existing field
 */
void MergeRecordPair(MARC::Record * const merge_record, MARC::Record * const import_record) {
    merge_record->reTag("260", "264");
    import_record->reTag("260", "264");

    for (auto &import_field : *import_record) {
        bool compare_indicators(import_field.isRepeatableField());
        MARC::Record::Field merge_field("999"); // arbitrary

        if (not RecordHasField(*merge_record, import_field, compare_indicators, &merge_field)) {
            merge_record->insertField(import_field);
            continue;
        }

        if (not MergeFieldPairWithControlFields(&merge_field, import_field)
            and not MergeFieldPair022(&merge_field, import_field, merge_record, *import_record)
            and not MergeFieldPair264(&merge_field, import_field, merge_record, *import_record)
            and not MergeFieldPair936(&merge_field, import_field))
        {
            if (import_field.isRepeatableField()) {
                merge_record->insertField(import_field);
            } else {
                const MARC::Tag repeatable_tag(GetTargetRepeatableTag(import_field.getTag()));
                if (repeatable_tag != import_field.getTag()) {
                    import_field.setTag(repeatable_tag);
                    merge_record->insertField(import_field);
                } else
                    MergeFieldPairWithNonRepeatableFields(&merge_field, import_field, merge_record, *import_record);
            }
        }
    }
}


MARC::Record ReadRecordFromOffsetOrDie(MARC::Reader * const marc_reader, const off_t offset) {
    const auto saved_offset(marc_reader->tell());
    if (unlikely(not marc_reader->seek(offset)))
        LOG_ERROR("can't seek to offset " + std::to_string(offset) + "!");
    MARC::Record record(marc_reader->read());
    if (unlikely(not record))
        LOG_ERROR("failed to read a record from offset " + std::to_string(offset) + "!");

    if (unlikely(not marc_reader->seek(saved_offset)))
        LOG_ERROR("failed to seek to previous position " + std::to_string(saved_offset) + "!");

    return record;
}


// Replaces 246$i "Nebentitel:" w/ "Abweichender Titel" (RDA).
void Patch246i(MARC::Record * const record) {
    for (auto &_246_field : record->getTagRange("246")) {
        MARC::Subfields _246_subfields(_246_field.getSubfields());
        if (_246_subfields.replaceAllSubfields('i', "Nebentitel:", "Abweichender Titel"))
            _246_field.setSubfields(_246_subfields);
    }
}


void DeleteCrossLinkFields(MARC::Record * const record) {
    std::vector<size_t> field_indices_for_deletion;
    for (auto field(record->begin()); field != record->end(); ++field) {
        std::string unused;
        if (MARC::IsCrossLinkField(*field, &unused))
            field_indices_for_deletion.emplace_back(field - record->begin());
    }
    record->deleteFields(field_indices_for_deletion);
}


// Merges the records in ppn_to_canonical_ppn_map in such a way that for each entry, "second" will be merged into "first".
// "second" will then be collected in "skip_ppns" for a future copy phase where it will be dropped.  Uplinks that referenced
// "second" will be replaced with "first".
void MergeRecordsAndPatchUplinks(MARC::Reader * const marc_reader, MARC::Writer * const marc_writer,
                                 const std::unordered_map<std::string, off_t> &ppn_to_offset_map,
                                 const std::unordered_map<std::string, std::string> &ppn_to_canonical_ppn_map,
                                 const std::unordered_multimap<std::string, std::string> &canonical_ppn_to_ppn_map)
{
    std::unordered_set<std::string> unprocessed_ppns;
    for (const auto &ppn_and_ppn : canonical_ppn_to_ppn_map)
        unprocessed_ppns.emplace(ppn_and_ppn.second);

    unsigned merged_count(0), patched_uplink_count(0);
    while (MARC::Record record = marc_reader->read()) {
        if (ppn_to_canonical_ppn_map.find(record.getControlNumber()) != ppn_to_canonical_ppn_map.cend())
            continue; // This record will be merged into the one w/ the canonical PPN.

        auto canonical_ppn_and_ppn(canonical_ppn_to_ppn_map.find(record.getControlNumber()));
        if (canonical_ppn_and_ppn != canonical_ppn_to_ppn_map.cend()) {
            std::set<std::string> merged_ppns{ record.getControlNumber() };
            for (/* Intentionally empty! */;
                 canonical_ppn_and_ppn != canonical_ppn_to_ppn_map.cend() and canonical_ppn_and_ppn->first == record.getControlNumber();
                 ++canonical_ppn_and_ppn)
            {
                const auto record2_ppn_and_offset(ppn_to_offset_map.find(canonical_ppn_and_ppn->second));
                if (unlikely(record2_ppn_and_offset == ppn_to_offset_map.cend()))
                    LOG_ERROR("this should *never* happen! missing PPN in ppn_to_offset_map: " + canonical_ppn_and_ppn->second);
                MARC::Record record2(ReadRecordFromOffsetOrDie(marc_reader, record2_ppn_and_offset->second));
                merged_ppns.emplace(record2.getControlNumber());
                Patch246i(&record); Patch246i(&record2);
                MergeRecordPair(&record, &record2);
                ++merged_count;
                unprocessed_ppns.erase(canonical_ppn_and_ppn->second);
            }
            DeleteCrossLinkFields(&record);

            // Mark the record as being both "print" as well as "electronic" and store the PPN's of the dropped records:
            merged_ppns.erase(*merged_ppns.rbegin()); // Remove max element
            UpdateMergedPPNs(&record, merged_ppns);
        }

        patched_uplink_count += PatchUplinks(&record, ppn_to_canonical_ppn_map);

        marc_writer->write(record);
    }

    if (unlikely(merged_count != canonical_ppn_to_ppn_map.size())) {
        LOG_ERROR("sanity check failed! (merged_count=" + std::to_string(merged_count) + ", canonical_ppn_to_ppn_map.size()="
                  + std::to_string(canonical_ppn_to_ppn_map.size()) + ". Missing PPNs: " + StringUtil::Join(unprocessed_ppns, ", "));
    }

    LOG_INFO("Patched uplinks of " + std::to_string(patched_uplink_count) + " MARC record(s).");
}


// Here we update subscriptions.  There are 3 possible cases for each user and mapped PPN:
// 1. The trivial case where no subscriptions exist for a mapped PPN.
// 2. A subscription only exists for the mapped PPN.
//    In this case we only have to swap the PPN for the subscription.
// 3. Subscriptions exist for both, electronic and print PPNs.
//    Here we have to delete the subscription for the mapped PPN and ensure that the max_last_modification_time of the
//    remaining subscription is the minimum of the two previously existing subscriptions.
void PatchSerialSubscriptions(DbConnection * connection, const std::unordered_map<std::string, std::string> &ppn_to_canonical_ppn_map) {
    for (const auto &ppn_and_ppn : ppn_to_canonical_ppn_map) {
        connection->queryOrDie("SELECT user_id,max_last_modification_time FROM ixtheo_journal_subscriptions WHERE "
                               "journal_control_number_or_bundle_name='" + ppn_and_ppn.first + "'");
        DbResultSet ppn_first_result_set(connection->getLastResultSet());
        while (const DbRow ppn_first_row = ppn_first_result_set.getNextRow()) {
            const std::string user_id(ppn_first_row["user_id"]);
            connection->queryOrDie("SELECT max_last_modification_time FROM ixtheo_journal_subscriptions "
                                   "WHERE user_id='" + user_id + "' AND journal_control_number_or_bundle_name='" + ppn_and_ppn.second + "'");
            DbResultSet ppn_second_result_set(connection->getLastResultSet());
            if (ppn_second_result_set.empty()) {
                connection->queryOrDie("UPDATE ixtheo_journal_subscriptions SET journal_control_number_or_bundle_name='"
                                       + ppn_and_ppn.second + "' WHERE user_id='" + user_id + "' AND journal_control_number_or_bundle_name='"
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
            connection->queryOrDie("DELETE FROM ixtheo_journal_subscriptions WHERE journal_control_number_or_bundle_name='"
                                   + ppn_and_ppn.first + "' and user_id='" + user_id + "'");
            if (ppn_first_row["max_last_modification_time"] > min_max_last_modification_time)
                connection->queryOrDie("UPDATE ixtheo_journal_subscriptions SET max_last_modification_time='"
                                       + min_max_last_modification_time + "' WHERE journal_control_number_or_bundle_name='"
                                       + ppn_and_ppn.second + "' and user_id='" + user_id + "'");
        }
    }
}


void PatchPDASubscriptions(DbConnection * connection, const std::unordered_map<std::string, std::string> &ppn_to_canonical_ppn_map) {
    for (const auto &ppn_and_ppn : ppn_to_canonical_ppn_map) {
        connection->queryOrDie("SELECT id FROM ixtheo_pda_subscriptions WHERE book_ppn='" + ppn_and_ppn.first + "'");
        DbResultSet result_set(connection->getLastResultSet());
        while (const DbRow row = result_set.getNextRow())
            connection->queryOrDie("UPDATE ixtheo_pda_subscriptions SET book_ppn='" + ppn_and_ppn.first + "' WHERE id='"
                                   + row["id"] + "' AND book_ppn='" + ppn_and_ppn.second + "'");
    }
}


void PatchResourceTable(DbConnection * connection, const std::unordered_map<std::string, std::string> &ppn_to_canonical_ppn_map) {
    for (const auto &ppn_and_ppn : ppn_to_canonical_ppn_map) {
        connection->queryOrDie("SELECT id FROM resource WHERE record_id='" + ppn_and_ppn.first + "'");
        DbResultSet result_set(connection->getLastResultSet());
        while (const DbRow row = result_set.getNextRow())
            connection->queryOrDie("UPDATE resource SET record_id='" + ppn_and_ppn.second + "' WHERE id=" + row["id"]);
    }
}


} // unnamed namespace


int Main(int argc, char *argv[]) {
    if (argc < 4)
        Usage();

    bool debug(false);
    if (std::strcmp(argv[1], "--debug") == 0) {
        debug = true;
        --argc, ++argv;
        if (argc < 4)
            Usage();
    }

    if (argc != 4)
        Usage();

    std::unique_ptr<MARC::Reader> marc_reader(MARC::Reader::Factory(argv[1], MARC::FileType::BINARY));
    std::unique_ptr<MARC::Writer> marc_writer(MARC::Writer::Factory(argv[2]));
    std::unique_ptr<File> missing_partners(FileUtil::OpenOutputFileOrDie(argv[3]));

    std::unordered_map<std::string, off_t> ppn_to_offset_map;
    std::unordered_map<std::string, std::string> ppn_to_canonical_ppn_map;
    std::unordered_multimap<std::string, std::string> canonical_ppn_to_ppn_map;
    CollectRecordOffsetsAndCrosslinks(debug, marc_reader.get(), &ppn_to_offset_map,
                                      &ppn_to_canonical_ppn_map, &canonical_ppn_to_ppn_map);

    EliminateDanglingOrUnreferencedCrossLinks(debug, ppn_to_offset_map, &ppn_to_canonical_ppn_map, &canonical_ppn_to_ppn_map);

    marc_reader->rewind();
    MergeRecordsAndPatchUplinks(marc_reader.get(), marc_writer.get(), ppn_to_offset_map, ppn_to_canonical_ppn_map, canonical_ppn_to_ppn_map);

    if (not debug) {
        std::shared_ptr<DbConnection> db_connection(VuFind::GetDbConnection());
        PatchSerialSubscriptions(db_connection.get(), ppn_to_canonical_ppn_map);
        PatchPDASubscriptions(db_connection.get(), ppn_to_canonical_ppn_map);
        PatchResourceTable(db_connection.get(), ppn_to_canonical_ppn_map);
    }

    return EXIT_SUCCESS;
}

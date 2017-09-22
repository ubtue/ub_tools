/** \file   MarcUtil.cc
 *  \brief  Implementation of various utility functions related to the processing of MARC-21 and MARC-XML records.
 *  \author Dr. Johannes Ruscheinski (johannes.ruscheinski@uni-tuebingen.de)
 *  \author Oliver Obenland (oliver.obenland@uni-tuebingen.de)
 *
 *  \copyright 2014-2017 Universitätsbibliothek Tübingen.  All rights reserved.
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

#include "MarcUtil.h"
#include "MiscUtil.h"
#include "RegexMatcher.h"


namespace MarcUtil {


bool GetGNDCode(const MarcRecord &record, std::string * const gnd_code) {
    gnd_code->clear();
    const size_t _035_index(record.getFieldIndex("035"));
    if (_035_index == MarcRecord::FIELD_NOT_FOUND)
        return false;
    const Subfields _035_subfields(record.getSubfields(_035_index));
    const std::string _035a_field(_035_subfields.getFirstSubfieldValue('a'));
    if (not StringUtil::StartsWith(_035a_field, "(DE-588)"))
        return false;
    *gnd_code = _035a_field.substr(8);
    return not gnd_code->empty();
}


bool UBTueIsElectronicResource(const MarcRecord &marc_record) {
    if (marc_record.isElectronicResource())
        return true;

    const std::string title_field(marc_record.getFieldData("245"));
    if (likely(not title_field.empty())) {
        Subfields title_subfields(title_field);
        if (::strcasecmp(title_subfields.getFirstSubfieldValue('h').c_str(), "[electronic resource]"))
            return true;
    }

    return false;
}


std::string GetParentPPN(const MarcRecord &marc_record) {
    static const std::vector<std::string> parent_reference_fields{ "800", "810", "830", "773", "776" };
    static RegexMatcher * const matcher(RegexMatcher::RegexMatcherFactory("^\\([^)]+\\)(.+)$"));
    for (auto tag : parent_reference_fields) {
        if (matcher->matched(marc_record.getSubfields(tag).getFirstSubfieldValue('w'))) {
            const std::string ppn_candidate((*matcher)[1]);
            if (MiscUtil::IsValidPPN(ppn_candidate))
                return ppn_candidate;
        }
    }

    return "";
}


/** \brief Populates a map of control numbers to record offsets.
 *  \return The number of processed records.
 */
unsigned CollectRecordOffsets(MarcReader * const marc_reader,
                              std::unordered_map<std::string, off_t> * const control_number_to_offset_map)
{
    unsigned record_count(0);
    for (;;) {
        const off_t offset(marc_reader->tell());
        const MarcRecord record(marc_reader->read());
        if (not record)
            break;
        ++record_count;
        (*control_number_to_offset_map)[record.getControlNumber()] = offset;
    }

    return record_count;
}


bool IsArticle(const MarcRecord &marc_record) {
    if (marc_record.getLeader().isArticle())
        return true;

    return HasSubfieldWithValue(marc_record, "935", 'c', "sodr");
}


bool HasSubfieldWithValue(const MarcRecord &marc_record, const std::string &tag, const char subfield_code,
                          const std::string &value)
{
    std::vector<size_t> field_indices;
    marc_record.getFieldIndices(tag, &field_indices);
    for (const size_t index : field_indices) {
        const Subfields subfields(marc_record.getFieldData(index));
        if (subfields.hasSubfieldWithValue(subfield_code, value))
            return true;
    }

    return false;
}


} // namespace MarcUtil

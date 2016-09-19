/** \brief Marc-Implementation
 *  \author Oliver Obenland (oliver.obenland@uni-tuebingen.de)
 *
 *  \copyright 2016 Universitätsbiblothek Tübingen.  All rights reserved.
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

#include "MarcRecord.h"
#include "util.h"
#include "MarcReader.h"


std::string MarcRecord::getFieldData(const std::string &tag) const {
    return getFieldData(getFieldIndex(tag));
}


std::string MarcRecord::getFieldData(const size_t index) const {
    if (directory_entries_.cbegin() + index >= directory_entries_.cend())
        return "";
    const DirectoryEntry &entry(directory_entries_[index]);
    return std::string(raw_data_, entry.getFieldOffset(), entry.getFieldLength() - 1);
}


Subfields MarcRecord::getSubfields(const std::string &tag) const {
    return getSubfields(getFieldIndex(tag));
}


Subfields MarcRecord::getSubfields(const size_t index) const {
    if (directory_entries_.cbegin() + index >= directory_entries_.cend())
        return Subfields();
    return Subfields(getFieldData(index));
}


std::string MarcRecord::getTag(const size_t index) const {
    if (directory_entries_.cbegin() + index >= directory_entries_.cend())
        return "";
    return directory_entries_[index].getTag();
}


size_t MarcRecord::getFieldIndex(const std::string &field_tag) const {
    auto const iter(DirectoryEntry::FindField(field_tag, directory_entries_));
    return (iter == directory_entries_.end()) ? MarcRecord::FIELD_NOT_FOUND : std::distance(directory_entries_.begin(), iter);
}


size_t MarcRecord::getFieldIndices(const std::string &field_tag, std::vector<size_t> * const field_indices) const {
    field_indices->clear();

    size_t field_index(getFieldIndex(field_tag));
    while (static_cast<size_t>(field_index) < directory_entries_.size() and directory_entries_[field_index].getTag() == field_tag) {
        field_indices->emplace_back(field_index);
        ++field_index;
    }

    return field_indices->size();
}


bool MarcRecord::updateField(const size_t field_index, const std::string &new_field_value) {
    DirectoryEntry &entry(directory_entries_[field_index]);
    size_t offset = raw_data_.size();
    size_t length = new_field_value.length() + 1 /* For new field separator. */;

    entry.setFieldLength(length);
    entry.setFieldOffset(offset);
    entry.setCache(new_field_value);
    raw_data_ += new_field_value + '\x1E';

    return true;
}


size_t MarcRecord::insertField(const std::string &new_field_tag, const std::string &new_field_value) {
    if (unlikely(new_field_tag.length() != DirectoryEntry::TAG_LENGTH))
        throw std::runtime_error("in MarcUtil::Record::insertField: \"new_field_tag\" must have a length of 3!");

    // Find the insertion location:
    auto insertion_location(directory_entries_.begin());
    while (insertion_location != directory_entries_.end() and new_field_tag >= insertion_location->getTag())
        ++insertion_location;

    const size_t offset(raw_data_.size());
    const size_t length(new_field_value.length() + 1) /* For new field separator. */;
    const auto inserted_location(directory_entries_.emplace(insertion_location, new_field_tag, length, offset));
    raw_data_ += new_field_value + '\x1E';

    const size_t index(std::distance(directory_entries_.begin(), inserted_location));
    directory_entries_[index].setCache(new_field_value);
    return index;
}


void MarcRecord::deleteField(const size_t field_index) {
    directory_entries_.erase(directory_entries_.begin() + field_index);
}


void MarcRecord::markFieldAsDeleted(const size_t field_index) {
    deleted_field_indices.insert(field_index);
}


void MarcRecord::commitDeletionMarks() {
    if (deleted_field_indices.empty())
        return;

    std::vector<DirectoryEntry> new_entries;
    new_entries.reserve(directory_entries_.size() - deleted_field_indices.size());

    size_t index(0);
    for (const size_t deleted_index : deleted_field_indices) {
        for (/*Empty*/; index < getNumberOfFields() and index != deleted_index; ++index)
            new_entries.push_back(std::move(directory_entries_[index]));
        ++index;
    }
    directory_entries_.clear();
    for (auto iter = new_entries.begin(); iter < new_entries.end(); ++iter) {
        directory_entries_.push_back(std::move(*iter));
    }
    deleted_field_indices.clear();
}


std::string MarcRecord::extractFirstSubfield(const std::string &tag, const char subfield_code) const {
    return getSubfields(tag).getFirstSubfieldValue(subfield_code);
}


size_t MarcRecord::extractAllSubfields(const std::string &tags, std::vector<std::string> * const values,
                           const std::string &ignore_subfield_codes) const
{
    values->clear();

    std::vector<std::string> individual_tags;
    StringUtil::Split(tags, ':', &individual_tags);
    for (const auto &tag : individual_tags) {
        size_t field_index(getFieldIndex(tag));
        while (static_cast<size_t>(field_index) < directory_entries_.size() and tag == directory_entries_[field_index].getTag()) {
            const Subfields subfields(getSubfields(field_index));
            for (const auto &subfield_code_and_value : subfields.getAllSubfields()) {
                if (ignore_subfield_codes.find(subfield_code_and_value.first) == std::string::npos)
                    values->emplace_back(subfield_code_and_value.second);
            }
            ++field_index;
        }
    }
    return values->size();
}


size_t MarcRecord::extractSubfield(const std::string &tag, const char subfield_code, std::vector<std::string> * const values) const {
    values->clear();

    size_t field_index(getFieldIndex(tag));
    while (static_cast<size_t>(field_index) < directory_entries_.size() and tag == directory_entries_[field_index].getTag()) {
        const Subfields subfields(getSubfields(field_index));
        const auto begin_end(subfields.getIterators(subfield_code));
        for (auto subfield_code_and_value(begin_end.first); subfield_code_and_value != begin_end.second; ++subfield_code_and_value)
            values->emplace_back(subfield_code_and_value->second);
        ++field_index;
    }
    return values->size();
}


size_t MarcRecord::extractSubfields(const std::string &tag, const std::string &subfield_codes, std::vector<std::string> * const values) const {
    values->clear();

    size_t field_index(getFieldIndex(tag));
    while (static_cast<size_t>(field_index) < directory_entries_.size() and tag == directory_entries_[field_index].getTag()) {
        const Subfields subfields(getSubfields(field_index));
        const std::unordered_multimap<char, std::string> &code_to_data_map(subfields.getAllSubfields());
        for (const auto &code_and_value : code_to_data_map) {
            if (subfield_codes.find(code_and_value.first) != std::string::npos)
                values->emplace_back(code_and_value.second);
        }
        ++field_index;
    }

    return values->size();
}


size_t MarcRecord::findAllLocalDataBlocks(std::vector<std::pair<size_t, size_t>> * const local_block_boundaries) const {
    local_block_boundaries->clear();

    size_t local_block_start(getFieldIndex("LOK"));
    size_t local_block_end(local_block_start + 1);
    while (local_block_end < directory_entries_.size()) {
        if (StringUtil::StartsWith(getFieldData(local_block_end), "  ""\x1F""0000")) {
            local_block_boundaries->emplace_back(std::make_pair(local_block_start, local_block_end));
            local_block_start = local_block_end;
        }
        ++local_block_end;
    }
    local_block_boundaries->emplace_back(std::make_pair(local_block_start, local_block_end));

    return local_block_boundaries->size();
}


static bool IndicatorsMatch(const std::string &indicator_pattern, const std::string &indicators) {
    if (indicator_pattern[0] != '?' and indicator_pattern[0] != indicators[0])
        return false;
    if (indicator_pattern[1] != '?' and indicator_pattern[1] != indicators[1])
        return false;
    return true;
}


size_t MarcRecord::findFieldsInLocalBlock(const std::string &field_tag, const std::string &indicators,
                              const std::pair<size_t, size_t> &block_start_and_end,
                              std::vector<size_t> * const field_indices) const {
    field_indices->clear();

    if (unlikely(field_tag.length() != 3))
        Error("in MarcUtil::FindFieldInLocalBlock: field_tag must be precisely 3 characters long!");
    if (unlikely(indicators.length() != 2))
        Error("in MarcUtil::FindFieldInLocalBlock: indicators must be precisely 2 characters long!");

    const std::string FIELD_PREFIX("  ""\x1F""0" + field_tag);
    for (size_t index(block_start_and_end.first); index < block_start_and_end.second; ++index) {
        const std::string &current_field(getFieldData(index));
        if (StringUtil::StartsWith(current_field, FIELD_PREFIX)
            and IndicatorsMatch(indicators, current_field.substr(7, 2)))
            field_indices->emplace_back(index);
    }
    return field_indices->size();
}


void MarcRecord::filterTags(const std::unordered_set<std::string> &drop_tags) {
    for (auto entry = directory_entries_.rbegin(); entry < directory_entries_.rend(); ++entry) {
        if (drop_tags.find(entry->getTag()) != drop_tags.end())
            markFieldAsDeleted(std::distance(directory_entries_.rbegin(), entry));
    }
    commitDeletionMarks();
}


std::string MarcRecord::getLanguage(const std::string &default_language_code) const {
    const std::string &language(extractFirstSubfield("041", 'a'));
    if (likely(not language.empty()))
        return language;
    return default_language_code;
}


std::string MarcRecord::getLanguageCode() const {
    auto const entry(DirectoryEntry::FindField("008", directory_entries_));

    // Language codes start at offset 35 and have a length of 3.
    if (entry == directory_entries_.end() || entry->getFieldLength() < 38)
        return "";

    return std::string(raw_data_, entry->getFieldOffset() + 35, 3);
}


void MarcRecord::combine(const MarcRecord &record) {
    const size_t offset(raw_data_.size());
    raw_data_ += record.raw_data_;

    // Ignore first field. We only need one 001-field.
    directory_entries_.reserve(record.directory_entries_.size() - 1);
    for (auto iter(record.directory_entries_.begin() + 1); iter < record.directory_entries_.end(); ++iter) {
        auto entry = directory_entries_.emplace_back(*iter);
        entry->setFieldOffset(iter->getFieldOffset() + offset);
    }
}


bool MarcRecord::ProcessRecords(File * const input, File * const output, RecordFunc process_record, std::string * const err_msg) {
    err_msg->clear();

    while (MarcRecord record = MarcReader::Read(input)) {
        if (not (*process_record)(&record, output, err_msg))
            return false;
        err_msg->clear();
    }

    return err_msg->empty();
}


bool MarcRecord::ProcessRecords(File * const input, XmlRecordFunc process_record, XmlWriter * const xml_writer, std::string * const err_msg) {
    err_msg->clear();

    while (MarcRecord record = MarcReader::ReadXML(input)) {
        if (not (*process_record)(&record, xml_writer, err_msg))
            return false;
        err_msg->clear();
    }

    return err_msg->empty();
}
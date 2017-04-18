/** \brief Implementation of the MarcRecord class.
 *  \author Oliver Obenland (oliver.obenland@uni-tuebingen.de)
 *  \author Dr. Johannes Ruscheinski (johannes.ruscheinski@uni-tuebingen.de)
 *
 *  \copyright 2016 Universitätsbibliothek Tübingen.  All rights reserved.
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
#include "MarcTag.h"
#include "util.h"


const size_t MarcRecord::FIELD_NOT_FOUND;


MarcRecord &MarcRecord::operator=(const MarcRecord &rhs) {
    if (likely(&rhs != this)) {
        leader_ = rhs.leader_;
        raw_data_ = rhs.raw_data_;
        directory_entries_ = rhs.directory_entries_;
    }
    return *this;
}


std::string MarcRecord::getFieldData(const size_t index) const {
    if (index == MarcRecord::FIELD_NOT_FOUND or directory_entries_.cbegin() + index >= directory_entries_.cend())
        return "";
    const DirectoryEntry &entry(directory_entries_[index]);
    return std::string(raw_data_, entry.getFieldOffset(), entry.getFieldLength() - 1);
}


Subfields MarcRecord::getSubfields(const size_t index) const {
    if (index == MarcRecord::FIELD_NOT_FOUND or directory_entries_.cbegin() + index >= directory_entries_.cend())
        return Subfields();
    return Subfields(getFieldData(index));
}


void MarcRecord::deleteSubfield(const size_t field_index, const char subfield_code) {
    Subfields subfields(getSubfields(field_index));
    subfields.erase(subfield_code);
    updateField(field_index, subfields.toString());
}


MarcTag MarcRecord::getTag(const size_t index) const {
    if (directory_entries_.cbegin() + index >= directory_entries_.cend())
        return "";
    return directory_entries_[index].getTag();
}


size_t MarcRecord::getFieldIndex(const MarcTag &field_tag) const {
    for (size_t i(0); i < getNumberOfFields(); ++i) {
        if (directory_entries_[i].getTag() == field_tag)
            return i;
    }
    return MarcRecord::FIELD_NOT_FOUND;
}


size_t MarcRecord::getFieldIndices(const MarcTag &field_tag, std::vector <size_t> *const field_indices) const {
    field_indices->clear();

    size_t field_index(getFieldIndex(field_tag));
    while (field_index < directory_entries_.size() and directory_entries_[field_index].getTag() == field_tag) {
        field_indices->emplace_back(field_index);
        ++field_index;
    }

    return field_indices->size();
}


void MarcRecord::updateField(const size_t field_index, const std::string &new_field_value) {
    if (unlikely(new_field_value.size() > MAX_FIELD_LENGTH))
        throw std::runtime_error("in MarcRecord::updateField: can't accept more than MarcRecord::MAX_FIELD_LENGTH "
                                 "of field data!");

    DirectoryEntry &entry(directory_entries_[field_index]);
    const size_t old_field_length(entry.getFieldLength());
    const size_t new_field_length(new_field_value.length() + 1 /* For new field separator. */);

    const ssize_t delta(static_cast<ssize_t>(new_field_length) - static_cast<ssize_t>(old_field_length));
    if (delta != 0) {
        entry.setFieldLength(entry.getFieldLength() + delta);
        for (size_t index(field_index + 1); index < directory_entries_.size(); ++index)
            directory_entries_[index].setFieldOffset(directory_entries_[index].getFieldOffset() + delta);

        if (delta > 0) // We need more room.
            raw_data_.resize(raw_data_.size() + delta);
        if (field_index != directory_entries_.size() - 1) // Not the last field.
            std::memmove(const_cast<char *>(raw_data_.data()) + entry.getFieldOffset() + new_field_length,
                         const_cast<char *>(raw_data_.data()) + entry.getFieldOffset() + old_field_length,
                         raw_data_.size() - directory_entries_[field_index + 1].getFieldOffset());
        if (delta < 0)
            raw_data_.resize(raw_data_.size() + delta);
    }
                
    std::memcpy(const_cast<char *>(raw_data_.data()) + entry.getFieldOffset(), new_field_value.data(),
                new_field_value.length());
    *(const_cast<char *>(raw_data_.data()) + entry.getFieldOffset() + entry.getFieldLength() - 1) = '\x1E';
}


bool MarcRecord::insertSubfield(const MarcTag &new_field_tag, const char subfield_code,
                                const std::string &new_subfield_value, const char indicator1, const char indicator2) {
    return insertField(new_field_tag, std::string(1, indicator1) + std::string(1, indicator2) + "\x1F"
                                      + std::string(1, subfield_code) + new_subfield_value);
}


bool MarcRecord::addSubfield(const MarcTag &field_tag, const char subfield_code, const std::string &subfield_value) {
    const size_t field_index(getFieldIndex(field_tag));
    if (unlikely(field_index == MarcRecord::FIELD_NOT_FOUND))
        return false;

    std::string new_field_value(getFieldData(field_index));
    new_field_value += std::string(1, subfield_code) + subfield_value;
    updateField(field_index, new_field_value);

    return true;
}


size_t MarcRecord::insertField(const MarcTag &new_field_tag, const std::string &new_field_value) {
    if (unlikely(new_field_value.size() > MAX_FIELD_LENGTH))
        throw std::runtime_error("in MarcRecord::insertField: can't accept more than MarcRecord::MAX_FIELD_LENGTH "
                                 "of field data!");

    // Find the insertion location:
    auto insertion_location(directory_entries_.begin());
    while (insertion_location != directory_entries_.end() and new_field_tag >= insertion_location->getTag())
        ++insertion_location;

    const size_t offset(raw_data_.size());
    const size_t length(new_field_value.length() + 1) /* For new field separator. */;
    const auto inserted_location(directory_entries_.emplace(insertion_location, new_field_tag, length, offset));
    raw_data_ += new_field_value + '\x1E';

    const size_t index(std::distance(directory_entries_.begin(), inserted_location));
    return index;
}


void MarcRecord::deleteField(const size_t field_index) {
    directory_entries_.erase(directory_entries_.begin() + field_index);
}


void MarcRecord::deleteFields(const std::vector <std::pair<size_t, size_t>> &blocks) {
    std::vector <DirectoryEntry> new_entries;
    new_entries.reserve(directory_entries_.size());

    size_t copy_start(0);
    for (const std::pair <size_t, size_t> block : blocks) {
        new_entries.insert(new_entries.end(), directory_entries_.begin() + copy_start,
                           directory_entries_.begin() + block.first);
        copy_start = block.second;
    }
    new_entries.insert(new_entries.end(), directory_entries_.begin() + copy_start, directory_entries_.end());
    new_entries.swap(directory_entries_);
}


std::string MarcRecord::extractFirstSubfield(const MarcTag &tag, const char subfield_code) const {
    const size_t index(getFieldIndex(tag));
    if (index == FIELD_NOT_FOUND)
        return "";
    return getSubfields(index).getFirstSubfieldValue(subfield_code);
}


std::string MarcRecord::extractFirstSubfield(const size_t field_index, const char subfield_code) const {
    const Subfields subfields(getSubfields(field_index));
    return subfields.getFirstSubfieldValue(subfield_code);
}


size_t MarcRecord::extractAllSubfields(const std::string &tags, std::vector <std::string> *const values,
                                       const std::string &ignore_subfield_codes) const
{
    values->clear();

    std::vector <std::string> individual_tags;
    StringUtil::Split(tags, ':', &individual_tags);
    for (const auto &tag : individual_tags) {
        size_t field_index(getFieldIndex(tag));
        while (field_index < directory_entries_.size() and directory_entries_[field_index].getTag() == tag) {
            const Subfields subfields(getSubfields(field_index));
            for (const auto &subfield : subfields) {
                if (ignore_subfield_codes.find(subfield.code_) == std::string::npos)
                    values->emplace_back(subfield.value_);
            }
            ++field_index;
        }
    }
    return values->size();
}


size_t MarcRecord::extractSubfield(const MarcTag &tag, const char subfield_code,
                                   std::vector <std::string> *const values) const
{
    values->clear();

    size_t field_index(getFieldIndex(tag));
    while (field_index < directory_entries_.size() and tag == directory_entries_[field_index].getTag()) {
        const Subfields subfields(getSubfields(field_index));
        const auto begin_end(subfields.getIterators(subfield_code));
        for (auto subfield_code_and_value(begin_end.first);
             subfield_code_and_value != begin_end.second; ++subfield_code_and_value)
            values->emplace_back(subfield_code_and_value->value_);
        ++field_index;
    }
    return values->size();
}


size_t MarcRecord::extractSubfields(const MarcTag &tag, const std::string &subfield_codes,
                                    std::vector <std::string> *const values) const
{
    values->clear();

    size_t field_index(getFieldIndex(tag));
    while (field_index < directory_entries_.size() and tag == directory_entries_[field_index].getTag()) {
        const Subfields subfields(getSubfields(field_index));
        for (const auto &subfield : subfields) {
            if (subfield_codes.find(subfield.code_) != std::string::npos)
                values->emplace_back(subfield.value_);
        }
        ++field_index;
    }

    return values->size();
}


size_t MarcRecord::findAllLocalDataBlocks(std::vector <std::pair<size_t, size_t>> *const local_block_boundaries) const {
    local_block_boundaries->clear();

    size_t local_block_start(getFieldIndex("LOK"));
    if (local_block_start == FIELD_NOT_FOUND)
        return 0;

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


static inline bool IndicatorsMatch(const std::string &indicator_pattern, const std::string &indicators) {
    if (indicator_pattern[0] != '?' and indicator_pattern[0] != indicators[0])
        return false;
    if (indicator_pattern[1] != '?' and indicator_pattern[1] != indicators[1])
        return false;
    return true;
}


size_t MarcRecord::findFieldsInLocalBlock(const MarcTag &field_tag, const std::string &indicators,
                                          const std::pair <size_t, size_t> &block_start_and_end,
                                          std::vector <size_t> *const field_indices) const
{
    field_indices->clear();
    if (unlikely(indicators.length() != 2))
        Error("in MarcRecord::FindFieldInLocalBlock: indicators must be precisely 2 characters long!");

    const std::string FIELD_PREFIX("  ""\x1F""0" + field_tag.to_string());
    for (size_t index(block_start_and_end.first); index < block_start_and_end.second; ++index) {
        const std::string &current_field(getFieldData(index));
        if (StringUtil::StartsWith(current_field, FIELD_PREFIX)
            and IndicatorsMatch(indicators, current_field.substr(7, 2)))
            field_indices->emplace_back(index);
    }
    return field_indices->size();
}


void MarcRecord::filterTags(const std::unordered_set <MarcTag> &drop_tags) {
    std::vector <std::pair<size_t, size_t>> deleted_blocks;
    for (auto entry = directory_entries_.begin(); entry < directory_entries_.end(); ++entry) {
        const auto tag_iter = drop_tags.find(entry->getTag());
        if (tag_iter == drop_tags.cend())
            continue;
        const size_t block_start = std::distance(directory_entries_.begin(), entry);
        for (/* empty */; entry < directory_entries_.end() and entry->getTag() == *tag_iter; ++entry);
        const size_t block_end = std::distance(directory_entries_.begin(), entry);

        deleted_blocks.emplace_back(block_start, block_end);
    }
    deleteFields(deleted_blocks);
}


std::string MarcRecord::getLanguage(const std::string &default_language_code) const {
    const std::string &language(extractFirstSubfield("041", 'a'));
    if (likely(not language.empty()))
        return language;
    return default_language_code;
}


std::string MarcRecord::getLanguageCode() const {
    const size_t _008_index(getFieldIndex("008"));
    if (_008_index == FIELD_NOT_FOUND)
        return "";
    // Language codes start at offset 35 and have a length of 3.
    const auto entry = directory_entries_[_008_index];
    if (entry.getFieldLength() < 38)
        return "";

    return std::string(raw_data_, entry.getFieldOffset() + 35, 3);
}


bool MarcRecord::isElectronicResource() const {
    if (std::toupper(leader_[6]) == 'M')
        return true;

    if (leader_.isMonograph()) {
        std::vector<size_t> _007_field_indices;
        getFieldIndices("007", &_007_field_indices);
        for (auto _007_field_index : _007_field_indices) {
            const std::string _007_field_contents(getFieldData(_007_field_index));
            if (not _007_field_contents.empty() and std::toupper(_007_field_contents[0]) == 'C')
                return true;
        }
    }

    return false;
}


void MarcRecord::combine(const MarcRecord &record) {
    const size_t offset(raw_data_.size() - record.directory_entries_[0].getFieldLength());
    raw_data_ += record.raw_data_.substr(record.directory_entries_[0].getFieldLength());

    // Ignore first field. We only need one 001-field.
    directory_entries_.reserve(directory_entries_.size() + record.directory_entries_.size() - 1);
    for (auto iter(record.directory_entries_.begin() + 1); iter < record.directory_entries_.end(); ++iter) {
        directory_entries_.emplace_back(*iter);
        directory_entries_.back().setFieldOffset(iter->getFieldOffset() + offset);
    }
}


std::string MarcRecord::calcChecksum() const {
    std::string blob;
    blob.reserve(200000); // Rougly twice the maximum size of a single MARC-21 record.

    blob += leader_.toString();
    for (const auto &dir_entry : directory_entries_)
        blob += dir_entry.toString();
    blob += raw_data_;

    return StringUtil::Sha1(blob);
}


bool MarcRecord::ProcessRecords(MarcReader * const marc_reader, RecordFunc process_record,
                                MarcWriter * const marc_writer, std::string * const err_msg)
{
    err_msg->clear();

    while (MarcRecord record = marc_reader->read()) {
        if (not (*process_record)(&record, marc_writer, err_msg))
            return false;
        err_msg->clear();
    }

    return err_msg->empty();
}


MarcRecord MarcRecord::ReadSingleRecord(File * const input) {
    MarcRecord record;
    if (input->eof())
        return record; // Return an empty instance!

    char leader_buf[Leader::LEADER_LENGTH];
    ssize_t read_count;
    const off_t record_start_pos(input->tell());
    if ((read_count = input->read(leader_buf, sizeof leader_buf)) != sizeof leader_buf) {
        if (read_count == 0)
            return record;
        throw std::runtime_error("in MarcRecord::ReadSingleRecord: failed to read leader bytes from \""
                                 + input->getPath() + "\"! (read count was " + std::to_string(read_count)
                                 + ", record_start_pos was " + std::to_string(record_start_pos) + ")");
    }

    std::string err_msg;
    if (not Leader::ParseLeader(std::string(leader_buf, Leader::LEADER_LENGTH), &record.getLeader(), &err_msg)) {
        err_msg.append(" (Bad record started at file offset " + std::to_string(record_start_pos) + " in "
                       + input->getPath() + ".)");
        throw std::runtime_error("in MarcRecord::ReadSingleRecord: failed to parse leader bytes: " + err_msg);
    }

    //
    // Parse directory entries.
    //

    const size_t DIRECTORY_LENGTH(record.getLeader().getBaseAddressOfData() - Leader::LEADER_LENGTH);
#pragma GCC diagnostic ignored "-Wvla"
    char directory_buf[DIRECTORY_LENGTH];
#pragma GCC diagnostic warning "-Wvla"
    if ((read_count = input->read(directory_buf, DIRECTORY_LENGTH)) != static_cast<ssize_t>(DIRECTORY_LENGTH))
        throw std::runtime_error("in MarcRecord::ReadSingleRecord: Short read for a directory or premature EOF in "
                                 + input->getPath() + "! (read count was " + std::to_string(read_count)
                                 + ", record_start_pos was " + std::to_string(record_start_pos) + ")");

    if (not DirectoryEntry::ParseDirEntries(std::string(directory_buf, DIRECTORY_LENGTH), &record.directory_entries_,
                                            &err_msg))
        throw std::runtime_error("in MarcRecord::ReadSingleRecord: failed to parse directory entries: " + err_msg);

    //
    // Read variable fields.
    //

    const size_t FIELD_DATA_SIZE(record.getLeader().getRecordLength() - record.getLeader().getBaseAddressOfData());
#pragma GCC diagnostic ignored "-Wvla"
    char raw_field_data[FIELD_DATA_SIZE];
#pragma GCC diagnostic warning "-Wvla"
    if ((read_count = input->read(raw_field_data, FIELD_DATA_SIZE)) != static_cast<ssize_t>(FIELD_DATA_SIZE))
        throw std::runtime_error("in MarcRecord::ReadSingleRecord: Short read for field data or premature EOF in "
                                 + input->getPath() + "! (Expected " + std::to_string(FIELD_DATA_SIZE)
                                 + " bytes, got " + std::to_string(read_count) + " bytes, record_start_pos was "
                                 + std::to_string(record_start_pos) + ", current: " + std::to_string(input->tell())
                                 + ")");

    // Sanity check for record end:
    if (raw_field_data[FIELD_DATA_SIZE - 1] != '\x1D')
        throw std::runtime_error("in MarcRecord::ReadSingleRecord: Record does not end with \\x1D! (in "
                                 + input->getPath() + ", record_start_pos was " + std::to_string(record_start_pos)
                                 + ", current: " + std::to_string(input->tell()) + ")");

    record.raw_data_.append(raw_field_data, FIELD_DATA_SIZE);

    return record;
}

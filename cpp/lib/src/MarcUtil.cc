#include "MarcUtil.h"
#include "Compiler.h"
#include <algorithm>
#include <iterator>
#include <memory>
#include <stdexcept>
#include "FileUtil.h"
#include "StringUtil.h"
#include "Subfields.h"
#include "util.h"


// Creates a binary, a.k.a. "raw" representation of a MARC21 record.
static std::string ComposeRecord(const std::vector<DirectoryEntry> &dir_entries, const std::vector<std::string> &fields,
				 Leader * const leader)
{
    size_t record_size(Leader::LEADER_LENGTH);
    const size_t directory_size(dir_entries.size() * DirectoryEntry::DIRECTORY_ENTRY_LENGTH);
    record_size += directory_size;
    ++record_size; // field terminator
    for (const auto &dir_entry : dir_entries)
        record_size += dir_entry.getFieldLength();
    ++record_size; // record terminator

    leader->setRecordLength(record_size);
    leader->setBaseAddressOfData(Leader::LEADER_LENGTH + directory_size + 1);

    std::string record;
    record.reserve(record_size);
    record += leader->toString();
    for (const auto &dir_entry : dir_entries)
        record += dir_entry.toString();
    record += '\x1E';
    for (const auto &field : fields) {
        record += field;
        record += '\x1E';
    }
    record += '\x1D';

    return record;
}


static bool ReadFields(const std::string &raw_fields, const std::vector<DirectoryEntry> &dir_entries,
                       std::vector<std::string> * const fields, std::string * const err_msg)
{
    fields->clear();
    fields->reserve(dir_entries.size());

    if (raw_fields[raw_fields.size() - 1] != '\x1D') {
        *err_msg = "missing trailing record terminator!";
        return false;
    }

    size_t field_start(0);
    for (const auto &dir_entry : dir_entries) {
        const size_t next_field_start = field_start + dir_entry.getFieldLength();
        if (next_field_start >= raw_fields.length()) {
            *err_msg = "misaligned field, extending past the record!";
            return false;
        }

        const std::string field(raw_fields.substr(field_start, dir_entry.getFieldLength()));
        if (field[field.length() - 1] != '\x1E') {
            *err_msg = "missing field terminator at end of field!";
            return false;
        }

        fields->push_back(field.substr(0, field.length() - 1));
        field_start = next_field_start;
    }

    if (field_start + 1 != raw_fields.length()) {
        *err_msg = "field extents do not exhaust record!";
        return false;
    }

    return true;
}


namespace MarcUtil {


Record::Record(FILE * const input) {
    if (std::feof(input))
	return; // Create an empty instance!

    //
    // Read leader.
    //

    char leader_buf[Leader::LEADER_LENGTH];
    ssize_t read_count;
    const long record_start_pos(std::ftell(input));
    if ((read_count = std::fread(leader_buf, sizeof leader_buf, 1, input)) != 1) {
	if (read_count == 0)
	    return;
        throw std::runtime_error("in MarcUtil::Record::Record: failed to read leader bytes from \""
				 + FileUtil::GetFileName(input) + "\"! (read count was " + std::to_string(read_count)
				 + ", record_start_pos was " + std::to_string(record_start_pos) + ")");
    }

    std::string err_msg;
    if (not Leader::ParseLeader(std::string(leader_buf, Leader::LEADER_LENGTH), &leader_, &err_msg)) {
        err_msg.append(" (Bad record started at file offset " + std::to_string(record_start_pos) + ".)");
        throw std::runtime_error("in MarcUtil::Record::Record: failed to parse leader bytes: " + err_msg);
    }

    raw_record_.reserve(leader_.getRecordLength());
    raw_record_.append(leader_buf, Leader::LEADER_LENGTH);

    //
    // Parse directory entries.
    //

    #pragma GCC diagnostic ignored "-Wvla"
    const ssize_t directory_length(leader_.getBaseAddressOfData() - Leader::LEADER_LENGTH);
    char directory_buf[directory_length];
    #pragma GCC diagnostic warning "-Wvla"
    if ((read_count = std::fread(directory_buf, 1, directory_length, input)) != directory_length)
        throw std::runtime_error("in MarcUtil::Record::Record: Short read for a directory or premature EOF!");

    if (not DirectoryEntry::ParseDirEntries(std::string(directory_buf, directory_length), &dir_entries_, &err_msg))
        throw std::runtime_error("in MarcUtil::Record::Record: failed to parse directory entries: " + err_msg);

    raw_record_.append(directory_buf, directory_length);

    //
    // Parse variable fields.
    //

    const size_t field_data_size(leader_.getRecordLength() - Leader::LEADER_LENGTH - directory_length);
    #pragma GCC diagnostic ignored "-Wvla"
    char raw_field_data[field_data_size];
    #pragma GCC diagnostic warning "-Wvla"
    if ((read_count = std::fread(raw_field_data, 1, field_data_size, input))
        != static_cast<ssize_t>(field_data_size))
        throw std::runtime_error("in MarcUtil::Record::Record: Short read for field data or premature EOF! (Expected "
				 + std::to_string(field_data_size) + " bytes, got "+ std::to_string(read_count) +" bytes.)");

    // Sanity check for record end:
    if (raw_field_data[field_data_size - 1] != '\x1D')
        throw std::runtime_error("in MarcUtil::Record::Record: Record does not end with \\x1D!");

    if (not ReadFields(std::string(raw_field_data, field_data_size), dir_entries_, &fields_, &err_msg))
        throw std::runtime_error("in MarcUtil::Record::Record: error while trying to parse field data: " + err_msg);

    raw_record_.append(raw_field_data, field_data_size);
}


bool Record::recordSeemsCorrect(std::string * const err_msg) const {
    if (raw_record_is_out_of_date_)
	UpdateRawRecord();

    if (raw_record_.size() < Leader::LEADER_LENGTH) {
        *err_msg = "record too small to contain leader!";
        return false;
    }

    if (leader_.getRecordLength() != raw_record_.length()) {
        *err_msg = "leader's record length (" + std::to_string(leader_.getRecordLength())
                   + ") does not equal actual record length (" + std::to_string(raw_record_.length()) + ")!";
        return false;
    }

    if (raw_record_.length() > 99999) {
        *err_msg = "record length (" + std::to_string(raw_record_.length())
                   + ") exceeds maxium legal record length (99999)!";
        return false;
    }

    if (leader_.getBaseAddressOfData() <= Leader::LEADER_LENGTH) {
        *err_msg = "impossible base address of data!";
        return false;
    }

    const size_t directory_length(leader_.getBaseAddressOfData() - Leader::LEADER_LENGTH);
    if ((directory_length % DirectoryEntry::DIRECTORY_ENTRY_LENGTH) != 1) {
        *err_msg = "directory length " + std::to_string(directory_length) + " is not a multiple of "
                   + std::to_string(DirectoryEntry::DIRECTORY_ENTRY_LENGTH) + " plus 1 for the RS at the end!";
        return false;
    }

    if (raw_record_[leader_.getBaseAddressOfData() - 1] != '\x1E') {
        *err_msg = "directory is not terminated with a field terminator!";
        return false;
    }

    if (not DirectoryEntry::ParseDirEntries(raw_record_.substr(Leader::LEADER_LENGTH, directory_length),
                                            &dir_entries_, err_msg))
    {
        *err_msg = "failed to parse directory entries!";
        return false;
    }

    size_t expected_start_offset(Leader::LEADER_LENGTH
                                 + dir_entries_.size() * DirectoryEntry::DIRECTORY_ENTRY_LENGTH
                                 + 1 /* For the field terminator at the end of the directory. */);
    for (const auto &dir_entry : dir_entries_) {
        const size_t dir_entry_start_offset(dir_entry.getFieldOffset() + leader_.getBaseAddressOfData());
        if (dir_entry_start_offset != expected_start_offset) {
            *err_msg = "expected field start offset (" + std::to_string(expected_start_offset)
                       + ") does not equal the field start offset in the corresponding directory entry ("
                       + std::to_string(dir_entry_start_offset) + ")!";
            return false;
        }
        expected_start_offset += dir_entry.getFieldLength();
    }

    const size_t computed_length(expected_start_offset + 1 /* For the record terminator. */);
    if (computed_length != raw_record_.length()) {
        *err_msg = "actual record length (" + std::to_string(raw_record_.length())
                   + ") does not equal the sum of the computed component lengths ("
                   + std::to_string(computed_length) + ")!";
        return false;
    }

    const size_t field_data_size(raw_record_.length() - Leader::LEADER_LENGTH - directory_length);
    if (not ReadFields(raw_record_.substr(Leader::LEADER_LENGTH + directory_length, field_data_size),
                       dir_entries_, &fields_, err_msg))
    {
        *err_msg = "failed to parse fields!";
        return false;
    }

    if (raw_record_[raw_record_.size() - 1] != '\x1D') {
        *err_msg = "record is not terminated with a record terminator!";
        return false;
    }

    return true;
}


namespace {


class MatchTag {
    const std::string tag_to_match_;
public:
    explicit MatchTag(const std::string &tag_to_match): tag_to_match_(tag_to_match) { }
    bool operator()(const DirectoryEntry &dir_entry) const { return dir_entry.getTag() == tag_to_match_; }
};


} // unnamed namespace


ssize_t Record::getFieldIndex(const std::string &field_tag) const {
    const auto iter(std::find_if(dir_entries_.begin(), dir_entries_.end(), MatchTag(field_tag)));
    return (iter == dir_entries_.end()) ? -1 : std::distance(dir_entries_.begin(), iter);
}


std::string Record::getLanguage(const std::string &default_language_code) const {
    const ssize_t index(getFieldIndex("041"));
    if (index == -1)
        return default_language_code;

    const Subfields subfields(fields_[index]);
    if (not subfields.hasSubfield('a'))
        return default_language_code;

    return subfields.getIterators('a').first->second;
}


std::string Record::getLanguageCode() const {
    const ssize_t _008_index(getFieldIndex("008"));
    if (_008_index == -1)
        return "";

    // Language codes start at offset 35 and have a length of 3.
    if (fields_[_008_index].length() < 38)
        return "";

    return fields_[_008_index].substr(35, 3);
}


void Record::updateField(const size_t field_index, const std::string &new_field_contents) {
    if (unlikely(field_index >= dir_entries_.size()))
        throw std::runtime_error("in MarcUtil::Record::updateField: \"field_index\" (" + std::to_string(field_index)
                                 + ") out of range!  (size: " + std::to_string(dir_entries_.size()) + ")");
    const size_t delta(new_field_contents.length() - fields_[field_index].length());
    leader_.setRecordLength(leader_.getRecordLength() + delta);
    dir_entries_[field_index].setFieldLength(new_field_contents.length() + 1 /* field terminator */);
    fields_[field_index] = new_field_contents;

    // Correct the offsets for fields following the slot where the field was updated:
    for (auto dir_entry = dir_entries_.begin() + field_index + 1; dir_entry != dir_entries_.end(); ++dir_entry)
        dir_entry->setFieldOffset(dir_entry->getFieldOffset() + delta);

    raw_record_is_out_of_date_ = true;
}


void Record::insertField(const std::string &new_field_tag, const std::string &new_field_value) {
    if (new_field_tag.length() != 3)
	throw std::runtime_error("in MarcUtil::Record::insertField: \"new_field_tag\" must have a length of 3!");

    leader_.setRecordLength(leader_.getRecordLength() + new_field_value.length()
                            + DirectoryEntry::DIRECTORY_ENTRY_LENGTH + 1 /* For new field separator. */);
    leader_.setBaseAddressOfData(leader_.getBaseAddressOfData() + DirectoryEntry::DIRECTORY_ENTRY_LENGTH);

    // Find the insertion location:
    auto dir_entry(dir_entries_.begin());
    while (dir_entry != dir_entries_.end() and new_field_tag > dir_entry->getTag())
        ++dir_entry;

    if (dir_entry == dir_entries_.end()) {
        auto previous_dir_entry = (dir_entries_.end() - 1);
        const size_t offset = previous_dir_entry->getFieldOffset() + previous_dir_entry->getFieldLength();
        dir_entries_.emplace_back(new_field_tag, new_field_value.length() + 1, offset);
        fields_.emplace_back(new_field_value);
        return;
    }

    const auto insertion_location(dir_entry);
    const auto insertion_offset(dir_entry->getFieldOffset());

    // Correct the offsets for old fields following the slot where the new field will be inserted:
    const std::vector<DirectoryEntry>::difference_type insertion_index(insertion_location - dir_entries_.begin());
    for (dir_entry = dir_entries_.begin() + insertion_index; dir_entry != dir_entries_.end(); ++dir_entry)
        dir_entry->setFieldOffset(dir_entry->getFieldOffset() + new_field_value.length() + 1);

    // Now insert the new field:
    dir_entries_.emplace(insertion_location, new_field_tag, new_field_value.length() + 1, insertion_offset);
    const auto field(fields_.begin() + insertion_index);
    fields_.emplace(field, new_field_value);

    raw_record_is_out_of_date_ = true;
}


void Record::deleteField(const size_t field_index) {
    if (unlikely(field_index >= dir_entries_.size()))
        throw std::runtime_error("in MarcUtil::deleteField: \"field_index\" (" + std::to_string(field_index)
                                 + ") out of range! (size: " + std::to_string(dir_entries_.size()) + ")" );

    const size_t deleted_field_size(fields_[field_index].length() + 1 /* field terminator */);
    leader_.setRecordLength(leader_.getRecordLength() - deleted_field_size - DirectoryEntry::DIRECTORY_ENTRY_LENGTH);
    leader_.setBaseAddressOfData(leader_.getBaseAddressOfData() - DirectoryEntry::DIRECTORY_ENTRY_LENGTH);

    dir_entries_.erase(dir_entries_.begin() + field_index);
    fields_.erase(fields_.begin() + field_index);

    // Correct the offsets for fields following the slot where the field was deleted:
    for (auto dir_entry = dir_entries_.begin() + field_index; dir_entry != dir_entries_.end(); ++dir_entry)
        dir_entry->setFieldOffset(dir_entry->getFieldOffset() - deleted_field_size);

    raw_record_is_out_of_date_ = true;
}


size_t Record::extractAllSubfields(const std::string &tags, std::vector<std::string> * const values,
				   const std::string &ignore_subfield_codes) const
{
    values->clear();

    std::vector<std::string> individual_tags;
    StringUtil::Split(tags, ':', &individual_tags);
    for (const auto &tag : individual_tags) {
        const ssize_t field_index(getFieldIndex(tag));
        if (field_index == -1)
            continue;

        const Subfields subfields(fields_[field_index]);
        for (const auto &subfield_code_and_value : subfields.getAllSubfields()) {
            if (ignore_subfield_codes.find(subfield_code_and_value.first) == std::string::npos)
                values->emplace_back(subfield_code_and_value.second);
        }
    }

    return values->size();
}


size_t Record::extractAllSubfields(const std::string &tags,
				   std::vector<std::pair<char, std::string>> * const subfield_codes_and_values,
				   const std::string &ignore_subfield_codes) const
{
    subfield_codes_and_values->clear();

    std::vector<std::string> individual_tags;
    StringUtil::Split(tags, ':', &individual_tags);
    for (const auto &tag : individual_tags) {
        const ssize_t field_index(getFieldIndex(tag));
        if (field_index == -1)
            continue;

        const Subfields subfields(fields_[field_index]);
        for (const auto &subfield_code_and_value : subfields.getAllSubfields()) {
            if (ignore_subfield_codes.find(subfield_code_and_value.first) == std::string::npos)
                subfield_codes_and_values->emplace_back(subfield_code_and_value);
        }
    }

    return subfield_codes_and_values->size();
}
    

size_t Record::extractSubfield(const std::string &tag, const char subfield_code, std::vector<std::string> * const values) const {
    values->clear();

    ssize_t field_index(getFieldIndex(tag));
    if (field_index == -1)
	return 0;

    while (static_cast<size_t>(field_index) < dir_entries_.size() and tag == dir_entries_[field_index].getTag()) {
        const Subfields subfields(fields_[field_index]);
	const auto begin_end(subfields.getIterators(subfield_code));
	for (auto subfield_code_and_value(begin_end.first); subfield_code_and_value != begin_end.second; ++subfield_code_and_value)
	    values->emplace_back(subfield_code_and_value->second);
	++field_index;
    }

    return values->size();
}


void Record::filterTags(const std::unordered_set<std::string> &drop_tags) {
    std::vector<size_t> matched_slots;
    matched_slots.reserve(dir_entries_.size());

    unsigned slot_no(0);
    for (const auto &dir_entry : dir_entries_) {
        if (drop_tags.find(dir_entry.getTag()) != drop_tags.end())
            matched_slots.push_back(slot_no);
        ++slot_no;
    }

    if (matched_slots.empty())
        return;
    const size_t new_size(dir_entries_.size() - matched_slots.size());

    std::vector<DirectoryEntry> old_dir_entries;
    dir_entries_.swap(old_dir_entries);
    dir_entries_.reserve(new_size);

    std::vector<std::string> old_fields;
    fields_.swap(old_fields);
    fields_.reserve(new_size);

    std::vector<size_t>::const_iterator matched_slot(matched_slots.begin());
    for (unsigned slot(0); slot < old_dir_entries.size(); ++slot) {
        if (matched_slot != matched_slots.end() and *matched_slot == slot)
            ++matched_slot; // skip tag and field
        else {
            dir_entries_.push_back(std::move(old_dir_entries[slot]));
            fields_.push_back(std::move(old_fields[slot]));
        }
    }

    raw_record_is_out_of_date_ = true;
}


std::string Record::extractFirstSubfield(const std::string &tag, const char subfield_code) const {
    auto const entry_iterator(DirectoryEntry::FindField(tag, dir_entries_));
    if (entry_iterator == dir_entries_.end())
        return "";

    const Subfields subfields(fields_[entry_iterator - dir_entries_.begin()]);
    return subfields.getFirstSubfieldValue(subfield_code);
}


size_t Record::findAllLocalDataBlocks(std::vector<std::pair<size_t, size_t>> * const local_block_boundaries) const {
    local_block_boundaries->clear();

    ssize_t local_block_start(getFieldIndex("LOK"));
    if (local_block_start == -1)
        return 0;

    size_t local_block_end(static_cast<size_t>(local_block_start) + 1);
    while (local_block_end < dir_entries_.size()) {
        if (StringUtil::StartsWith(fields_[local_block_end], "  ""\x1F""0000")) {
            local_block_boundaries->emplace_back(std::make_pair(static_cast<size_t>(local_block_start),
                                                                local_block_end));
            local_block_start = static_cast<ssize_t>(local_block_end);
        }
        ++local_block_end;
    }
    local_block_boundaries->emplace_back(std::make_pair(static_cast<size_t>(local_block_start), local_block_end));

    return local_block_boundaries->size();
}


static bool IndicatorsMatch(const std::string &indicator_pattern, const std::string &indicators) {
    if (indicator_pattern[0] != '?' and indicator_pattern[0] != indicators[0])
        return false;
    if (indicator_pattern[1] != '?' and indicator_pattern[1] != indicators[1])
        return false;
    return true;
}
    

size_t Record::findFieldsInLocalBlock(const std::string &field_tag, const std::string &indicators,
				      const std::pair<size_t, size_t> &block_start_and_end,
				      std::vector<size_t> * const field_indices) const
{
    field_indices->clear();

    if (unlikely(field_tag.length() != 3))
        Error("in MarcUtil::FindFieldInLocalBlock: field_tag must be precisely 3 characters long!");
    if (unlikely(indicators.length() != 2))
        Error("in MarcUtil::FindFieldInLocalBlock: indicators must be precisely 2 characters long!");

    const std::string FIELD_PREFIX("  ""\x1F""0" + field_tag);
    for (size_t index(block_start_and_end.first); index < block_start_and_end.second; ++index) {
        const std::string &current_field(fields_[index]);
        if (StringUtil::StartsWith(current_field, FIELD_PREFIX)
            and IndicatorsMatch(indicators, current_field.substr(7, 2)))
            field_indices->emplace_back(index);
    }

    return field_indices->size();
}


void Record::write(FILE * const output) const {
    const std::string &raw_record(getRawRecord());

    const size_t write_count(std::fwrite(raw_record.data(), 1, raw_record.size(), output));
    if (write_count != raw_record.size())
        Error("Failed to write " + std::to_string(raw_record.size()) + " bytes to MARC output!");
}


void Record::UpdateRawRecord() const {
    raw_record_ = ComposeRecord(dir_entries_, fields_, &leader_);
    raw_record_is_out_of_date_ = false;
}


bool ProcessRecords(FILE * const input, RecordFunc process_record, std::string * const err_msg) {
    err_msg->clear();

    while (std::feof(input) == 0) {
	Record record(input);
        if (not (*process_record)(&record, err_msg))
            return false;
        err_msg->clear();
    }

    return err_msg->empty();
}


} // namespace MarcUtil

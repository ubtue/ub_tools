#include "MarcUtil.h"
#include "Compiler.h"
#include <algorithm>
#include <iterator>
#include <memory>
#include <stdexcept>
#include "StringUtil.h"
#include "Subfields.h"
#include "util.h"

namespace MarcUtil {


bool ReadFields(const std::string &raw_fields, const std::vector<DirectoryEntry> &dir_entries,
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


namespace {


class MatchTag {
    const std::string tag_to_match_;
public:
    explicit MatchTag(const std::string &tag_to_match): tag_to_match_(tag_to_match) { }
    bool operator()(const DirectoryEntry &dir_entry) const { return dir_entry.getTag() == tag_to_match_; }
};


} // unnamed namespace


ssize_t GetFieldIndex(const std::vector<DirectoryEntry> &dir_entries, const std::string &field_tag)
{
    const auto iter(std::find_if(dir_entries.begin(), dir_entries.end(), MatchTag(field_tag)));
    return (iter == dir_entries.end()) ? -1 : std::distance(dir_entries.begin(), iter);
}


// Returns false on error and EOF.  To distinguish between the two: on EOF "err_msg" is empty but not when an
// error has been detected.  For each entry in "dir_entries" there will be a corresponding entry in "field_data".
bool ReadNextRecord(FILE * const input, std::shared_ptr<Leader> &leader,
		    std::vector<DirectoryEntry> * const dir_entries, std::vector<std::string> * const field_data,
		    std::string * const err_msg, std::string * const entire_record)
{
    dir_entries->clear();
    field_data->clear();
    err_msg->clear();
    if (entire_record != nullptr)
	entire_record->clear();

    //
    // Read leader.
    //

    char leader_buf[Leader::LEADER_LENGTH];
    ssize_t read_count;
    const long record_start_pos(std::ftell(input));
    if ((read_count = std::fread(leader_buf, sizeof leader_buf, 1, input)) != 1)
        return false;

    if (not Leader::ParseLeader(std::string(leader_buf, Leader::LEADER_LENGTH), leader, err_msg)) {
	err_msg->append(" (Bad record started at file offset " + std::to_string(record_start_pos) + ".)");
        return false;
    }

    if (entire_record != nullptr) {
	entire_record->reserve(leader->getRecordLength());
	entire_record->append(leader_buf, Leader::LEADER_LENGTH);
    }

    //
    // Parse directory entries.
    //

    #pragma GCC diagnostic ignored "-Wvla"
    const ssize_t directory_length(leader->getBaseAddressOfData() - Leader::LEADER_LENGTH);
    char directory_buf[directory_length];
    #pragma GCC diagnostic warning "-Wvla"
    if ((read_count = std::fread(directory_buf, 1, directory_length, input)) != directory_length) {
        *err_msg = "Short read for a directory or premature EOF!";
        return false;
    }

    if (not DirectoryEntry::ParseDirEntries(std::string(directory_buf, directory_length), dir_entries, err_msg))
        return false;

    if (entire_record != nullptr)
	entire_record->append(directory_buf, directory_length);

    //
    // Parse variable fields.
    //

    const size_t field_data_size(leader->getRecordLength() - Leader::LEADER_LENGTH - directory_length);
    #pragma GCC diagnostic ignored "-Wvla"
    char raw_field_data[field_data_size];
    #pragma GCC diagnostic warning "-Wvla"
    if ((read_count = std::fread(raw_field_data, 1, field_data_size, input))
        != static_cast<ssize_t>(field_data_size))
    {
        *err_msg = "Short read for field data or premature EOF! (Expected "
                   + std::to_string(field_data_size) + " bytes, got "+ std::to_string(read_count) +" bytes.)";
        return false;
    }

    // Sanity check for record end:
    if (raw_field_data[field_data_size - 1] != '\x1D') {
	*err_msg = "Record does not end with \\x1D!";
        return false;
    }

    if (not ReadFields(std::string(raw_field_data, field_data_size), *dir_entries, field_data, err_msg))
        return false;

    if (entire_record != nullptr)
	entire_record->append(raw_field_data, field_data_size);

    return true;
}


bool ProcessRecords(FILE * const input, RecordFunc process_record, std::string * const err_msg) {
    err_msg->clear();

    std::shared_ptr<Leader> leader;
    std::vector<DirectoryEntry> dir_entries;
    std::vector<std::string> field_data;
    while (MarcUtil::ReadNextRecord(input, leader, &dir_entries, &field_data, err_msg)) {
	if (not (*process_record)(leader, &dir_entries, &field_data, err_msg))
	    return false;
	err_msg->clear();
    }

    return err_msg->empty();
}


void InsertField(const std::string &new_contents, const std::string &new_tag, const std::shared_ptr<Leader> &leader,
                 std::vector<DirectoryEntry> * const dir_entries, std::vector<std::string> * const fields)
{
    leader->setRecordLength(leader->getRecordLength() + new_contents.length()
                            + DirectoryEntry::DIRECTORY_ENTRY_LENGTH + 1 /* For new field separator. */);
    leader->setBaseAddressOfData(leader->getBaseAddressOfData() + DirectoryEntry::DIRECTORY_ENTRY_LENGTH);

    // Find the insertion location:
    auto dir_entry(dir_entries->begin());
    while (dir_entry != dir_entries->end() and new_tag > dir_entry->getTag())
        ++dir_entry;

    if (dir_entry == dir_entries->end()) {
	dir_entries->emplace_back(new_tag, new_contents.length() + 1, (dir_entries->end() - 1)->getFieldOffset());
	fields->emplace_back(new_contents);
	return;
    }

    const auto insertion_location(dir_entry);

    // Correct the offsets for old fields following the slot where the new field will be inserted:
    const std::vector<DirectoryEntry>::difference_type insertion_index(insertion_location - dir_entries->begin());
    for (dir_entry = dir_entries->begin() + insertion_index; dir_entry != dir_entries->end(); ++dir_entry)
        dir_entry->setFieldOffset(dir_entry->getFieldOffset() + new_contents.length() + 1);

    // Now insert the new field:
    dir_entries->emplace(insertion_location, new_tag, new_contents.length() + 1,
                         (insertion_location - 1)->getFieldOffset());
    const auto field(fields->begin() + insertion_index);
    fields->emplace(field, new_contents);
}


// Creates a binary, a.k.a. "raw" representation of a MARC21 record.
std::string ComposeRecord(const std::vector<DirectoryEntry> &dir_entries, const std::vector<std::string> &fields,
                          const std::shared_ptr<Leader> &leader)
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


// Performs a few sanity checks.
bool RecordSeemsCorrect(const std::string &record, std::string * const err_msg) {
    if (record.size() < Leader::LEADER_LENGTH) {
        *err_msg = "record too small to contain leader!";
        return false;
    }

    std::shared_ptr<Leader> leader;
    if (not Leader::ParseLeader(record.substr(0, Leader::LEADER_LENGTH), leader, err_msg)) {
	*err_msg = "failed to parse the leader!";
        return false;
    }

    if (leader->getRecordLength() != record.length()) {
        *err_msg = "leader's record length (" + std::to_string(leader->getRecordLength())
                   + ") does not equal actual record length (" + std::to_string(record.length()) + ")!";
        return false;
    }

    if (record.length() > 99999) {
        *err_msg = "record length (" + std::to_string(record.length())
                   + ") exceeds maxium legal record length (99999)!";
        return false;
    }

    if (leader->getBaseAddressOfData() <= Leader::LEADER_LENGTH) {
        *err_msg = "impossible base address of data!";
        return false;
    }

    const size_t directory_length(leader->getBaseAddressOfData() - Leader::LEADER_LENGTH);
    if ((directory_length % DirectoryEntry::DIRECTORY_ENTRY_LENGTH) != 1) {
        *err_msg = "directory length " + std::to_string(directory_length) + " is not a multiple of "
                   + std::to_string(DirectoryEntry::DIRECTORY_ENTRY_LENGTH) + " plus 1 for the RS at the end!";
        return false;
    }

    if (record[leader->getBaseAddressOfData() - 1] != '\x1E') {
        *err_msg = "directory is not terminated with a field terminator!";
        return false;
    }

    std::vector<DirectoryEntry> dir_entries;
    if (not DirectoryEntry::ParseDirEntries(record.substr(Leader::LEADER_LENGTH, directory_length),
					    &dir_entries, err_msg))
    {
	*err_msg = "failed to parse directory entries!";
        return false;
    }

    size_t expected_start_offset(Leader::LEADER_LENGTH
                                 + dir_entries.size() * DirectoryEntry::DIRECTORY_ENTRY_LENGTH
				 + 1 /* For the field terminator at the end of the directory. */);
    for (const auto &dir_entry : dir_entries) {
	const size_t dir_entry_start_offset(dir_entry.getFieldOffset() + leader->getBaseAddressOfData());
	if (dir_entry_start_offset != expected_start_offset) {
	    *err_msg = "expected field start offset (" + std::to_string(expected_start_offset)
		       + ") does not equal the field start offset in the corresponding directory entry ("
		       + std::to_string(dir_entry_start_offset) + ")!";
	    return false;
	}
	expected_start_offset += dir_entry.getFieldLength();
    }

    const size_t computed_length(expected_start_offset + 1 /* For the record terminator. */);
    if (computed_length != record.length()) {
        *err_msg = "actual record length (" + std::to_string(record.length())
	           + ") does not equal the sum of the computed component lengths ("
	           + std::to_string(computed_length) + ")!";
        return false;
    }

    const size_t field_data_size(record.length() - Leader::LEADER_LENGTH - directory_length);
    std::vector<std::string> field_data;
    if (not ReadFields(record.substr(Leader::LEADER_LENGTH + directory_length, field_data_size),
		       dir_entries, &field_data, err_msg))
    {
	*err_msg = "failed to parse fields!";
        return false;
    }

    if (record[record.size() - 1] != '\x1D') {
        *err_msg = "record is not terminated with a record terminator!";
        return false;
    }

    return true;
}


void ComposeAndWriteRecord(FILE * const output, const std::vector<DirectoryEntry> &dir_entries,
                           const std::vector<std::string> &field_data,
			   const std::shared_ptr<Leader> &leader)
{
    std::string err_msg;
    const std::string record(MarcUtil::ComposeRecord(dir_entries, field_data, leader));
    if (not MarcUtil::RecordSeemsCorrect(record, &err_msg))
        Error("Bad record! (" + err_msg + ")");

    const size_t write_count = std::fwrite(record.data(), 1, record.size(), output);
    if (write_count != record.size())
        Error("Failed to write " + std::to_string(record.size()) + " bytes to MARC output!");

}


void UpdateField(const size_t field_index, const std::string &new_field_contents,
		 const std::shared_ptr<Leader> &leader, std::vector<DirectoryEntry> * const dir_entries,
		 std::vector<std::string> * const field_data)
{
    if (unlikely(field_index >= dir_entries->size()))
        throw std::runtime_error("in MarcUtil::UpdateField: \"field_index\" (" + std::to_string(field_index)
                                 + ") out of range!");

    leader->setRecordLength(leader->getRecordLength() + new_field_contents.length()
                            - (*field_data)[field_index].length());
    (*dir_entries)[field_index].setFieldLength(new_field_contents.length() + 1 /* field terminator */);
    (*field_data)[field_index] = new_field_contents;

}


std::string GetLanguage(const std::vector<DirectoryEntry> &dir_entries, const std::vector<std::string> &fields,
                        const std::string &default_language_code)
{
    const ssize_t index(GetFieldIndex(dir_entries, "041"));
    if (index == -1)
        return default_language_code;

    const Subfields subfields(fields[index]);
    if (not subfields.hasSubfield('a'))
        return default_language_code;

    return subfields.getIterators('a').first->second;
}


std::string GetLanguageCode(const std::vector<DirectoryEntry> &dir_entries, const std::vector<std::string> &fields) {
    const ssize_t _008_index(MarcUtil::GetFieldIndex(dir_entries, "008"));
    if (_008_index == -1)
        return "";

    // Language codes start at offset 35 and have a length of 3.
    if (fields[_008_index].length() < 38)
        return "";

    return fields[_008_index].substr(35, 3);
}


std::string ExtractFirstSubfield(const std::string &tag, const char subfield_code,
				 const std::vector<DirectoryEntry> &dir_entries,
				 const std::vector<std::string> &field_data)
{
    auto const entry_iterator(DirectoryEntry::FindField(tag, dir_entries));
    if (entry_iterator == dir_entries.end())
	return "";

    const Subfields subfields(field_data[entry_iterator - dir_entries.begin()]);
    return subfields.getFirstSubfieldValue(subfield_code);
}


size_t ExtractAllSubfields(const std::string &tags, const std::vector<DirectoryEntry> &dir_entries,
			   const std::vector<std::string> &field_data, std::vector<std::string> * const values,
			   const std::string &ignore_subfield_codes)
{
    values->clear();

    std::vector<std::string> fields;
    StringUtil::Split(tags, ':', &fields);
    for (const auto &field : fields) {
	const ssize_t field_index(MarcUtil::GetFieldIndex(dir_entries, field));
	if (field_index == -1)
	    continue;

	const Subfields subfields(field_data[field_index]);
	for (const auto &subfield_code_and_value : subfields.getAllSubfields()) {
	    if (ignore_subfield_codes.find(subfield_code_and_value.first) == std::string::npos)
		values->emplace_back(subfield_code_and_value.second);
	}
    }

    return values->size();
}


size_t FindAllLocalDataBlocks(const std::vector<DirectoryEntry> &dir_entries,
			      const std::vector<std::string> &field_data,
			      std::vector<std::pair<size_t, size_t>> * const local_block_boundaries)
{
    local_block_boundaries->clear();

    ssize_t local_block_start(GetFieldIndex(dir_entries, "LOK"));
    if (local_block_start == -1)
	return 0;

    size_t local_block_end(static_cast<size_t>(local_block_start) + 1);
    while (local_block_end < dir_entries.size()) {
	if (StringUtil::StartsWith(field_data[local_block_end], "  ""\x1F""0000")) {
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
    

size_t FindFieldsInLocalBlock(const std::string &field_tag, const std::string &indicators,
			      const std::pair<size_t, size_t> &block_start_and_end,
			      const std::vector<std::string> &field_data,
			      std::vector<size_t> * const field_indices)
{
    field_indices->clear();

    if (unlikely(field_tag.length() != 3))
	Error("in MarcUtil::FindFieldInLocalBlock: field_tag must be precisely 3 characters long!");
    if (unlikely(indicators.length() != 2))
	Error("in MarcUtil::FindFieldInLocalBlock: indicators must be precisely 2 characters long!");

    const std::string FIELD_PREFIX("  ""\x1F""0" + field_tag);
    for (size_t index(block_start_and_end.first); index < block_start_and_end.second; ++index) {
	const std::string &current_field(field_data[index]);
	if (StringUtil::StartsWith(current_field, FIELD_PREFIX)
	    and IndicatorsMatch(indicators, current_field.substr(7, 2)))
	    field_indices->emplace_back(index);
    }

    return field_indices->size();
}


} // namespace MarcUtil

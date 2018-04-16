/** \brief Various classes, functions etc. having to do with the Library of Congress MARC bibliographic format.
 *  \author Dr. Johannes Ruscheinski (johannes.ruscheinski@uni-tuebingen.de)
 *
 *  \copyright 2017,2018 Universitätsbibliothek Tübingen.  All rights reserved.
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

#include "MARC.h"
#include <set>
#include <unordered_map>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include "FileLocker.h"
#include "FileUtil.h"
#include "RegexMatcher.h"
#include "StringUtil.h"
#include "util.h"


namespace {


inline unsigned ToUnsigned(const char *cp, const unsigned count) {
    unsigned retval(0);
    for (unsigned i(0); i < count; ++i)
        retval = retval * 10 + (*cp++ - '0');

    return retval;
}


inline std::string ToStringWithLeadingZeros(const unsigned n, const unsigned width) {
    const std::string as_string(std::to_string(n));
    return (as_string.length() < width) ? (std::string(width - as_string.length(), '0') + as_string) : as_string;
}


} // unnamed namespace


namespace MARC {


void Subfields::addSubfield(const char subfield_code, const std::string &subfield_value) {
    auto insertion_location(subfields_.begin());
    while (insertion_location != subfields_.end() and insertion_location->code_ < subfield_code)
        ++insertion_location;
    subfields_.emplace(insertion_location, subfield_code, subfield_value);
}


bool Subfields::replaceFirstSubfield(const char subfield_code, const std::string &new_subfield_value) {
    auto replacement_location(subfields_.begin());
    while (replacement_location != subfields_.end() and replacement_location->code_ < subfield_code)
        ++replacement_location;
    if (replacement_location == subfields_.end())
        return false;
    replacement_location->value_ = new_subfield_value;
    return true;
}


std::vector<std::string> Subfields::extractSubfieldsAndNumericSubfields(const std::string &subfield_spec) const {
    std::set<std::string> numeric_subfield_specs;
    std::string subfield_codes;

    for (auto code(subfield_spec.cbegin()); code != subfield_spec.cend(); ++code) {
        if (StringUtil::IsDigit(*code)) {
            if (unlikely((code + 1) == subfield_spec.cend()))
                ERROR("numeric subfield code is missing a following character!");
            numeric_subfield_specs.insert(std::string(1, *code) + std::string(1, *(code + 1)));
            subfield_codes += *code;
            ++code;
        } else
            subfield_codes += *code;
    }

    std::vector<std::string> subfield_values;
    if (subfield_codes.empty())
        return subfield_values;

    for (const auto &subfield : subfields_) {
        if (subfield_codes.find(subfield.code_) != std::string::npos) {
            if (not StringUtil::IsDigit(subfield.code_))
                subfield_values.emplace_back(subfield.value_);
            else {
               numeric_subfield_specs.find(std::string(1, subfield.code_) + subfield.value_[0]);
               if (subfield.value_[1] == ':')
                    subfield_values.emplace_back(subfield.value_.substr(2));
            }
        }
    }
    return subfield_values;
}


bool Record::Field::operator<(const Record::Field &rhs) const {
    if (tag_ < rhs.tag_)
        return true;
    if (tag_ > rhs.tag_)
        return false;
    return contents_ < rhs.contents_;
}


void Record::Field::deleteAllSubfieldsWithCode(const char subfield_code) {
    if (contents_.size() < 5)
        return;

    std::string new_contents;
    new_contents.reserve(contents_.size());

    new_contents += contents_[0]; // indicator 1
    new_contents += contents_[1]; // indicator 2

    auto ch(contents_.begin() + 2 /* skip over the indicators */ + 1 /* \x1F */);
    while (ch != contents_.end()) {
        if (*ch == subfield_code) {
            while (ch != contents_.end() and *ch != '\x1F')
                ++ch;
        } else {
            new_contents += '\x1F';
            while (ch != contents_.end() and *ch != '\x1F')
                new_contents += *ch++;
        }
        if (ch != contents_.end())
            ++ch;
    }

    contents_.swap(new_contents);
}


Record::Record(const std::string &leader): leader_(leader) {
    if (unlikely(leader_.length() != LEADER_LENGTH))
        ERROR("supposed leader has invalid length!");
}


Record::Record(const size_t record_size, char * const record_start)
    : record_size_(record_size), leader_(record_start, LEADER_LENGTH)
{
    const char * const base_address_of_data(record_start + ToUnsigned(record_start + 12, 5));

    // Process directory:
    const char *directory_entry(record_start + LEADER_LENGTH);
    while (directory_entry != base_address_of_data - 1) {
        if (unlikely(directory_entry > base_address_of_data))
            ERROR("directory_entry > base_address_of_data!");
        std::string tag;
        tag += directory_entry[0];
        tag += directory_entry[1];
        tag += directory_entry[2];
        const unsigned field_length(ToUnsigned(directory_entry + 3, 4));
        const unsigned field_offset(ToUnsigned(directory_entry + 7, 5));
        const std::string field_contents(base_address_of_data + field_offset, field_length - 1);
        fields_.emplace_back(tag, field_contents);
        directory_entry += 3 /* tag */ + 4 /* field length */ + 5 /* field offset */;
    }
}


static std::string TypeOfRecordToSTring(const Record::TypeOfRecord type_of_record) {
    switch (type_of_record) {
    case Record::LANGUAGE_MATERIAL:
        return std::string(1, 'a');
    case Record::NOTATED_MUSIC:
        return std::string(1, 'c');
    case Record::MANUSCRIPT_NOTATED_MUSIC:
        return std::string(1, 'd');
    case Record::CARTOGRAPHIC_MATERIAL:
        return std::string(1, 'e');
    case Record::MANUSCRIPT_CARTOGRAPHIC_MATERIAL:
        return std::string(1, 'f');
    case Record::PROJECTED_MEDIUM:
        return std::string(1, 'g');
    case Record::NONMUSICAL_SOUND_RECORDING:
        return std::string(1, 'i');
    case Record::MUSICAL_SOUND_RECORDING:
        return std::string(1, 'j');
    case Record::TWO_DIMENSIONAL_NONPROJECTABLE_GRAPHIC:
        return std::string(1, 'k');
    case Record::COMPUTER_FILE:
        return std::string(1, 'm');
    case Record::KIT:
        return std::string(1, 'o');
    case Record::MIXED_MATERIALS:
        return std::string(1, 'p');
    case Record::THREE_DIMENSIONAL_ARTIFACT_OR_NATURALLY_OCCURRING_OBJECT:
        return std::string(1, 'r');
    case Record::MANUSCRIPT_LANGUAGE_MATERIAL:
        return std::string(1, 't');
    default:
        ERROR("unknown type-of-record: " + std::to_string(type_of_record) + "!");
    }
}


static std::string BibliographicLevelToString(const Record::BibliographicLevel bibliographic_level) {
    switch (bibliographic_level) {
    case Record::MONOGRAPHIC_COMPONENT_PART:
        return std::string(1, 'a');
    case Record::SERIAL_COMPONENT_PART:
        return std::string(1, 'b');
    case Record::COLLECTION:
        return std::string(1, 'c');
    case Record::SUBUNIT:
        return std::string(1, 'd');
    case Record::INTEGRATING_RESOURCE:
        return std::string(1, 'i');
    case Record::MONOGRAPH_OR_ITEM:
        return std::string(1, 'm');
    case Record::SERIAL:
        return std::string(1, 's');
    default:
        ERROR("unknown bibliographic level: " + std::to_string(bibliographic_level) + "!");
    }
}


Record::Record(const TypeOfRecord type_of_record, const BibliographicLevel bibliographic_level,
               const std::string &control_number)
{
    leader_ = "00000" "n" + TypeOfRecordToSTring(type_of_record) + BibliographicLevelToString(bibliographic_level)
              + " a22004452  4500";

    if (not control_number.empty())
        insertField("001", control_number);
}



void Record::merge(const Record &other) {
    for (const auto &other_field : other)
        insertField(other_field);
}


bool Record::isElectronicResource() const {
    if (std::toupper(leader_[6]) == 'M')
        return true;

    if (isMonograph()) {
        for (const auto &_007_field : getTagRange("007")) {
            const std::string &_007_field_contents(_007_field.getContents());
            if (not _007_field_contents.empty() and std::toupper(_007_field_contents[0]) == 'C')
                return true;
        }
    }

    for (const auto &_245_field : getTagRange("245")) {
        const Subfields subfields(_245_field.getSubfields());
        for (const auto &subfield : subfields) {
            if (subfield.code_ != 'h')
                continue;
            if (::strcasestr(subfield.value_.c_str(), "[Elektronische Ressource]") != nullptr
                or ::strcasestr(subfield.value_.c_str(), "[electronic resource]") != nullptr)
                return true;
        }
    }

    return false;
}


Record::ConstantRange Record::getTagRange(const Tag &tag) const {
    const auto begin(std::find_if(fields_.begin(), fields_.end(),
                                  [&tag](const Field &field) -> bool { return field.getTag() == tag; }));
    if (begin == fields_.end())
        return ConstantRange(fields_.end(), fields_.end());

    auto end(begin);
    while (end != fields_.end() and end->getTag() == tag)
        ++end;

    return ConstantRange(begin, end);
}


Record::Range Record::getTagRange(const Tag &tag) {
    auto begin(std::find_if(fields_.begin(), fields_.end(),
                            [&tag](const Field &field) -> bool { return field.getTag() == tag; }));
    if (begin == fields_.end())
        return Range(fields_.end(), fields_.end());

    auto end(begin);
    while (end != fields_.end() and end->getTag() == tag)
        ++end;

    return Range(begin, end);
}


bool Record::hasTagWithIndicators(const Tag &tag, const char indicator1, const char indicator2) const {
    for (const auto &field : getTagRange(tag)) {
        if (field.getIndicator1() == indicator1 and field.getIndicator2() == indicator2)
            return true;
    }
    return false;
}

std::vector<std::string> Record::getSubfieldValues(const Tag &tag, const char subfield_code) const {
    std::vector<std::string> subfield_values;
    for (const auto &field : getTagRange(tag)) {
        const Subfields subfields(field.getContents());
        for (const auto &subfield_value : subfields.extractSubfields(subfield_code))
            subfield_values.emplace_back(subfield_value);
    }

    return subfield_values;
}


std::vector<std::string> Record::getSubfieldValues(const Tag &tag, const std::string &subfield_codes) const {
    std::vector<std::string> subfield_values;
    for (const auto &field : getTagRange(tag)) {
        const Subfields subfields(field.getContents());
        for (const auto &subfield_code : subfield_codes)
            for (const auto &subfield_value : subfields.extractSubfields(subfield_code))
                subfield_values.emplace_back(subfield_value);
    }
    return subfield_values;
}


std::vector<std::string> Record::getSubfieldAndNumericSubfieldValues(const Tag &tag, const std::string &subfield_spec) const {
    std::vector<std::string> subfield_values;
    for (const auto &field : getTagRange(tag)) {
        const Subfields subfields(field.getContents());
        const std::vector<std::string> one_tag_subfield_values(subfields.extractSubfieldsAndNumericSubfields(subfield_spec));
        subfield_values.insert(std::end(subfield_values), one_tag_subfield_values.cbegin(), one_tag_subfield_values.cend());
    }
    return subfield_values;
}


size_t Record::findAllLocalDataBlocks(
    std::vector<std::pair<const_iterator, const_iterator>> * const local_block_boundaries) const
{
    local_block_boundaries->clear();

    auto local_block_start(getFirstField("LOK"));
    if (local_block_start == fields_.end())
        return 0;

    auto local_block_end(local_block_start + 1);
    while (local_block_end < fields_.end()) {
        if (StringUtil::StartsWith(local_block_end->getContents(), "  ""\x1F""0000")) {
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


size_t Record::findFieldsInLocalBlock(const Tag &field_tag, const std::string &indicators,
                                      const std::pair<const_iterator, const_iterator> &block_start_and_end,
                                      std::vector<const_iterator> * const fields) const
{
    fields->clear();
    if (unlikely(indicators.length() != 2))
        ERROR("indicators must be precisely 2 characters long!");

    const std::string FIELD_PREFIX("  ""\x1F""0" + field_tag.to_string());
    for (auto field(block_start_and_end.first); field < block_start_and_end.second; ++field) {
        const std::string &field_contents(field->getContents());
        if (StringUtil::StartsWith(field_contents, FIELD_PREFIX)
            and IndicatorsMatch(indicators, field_contents.substr(7, 2)))
            fields->emplace_back(field);
    }
    return fields->size();
}


bool Record::insertField(const Tag &new_field_tag, const std::string &new_field_value) {
    auto insertion_location(fields_.begin());
    while (insertion_location != fields_.end() and new_field_tag > insertion_location->getTag())
        ++insertion_location;
    if (insertion_location != fields_.begin() and (insertion_location - 1)->getTag() == new_field_tag
        and not IsRepeatableField(new_field_tag))
        return false;
    fields_.emplace(insertion_location, new_field_tag, new_field_value);
    record_size_ += DIRECTORY_ENTRY_LENGTH + new_field_value.length() + 1 /* field separator */;
    return true;
}


bool Record::addSubfield(const Tag &field_tag, const char subfield_code, const std::string &subfield_value) {
    const auto field(std::find_if(fields_.begin(), fields_.end(),
                                  [&field_tag](const Field &field1) -> bool { return field1.getTag() == field_tag; }));
    if (field == fields_.end())
        return false;

    Subfields subfields(field->getContents());
    subfields.addSubfield(subfield_code, subfield_value);

    std::string new_field_value;
    new_field_value += field->getIndicator1();
    new_field_value += field->getIndicator2();
    new_field_value += subfields.toString();
    field->contents_ = new_field_value;

    return true;
}


void Record::deleteFields(std::vector<size_t> field_indices) {
    std::sort(field_indices.begin(), field_indices.end(), std::greater<size_t>());
    for (const auto field_index : field_indices)
        fields_.erase(fields_.begin() + field_index);
}


bool Record::isValid(std::string * const error_message) const {
    if (fields_.empty() or fields_.front().getTag() != "001") {
        *error_message = "001 field is missing!";
        return false;
    }

    for (const auto &field : fields_) {
        if (field.isDataField()) {
            // Check subfield structure:
            if (unlikely(field.contents_.length() < 5)) {
                *error_message = "field contents are too small (< 5 bytes)! (tag: " + field.getTag().to_string() + ")";
                return false;
            }

            auto ch(field.contents_.begin() + 2 /* indicators */);
            while (ch != field.contents_.end()) {
                if (unlikely(*ch != '\x1F')) {
                    *error_message = "subfield does not start with 0x1F! (tag: " + field.getTag().to_string() + ")";
                    return false;
                }
                ++ch; // Skip over 0x1F.
                if (unlikely(ch == field.contents_.end())) {
                    *error_message = "subfield is missing a subfield code! (tag: " + field.getTag().to_string() + ")";
                    return false;
                }
                ++ch; // Skip over the subfield code.
                if (unlikely(ch == field.contents_.end() or *ch == '\x1F'))
                    WARNING("subfield '" + std::string(1, *(ch - 1)) + "' is empty! (tag: " + field.getTag().to_string() + ")");

                // Skip over the subfield contents:
                while (ch != field.contents_.end() and *ch != '\x1F')
                    ++ch;
            }
        }
    }

    return true;
}


enum class MediaType { XML, MARC21, OTHER };


static MediaType GetMediaType(const std::string &input_filename) {
    File input(input_filename, "r");
    if (input.anErrorOccurred())
        return MediaType::OTHER;

    char magic[8];
    const size_t read_count(input.read(magic, sizeof(magic) - 1));
    if (read_count != sizeof(magic) - 1) {
        if (read_count == 0) {
            WARNING("empty input file \"" + input_filename + "\"!");
            const std::string extension(FileUtil::GetExtension(input_filename));
            if (::strcasecmp(extension.c_str(), "mrc") == 0 or ::strcasecmp(extension.c_str(), "marc") == 0
                or ::strcasecmp(extension.c_str(), "raw") == 0)
                return MediaType::MARC21;
        }
        return MediaType::OTHER;
    }
    magic[sizeof(magic) - 1] = '\0';

    if (StringUtil::StartsWith(magic, "<?xml"))
        return MediaType::XML;

    static RegexMatcher *marc21_matcher;
    if (marc21_matcher == nullptr) {
        std::string err_msg;
        marc21_matcher = RegexMatcher::RegexMatcherFactory("(^[0-9]{5})([acdnp][^bhlnqsu-z]|[acdnosx][z]|[cdn][uvxy]|[acdn][w]|[cdn][q])", &err_msg);
        if (marc21_matcher == nullptr)
            ERROR("failed to compile a regex! (" + err_msg + ")");
    }

    return marc21_matcher->matched(magic) ? MediaType::MARC21 : MediaType::XML;
}


std::unique_ptr<Reader> Reader::Factory(const std::string &input_filename, ReaderType reader_type) {
    if (reader_type == AUTO) {
        const MediaType media_type(GetMediaType(input_filename));
        if (media_type == MediaType::OTHER)
            ERROR("can't determine media type of \"" + input_filename + "\"!");
        reader_type = (media_type == MediaType::XML) ? Reader::XML : Reader::BINARY;
    }

    std::unique_ptr<File> input(FileUtil::OpenInputFileOrDie(input_filename));
    return (reader_type == XML) ? std::unique_ptr<Reader>(new XmlReader(input.release()))
                                : std::unique_ptr<Reader>(new BinaryReader(input.release()));
}


Record BinaryReader::read() {
    if (unlikely(not last_record_))
        return last_record_;

    Record new_record;
    do {
        next_record_start_ = input_->tell();
        new_record = actualRead();
        if (unlikely(new_record.getControlNumber() == last_record_.getControlNumber()))
            last_record_.merge(new_record);
    } while (new_record.getControlNumber() == last_record_.getControlNumber());

    new_record.swap(last_record_);
    return new_record;
}


Record BinaryReader::actualRead() {
    char buf[Record::MAX_RECORD_LENGTH];
    size_t bytes_read;
    if (unlikely((bytes_read = input_->read(buf, Record::RECORD_LENGTH_FIELD_LENGTH)) == 0))
        return Record();

    if (unlikely(bytes_read != Record::RECORD_LENGTH_FIELD_LENGTH))
        ERROR("failed to read record length!");
    const unsigned record_length(ToUnsigned(buf, Record::RECORD_LENGTH_FIELD_LENGTH));

    bytes_read = input_->read(buf + Record::RECORD_LENGTH_FIELD_LENGTH, record_length - Record::RECORD_LENGTH_FIELD_LENGTH);
    if (unlikely(bytes_read != record_length - Record::RECORD_LENGTH_FIELD_LENGTH))
        ERROR("failed to read a record from \"" + input_->getPath() + "\"!");

    return Record(record_length, buf);
}


bool BinaryReader::seek(const off_t offset, const int whence) {
    if (input_->seek(offset, whence)) {
        next_record_start_ = input_->tell();
        last_record_ = actualRead();
        return true;
    } else
        return false;
}


Record XmlReader::read() {
    Record new_record;

    SimpleXmlParser<File>::Type type;
    std::map<std::string, std::string> attrib_map;
    std::string data;
    while (getNext(&type, &attrib_map, &data) and type == SimpleXmlParser<File>::CHARACTERS)
        /* Intentionally empty! */;

    if (unlikely(type == SimpleXmlParser<File>::CLOSING_TAG and data == namespace_prefix_ + "collection"))
        return new_record;

        //
        // Now parse a <record>:
        //

    if (unlikely(type != SimpleXmlParser<File>::OPENING_TAG or data != namespace_prefix_ + "record")) {
        const bool tag_found(type == SimpleXmlParser<File>::OPENING_TAG
                             or type == SimpleXmlParser<File>::CLOSING_TAG);
        if (type == SimpleXmlParser<File>::ERROR)
            throw std::runtime_error("in MARC::XmlReader::read: opening <" + namespace_prefix_
                                     + "record> tag expected while parsing \"" + input_->getPath() + "\" on line "
                                     + std::to_string(xml_parser_->getLineNo()) + "! ("
                                     + xml_parser_->getLastErrorMessage() + ")");
        else
            throw std::runtime_error("in MARC::XmlReader::read: opening <" + namespace_prefix_
                                     + "record> tag expected while parsing \"" + input_->getPath() + "\" on line "
                                     + std::to_string(xml_parser_->getLineNo()) + "! (Found: "
                                     + SimpleXmlParser<File>::TypeToString(type)
                                     + (tag_found ? (":" + data + ")") : ")"));
    }

    parseLeader(input_->getPath(), &new_record);

    bool datafield_seen(false);
    for (;;) { // Process "datafield" and "controlfield" sections.
        if (unlikely(not getNext(&type, &attrib_map, &data)))
            throw std::runtime_error("in MARC::XmlReader::read: error while parsing \"" + input_->getPath()
                                     + "\": " + xml_parser_->getLastErrorMessage() + " on line "
                                     + std::to_string(xml_parser_->getLineNo()) + "!");

        if (type == SimpleXmlParser<File>::CLOSING_TAG) {
            if (unlikely(data != namespace_prefix_ + "record"))
                throw std::runtime_error("in MARC::MarcUtil::Record::XmlFactory: closing </record> tag expected "
                                         "while parsing \"" + input_->getPath() + "\" on line "
                                         + std::to_string(xml_parser_->getLineNo()) + "!");
            return new_record;
        }

        if (type != SimpleXmlParser<File>::OPENING_TAG
            or (data != namespace_prefix_ + "datafield" and data != namespace_prefix_ + "controlfield"))
            throw std::runtime_error("in MARC::XmlReader::read: expected either <" + namespace_prefix_
                                     + "controlfield> or <" + namespace_prefix_ + "datafield> on line "
                                     + std::to_string(xml_parser_->getLineNo()) + " in file \"" + input_->getPath()
                                     + "\"!");

        if (unlikely(attrib_map.find("tag") == attrib_map.end()))
            throw std::runtime_error("in MARC::XmlReader::read: expected a \"tag\" attribute as part of an opening "
                                     "<" + namespace_prefix_ + "controlfield> or <" + namespace_prefix_
                                     + "datafield> tag on line " + std::to_string(xml_parser_->getLineNo())
                                     + " in file \"" + input_->getPath() + "\"!");

        if (data == namespace_prefix_ + "controlfield") {
            if (unlikely(datafield_seen))
                throw std::runtime_error("in MARC::MarcUtil::Record::XmlFactory: <" + namespace_prefix_
                                         + "controlfield> found after <" + namespace_prefix_ + "datafield> on "
                                         "line " + std::to_string(xml_parser_->getLineNo()) + " in file \""
                                         + input_->getPath() + "\"!");
            parseControlfield(input_->getPath(), attrib_map["tag"], &new_record);

        } else {
            datafield_seen = true;
            parseDatafield(input_->getPath(), attrib_map, attrib_map["tag"], &new_record);
        }
    }
}


void XmlReader::rewind() {
    // We can't handle FIFO's here:
    struct stat stat_buf;
    if (unlikely(fstat(input_->getFileDescriptor(), &stat_buf) and S_ISFIFO(stat_buf.st_mode)))
        ERROR("can't rewind a FIFO!");

    input_->rewind();

    delete xml_parser_;
    xml_parser_ = new SimpleXmlParser<File>(input_);

    skipOverStartOfDocument();
}


namespace {


bool ParseLeader(const std::string &leader_string, std::string * const leader, std::string * const err_msg) {
    if (err_msg != nullptr)
        err_msg->clear();

    if (leader_string.size() != Record::LEADER_LENGTH) {
        if (err_msg != nullptr)
            *err_msg = "Leader length must be " + std::to_string(Record::LEADER_LENGTH) +
                       ", found " + std::to_string(leader_string.size()) + "! (Leader bytes are "
                       + StringUtil:: CStyleEscape(leader_string) + ")";
        return false;
    }

    unsigned record_length;
    if (std::sscanf(leader_string.substr(0, 5).data(), "%5u", &record_length) != 1) {
        if (err_msg != nullptr)
            *err_msg = "Can't parse record length! (Found \"" + StringUtil::CStyleEscape(leader_string.substr(0, 5))
                       + "\")";
        return false;
    }

    unsigned base_address_of_data;
    if (std::sscanf(leader_string.substr(12, 5).data(), "%5u", &base_address_of_data) != 1) {
        if (err_msg != nullptr)
            *err_msg = "Can't parse base address of data!";
        return false;
    }

    //
    // Validity checks:
    //

    // Check indicator count:
    if (leader_string[10] != '2')
        WARNING("invalid indicator count '" + leader_string.substr(10, 1) + "'!");

    // Check subfield code length:
    if (leader_string[11] != '2')
        WARNING("invalid subfield code length! (Leader bytes are " + StringUtil:: CStyleEscape(leader_string) + ")");

    // Check entry map:
    if (leader_string.substr(20, 3) != "450")
        WARNING("invalid entry map!");

    *leader = leader_string;

    return true;
}


} // unnamed namespace


void XmlReader::parseLeader(const std::string &input_filename, Record * const new_record) {
    SimpleXmlParser<File>::Type type;
    std::map<std::string, std::string> attrib_map;
    std::string data;

    while (getNext(&type, &attrib_map, &data) and type == SimpleXmlParser<File>::CHARACTERS)
        /* Intentionally empty! */;
    if (unlikely(type != SimpleXmlParser<File>::OPENING_TAG or data != namespace_prefix_ + "leader"))
        throw std::runtime_error("in MARC::XmlReader::ParseLeader: opening <marc:leader> tag expected while "
                                 "parsing \"" + input_filename + "\" on line "
                                 + std::to_string(xml_parser_->getLineNo()) + ".");

    if (unlikely(not getNext(&type, &attrib_map, &data)))
        throw std::runtime_error("in MARC::XmlReader::ParseLeader: error while parsing \"" + input_filename + "\": "
                                 + xml_parser_->getLastErrorMessage() + " on line "
                                 + std::to_string(xml_parser_->getLineNo()) + ".");
    if (unlikely(type != SimpleXmlParser<File>::CHARACTERS or data.length() != Record::LEADER_LENGTH)) {
        WARNING("leader data expected while parsing \"" + input_filename + "\" on line "
                + std::to_string(xml_parser_->getLineNo()) + ".");
        if (unlikely(not getNext(&type, &attrib_map, &data)))
            throw std::runtime_error("in MARC::XmlReader::ParseLeader: error while skipping to </"
                                     + namespace_prefix_ + "leader>!");
        if (unlikely(type != SimpleXmlParser<File>::CLOSING_TAG or data != namespace_prefix_ + "leader")) {
            const bool tag_found(type == SimpleXmlParser<File>::OPENING_TAG
                                 or type == SimpleXmlParser<File>::CLOSING_TAG);
            throw std::runtime_error("in MARC::XmlReader::ParseLeader: closing </" + namespace_prefix_
                                     + "leader> tag expected while parsing \"" + input_filename + "\" on line "
                                     + std::to_string(xml_parser_->getLineNo())
                                     + ". (Found: " + SimpleXmlParser<File>::TypeToString(type)
                                     + (tag_found ? (":" + data) : ""));
        }
        return;
    }

    if (data.substr(0, 5) == "     ") // record length
        data = "00000" + data.substr(5);
    if (data.substr(12, 5) == "     ") // base address of data
        data = data.substr(0, 12) + "00000" + data.substr(12 + 5);
    std::string err_msg;
    if (unlikely(not ParseLeader(data, &new_record->leader_, &err_msg)))
        throw std::runtime_error("in MARC::XmlReader::ParseLeader: error while parsing leader data: " + err_msg);

    if (unlikely(not getNext(&type, &attrib_map, &data)))
        throw std::runtime_error("in MARC::XmlReader::ParseLeader: error while parsing \"" + input_filename + "\": "
                                 + xml_parser_->getLastErrorMessage() + " on line "
                                 + std::to_string(xml_parser_->getLineNo()) + ".");
    if (unlikely(type != SimpleXmlParser<File>::CLOSING_TAG or data != namespace_prefix_ + "leader")) {
        const bool tag_found(type == SimpleXmlParser<File>::OPENING_TAG
                             or type == SimpleXmlParser<File>::CLOSING_TAG);
        throw std::runtime_error("in MARC::XmlReader::ParseLeader: closing </" + namespace_prefix_
                                 + "leader> tag expected while parsing \"" + input_filename + "\" on line "
                                 + std::to_string(xml_parser_->getLineNo())
                                 + ". (Found: " + SimpleXmlParser<File>::TypeToString(type)
                                 + (tag_found ? (":" + data) : ""));
    }
}


// Returns true if we found a normal control field and false if we found an empty control field.
void XmlReader::parseControlfield(const std::string &input_filename, const std::string &tag,
                                  Record * const record)
{
    SimpleXmlParser<File>::Type type;
    std::map<std::string, std::string> attrib_map;
    std::string data;
    if (unlikely(not getNext(&type, &attrib_map, &data)))
        throw std::runtime_error("in MARC::XmlReader::parseControlfield: failed to get next XML element!");

        // Do we have an empty control field?
    if (unlikely(type == SimpleXmlParser<File>::CLOSING_TAG and data == namespace_prefix_ + "controlfield")) {
        WARNING("empty \"" + tag + "\" control field on line " + std::to_string(xml_parser_->getLineNo()) + " in file \""
                + input_filename + "\"!");
        return;
    }

    if (type != SimpleXmlParser<File>::CHARACTERS)
        std::runtime_error("in MARC::XmlReader::parseControlfield: character data expected on line "
                           + std::to_string(xml_parser_->getLineNo()) + " in file \"" + input_filename + "\"!");
    Record::Field new_field(tag, data);

    if (unlikely(not getNext(&type, &attrib_map, &data) or type != SimpleXmlParser<File>::CLOSING_TAG
                 or data != namespace_prefix_ + "controlfield"))
        throw std::runtime_error("in MARC::XmlReader::parseControlfield: </controlfield> expected on line "
                                 + std::to_string(xml_parser_->getLineNo()) + " in file \"" + input_filename
                                 + "\"!");

    record->fields_.emplace_back(new_field);
    record->record_size_ += Record::DIRECTORY_ENTRY_LENGTH + new_field.getContents().size() + 1 /* end-of-field */;
    return;
}


void XmlReader::parseDatafield(const std::string &input_filename,
                               const std::map<std::string, std::string> &datafield_attrib_map,
                               const std::string &tag, Record * const record)
{
    const auto ind1(datafield_attrib_map.find("ind1"));
    if (unlikely(ind1 == datafield_attrib_map.cend() or ind1->second.length() != 1))
        throw std::runtime_error("in MARC::XmlReader::ParseDatafield: bad or missing \"ind1\" attribute on line "
                                 + std::to_string(xml_parser_->getLineNo()) + " in file \"" + input_filename
                                 + "\"!");
    std::string field_data(ind1->second);

    const auto ind2(datafield_attrib_map.find("ind2"));
    if (unlikely(ind2 == datafield_attrib_map.cend() or ind2->second.length() != 1))
        throw std::runtime_error("in MARC::XmlReader::ParseDatafield: bad or missing \"ind2\" attribute on line "
                                 + std::to_string(xml_parser_->getLineNo()) + " in file \"" + input_filename
                                 + "\"!");
    field_data += ind2->second;

    SimpleXmlParser<File>::Type type;
    std::map<std::string, std::string> attrib_map;
    std::string data;
    for (;;) {
        while (getNext(&type, &attrib_map, &data) and type == SimpleXmlParser<File>::CHARACTERS)
            /* Intentionally empty! */;

        if (type == SimpleXmlParser<File>::ERROR)
            throw std::runtime_error("in MARC::XmlReader::parseDatafield: error while parsing a data field on line "
                                     + std::to_string(xml_parser_->getLineNo()) + " in file \"" + input_filename
                                     + "\": " + xml_parser_->getLastErrorMessage());

        if (type == SimpleXmlParser<File>::CLOSING_TAG and data == namespace_prefix_ + "datafield") {
            record->fields_.emplace_back(tag, field_data);
            record->record_size_ += Record::DIRECTORY_ENTRY_LENGTH + field_data.length() + 1 /* end-of-field */;
            return;
        }

        // 1. <subfield code=...>
        if (unlikely(type != SimpleXmlParser<File>::OPENING_TAG or data != namespace_prefix_ + "subfield")) {
            const bool tag_found(type == SimpleXmlParser<File>::OPENING_TAG
                                 or type == SimpleXmlParser<File>::CLOSING_TAG);
            throw std::runtime_error("in MARC::XmlReader::parseDatafield: expected <" + namespace_prefix_ +
                                     "subfield> opening tag on line " + std::to_string(xml_parser_->getLineNo())
                                     + " in file \"" + input_filename
                                     + "\"! (Found: " + SimpleXmlParser<File>::TypeToString(type)
                                     + (tag_found ? (":" + data) : ""));
        }
        if (unlikely(attrib_map.find("code") == attrib_map.cend() or attrib_map["code"].length() != 1))
            throw std::runtime_error("in MARC::XmlReader::parseDatafield: missing or invalid \"code\" attribute as "
                                     "rt   of the <subfield> tag " + std::to_string(xml_parser_->getLineNo())
                                     + " in file \"" + input_filename + "\"!");
        field_data += '\x1F' + attrib_map["code"];

        // 2. Subfield data.
        if (unlikely(not getNext(&type, &attrib_map, &data) or type != SimpleXmlParser<File>::CHARACTERS)) {
            if (type == SimpleXmlParser<File>::CLOSING_TAG and data == namespace_prefix_ + "subfield") {
                WARNING("found an empty subfield on line " + std::to_string(xml_parser_->getLineNo()) + " in file \""
                        + input_filename + "\"!");
                field_data.resize(field_data.length() - 2); // Remove subfield delimiter and code.
                continue;
            }
            throw std::runtime_error("in MARC::XmlReader::parseDatafield: error while looking for character data "
                                     "after <" + namespace_prefix_ + "subfield> tag on line "
                                     + std::to_string(xml_parser_->getLineNo()) + " in file \"" + input_filename
                                     + "\": " + xml_parser_->getLastErrorMessage());
        }
        field_data += data;

        // 3. </subfield>
        if (unlikely(not getNext(&type, &attrib_map, &data) or type != SimpleXmlParser<File>::CLOSING_TAG
                     or data != namespace_prefix_ + "subfield"))
        {
            const bool tag_found(type == SimpleXmlParser<File>::OPENING_TAG
                                 or type == SimpleXmlParser<File>::CLOSING_TAG);
            throw std::runtime_error("in MARC::XmlReader::parseDatafield: expected </" + namespace_prefix_
                                     + "subfield> closing tag on line "
                                     + std::to_string(xml_parser_->getLineNo()) + " in file \"" + input_filename
                                     + "\"! (Found: " + SimpleXmlParser<File>::TypeToString(type)
                                     + (tag_found ? (":" + data) : ""));
        }
    }
}


void XmlReader::skipOverStartOfDocument() {
    SimpleXmlParser<File>::Type type;
    std::map<std::string, std::string> attrib_map;
    std::string data;

    while (getNext(&type, &attrib_map, &data)) {
        if (type == SimpleXmlParser<File>::OPENING_TAG and data == namespace_prefix_ + "collection")
            return;
    }

        // We should never get here!
    throw std::runtime_error("in MARC::XmlReader::skipOverStartOfDocument: error while trying to skip to "
                             "<" + namespace_prefix_ + "collection>:  \""
                             + xml_parser_->getDataSource()->getPath() + "\": "
                             + xml_parser_->getLastErrorMessage() + " on line "
                             + std::to_string(xml_parser_->getLineNo()) + "!");
}


bool XmlReader::getNext(SimpleXmlParser<File>::Type * const type,
                        std::map<std::string, std::string> * const attrib_map, std::string * const data)
{
    if (unlikely(not xml_parser_->getNext(type, attrib_map, data)))
        return false;

    if (*type != SimpleXmlParser<File>::OPENING_TAG)
        return true;

    auto key_and_value(attrib_map->find("xmlns"));
    if (unlikely(key_and_value != attrib_map->cend() and key_and_value->second != "http://www.loc.gov/MARC21/slim"))
        throw std::runtime_error("in MARC::XmlReader::getNext: opening tag has unsupported \"xmlns\" attribute "
                                 "near line #" + std::to_string(xml_parser_->getLineNo()) + " in \"" + getPath()
                                 + "\"!");

    key_and_value = attrib_map->find("xmlns:marc");
    if (unlikely(key_and_value != attrib_map->cend())) {
        if (unlikely(key_and_value->second != "http://www.loc.gov/MARC21/slim"))
            throw std::runtime_error("in MARC::XmlReader::getNext: opening tag has unsupported \"xmlns:marc\" "
                                     "attribute near line #" + std::to_string(xml_parser_->getLineNo()) + " in \""
                                     + getPath() + "\"!");
        else
            namespace_prefix_ = "marc:";
    }

    return true;
}


std::unique_ptr<Writer> Writer::Factory(const std::string &output_filename, WriterType writer_type,
                                        const WriterMode writer_mode)
{
    std::unique_ptr<File> output(writer_mode == WriterMode::OVERWRITE
                                 ? FileUtil::OpenOutputFileOrDie(output_filename)
                                 : FileUtil::OpenForAppendingOrDie(output_filename));
    if (writer_type == AUTO) {
        if (StringUtil::EndsWith(output_filename, ".mrc") or StringUtil::EndsWith(output_filename, ".marc"))
            writer_type = BINARY;
        else if (StringUtil::EndsWith(output_filename, ".xml"))
            writer_type = XML;
        else
            throw std::runtime_error("in MARC::Writer::Factory: WriterType is AUTO but filename \""
                                     + output_filename + "\" does not end in \".mrc\" or \".xml\"!");
    }
    return (writer_type == XML) ? std::unique_ptr<Writer>(new XmlWriter(output.release()))
                                : std::unique_ptr<Writer>(new BinaryWriter(output.release()));
}


void BinaryWriter::write(const Record &record) {
    std::string error_message;
    if (not record.isValid(&error_message))
        ERROR("trying to write an invalid record: " + error_message + " (Control number: " + record.getControlNumber() + ")");

    Record::const_iterator start(record.begin());
    do {
        const bool record_is_oversized(start > record.begin());
        Record::const_iterator end(start);
        unsigned record_size(Record::LEADER_LENGTH + 2 /* end-of-directory and end-of-record */);
        if (record_is_oversized) // Include size of the 001 field.
            record_size += record.fields_.front().getContents().length() + 1 + Record::DIRECTORY_ENTRY_LENGTH;
        while (end != record.end()
               and (record_size + end->getContents().length() + 1 + Record::DIRECTORY_ENTRY_LENGTH < Record::MAX_RECORD_LENGTH))
        {
            record_size += end->getContents().length() + 1 + Record::DIRECTORY_ENTRY_LENGTH;
            ++end;
        }

        std::string raw_record;
        raw_record.reserve(record_size);
        const unsigned no_of_fields(end - start);
        raw_record += ToStringWithLeadingZeros(record_size, /* width = */ 5);
        raw_record += record.leader_.substr(5, 12 - 5);
        const unsigned base_address_of_data(Record::LEADER_LENGTH + no_of_fields * Record::DIRECTORY_ENTRY_LENGTH
                                            + 1 /* end-of-directory */);
        raw_record += ToStringWithLeadingZeros(base_address_of_data, /* width = */ 5);
        raw_record += record.leader_.substr(17, Record::LEADER_LENGTH - 17);

        // Append the directory:
        unsigned field_start_offset(0);
        if (record_is_oversized) {
            raw_record += "001"
                          + ToStringWithLeadingZeros(record.fields_.front().getContents().length() + 1 /* field terminator */, 4)
                          + ToStringWithLeadingZeros(field_start_offset, /* width = */ 5);
            field_start_offset += record.fields_.front().getContents().length() + 1 /* field terminator */;
        }
        for (Record::const_iterator entry(start); entry != end; ++entry) {
            raw_record += entry->getTag().to_string()
                          + ToStringWithLeadingZeros(entry->getContents().length() + 1 /* field terminator */, 4)
                          + ToStringWithLeadingZeros(field_start_offset, /* width = */ 5);
            field_start_offset += entry->getContents().length() + 1 /* field terminator */;
        }
        raw_record += '\x1E'; // end-of-directory

        // Now append the field data:
        if (record_is_oversized) {
            raw_record += record.fields_.front().getContents();
            raw_record += '\x1E'; // end-of-field
        }
        for (Record::const_iterator entry(start); entry != end; ++entry) {
            raw_record += entry->getContents();
            raw_record += '\x1E'; // end-of-field
        }
        raw_record += '\x1D'; // end-of-record

        output_->write(raw_record);

        start = end;
    } while (start != record.end());
}


XmlWriter::XmlWriter(File * const output_file, const unsigned indent_amount,
                     const MarcXmlWriter::TextConversionType text_conversion_type)
{
    xml_writer_ = new MarcXmlWriter(output_file, indent_amount, text_conversion_type);
}


void XmlWriter::write(const Record &record) {
    xml_writer_->openTag("record");

    xml_writer_->writeTagsWithData("leader", record.leader_, /* suppress_newline = */ true);

    for (const auto &field : record) {
        if (field.isControlField())
            xml_writer_->writeTagsWithData("controlfield", { std::make_pair("tag", field.getTag().to_string()) },
                                           field.getContents(),
                    /* suppress_newline = */ true);
        else { // We have a data field.
            xml_writer_->openTag("datafield",
                                 { std::make_pair("tag", field.getTag().to_string()),
                                   std::make_pair("ind1", std::string(1, field.getIndicator1())),
                                   std::make_pair("ind2", std::string(1, field.getIndicator2()))
                                 });

            const Subfields subfields(field.getSubfields());
            for (const auto &subfield : subfields)
                xml_writer_->writeTagsWithData("subfield", { std::make_pair("code", std::string(1, subfield.code_)) },
                                               subfield.value_, /* suppress_newline = */ true);

            xml_writer_->closeTag(); // Close "datafield".
        }
    }

    xml_writer_->closeTag(); // Close "record".
}


void FileLockedComposeAndWriteRecord(Writer * const marc_writer, const Record &record) {
    FileLocker file_locker(marc_writer->getFile().getFileDescriptor(), FileLocker::WRITE_ONLY);
    if (unlikely(not (marc_writer->getFile().seek(0, SEEK_END))))
        ERROR("failed to seek to the end of \"" + marc_writer->getFile().getPath() + "\"!");
    marc_writer->write(record);
    if (unlikely(not marc_writer->flush()))
        ERROR("failed to flush to \"" +  marc_writer->getFile().getPath() + "\"!");
}


unsigned RemoveDuplicateControlNumberRecords(const std::string &marc_filename) {
    unsigned dropped_count(0);
    std::string temp_filename;
    // Open a scope because we need the MARC::Reader to go out-of-scope before we unlink the associated file.
    {
        std::unique_ptr<Reader> marc_reader(Reader::Factory(marc_filename));
        temp_filename = "/tmp/" + std::string(::basename(::progname)) + std::to_string(::getpid())
                        + (marc_reader->getReaderType() == Reader::XML ? ".xml" : ".mrc");
        std::unique_ptr<Writer> marc_writer(Writer::Factory(temp_filename));
        std::unordered_set<std::string> already_seen_control_numbers;
        while (const Record record = marc_reader->read()) {
            const std::string control_number(record.getControlNumber());
            if (already_seen_control_numbers.find(control_number) == already_seen_control_numbers.end()) {
                marc_writer->write(record);
                already_seen_control_numbers.emplace(control_number);
            } else
                ++dropped_count;
        }
    }
    FileUtil::RenameFileOrDie(temp_filename, marc_filename, true /* remove_target */, true /* copy accros filesystems */);
    return dropped_count;
}


bool IsValidMarcFile(const std::string &filename, std::string * const err_msg,
                     const Reader::ReaderType reader_type)
{
    try {
      std::unique_ptr<Reader> reader(Reader::Factory(filename, reader_type));
      while (const Record record = reader->read()) {
          if (not record.isValid(err_msg)) {
              return false;
          }
      }
      return true;
    } catch (const std::exception &x) {
      *err_msg = x.what();
      return false;
    }
}


std::string GetLanguageCode(const Record &record) {
    const auto _008_field(record.getFirstField("008"));
    if (_008_field == record.end())
        return "";
    // Language codes start at offset 35 and have a length of 3.
    const auto _008_contents(_008_field->getContents());
    if (_008_contents.length() < 38)
        return "";

    return _008_contents.substr(35, 3);
}


bool GetGNDCode(const Record &record, std::string * const gnd_code) {
    gnd_code->clear();
    for (const auto &_035_field : record.getTagRange("035")) {
        const Subfields _035_subfields(_035_field.getSubfields());
        const std::string _035a_field(_035_subfields.getFirstSubfieldWithCode('a'));
        if (StringUtil::StartsWith(_035a_field, "(DE-588)")) {
            *gnd_code = _035a_field.substr(8);
            return not gnd_code->empty();
        }
    }
    return false;
}


static inline bool CompareField(const Record::Field * const field1, const Record::Field * const field2) {
    if (field1->getTag() < field2->getTag())
        return true;
    if (field1->getTag() > field2->getTag())
        return false;
    return field1->getContents() < field2->getContents();
}


std::string CalcChecksum(const Record &record, const bool exclude_001) {
    std::vector<const Record::Field *> field_refs;
    field_refs.reserve(record.fields_.size());

    // Our Strategy here is it to sort references to fields based on tags and contents and then to create a blob using the
    // sorted order.  This allows us to generate checksums that are identical for non-equal but equivalent records.

    for (const auto &field : record.fields_) {
        if (not exclude_001 or likely(field.getTag().to_string() != "001"))
            field_refs.emplace_back(&field);
    }

    std::sort(field_refs.begin(), field_refs.end(), CompareField);

    std::string blob;
    blob.reserve(200000); // Roughly twice the maximum size of a single MARC-21 record.
    blob += record.leader_;

    for (const auto &field_ref : field_refs)
        blob += field_ref->getTag().to_string() + field_ref->getContents();

    return StringUtil::Sha1(blob);
}


// See https://www.loc.gov/marc/bibliographic/ for how to construct this map:
static std::unordered_map<Tag, bool> tag_to_repeatable_map{
    { Tag("001"), false },
    { Tag("003"), false },
    { Tag("005"), false },
    { Tag("007"), true  },
    { Tag("008"), false },
    { Tag("010"), false },
    { Tag("013"), true  },
    { Tag("015"), true  },
    { Tag("016"), true  },
    { Tag("017"), true  },
    { Tag("018"), false },
    { Tag("020"), true  },
    { Tag("022"), true  },
    { Tag("024"), true  },
    { Tag("025"), true  },
    { Tag("026"), true  },
    { Tag("027"), true  },
    { Tag("028"), true  },
    { Tag("030"), true  },
    { Tag("031"), true  },
    { Tag("032"), true  },
    { Tag("033"), true  },
    { Tag("034"), true  },
    { Tag("035"), true  },
    { Tag("036"), false },
    { Tag("037"), true  },
    { Tag("038"), false },
    { Tag("040"), false },
    { Tag("041"), true  },
    { Tag("042"), false },
    { Tag("043"), false },
    { Tag("044"), false },
    { Tag("045"), false },
    { Tag("046"), true  },
    { Tag("047"), true  },
    { Tag("048"), true  },
    { Tag("050"), true  },
    { Tag("051"), true  },
    { Tag("052"), true  },
    { Tag("055"), true  },
    { Tag("060"), true  },
    { Tag("061"), true  },
    { Tag("066"), false },
    { Tag("070"), true  },
    { Tag("071"), true  },
    { Tag("072"), true  },
    { Tag("074"), true  },
    { Tag("080"), true  },
    { Tag("082"), true  },
    { Tag("083"), true  },
    { Tag("084"), true  },
    { Tag("085"), true  },
    { Tag("086"), true  },
    { Tag("088"), true  },
    { Tag("100"), false },
    { Tag("110"), false },
    { Tag("111"), false },
    { Tag("130"), false },
    { Tag("186"), true  }, // non-standard field only used locally
    { Tag("210"), true  },
    { Tag("222"), true  },
    { Tag("240"), false },
    { Tag("242"), true  },
    { Tag("243"), false },
    { Tag("245"), false },
    { Tag("246"), true  },
    { Tag("247"), true  },
    { Tag("250"), true  },
    { Tag("254"), false },
    { Tag("255"), true  },
    { Tag("256"), false },
    { Tag("257"), true  },
    { Tag("258"), true  },
    { Tag("260"), true  },
    { Tag("263"), false },
    { Tag("264"), true  },
    { Tag("270"), true  },
    { Tag("300"), true  },
    { Tag("306"), false },
    { Tag("307"), true  },
    { Tag("310"), false },
    { Tag("321"), true  },
    { Tag("336"), true  },
    { Tag("337"), true  },
    { Tag("338"), true  },
    { Tag("340"), true  },
    { Tag("342"), true  },
    { Tag("343"), true  },
    { Tag("344"), true  },
    { Tag("345"), true  },
    { Tag("346"), true  },
    { Tag("347"), true  },
    { Tag("348"), true  },
    { Tag("351"), true  },
    { Tag("352"), true  },
    { Tag("355"), true  },
    { Tag("357"), false },
    { Tag("362"), true  },
    { Tag("363"), true  },
    { Tag("365"), true  },
    { Tag("366"), true  },
    { Tag("370"), true  },
    { Tag("377"), true  },
    { Tag("380"), true  },
    { Tag("381"), true  },
    { Tag("382"), true  },
    { Tag("383"), true  },
    { Tag("384"), false },
    { Tag("385"), true  },
    { Tag("386"), true  },
    { Tag("388"), true  },
    { Tag("490"), true  },
    { Tag("500"), true  },
    { Tag("501"), true  },
    { Tag("502"), true  },
    { Tag("504"), true  },
    { Tag("505"), true  },
    { Tag("506"), true  },
    { Tag("507"), true  },
    { Tag("508"), true  },
    { Tag("510"), true  },
    { Tag("511"), true  },
    { Tag("513"), true  },
    { Tag("514"), false },
    { Tag("515"), true  },
    { Tag("516"), true  },
    { Tag("518"), true  },
    { Tag("520"), true  },
    { Tag("521"), true  },
    { Tag("522"), true  },
    { Tag("524"), true  },
    { Tag("525"), true  },
    { Tag("526"), true  },
    { Tag("530"), true  },
    { Tag("533"), true  },
    { Tag("534"), true  },
    { Tag("535"), true  },
    { Tag("536"), true  },
    { Tag("538"), true  },
    { Tag("540"), true  },
    { Tag("541"), true  },
    { Tag("542"), true  },
    { Tag("545"), true  },
    { Tag("546"), true  },
    { Tag("547"), true  },
    { Tag("550"), true  },
    { Tag("552"), true  },
    { Tag("555"), true  },
    { Tag("556"), true  },
    { Tag("561"), true  },
    { Tag("562"), true  },
    { Tag("563"), true  },
    { Tag("565"), true  },
    { Tag("567"), true  },
    { Tag("580"), true  },
    { Tag("581"), true  },
    { Tag("583"), true  },
    { Tag("584"), true  },
    { Tag("585"), true  },
    { Tag("586"), true  },
    { Tag("588"), true  },
    { Tag("600"), true  },
    { Tag("601"), true  }, // non-standard field only used locally
    { Tag("610"), true  },
    { Tag("611"), true  },
    { Tag("630"), true  },
    { Tag("647"), true  },
    { Tag("648"), true  },
    { Tag("650"), true  },
    { Tag("651"), true  },
    { Tag("652"), false }, // non-standard field only used locally
    { Tag("653"), true  },
    { Tag("654"), true  },
    { Tag("655"), true  },
    { Tag("657"), true  },
    { Tag("658"), true  },
    { Tag("662"), true  },
    { Tag("700"), true  },
    { Tag("710"), true  },
    { Tag("711"), true  },
    { Tag("720"), true  },
    { Tag("730"), true  },
    { Tag("740"), true  },
    { Tag("751"), true  },
    { Tag("752"), true  },
    { Tag("752"), true  },
    { Tag("754"), true  },
    { Tag("758"), true  },
    { Tag("760"), true  },
    { Tag("762"), true  },
    { Tag("765"), true  },
    { Tag("767"), true  },
    { Tag("770"), true  },
    { Tag("772"), true  },
    { Tag("773"), true  },
    { Tag("774"), true  },
    { Tag("775"), true  },
    { Tag("776"), true  },
    { Tag("777"), true  },
    { Tag("780"), true  },
    { Tag("785"), true  },
    { Tag("786"), true  },
    { Tag("787"), true  },
    { Tag("800"), true  },
    { Tag("810"), true  },
    { Tag("811"), true  },
    { Tag("830"), true  },
    { Tag("841"), false },
    { Tag("842"), false },
    { Tag("843"), true  },
    { Tag("844"), true  },
    { Tag("845"), true  },
    { Tag("850"), true  },
    { Tag("852"), true  },
    { Tag("853"), true  },
    { Tag("854"), true  },
    { Tag("855"), true  },
    { Tag("856"), true  },
    { Tag("863"), true  },
    { Tag("864"), true  },
    { Tag("865"), true  },
    { Tag("866"), true  },
    { Tag("867"), true  },
    { Tag("868"), true  },
    { Tag("876"), true  },
    { Tag("877"), true  },
    { Tag("878"), true  },
    { Tag("880"), true  },
    { Tag("882"), true  },
    { Tag("883"), true  },
    { Tag("884"), true  },
    { Tag("885"), true  },
    { Tag("886"), true  },
    { Tag("887"), true  },
};


bool IsRepeatableField(const Tag &tag) {
    // 1. Handle all local fields.
    if (tag.to_string().find('9') != std::string::npos or StringUtil::IsAsciiLetter(tag.to_string()[0]))
        return true;

    // 2. Handle all other fields.
    const auto tag_and_repeatable(tag_to_repeatable_map.find(tag));
    if (unlikely(tag_and_repeatable == tag_to_repeatable_map.end()))
        ERROR(tag.to_string() + " is not in our map!");
    return tag_and_repeatable->second;
}


bool IsOpenAccess(const Record &marc_record) {
    for (const auto &_856_field : marc_record.getTagRange("856")) {
        const Subfields subfields(_856_field.getSubfields());
        const std::string subfield_z_contents(StringUtil::ToLower(subfields.getFirstSubfieldWithCode('z')));
        if (StringUtil::StartsWith(subfield_z_contents, "kostenfrei")) {
            const std::string subfield_3_contents(StringUtil::ToLower(subfields.getFirstSubfieldWithCode('3')));
            if (subfield_3_contents == "volltext")
                return true;
        }
    }

    return false;
}


} // namespace MARC

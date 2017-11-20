/** \brief Various classes, functions etc. having to do with the Library of Congress MARC bibliographic format.
 *  \author Dr. Johannes Ruscheinski (johannes.ruscheinski@uni-tuebingen.de)
 *
 *  \copyright 2017 Universitätsbibliothek Tübingen.  All rights reserved.
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
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include "FileUtil.h"
#include "MediaTypeUtil.h"
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


Record::Record(const size_t record_size, char * const record_start)
    : record_size_(record_size), leader_(record_start, record_size)
{
    const char * const base_address_of_data(record_start + ToUnsigned(record_start + 12, 5));

    // Process directory:
    const char *directory_entry(record_start + LEADER_LENGTH);
    while (directory_entry != base_address_of_data - 1) {
        if (unlikely(directory_entry > base_address_of_data))
            logger->error("in MARC::Record::Record: directory_entry > base_address_of_data!");
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


ssize_t Record::getFirstFieldIndex(const std::string &tag) const {
    const auto iter(std::find_if(fields_.cbegin(), fields_.cend(),
                                 [&tag](const Field &field){ return field.getTag() == tag; }));
    return (iter == fields_.cend()) ? -1 : std::distance(fields_.cbegin(), iter);
}


size_t Record::findAllLocalDataBlocks(std::vector<std::pair<size_t, size_t>> *const local_block_boundaries) const
{
    local_block_boundaries->clear();

    size_t local_block_start(getFirstFieldIndex("LOK"));
    if (static_cast<ssize_t>(local_block_start) == -1)
        return 0;

    size_t local_block_end(local_block_start + 1);
    while (local_block_end < fields_.size()) {
        if (StringUtil::StartsWith(fields_[local_block_end].getContents(), "  ""\x1F""0000")) {
            local_block_boundaries->emplace_back(std::make_pair(local_block_start, local_block_end));
            local_block_start = local_block_end;
        }
        ++local_block_end;
    }
    local_block_boundaries->emplace_back(std::make_pair(local_block_start, local_block_end));

    return local_block_boundaries->size();
}


std::unique_ptr<Reader> Reader::Factory(const std::string &input_filename, ReaderType reader_type) {
    if (reader_type == AUTO) {
        const std::string media_type(MediaTypeUtil::GetFileMediaType(input_filename));
        if (unlikely(media_type == "cannot"))
            logger->error("not found or no permissions: \"" + input_filename + "\"!");
        if (unlikely(media_type.empty()))
            logger->error("can't determine media type of \"" + input_filename + "\"!");
        if (media_type != "application/xml" and media_type != "application/marc" and media_type != "text/xml")
            logger->error("\"" + input_filename + "\" is neither XML nor MARC-21 data!");
        reader_type = (media_type == "application/xml" or media_type == "text/xml") ? XML : BINARY;
    }

    std::unique_ptr<File> input(FileUtil::OpenInputFileOrDie(input_filename));
    return (reader_type == XML) ? std::unique_ptr<Reader>(new XmlReader(input.release()))
        : std::unique_ptr<Reader>(new BinaryReader(input.release()));
}


Record BinaryReader::read() {
    char buf[Record::MAX_RECORD_LENGTH];
    size_t bytes_read;
    if (unlikely((bytes_read = input_->read(buf, Record::RECORD_LENGTH_FIELD_LENGTH)) == 0))
        return Record();

    if (unlikely(bytes_read != Record::RECORD_LENGTH_FIELD_LENGTH))
        logger->error("in MARC::BinaryReader::read: failed to read record length!");
    const unsigned record_length(ToUnsigned(buf, Record::RECORD_LENGTH_FIELD_LENGTH));

    bytes_read = input_->read(buf + Record::RECORD_LENGTH_FIELD_LENGTH,
                              record_length - Record::RECORD_LENGTH_FIELD_LENGTH);
    if (unlikely(bytes_read != record_length - Record::RECORD_LENGTH_FIELD_LENGTH))
        logger->error("in MARC::BinaryReader::read: failed to read a record!");

    return Record(record_length, buf);
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
        logger->error("in XmlReader::rewind: can't rewind a FIFO!");

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
        logger->warning("in ParseLeader(MARC.cc): Invalid indicator count '" + leader_string.substr(10, 1) + "'!");

    // Check subfield code length:
    if (leader_string[11] != '2')
        logger->warning("in ParseLeader(MARC.cc): Invalid subfield code length! (Leader bytes are "
                        + StringUtil:: CStyleEscape(leader_string) + ")");

    // Check entry map:
    if (leader_string.substr(20, 3) != "450")
        logger->warning("in Leader::ParseLeader: Invalid entry map!");

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
        logger->warning("in XmlReader::ParseLeader: leader data expected while parsing \"" + input_filename
                        + "\" on line "
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
        logger->warning("in MARC::XmlReader::parseControlfield: empty \"" + tag + "\" control field on line "
                        + std::to_string(xml_parser_->getLineNo()) + " in file \"" + input_filename + "\"!");
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
                logger->warning("in MARC::XmlReader::parseDatafield: lFound an empty subfield on line "
                                + std::to_string(xml_parser_->getLineNo()) + " in file \"" + input_filename
                                + "\"!");
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
    Record::const_iterator start(record.begin());
    do {
        Record::const_iterator end(start);
        unsigned record_size(Record::LEADER_LENGTH + 2 /* end-of-directory and end-of-record */);
        while (end != record.end()
               and (record_size + end->getContents().length() + 1 + Record::DIRECTORY_ENTRY_LENGTH
                    < Record::MAX_RECORD_LENGTH))
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
        for (Record::const_iterator entry(start); entry != end; ++entry) {
            raw_record += entry->getTag()
                          + ToStringWithLeadingZeros(entry->getContents().length() + 1 /* field terminator */, 4)
                          + ToStringWithLeadingZeros(field_start_offset, /* width = */ 5);
            field_start_offset += entry->getContents().length() + 1 /* field terminator */;
        }
        raw_record += '\x1E'; // end-of-directory

        // Now append the field data:
        for (Record::const_iterator entry(start); entry != end; ++entry) {
            raw_record += entry->getContents();
            raw_record += '\x1E'; // end-of-field
        }
        raw_record += '\x1D'; // end-of-record

        output_.write(raw_record);

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
            xml_writer_->writeTagsWithData("controlfield", { std::make_pair("tag", field.getTag()) },
                                           field.getContents(),
                    /* suppress_newline = */ true);
        else { // We have a data field.
            xml_writer_->openTag("datafield",
                                { std::make_pair("tag", field.getTag()),
                                  std::make_pair("ind1", std::string(1, field.getContents()[0])),
                                  std::make_pair("ind2", std::string(1, field.getContents()[1]))
                                });

            std::string::const_iterator ch(field.getContents().cbegin() + 2 /* Skip over the indicators. */);

            while (ch != field.getContents().cend()) {
                if (*ch != '\x1F')
                    std::runtime_error("in MARC::XmlWriter::write: expected subfield code delimiter not found! "
                                       "Found " + std::string(1, *ch) + "! (Control number is "
                                       + record.getControlNumber() + ".)");

                ++ch;
                if (ch == field.getContents().cend())
                    std::runtime_error("in MARC::XmlWriter::write: unexpected subfield data end while expecting a "
                                       "subfield code!");
                const std::string subfield_code(1, *ch++);

                std::string subfield_data;
                while (ch != field.getContents().cend() and *ch != '\x1F')
                    subfield_data += *ch++;
                if (subfield_data.empty())
                    continue;

                xml_writer_->writeTagsWithData("subfield", { std::make_pair("code", subfield_code) },
                                              subfield_data, /* suppress_newline = */ true);
            }

            xml_writer_->closeTag(); // Close "datafield".
        }
    }

    xml_writer_->closeTag(); // Close "record".
}


} // namespace MARC

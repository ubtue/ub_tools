/** \brief Reader for Marc files.
 *  \author Oliver Obenland (oliver.obenland@uni-tuebingen.de)
 *  \author Dr. Johannes Ruscheinski (johannes.ruscheinski@uni-tuebingen.de)
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

#include "FileUtil.h"
#include "MarcReader.h"
#include "MarcRecord.h"
#include "MediaTypeUtil.h"
#include "SimpleXmlParser.h"
#include <sys/types.h>
#include <sys/stat.h>


MarcRecord BinaryMarcReader::read() {
    MarcRecord current_record(MarcRecord::ReadSingleRecord(input_));
    if (not current_record)
        return current_record;

    bool is_multi_part(current_record.getLeader().isMultiPartRecord());
    while (is_multi_part) {
        const MarcRecord next_record(MarcRecord::ReadSingleRecord(input_));
        current_record.combine(next_record);
        is_multi_part = next_record.getLeader().isMultiPartRecord();
    }
    return current_record;
}


static void ParseLeader(const std::string &input_filename, Leader * const leader, SimpleXmlParser * const xml_parser)
{
    SimpleXmlParser::Type type;
    std::map<std::string, std::string> attrib_map;
    std::string data;

    while (xml_parser->getNext(&type, &attrib_map, &data) and type == SimpleXmlParser::CHARACTERS)
        /* Intentionally empty! */;
    if (unlikely(type != SimpleXmlParser::OPENING_TAG or data != "marc:leader"))
        throw std::runtime_error("in MarcReader::ParseLeader: opening <marc:leader> tag expected while parsing \""
                                 + input_filename + "\" on line " + std::to_string(xml_parser->getLineNo()) + ".");

    if (unlikely(not xml_parser->getNext(&type, &attrib_map, &data)))
        throw std::runtime_error("in MarcReader::ParseLeader: error while parsing \"" + input_filename + "\": "
                                 + xml_parser->getLastErrorMessage() + " on line "
                                 + std::to_string(xml_parser->getLineNo()) + ".");
    if (unlikely(type != SimpleXmlParser::CHARACTERS or data.length() != Leader::LEADER_LENGTH))
        throw std::runtime_error("in MarcReader::ParseLeader: leader data expected while parsing \"" + input_filename
                                 + "\" on line " + std::to_string(xml_parser->getLineNo()) + ".");

    if (data.substr(0, 5) == "     ") // record length
        data = "00000" + data.substr(5);
    if (data.substr(12, 5) == "     ") // base address of data
        data = data.substr(0, 12) + "00000" + data.substr(12 + 5);
    std::string err_msg;
    if (unlikely(not Leader::ParseLeader(data, leader, &err_msg)))
        throw std::runtime_error("in MarcReader::ParseLeader: error while parsing leader data: " + err_msg);

    if (unlikely(not xml_parser->getNext(&type, &attrib_map, &data)))
        throw std::runtime_error("in MarcReader::ParseLeader: error while parsing \"" + input_filename + "\": "
                                 + xml_parser->getLastErrorMessage() + " on line "
                                 + std::to_string(xml_parser->getLineNo()) + ".");
    if (unlikely(type != SimpleXmlParser::CLOSING_TAG or data != "marc:leader")) {
        const bool tag_found(type == SimpleXmlParser::OPENING_TAG or type == SimpleXmlParser::CLOSING_TAG);
        throw std::runtime_error("in MarcReader::ParseLeader: closing </leader> tag expected while parsing \""
                                 + input_filename + "\" on line " + std::to_string(xml_parser->getLineNo())
                                 + ". (Found: " + SimpleXmlParser::TypeToString(type)
                                 + (tag_found ? (":" + data) : ""));
    }
}


static DirectoryEntry ParseControlfield(const std::string &input_filename, SimpleXmlParser * const xml_parser,
                                        const std::string &tag, std::string &raw_data)
{
    const size_t offset = raw_data.size();

    SimpleXmlParser::Type type;
    std::map<std::string, std::string> attrib_map;
    std::string data;
    if (unlikely(not xml_parser->getNext(&type, &attrib_map, &data) or type != SimpleXmlParser::CHARACTERS))
        throw std::runtime_error("in MarcReader::ParseControlfield: character data expected on line "
                                 + std::to_string(xml_parser->getLineNo()) + " in file \"" + input_filename + "\"!");
    raw_data += data + '\x1E';

    if (unlikely(not xml_parser->getNext(&type, &attrib_map, &data) or type != SimpleXmlParser::CLOSING_TAG
                 or data != "marc:controlfield"))
        throw std::runtime_error("in MarcReader::ParseControlfield: </controlfield> expected on line "
                                 + std::to_string(xml_parser->getLineNo()) + " in file \"" + input_filename + "\"!");

    return DirectoryEntry(tag, raw_data.size() - offset, offset);
}


static DirectoryEntry ParseDatafield(const std::string &input_filename,
                                     const std::map<std::string, std::string> &datafield_attrib_map,
                                     SimpleXmlParser * const xml_parser, std::string tag, std::string &raw_data)
{
    const auto ind1(datafield_attrib_map.find("ind1"));
    if (unlikely(ind1 == datafield_attrib_map.cend() or ind1->second.length() != 1))
        throw std::runtime_error("in MarcReader::ParseDatafield: bad or missing \"ind1\" attribute on line "
                                 + std::to_string(xml_parser->getLineNo()) + " in file \"" + input_filename + "\"!");
    std::string field_data(ind1->second);

    const auto ind2(datafield_attrib_map.find("ind2"));
    if (unlikely(ind2 == datafield_attrib_map.cend() or ind2->second.length() != 1))
        throw std::runtime_error("in MarcReader::ParseDatafield: bad or missing \"ind2\" attribute on line "
                                 + std::to_string(xml_parser->getLineNo()) + " in file \"" + input_filename + "\"!");
    field_data += ind2->second;

    const size_t offset = raw_data.size();
    SimpleXmlParser::Type type;
    std::map<std::string, std::string> attrib_map;
    std::string data;
    for (;;) {
        while (xml_parser->getNext(&type, &attrib_map, &data) and type == SimpleXmlParser::CHARACTERS)
            /* Intentionally empty! */;

        if (type == SimpleXmlParser::ERROR)
            throw std::runtime_error("in MarcReader::ParseDatafield: error while parsing a data field on line "
                                     + std::to_string(xml_parser->getLineNo()) + " in file \"" + input_filename
                                     + "\": " + xml_parser->getLastErrorMessage());

        if (type == SimpleXmlParser::CLOSING_TAG and data == "marc:datafield") {
            raw_data += field_data + '\x1E';
            return DirectoryEntry(tag, raw_data.size() - offset, offset);
        }

        // 1. <subfield code=...>
        if (unlikely(type != SimpleXmlParser::OPENING_TAG or data != "marc:subfield")) {
            const bool tag_found(type == SimpleXmlParser::OPENING_TAG or type == SimpleXmlParser::CLOSING_TAG);
            throw std::runtime_error("in MarcReader::ParseDatafield: expected <marc:subfield> opening tag on line "
                                     + std::to_string(xml_parser->getLineNo()) + " in file \"" + input_filename
                                     + "\"! (Found: " + SimpleXmlParser::TypeToString(type)
                                     + (tag_found ? (":" + data) : ""));
        }
        if (unlikely(attrib_map.find("code") == attrib_map.cend() or attrib_map["code"].length() != 1))
            throw std::runtime_error("in MarcReader::ParseDatafield: missing or invalid \"code\" attribute as part "
                                     "of the <subfield> tag " + std::to_string(xml_parser->getLineNo())
                                     + " in file \"" + input_filename + "\"!");
        field_data += '\x1F' + attrib_map["code"];

        // 2. Subfield data.
        if (unlikely(not xml_parser->getNext(&type, &attrib_map, &data) or type != SimpleXmlParser::CHARACTERS))
            throw std::runtime_error("in MarcReader::ParseDatafield: error while looking for character data after a "
                                     "<subfield> tag on line " + std::to_string(xml_parser->getLineNo())
                                     + " in file \"" + input_filename + "\": " + xml_parser->getLastErrorMessage());
        field_data += data;

        // 3. </subfield>
        while (xml_parser->getNext(&type, &attrib_map, &data) and type == SimpleXmlParser::CHARACTERS)
            /* Intentionally empty! */;
        if (unlikely(type != SimpleXmlParser::CLOSING_TAG or data != "marc:subfield")) {
            const bool tag_found(type == SimpleXmlParser::OPENING_TAG or type == SimpleXmlParser::CLOSING_TAG);
            throw std::runtime_error("in MarcReader::ParseDatafield: expected </subfield> closing tag on line "
                                     + std::to_string(xml_parser->getLineNo()) + " in file \"" + input_filename
                                     + "\"! (Found: " + SimpleXmlParser::TypeToString(type)
                                     + (tag_found ? (":" + data) : ""));
        }
    }
}


static void SkipOverStartOfDocument(SimpleXmlParser * const xml_parser) {
    SimpleXmlParser::Type type;
    std::map<std::string, std::string> attrib_map;
    std::string data;
    if (unlikely(not xml_parser->getNext(&type, &attrib_map, &data) and type != SimpleXmlParser::START_OF_DOCUMENT))
        throw std::runtime_error("in MarcReader::SkipOverStartOfDocument: error while parsing start of \""
                                 + xml_parser->getInputFile()->getPath() + "\": " + xml_parser->getLastErrorMessage()
                                 + " on line " + std::to_string(xml_parser->getLineNo())
                                 + "! (Expected start-of-document.)");
    if (unlikely(not xml_parser->getNext(&type, &attrib_map, &data)))
        throw std::runtime_error("in MarcReader::SkipOverStartOfDocument: error while parsing start of \""
                                 + xml_parser->getInputFile()->getPath() + "\": " + xml_parser->getLastErrorMessage()
                                 + " on line " + std::to_string(xml_parser->getLineNo()) + "!");
    if (unlikely(type != SimpleXmlParser::OPENING_TAG or data != "marc:collection")) {
        const bool tag_found(type == SimpleXmlParser::OPENING_TAG or type == SimpleXmlParser::CLOSING_TAG);
        throw std::runtime_error("in MarcReader::SkipOverStartOfDocument: opening <marc:collection> tag expected "
                                         "while parsing \"" + xml_parser->getInputFile()->getPath() + "\" on line "
                                 + std::to_string(xml_parser->getLineNo()) + "! (Found: "
                                 + SimpleXmlParser::TypeToString(type) + (tag_found ? (":" + data) : "") + ")");
    }
}


XmlMarcReader::XmlMarcReader(File * const input): MarcReader(input), xml_parser_(new SimpleXmlParser(input)) {
    // If we use FIFO's we may not use tell but have to skip over the start of the XML document anyway:
    struct stat stat_buf;
    if ((not fstat(input_->getFileDescriptor(), &stat_buf) and S_ISFIFO(stat_buf.st_mode))
        or (input_->tell() == 0))
        SkipOverStartOfDocument(xml_parser_);
}


XmlMarcReader::~XmlMarcReader() {
    delete xml_parser_;
}


MarcRecord XmlMarcReader::read() {
    Leader leader;
    std::vector<DirectoryEntry> dir_entries;
    std::string raw_data;

    SimpleXmlParser::Type type;
    std::map<std::string, std::string> attrib_map;
    std::string data;
    while (xml_parser_->getNext(&type, &attrib_map, &data) and type == SimpleXmlParser::CHARACTERS)
        /* Intentionally empty! */;

    if (unlikely(type == SimpleXmlParser::CLOSING_TAG and data == "marc:collection"))
        return MarcRecord(leader, dir_entries, raw_data);

    //
    // Now parse a <record>:
    //

    if (unlikely(type != SimpleXmlParser::OPENING_TAG or data != "marc:record")) {
        const bool tag_found(type == SimpleXmlParser::OPENING_TAG or type == SimpleXmlParser::CLOSING_TAG);
        if (type == SimpleXmlParser::ERROR)
            throw std::runtime_error("in XmlMarcReader::read: opening <record> tag expected while parsing \""
                                     + input_->getPath() + "\" on line " + std::to_string(xml_parser_->getLineNo())
                                     + "! (" + xml_parser_->getLastErrorMessage() + ")");
        else
            throw std::runtime_error("in XmlMarcReader::read: opening <record> tag expected while parsing \""
                                     + input_->getPath() + "\" on line " + std::to_string(xml_parser_->getLineNo())
                                     + "! (Found: " + SimpleXmlParser::TypeToString(type)
                                     + (tag_found ? (":" + data + ")") : ")"));
    }

    ParseLeader(input_->getPath(), &leader, xml_parser_);

    bool datafield_seen(false);
    for (;;) { // Process "datafield" and "controlfield" sections.
        if (unlikely(not xml_parser_->getNext(&type, &attrib_map, &data)))
            throw std::runtime_error("in XmlMarcReader::read: error while parsing \"" + input_->getPath()
                                     + "\": " + xml_parser_->getLastErrorMessage() + " on line "
                                     + std::to_string(xml_parser_->getLineNo()) + "!");

        if (type == SimpleXmlParser::CLOSING_TAG) {
            if (unlikely(data != "marc:record"))
                throw std::runtime_error("in MarcUtil::Record::XmlFactory: closing </record> tag expected while "
                                         "parsing \"" + input_->getPath() + "\" on line "
                                         + std::to_string(xml_parser_->getLineNo()) + "!");
            return MarcRecord(leader, dir_entries, raw_data);
        }

        if (type != SimpleXmlParser::OPENING_TAG or (data != "marc:datafield" and data != "marc:controlfield"))
            throw std::runtime_error("in XmlMarcReader::read: expected either <controlfield> or <datafield> on line "
                                     + std::to_string(xml_parser_->getLineNo()) + " in file \"" + input_->getPath()
                                     + "\"!");

        if (unlikely(attrib_map.find("tag") == attrib_map.end()))
            throw std::runtime_error("in XmlMarcReader::read: expected a \"tag\" attribute as part of an opening "
                                     "<controlfield> or <datafield> tag on line "
                                     + std::to_string(xml_parser_->getLineNo()) + " in file \"" + input_->getPath()
                                     + "\"!");

        if (data == "marc:controlfield") {
            if (unlikely(datafield_seen))
                throw std::runtime_error("in MarcUtil::Record::XmlFactory: <controlfield> found after <datafield> on "
                                         "line " + std::to_string(xml_parser_->getLineNo()) + " in file \""
                                         + input_->getPath() + "\"!");
            dir_entries.emplace_back(ParseControlfield(input_->getPath(), xml_parser_, attrib_map["tag"], raw_data));
        } else {
            datafield_seen = true;
            dir_entries.emplace_back(ParseDatafield(input_->getPath(), attrib_map, xml_parser_, attrib_map["tag"],
                                                    raw_data));
        }
    }
}


void XmlMarcReader::rewind() {
    delete xml_parser_;
    xml_parser_ = new SimpleXmlParser(input_);

    // If we use FIFO's we may not use tell but have to skip over the start of the XML document anyway:
    struct stat stat_buf;
    if ((not fstat(input_->getFileDescriptor(), &stat_buf) and S_ISFIFO(stat_buf.st_mode)) or (input_->tell() == 0))
        SkipOverStartOfDocument(xml_parser_);
}


std::unique_ptr<MarcReader> MarcReader::Factory(const std::string &input_filename,
                                                ReaderType reader_type)
{
    if (reader_type == AUTO) {
        const std::string media_type(MediaTypeUtil::GetFileMediaType(input_filename));
        if (unlikely(media_type.empty()))
            Error("can't determine media type of \"" + input_filename + "\"!");
        if (media_type != "application/xml" and media_type != "application/marc")
            Error("\"" + input_filename + "\" is neither XML nor MARC-21 data!");
        reader_type = (media_type == "application/xml") ? XML : BINARY;
    }

    std::unique_ptr<File> input(FileUtil::OpenInputFileOrDie(input_filename));
    return (reader_type == XML) ? std::unique_ptr<MarcReader>(new XmlMarcReader(input.release()))
                                : std::unique_ptr<MarcReader>(new BinaryMarcReader(input.release()));
}

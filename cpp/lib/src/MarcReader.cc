/** \brief Reader for Marc files.
 *  \author Oliver Obenland (oliver.obenland@uni-tuebingen.de)
 *  \author Dr. Johannes Ruscheinski (johannes.ruscheinski@uni-tuebingen.de)
 *
 *  \copyright 2016,2017 Universitätsbibliothek Tübingen.  All rights reserved.
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


MarcRecord XmlMarcReader::read() {
    Leader leader;
    std::vector<DirectoryEntry> dir_entries;
    std::string raw_data;

    XMLParser::XMLPart xml_part;
    while (getNext(&xml_part) and xml_part.type_ == XMLParser::XMLPart::CHARACTERS)
        /* Intentionally empty! */;

    if (unlikely(xml_part.type_ == XMLParser::XMLPart::CLOSING_TAG and xml_part.data_ == namespace_prefix_ + "collection"))
        return MarcRecord(leader, dir_entries, raw_data);

        //
        // Now parse a <record>:
        //

    if (unlikely(xml_part.type_ != XMLParser::XMLPart::OPENING_TAG or xml_part.data_ != namespace_prefix_ + "record")) {
        const bool tag_found(xml_part.type_ == XMLParser::XMLPart::OPENING_TAG or xml_part.type_ == XMLParser::XMLPart::CLOSING_TAG);
        throw std::runtime_error("in XmlMarcReader::read: opening <" + namespace_prefix_
                                 + "record> tag expected while parsing \"" + input_->getPath() + "\" on line "
                                 + std::to_string(xml_parser_->getLineNo()) + "! (Found: "
                                 + XMLParser::XMLPart::TypeToString(xml_part.type_)
                                 + (tag_found ? (":" + xml_part.data_ + ")") : ")"));
    }

    parseLeader(input_->getPath(), &leader);

    bool datafield_seen(false);
    for (;;) { // Process "datafield" and "controlfield" sections.
        if (unlikely(not getNext(&xml_part)))
            throw std::runtime_error("in XmlMarcReader::read: error while parsing \"" + input_->getPath()
                                     + "\": unexpected end of file on line "
                                     + std::to_string(xml_parser_->getLineNo()) + "!");

        if (xml_part.type_ == XMLParser::XMLPart::CLOSING_TAG) {
            if (unlikely(xml_part.data_ != namespace_prefix_ + "record"))
                throw std::runtime_error("in MarcUtil::Record::XmlFactory: closing </record> tag expected while "
                                         "parsing \"" + input_->getPath() + "\" on line "
                                         + std::to_string(xml_parser_->getLineNo()) + "!");
            return MarcRecord(leader, dir_entries, raw_data);
        }

        if (xml_part.type_ != XMLParser::XMLPart::OPENING_TAG
            or (xml_part.data_ != namespace_prefix_ + "datafield" and xml_part.data_ != namespace_prefix_ + "controlfield"))
            throw std::runtime_error("in XmlMarcReader::read: expected either <" + namespace_prefix_
                                     + "controlfield> or <" + namespace_prefix_ + "datafield> on line "
                                     + std::to_string(xml_parser_->getLineNo()) + " in file \"" + input_->getPath()
                                     + "\"!");

        if (unlikely(xml_part.attributes_.find("tag") == xml_part.attributes_.end()))
            throw std::runtime_error("in XmlMarcReader::read: expected a \"tag\" attribute as part of an opening "
                                     "<" + namespace_prefix_ + "controlfield> or <" + namespace_prefix_
                                     + "datafield> tag on line " + std::to_string(xml_parser_->getLineNo())
                                     + " in file \"" + input_->getPath() + "\"!");

        if (xml_part.data_ == namespace_prefix_ + "controlfield") {
            if (unlikely(datafield_seen))
                throw std::runtime_error("in MarcUtil::Record::XmlFactory: <" + namespace_prefix_
                                         + "controlfield> found after <" + namespace_prefix_ + "datafield> on "
                                         "line " + std::to_string(xml_parser_->getLineNo()) + " in file \""
                                         + input_->getPath() + "\"!");
            DirectoryEntry dir_entry("000", 0, 0);
            if (parseControlfield(input_->getPath(), xml_part.attributes_["tag"], &dir_entry, &raw_data))
                dir_entries.emplace_back(dir_entry);
        } else {
            datafield_seen = true;
            dir_entries.emplace_back(parseDatafield(input_->getPath(), xml_part.attributes_, xml_part.attributes_["tag"], raw_data));
        }
    }
}


void XmlMarcReader::rewind() {
        // We can't handle FIFO's here:
    struct stat stat_buf;
    if (unlikely(fstat(input_->getFileDescriptor(), &stat_buf) and S_ISFIFO(stat_buf.st_mode)))
        logger->error("in XmlMarcReader::rewind: can't rewind a FIFO!");

    xml_parser_->rewind();
    skipOverStartOfDocument();
}


void XmlMarcReader::parseLeader(const std::string &input_filename, Leader * const leader) {
    XMLParser::XMLPart xml_part;
    while (getNext(&xml_part) and xml_part.type_ == XMLParser::XMLPart::CHARACTERS)
        /* Intentionally empty! */;
    if (unlikely(xml_part.type_ != XMLParser::XMLPart::OPENING_TAG or xml_part.data_ != namespace_prefix_ + "leader"))
        throw std::runtime_error("in XmlMarcReader::ParseLeader: opening <marc:leader> tag expected while parsing \""
                                 + input_filename + "\" on line " + std::to_string(xml_parser_->getLineNo()) + ".");

    if (unlikely(not getNext(&xml_part)))
        throw std::runtime_error("in XmlMarcReader::ParseLeader: error while parsing \"" + input_filename + "\": "
                                 + "unexpected end of file on line "
                                 + std::to_string(xml_parser_->getLineNo()) + ".");
    if (unlikely(xml_part.type_ != XMLParser::XMLPart::CHARACTERS or xml_part.data_.length() != Leader::LEADER_LENGTH)) {
        logger->warning("in XmlMarcReader::ParseLeader: leader data expected while parsing \"" + input_filename
                        + "\" on line "
                + std::to_string(xml_parser_->getLineNo()) + ".");
        if (unlikely(not getNext(&xml_part)))
            throw std::runtime_error("in XmlMarcReader::ParseLeader: error while skipping to </" + namespace_prefix_
                                     + "leader>!");
        if (unlikely(xml_part.type_ != XMLParser::XMLPart::CLOSING_TAG or xml_part.data_ != namespace_prefix_ + "leader")) {
            const bool tag_found(xml_part.type_ == XMLParser::XMLPart::OPENING_TAG or xml_part.type_ == XMLParser::XMLPart::CLOSING_TAG);
            throw std::runtime_error("in XmlMarcReader::ParseLeader: closing </" + namespace_prefix_
                                     + "leader> tag expected while parsing \"" + input_filename + "\" on line "
                                     + std::to_string(xml_parser_->getLineNo())
                                     + ". (Found: " + XMLParser::XMLPart::TypeToString(xml_part.type_)
                                     + (tag_found ? (":" + xml_part.data_) : ""));
        }
        return;
    }

    if (xml_part.data_.substr(0, 5) == "     ") // record length
        xml_part.data_ = "00000" + xml_part.data_.substr(5);
    if (xml_part.data_.substr(12, 5) == "     ") // base address of data
        xml_part.data_ = xml_part.data_.substr(0, 12) + "00000" + xml_part.data_.substr(12 + 5);
    std::string err_msg;
    if (unlikely(not Leader::ParseLeader(xml_part.data_, leader, &err_msg)))
        throw std::runtime_error("in XmlMarcReader::ParseLeader: error while parsing leader data: " + err_msg);

    if (unlikely(not getNext(&xml_part)))
        throw std::runtime_error("in XmlMarcReader::ParseLeader: error while parsing \"" + input_filename + "\": "
                                 + "unexpected end of file on line "
                                 + std::to_string(xml_parser_->getLineNo()) + ".");
    if (unlikely(xml_part.type_ != XMLParser::XMLPart::CLOSING_TAG or xml_part.data_ != namespace_prefix_ + "leader")) {
        const bool tag_found(xml_part.type_ == XMLParser::XMLPart::OPENING_TAG
                             or xml_part.type_ == XMLParser::XMLPart::CLOSING_TAG);
        throw std::runtime_error("in XmlMarcReader::ParseLeader: closing </" + namespace_prefix_
                                                                  + "leader> tag expected while parsing \"" + input_filename + "\" on line "
                                 + std::to_string(xml_parser_->getLineNo())
                                 + ". (Found: " + XMLParser::XMLPart::TypeToString(xml_part.type_)
                                 + (tag_found ? (":" + xml_part.data_) : ""));
    }
}


// Returns true if we found a normal control field and false if we found an empty control field.
bool XmlMarcReader::parseControlfield(const std::string &input_filename, const std::string &tag,
                                      DirectoryEntry *dir_entry, std::string * const raw_data)
{
    const size_t offset(raw_data->size());

    XMLParser::XMLPart xml_part;
    if (unlikely(not getNext(&xml_part)))
        throw std::runtime_error("in XmlMarcReader::ParseControlfield: failed to get next XML element!");

        // Do we have an empty control field?
    if (unlikely(xml_part.type_ == XMLParser::XMLPart::CLOSING_TAG and xml_part.data_ == namespace_prefix_ + "controlfield")) {
        logger->warning("in XmlMarcReader::ParseControlfield: empty \"" + tag + "\" control field on line "
                        + std::to_string(xml_parser_->getLineNo()) + " in file \"" + input_filename + "\"!");
        return false;
    }

    if (xml_part.type_ != XMLParser::XMLPart::CHARACTERS)
        std::runtime_error("in XmlMarcReader::ParseControlfield: character data expected on line "
                           + std::to_string(xml_parser_->getLineNo()) + " in file \"" + input_filename + "\"!");
    *raw_data += xml_part.data_ + '\x1E';

    if (unlikely(not getNext(&xml_part) or xml_part.type_ != XMLParser::XMLPart::CLOSING_TAG
                 or xml_part.data_ != namespace_prefix_ + "controlfield"))
        throw std::runtime_error("in XmlMarcReader::ParseControlfield: </controlfield> expected on line "
                                 + std::to_string(xml_parser_->getLineNo()) + " in file \"" + input_filename + "\"!");

    dir_entry = new (reinterpret_cast<void *>(dir_entry)) DirectoryEntry(tag, raw_data->size() - offset, offset);
    return true;
}


DirectoryEntry XmlMarcReader::parseDatafield(const std::string &input_filename,
                                             const std::map<std::string, std::string> &datafield_attrib_map,
                                             std::string tag, std::string &raw_data)
{
    const auto ind1(datafield_attrib_map.find("ind1"));
    if (unlikely(ind1 == datafield_attrib_map.cend() or ind1->second.length() != 1))
        throw std::runtime_error("in XmlMarcReader::ParseDatafield: bad or missing \"ind1\" attribute on line "
                                 + std::to_string(xml_parser_->getLineNo()) + " in file \"" + input_filename + "\"!");
    std::string field_data(ind1->second);

    const auto ind2(datafield_attrib_map.find("ind2"));
    if (unlikely(ind2 == datafield_attrib_map.cend() or ind2->second.length() != 1))
        throw std::runtime_error("in XmlMarcReader::ParseDatafield: bad or missing \"ind2\" attribute on line "
                                 + std::to_string(xml_parser_->getLineNo()) + " in file \"" + input_filename + "\"!");
    field_data += ind2->second;

    const size_t offset = raw_data.size();
    XMLParser::XMLPart xml_part;
    for (;;) {
        while (getNext(&xml_part) and xml_part.type_ == XMLParser::XMLPart::CHARACTERS)
            /* Intentionally empty! */;

        if (xml_part.type_ == XMLParser::XMLPart::CLOSING_TAG and xml_part.data_ == namespace_prefix_ + "datafield") {
            raw_data += field_data + '\x1E';
            return DirectoryEntry(tag, raw_data.size() - offset, offset);
        }

                // 1. <subfield code=...>
        if (unlikely(xml_part.type_ != XMLParser::XMLPart::OPENING_TAG or xml_part.data_ != namespace_prefix_ + "subfield")) {
            const bool tag_found(xml_part.type_ == XMLParser::XMLPart::OPENING_TAG
                                 or xml_part.type_ == XMLParser::XMLPart::CLOSING_TAG);
            throw std::runtime_error("in XmlMarcReader::ParseDatafield: expected <" + namespace_prefix_ +
                                     "subfield> opening tag on line " + std::to_string(xml_parser_->getLineNo())
                                     + " in file \"" + input_filename
                                     + "\"! (Found: " + XMLParser::XMLPart::TypeToString(xml_part.type_)
                                     + (tag_found ? (":" + xml_part.data_) : ""));
        }
        if (unlikely(xml_part.attributes_.find("code") == xml_part.attributes_.cend() or xml_part.attributes_["code"].length() != 1))
            throw std::runtime_error("in XmlMarcReader::ParseDatafield: missing or invalid \"code\" attribute as "
                                     "rt   of the <subfield> tag " + std::to_string(xml_parser_->getLineNo())
                                     + " in file \"" + input_filename + "\"!");
        field_data += '\x1F' + xml_part.attributes_["code"];

                // 2. Subfield data.
        if (unlikely(not getNext(&xml_part) or xml_part.type_ != XMLParser::XMLPart::CHARACTERS)) {
            if (xml_part.type_ == XMLParser::XMLPart::CLOSING_TAG and xml_part.data_ == namespace_prefix_ + "subfield") {
                logger->warning("Found an empty subfield on line " + std::to_string(xml_parser_->getLineNo())
                                + " in file \"" + input_filename + "\"!");
                field_data.resize(field_data.length() - 2); // Remove subfield delimiter and code.
                continue;
            }
            throw std::runtime_error("in XmlMarcReader::ParseDatafield: error while looking for character data after "
                                     "  <" + namespace_prefix_ + "subfield> tag on line "
                                     + std::to_string(xml_parser_->getLineNo()) + " in file \"" + input_filename + "\"");
        }
        field_data += xml_part.data_;

                // 3. </subfield>
        if (unlikely(not getNext(&xml_part) or xml_part.type_ != XMLParser::XMLPart::CLOSING_TAG
                     or xml_part.data_ != namespace_prefix_ + "subfield"))
            {
                const bool tag_found(xml_part.type_ == XMLParser::XMLPart::OPENING_TAG
                                     or xml_part.type_ == XMLParser::XMLPart::CLOSING_TAG);
                throw std::runtime_error("in XmlMarcReader::ParseDatafield: expected </" + namespace_prefix_
                                         + "subfield> closing tag on line " + std::to_string(xml_parser_->getLineNo())
                                                                              + " in file \"" + input_filename
                                         + "\"! (Found: " + XMLParser::XMLPart::TypeToString(xml_part.type_)
                                         + (tag_found ? (":" + xml_part.data_) : ""));
            }
    }
}


void XmlMarcReader::skipOverStartOfDocument() {
    XMLParser::XMLPart xml_part;
    while (getNext(&xml_part)) {
        if (xml_part.type_ == XMLParser::XMLPart::OPENING_TAG and xml_part.data_ == namespace_prefix_ + "collection")
            return;
    }

        // We should never get here!
    throw std::runtime_error("in XmlMarcReader::SkipOverStartOfDocument: error while trying to skip to "
                                                          "<" + namespace_prefix_ + "collection>:  \""
                             + input_->getPath() + "\": on line " + std::to_string(xml_parser_->getLineNo()) + "!");
}


bool XmlMarcReader::getNext(XMLParser::XMLPart * const xml_part) {
    if (unlikely(not xml_parser_->getNext(xml_part)))
        return false;

    if (xml_part->type_ != XMLParser::XMLPart::OPENING_TAG)
        return true;

    auto key_and_value(xml_part->attributes_.find("xmlns"));
    if (unlikely(key_and_value != xml_part->attributes_.cend() and key_and_value->second != "http://www.loc.gov/MARC21/slim"))
        throw std::runtime_error("in XmlMarcReader::getNext: opening tag has unsupported \"xmlns\" attribute near "
                                 "line #" + std::to_string(xml_parser_->getLineNo()) + " in \"" + getPath() + "\"!");

    key_and_value = xml_part->attributes_.find("xmlns:marc");
    if (unlikely(key_and_value != xml_part->attributes_.cend())) {
        if (unlikely(key_and_value->second != "http://www.loc.gov/MARC21/slim"))
            throw std::runtime_error("in XmlMarcReader::getNext: opening tag has unsupported \"xmlns:marc\" "
                                     "attribute near line #" + std::to_string(xml_parser_->getLineNo()) + " in \""
                                     + getPath() + "\"!");
        else
            namespace_prefix_ = "marc:";
    }

    return true;
}


std::unique_ptr<MarcReader> MarcReader::Factory(const std::string &input_filename, ReaderType reader_type) {
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
    return (reader_type == XML) ? std::unique_ptr<MarcReader>(new XmlMarcReader(input.release()))
        : std::unique_ptr<MarcReader>(new BinaryMarcReader(input.release()));
}

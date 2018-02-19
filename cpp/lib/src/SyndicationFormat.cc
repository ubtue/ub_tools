/** \brief Interface of the SyndicationFormat class and descendents thereof.
 *  \author Dr. Johannes Ruscheinski (johannes.ruscheinski@uni-tuebingen.de)
 *
 *  \copyright 2018 Universitätsbibliothek Tübingen.  All rights reserved.
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

#include "SyndicationFormat.h"
#include <regex>
#include <stdexcept>
#include "Compiler.h"
#include "SimpleXmlParser.h"
#include "StringDataSource.h"
#include "TimeUtil.h"
#include "util.h"


void SyndicationFormat::const_iterator::operator++() {
    item_ = syndication_format_->getNextItem();
}


bool SyndicationFormat::const_iterator::operator==(const SyndicationFormat::const_iterator &rhs) {
    if (item_ == nullptr and rhs.item_ == nullptr)
        return true;
    if ((item_ == nullptr and rhs.item_ != nullptr) or (item_ != nullptr and rhs.item_ == nullptr))
        return false;
    return *item_ == *rhs.item_;
}


SyndicationFormat::SyndicationFormat(const std::string &xml_document)
    : data_source_(new StringDataSource(xml_document)), xml_parser_(new SimpleXmlParser<StringDataSource>(data_source_))
{
}


SyndicationFormat::~SyndicationFormat() {
    delete xml_parser_;
    delete data_source_;
}


namespace {


enum SyndicationFormatType { TYPE_UNKNOWN, TYPE_RSS20, TYPE_RSS091, TYPE_ATOM };


static const std::regex RSS20_REGEX("<rss[^>]+version=\"2.0\">");
static const std::regex RSS091_REGEX("<rss[^>]+version=\"0.91\">");
static const std::regex ATOM_REGEX("<feed[^>]+2005/Atom\">");


SyndicationFormatType GetFormatType(const std::string &xml_document) {
    if (std::regex_match(xml_document, RSS20_REGEX))
        return TYPE_RSS20;
    if (std::regex_match(xml_document, RSS091_REGEX))
        return TYPE_RSS091;
    if (std::regex_match(xml_document, ATOM_REGEX))
        return TYPE_ATOM;

    return TYPE_UNKNOWN;
}


} // unnamed namespace


std::unique_ptr<SyndicationFormat> SyndicationFormat::Factory(const std::string &xml_document, std::string * const err_msg) {
    try {
        switch (GetFormatType(xml_document)) {
        case TYPE_UNKNOWN:
            *err_msg = "can't determine syndication format!";
            return nullptr;
        case TYPE_RSS20:
            return std::unique_ptr<SyndicationFormat>(new RSS20(xml_document));
        case TYPE_RSS091:
            return std::unique_ptr<SyndicationFormat>(new RSS091(xml_document));
        case TYPE_ATOM:
            return std::unique_ptr<SyndicationFormat>(new Atom(xml_document));
        }
    } catch (const std::runtime_error &x) {
        *err_msg = "Error while parsing syndication format: " + std::string(x.what());
        return nullptr;
    }
}


RSS20::RSS20(const std::string &xml_document): SyndicationFormat(xml_document) {
    if (unlikely(not xml_parser_->skipTo(SimpleXmlParser<StringDataSource>::OPENING_TAG, "title")))
        throw std::runtime_error("in RSS20::RSS20: opening \"title\" tag not found!");

    SimpleXmlParser<StringDataSource>::Type type;
    std::map<std::string, std::string> attrib_map;
    if (unlikely(not xml_parser_->getNext(&type, &attrib_map, &title_) or type != SimpleXmlParser<StringDataSource>::CHARACTERS))
        throw std::runtime_error("in RSS20::RSS20: title characters not found!");

    if (unlikely(not xml_parser_->skipTo(SimpleXmlParser<StringDataSource>::OPENING_TAG, "link")))
        throw std::runtime_error("in RSS20::RSS20: opening \"link\" tag not found!");
    if (unlikely(not xml_parser_->getNext(&type, &attrib_map, &link_) or type != SimpleXmlParser<StringDataSource>::CHARACTERS))
        throw std::runtime_error("in RSS20::RSS20: link characters not found!");

    if (unlikely(not xml_parser_->skipTo(SimpleXmlParser<StringDataSource>::OPENING_TAG, "description")))
        throw std::runtime_error("in RSS20::RSS20: opening \"description\" tag not found!");
    if (unlikely(not xml_parser_->getNext(&type, &attrib_map, &description_)
                 or type != SimpleXmlParser<StringDataSource>::CHARACTERS))
        throw std::runtime_error("in RSS20::RSS20: description characters not found!");
}


std::unique_ptr<SyndicationFormat::Item> RSS20::getNextItem() {
    if (unlikely(not xml_parser_->skipTo(SimpleXmlParser<StringDataSource>::OPENING_TAG, "item")))
        return nullptr;
    if (unlikely(not xml_parser_->skipTo(SimpleXmlParser<StringDataSource>::OPENING_TAG, "description")))
        throw std::runtime_error("in RSS20::getNextItem: opening \"description\" tag not found!");

    std::string description;
    std::map<std::string, std::string> attrib_map;
    if (unlikely(not xml_parser_->skipTo(SimpleXmlParser<StringDataSource>::CLOSING_TAG, "description", &attrib_map,
                                         &description)))
        throw std::runtime_error("in RSS20::getNextItem: closing \"description\" tag not found!");

    // pubDate is optional!
    if (not xml_parser_->skipTo(SimpleXmlParser<StringDataSource>::OPENING_TAG, "pubDate"))
        return std::unique_ptr<SyndicationFormat::Item>(new Item(description, TimeUtil::BAD_TIME_T));

    std::string pub_date_string;
    if (unlikely(not xml_parser_->skipTo(SimpleXmlParser<StringDataSource>::CLOSING_TAG, "pubDate", &attrib_map,
                                         &pub_date_string)))
        throw std::runtime_error("in RSS20::getNextItem: missing \"pubDate\" closing tag!");
    time_t pub_date;
    if (unlikely(not TimeUtil::ParseRFC1123DateTime(pub_date_string, &pub_date))) {
        WARNING("couldn't parse \"" + pub_date_string + "\"!");
        return std::unique_ptr<SyndicationFormat::Item>(new Item(description, TimeUtil::BAD_TIME_T));
    }

    return std::unique_ptr<SyndicationFormat::Item>(new Item(description, pub_date));
}


RSS091::RSS091(const std::string &xml_document): SyndicationFormat(xml_document) {
    if (unlikely(not xml_parser_->skipTo(SimpleXmlParser<StringDataSource>::OPENING_TAG, "title")))
        throw std::runtime_error("in RSS091::RSS091: opening \"title\" tag not found!");

    SimpleXmlParser<StringDataSource>::Type type;
    std::map<std::string, std::string> attrib_map;
    if (unlikely(not xml_parser_->getNext(&type, &attrib_map, &title_) or type != SimpleXmlParser<StringDataSource>::CHARACTERS))
        throw std::runtime_error("in RSS091::RSS091: title characters not found!");

    if (unlikely(not xml_parser_->skipTo(SimpleXmlParser<StringDataSource>::OPENING_TAG, "link")))
        throw std::runtime_error("in RSS091::RSS091: opening \"link\" tag not found!");
    if (unlikely(not xml_parser_->getNext(&type, &attrib_map, &link_) or type != SimpleXmlParser<StringDataSource>::CHARACTERS))
        throw std::runtime_error("in RSS091::RSS091: link characters not found!");

    if (unlikely(not xml_parser_->skipTo(SimpleXmlParser<StringDataSource>::OPENING_TAG, "description")))
        throw std::runtime_error("in RSS091::RSS091: opening \"description\" tag not found!");
    if (unlikely(not xml_parser_->getNext(&type, &attrib_map, &description_)
                 or type != SimpleXmlParser<StringDataSource>::CHARACTERS))
        throw std::runtime_error("in RSS091::RSS091: description characters not found!");
}


std::unique_ptr<SyndicationFormat::Item> RSS091::getNextItem() {
    if (unlikely(not xml_parser_->skipTo(SimpleXmlParser<StringDataSource>::OPENING_TAG, "item")))
        return nullptr;
    if (unlikely(not xml_parser_->skipTo(SimpleXmlParser<StringDataSource>::OPENING_TAG, "description")))
        throw std::runtime_error("in RSS091::getNextItem: opening \"description\" tag not found!");

    std::string description;
    std::map<std::string, std::string> attrib_map;
    if (unlikely(not xml_parser_->skipTo(SimpleXmlParser<StringDataSource>::CLOSING_TAG, "description", &attrib_map,
                                         &description)))
        throw std::runtime_error("in RSS091::getNextItem: closing \"description\" tag not found!");

    return std::unique_ptr<SyndicationFormat::Item>(new Item(description, TimeUtil::BAD_TIME_T));
}


Atom::Atom(const std::string &xml_document): SyndicationFormat(xml_document) {
    if (unlikely(not xml_parser_->skipTo(SimpleXmlParser<StringDataSource>::OPENING_TAG, "title")))
        throw std::runtime_error("in Atom::Atom: opening \"title\" tag not found!");

    SimpleXmlParser<StringDataSource>::Type type;
    std::map<std::string, std::string> attrib_map;
    if (unlikely(not xml_parser_->getNext(&type, &attrib_map, &title_) or type != SimpleXmlParser<StringDataSource>::CHARACTERS))
        throw std::runtime_error("in Atom::Atom: title characters not found!");

    if (unlikely(not xml_parser_->skipTo(SimpleXmlParser<StringDataSource>::OPENING_TAG, "link")))
        throw std::runtime_error("in Atom::Atom: opening \"link\" tag not found!");
    if (unlikely(not xml_parser_->getNext(&type, &attrib_map, &link_) or type != SimpleXmlParser<StringDataSource>::CHARACTERS))
        throw std::runtime_error("in Atom::Atom: link characters not found!");

    if (unlikely(not xml_parser_->skipTo(SimpleXmlParser<StringDataSource>::OPENING_TAG, "description")))
        throw std::runtime_error("in Atom::Atom: opening \"description\" tag not found!");
    if (unlikely(not xml_parser_->getNext(&type, &attrib_map, &description_)
                 or type != SimpleXmlParser<StringDataSource>::CHARACTERS))
        throw std::runtime_error("in Atom::Atom: description characters not found!");
}


std::unique_ptr<SyndicationFormat::Item> Atom::getNextItem() {
    if (unlikely(not xml_parser_->skipTo(SimpleXmlParser<StringDataSource>::OPENING_TAG, "entry")))
        return nullptr;
    if (unlikely(not xml_parser_->skipTo(SimpleXmlParser<StringDataSource>::OPENING_TAG, "title")))
        throw std::runtime_error("in Atom::getNextItem: opening \"title\" tag not found!");

    std::string description;
    std::map<std::string, std::string> attrib_map;
    if (unlikely(not xml_parser_->skipTo(SimpleXmlParser<StringDataSource>::CLOSING_TAG, "title", &attrib_map,
                                         &description)))
        throw std::runtime_error("in Atom::getNextItem: closing \"description\" tag not found!");

    if (not xml_parser_->skipTo(SimpleXmlParser<StringDataSource>::OPENING_TAG, "updated"))
        throw std::runtime_error("in Atom::getNextItem: opening \"updated\" tag not found!");

    std::string updated_string;
    if (unlikely(not xml_parser_->skipTo(SimpleXmlParser<StringDataSource>::CLOSING_TAG, "updated", &attrib_map,
                                         &updated_string)))
        throw std::runtime_error("in Atom::getNextItem: missing \"updated\" closing tag!");
    const time_t updated(TimeUtil::Iso8601StringToTimeT(updated_string, TimeUtil::UTC));

    return std::unique_ptr<SyndicationFormat::Item>(new Item(description, updated));
}

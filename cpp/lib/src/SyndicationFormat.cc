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
#include <set>
#include <stdexcept>
#include "Compiler.h"
#include "RegexMatcher.h"
#include "SimpleXmlParser.h"
#include "StringDataSource.h"
#include "TimeUtil.h"
#include "util.h"


void SyndicationFormat::const_iterator::operator++() {
    item_ = syndication_format_->getNextItem();
}


bool SyndicationFormat::const_iterator::operator==(const SyndicationFormat::const_iterator &rhs) const {
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


enum SyndicationFormatType { TYPE_UNKNOWN, TYPE_RSS20, TYPE_RSS091, TYPE_ATOM, TYPE_RDF };


SyndicationFormatType GetFormatType(const std::string &xml_document) {
    static RegexMatcher *rss20_regex_matcher;
    if (unlikely(rss20_regex_matcher == nullptr))
        rss20_regex_matcher = RegexMatcher::FactoryOrDie("<rss[^>]+version=\"2.0\"");
    if (rss20_regex_matcher->matched(xml_document))
        return TYPE_RSS20;

    static RegexMatcher *rss091_regex_matcher;
    if (unlikely(rss091_regex_matcher == nullptr))
        rss091_regex_matcher = RegexMatcher::FactoryOrDie("<rss[^>]+version=\"0.91\"");
    if (rss091_regex_matcher->matched(xml_document))
        return TYPE_RSS091;

    static RegexMatcher *atom_regex_matcher;
    if (unlikely(atom_regex_matcher == nullptr))
        atom_regex_matcher = RegexMatcher::FactoryOrDie("<feed[^2>]+2005/Atom\"\"");
    if (atom_regex_matcher->matched(xml_document))
        return TYPE_ATOM;

    static RegexMatcher *rdf_regex_matcher;
    if (unlikely(rdf_regex_matcher == nullptr))
        rdf_regex_matcher = RegexMatcher::FactoryOrDie("<rdf:RDF|<RDF");
    if (rdf_regex_matcher->matched(xml_document))
        return TYPE_RDF;

    return TYPE_UNKNOWN;
}


static std::string ExtractText(SimpleXmlParser<StringDataSource> * const parser, const std::string &closing_tag,
                               const std::string &extra = "")
{
    SimpleXmlParser<StringDataSource>::Type type;
    std::map<std::string, std::string> attrib_map;
    std::string data;
    if (unlikely(not parser->getNext(&type, &attrib_map, &data)))
        throw std::runtime_error("in ExtractText(SyndicationFormat.cc): parse error while looking for characters for \""
                                 + closing_tag + "\" tag!" + extra);
    std::string extracted_text;
    if (type == SimpleXmlParser<StringDataSource>::CHARACTERS)
        extracted_text = data;
    else if (type == SimpleXmlParser<StringDataSource>::CLOSING_TAG) {
        if (unlikely(data != closing_tag))
            throw std::runtime_error("in ExtractText(SyndicationFormat.cc): unexpected closing tag \"" + data
                                     + "\" while looking for the \"" + closing_tag + "\" closing tag!" + extra);
        return "";
    } else
        throw std::runtime_error("in ExtractText(SyndicationFormat.cc): unexpected "
                                 + SimpleXmlParser<StringDataSource>::TypeToString(type) + " while looking for a closing \""
                                 + closing_tag + "\" tag!" + extra);
    if (not parser->getNext(&type, &attrib_map, &data)
        or type != SimpleXmlParser<StringDataSource>::CLOSING_TAG or data != closing_tag)
        throw std::runtime_error("in ExtractText(SyndicationFormat.cc): " + closing_tag + " closing tag not found!" + extra);

    return extracted_text;
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
        case TYPE_RDF:
            return std::unique_ptr<SyndicationFormat>(new RDF(xml_document));
        }
    } catch (const std::runtime_error &x) {
        *err_msg = "Error while parsing syndication format: " + std::string(x.what());
        return nullptr;
    }

    __builtin_unreachable();
}


RSS20::RSS20(const std::string &xml_document): SyndicationFormat(xml_document) {
    SimpleXmlParser<StringDataSource>::Type type;
    std::map<std::string, std::string> attrib_map;
    std::string data;
    while (xml_parser_->getNext(&type, &attrib_map, &data)) {
        if (unlikely(type == SimpleXmlParser<StringDataSource>::END_OF_DOCUMENT))
            throw std::runtime_error("in RSS20::RSS20: unexpected end-of-document!");
        if (type == SimpleXmlParser<StringDataSource>::OPENING_TAG and data == "item")
            return;
        if (type == SimpleXmlParser<StringDataSource>::OPENING_TAG and data == "image") {
            if (unlikely(not xml_parser_->skipTo(SimpleXmlParser<StringDataSource>::CLOSING_TAG, "image")))
                throw std::runtime_error("in RSS20::RSS20: closing image tag not found!");
        }

        if (type == SimpleXmlParser<StringDataSource>::OPENING_TAG and data == "title")
            title_ = ExtractText(xml_parser_, "title", " (RSS20::RSS20)");
        if (type == SimpleXmlParser<StringDataSource>::OPENING_TAG and data == "link")
            link_ = ExtractText(xml_parser_, "link", " (RSS20::RSS20)");
        if (type == SimpleXmlParser<StringDataSource>::OPENING_TAG and data == "description")
            description_ = ExtractText(xml_parser_, "description", " (RSS20::RSS20)");
    }
    if (unlikely(type == SimpleXmlParser<StringDataSource>::ERROR))
        throw std::runtime_error("in RSS20::RSS20: found XML error: " + xml_parser_->getLastErrorMessage());
}


std::unique_ptr<SyndicationFormat::Item> RSS20::getNextItem() {
    std::string title, description, link, id;
    time_t pub_date(TimeUtil::BAD_TIME_T);
    SimpleXmlParser<StringDataSource>::Type type;
    std::map<std::string, std::string> attrib_map;
    std::string data;
    while (xml_parser_->getNext(&type, &attrib_map, &data)) {
        if (type == SimpleXmlParser<StringDataSource>::CLOSING_TAG and data == "item")
            return std::unique_ptr<SyndicationFormat::Item>(new Item(title, description, link, id, pub_date));
        else if (type == SimpleXmlParser<StringDataSource>::END_OF_DOCUMENT)
            return nullptr;
        else if (type == SimpleXmlParser<StringDataSource>::OPENING_TAG and data == "title")
            title = ExtractText(xml_parser_, "title", " (RSS20::getNextItem)");
        else if (type == SimpleXmlParser<StringDataSource>::OPENING_TAG and data == "description")
            description = ExtractText(xml_parser_, "description", " (RSS20::getNextItem)");
        else if (type == SimpleXmlParser<StringDataSource>::OPENING_TAG and data == "link")
            link = ExtractText(xml_parser_, "link", " (RSS20::getNextItem)");
        else if (type == SimpleXmlParser<StringDataSource>::OPENING_TAG and data == "guid")
            id = ExtractText(xml_parser_, "guid", " (RSS20::getNextItem)");
        else if (type == SimpleXmlParser<StringDataSource>::OPENING_TAG and data == "pubDate") {
            const std::string pub_date_string(ExtractText(xml_parser_, "pubDate"));
            if (unlikely(not TimeUtil::ParseRFC1123DateTime(pub_date_string, &pub_date)))
                WARNING("couldn't parse \"" + pub_date_string + "\"!");
        }
    }
    if (unlikely(type == SimpleXmlParser<StringDataSource>::ERROR))
        throw std::runtime_error("in RSS20::getNextItem: found XML error: " + xml_parser_->getLastErrorMessage());

    return std::unique_ptr<SyndicationFormat::Item>(new Item(title, description, link, id, pub_date));
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
    std::string title, description, link;
    SimpleXmlParser<StringDataSource>::Type type;
    std::map<std::string, std::string> attrib_map;
    std::string data;
    while (xml_parser_->getNext(&type, &attrib_map, &data)) {
        if (type == SimpleXmlParser<StringDataSource>::CLOSING_TAG and data == "item")
            return std::unique_ptr<SyndicationFormat::Item>(new Item(title, description, link, /* id = */"",
                                                                     TimeUtil::BAD_TIME_T));
        else if (type == SimpleXmlParser<StringDataSource>::END_OF_DOCUMENT)
            return nullptr;
        else if (type == SimpleXmlParser<StringDataSource>::OPENING_TAG and data == "title")
            title = ExtractText(xml_parser_, "title");
        else if (type == SimpleXmlParser<StringDataSource>::OPENING_TAG and data == "description")
            description = ExtractText(xml_parser_, "description");
        else if (type == SimpleXmlParser<StringDataSource>::OPENING_TAG and data == "link")
            link = ExtractText(xml_parser_, "link");
    }
    if (unlikely(type == SimpleXmlParser<StringDataSource>::ERROR))
        throw std::runtime_error("in RSS091::getNextItem: found XML error: " + xml_parser_->getLastErrorMessage());

    return std::unique_ptr<SyndicationFormat::Item>(new Item(title, description, link, /* id= */"", TimeUtil::BAD_TIME_T));
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
    std::string title, summary, link, id;
    time_t updated(TimeUtil::BAD_TIME_T);
    SimpleXmlParser<StringDataSource>::Type type;
    std::map<std::string, std::string> attrib_map;
    std::string data;
    while (xml_parser_->getNext(&type, &attrib_map, &data)) {
        if (type == SimpleXmlParser<StringDataSource>::CLOSING_TAG and data == "item")
            return std::unique_ptr<SyndicationFormat::Item>(new Item(title, summary, id, link, updated));
        else if (type == SimpleXmlParser<StringDataSource>::END_OF_DOCUMENT)
            return nullptr;
        else if (type == SimpleXmlParser<StringDataSource>::OPENING_TAG and data == "title")
            title = ExtractText(xml_parser_, "title");
        else if (type == SimpleXmlParser<StringDataSource>::OPENING_TAG and data == "summary")
            summary = ExtractText(xml_parser_, "summary");
        else if (type == SimpleXmlParser<StringDataSource>::OPENING_TAG and data == "link")
            link = ExtractText(xml_parser_, "link");
        else if (type == SimpleXmlParser<StringDataSource>::OPENING_TAG and data == "id")
            id = ExtractText(xml_parser_, "id", " (RSS20::getNextItem)");
        else if (type == SimpleXmlParser<StringDataSource>::OPENING_TAG and data == "updated") {
            const std::string updated_string(ExtractText(xml_parser_, "updated"));
            updated = TimeUtil::Iso8601StringToTimeT(updated_string, TimeUtil::UTC);
        }
    }
    if (unlikely(type == SimpleXmlParser<StringDataSource>::ERROR))
        throw std::runtime_error("in Atom::getNextItem: found XML error: " + xml_parser_->getLastErrorMessage());

    return std::unique_ptr<SyndicationFormat::Item>(new Item(title, summary, link, id, updated));
}


static std::string ExtractNamespacePrefix(const std::string &xmlns_string) {
    if (not StringUtil::StartsWith(xmlns_string, "xmlns"))
        throw std::runtime_error("in ExtractNamespacePrefix(SyndicationFormat.cc): unexpected attribute key: \""
                                 + xmlns_string + "\"! (1)");
    if (xmlns_string == "xmlns")
        return "";
    if (xmlns_string[__builtin_strlen("xmlns")] != ':')
        throw std::runtime_error("in ExtractRSSNamespace(SyndicationFormat.cc): unexpected attribute key: \""
                                 + xmlns_string + "\"! (2)");
    return xmlns_string.substr(__builtin_strlen("xmlns") + 1) + ":";
}


// Helper for RDF::RDF.
static void ExtractNamespaces(SimpleXmlParser<StringDataSource> * const parser, std::string * const rss_namespace,
                              std::string * const dc_namespace, std::string * const prism_namespace)
{
    std::map<std::string, std::string> attrib_map;
    if (not parser->skipTo(SimpleXmlParser<StringDataSource>::OPENING_TAG, "rdf:RDF", &attrib_map))
        throw std::runtime_error("in ExtractRSSNamespace(SyndicationFormat.cc): missing rdf:RDF opening tag!");

    for (const auto &key_and_value : attrib_map) {
        if (key_and_value.second == "http://purl.org/rss/1.0/")
            *rss_namespace = ExtractNamespacePrefix(key_and_value.first);
        else if (key_and_value.second == "http://purl.org/dc/elements/1.1/")
            *dc_namespace = ExtractNamespacePrefix(key_and_value.first);
        else if (key_and_value.second == "http://prismstandard.org/namespaces/2.0/basic/")
            *prism_namespace = ExtractNamespacePrefix(key_and_value.first);
    }
}


RDF::RDF(const std::string &xml_document): SyndicationFormat(xml_document) {
    ExtractNamespaces(xml_parser_, &rss_namespace_, &dc_namespace_, &prism_namespace_);

    SimpleXmlParser<StringDataSource>::Type type;
    std::map<std::string, std::string> attrib_map;
    std::string data;
    while (xml_parser_->getNext(&type, &attrib_map, &data)) {
        if (unlikely(type == SimpleXmlParser<StringDataSource>::END_OF_DOCUMENT))
            throw std::runtime_error("in RDF::RDF: unexpected end-of-document!");
        else if (type == SimpleXmlParser<StringDataSource>::OPENING_TAG and data == rss_namespace_ + "item")
            return;
        else if (type == SimpleXmlParser<StringDataSource>::OPENING_TAG and data == rss_namespace_ + "image") {
            if (unlikely(not xml_parser_->skipTo(SimpleXmlParser<StringDataSource>::CLOSING_TAG, rss_namespace_ + "image")))
                throw std::runtime_error("in RSS20::RSS20: closing image tag not found!");
        } else if (type == SimpleXmlParser<StringDataSource>::OPENING_TAG and data == rss_namespace_ + "title")
            title_ = ExtractText(xml_parser_, rss_namespace_ + "title");
        else if (type == SimpleXmlParser<StringDataSource>::OPENING_TAG and data == rss_namespace_ + "link")
            link_ = ExtractText(xml_parser_, rss_namespace_ + "link");
        else if (type == SimpleXmlParser<StringDataSource>::OPENING_TAG and data == rss_namespace_ + "description")
            description_ = ExtractText(xml_parser_, rss_namespace_ + "description");
    }
    if (unlikely(type == SimpleXmlParser<StringDataSource>::ERROR))
        throw std::runtime_error("in RSS20::RSS20: found XML error: " + xml_parser_->getLastErrorMessage());
}


// The following, hopefully exhaustive list of XML tag names lists all tags that are part of the PRISM standard that have no
// character data but instead a single attribute named "rdf:resource".
static const std::unordered_set<std::string> PRISM_TAGS_WITH_RDF_RESOURCE_ATTRIBS{
    "hasAlternative",
    "hasCorrection",
    "hasFormat",
    "hasPart",
    "hasPreviousVersion",
    "hasTranslation",
    "industry",
    "isCorrectionOf",
    "isFormatOf",
    "isPartOf",
    "isReferencedBy",
    "isRequiredBy"
};


void ExtractPrismData(SimpleXmlParser<StringDataSource> * const xml_parser, const std::string &tag,
                      const std::map<std::string, std::string> &attrib_map, const std::string &prism_namespace,
                      std::unordered_map<std::string, std::string> * const dc_and_prism_data)
{
    const std::string tag_suffix(tag.substr(prism_namespace.length()));
    if (attrib_map.size() != 1)
        (*dc_and_prism_data)["prism:" + tag_suffix] = ExtractText(xml_parser, tag);
    else {
        const auto subtag(PRISM_TAGS_WITH_RDF_RESOURCE_ATTRIBS.find(tag_suffix));
        if (subtag != PRISM_TAGS_WITH_RDF_RESOURCE_ATTRIBS.end()) {
            const auto key_and_value(attrib_map.find("rdf:resource"));
            if (likely(key_and_value != attrib_map.end()))
                (*dc_and_prism_data)["prism:" + tag_suffix] = key_and_value->second;
            else
                WARNING("don't know what to do w/ \"" + tag + "\" tag attribute!");
        } else
            WARNING("don't know what to do w/ PRISM \"" + tag + "\" tag!");
        if (not xml_parser->skipTo(SimpleXmlParser<StringDataSource>::CLOSING_TAG, tag))
            throw std::runtime_error("in RDF::getNextItem: missing closing \"" + tag + "\" tag!");
    }
}


std::unique_ptr<SyndicationFormat::Item> RDF::getNextItem() {
    std::string title, description, link, id;
    time_t pub_date(TimeUtil::BAD_TIME_T);
    std::unordered_map<std::string, std::string> dc_and_prism_data;
    SimpleXmlParser<StringDataSource>::Type type;
    std::map<std::string, std::string> attrib_map;
    std::string data;
    while (xml_parser_->getNext(&type, &attrib_map, &data)) {
        if (type == SimpleXmlParser<StringDataSource>::OPENING_TAG and data == rss_namespace_ + "item") {
            const auto rdf_about(attrib_map.find("rdf:about"));
            if (rdf_about != attrib_map.cend())
                id = rdf_about->second;
        } else if (type == SimpleXmlParser<StringDataSource>::CLOSING_TAG and data == rss_namespace_ + "item")
            return std::unique_ptr<SyndicationFormat::Item>(new Item(title, description, link, id, pub_date, dc_and_prism_data));
        else if (type == SimpleXmlParser<StringDataSource>::END_OF_DOCUMENT)
            return nullptr;
        else if (type == SimpleXmlParser<StringDataSource>::OPENING_TAG) {
            if (data == rss_namespace_ + "title")
                title = ExtractText(xml_parser_, rss_namespace_ + "title");
            else if (data == rss_namespace_ + "description")
                description = ExtractText(xml_parser_, rss_namespace_ + "description");
            else if (data == rss_namespace_ + "link")
                link = ExtractText(xml_parser_, rss_namespace_ + "link");
            else if (data == rss_namespace_ + "pubDate") {
                const std::string pub_date_string(ExtractText(xml_parser_, rss_namespace_ + "pubDate"));
                if (unlikely(not TimeUtil::ParseRFC1123DateTime(pub_date_string, &pub_date)))
                    WARNING("couldn't parse \"" + pub_date_string + "\"!");
            } else if (not dc_namespace_.empty() and StringUtil::StartsWith(data, dc_namespace_)) {
                const std::string tag(data);
                const std::string tag_suffix(tag.substr(dc_namespace_.length()));
                dc_and_prism_data["dc:" + tag_suffix] = ExtractText(xml_parser_, tag);
            } else if (not prism_namespace_.empty() and StringUtil::StartsWith(data, prism_namespace_))
                ExtractPrismData(xml_parser_, data, attrib_map, prism_namespace_, &dc_and_prism_data);
        }
    }
    if (unlikely(type == SimpleXmlParser<StringDataSource>::ERROR))
        throw std::runtime_error("in RDF::getNextItem: found XML error: " + xml_parser_->getLastErrorMessage());

    return std::unique_ptr<SyndicationFormat::Item>(new Item(title, description, link, id, pub_date, dc_and_prism_data));
}

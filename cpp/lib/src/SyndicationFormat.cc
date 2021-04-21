/** \brief Interface of the SyndicationFormat class and descendents thereof.
 *  \author Dr. Johannes Ruscheinski (johannes.ruscheinski@uni-tuebingen.de)
 *
 *  \copyright 2018-2021 Universitätsbibliothek Tübingen.  All rights reserved.
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
#include <unordered_set>
#include "Compiler.h"
#include "RegexMatcher.h"
#include "StringUtil.h"
#include "TextUtil.h"
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


void SyndicationFormat::iterator::operator++() {
    item_ = syndication_format_->getNextItem();
}


bool SyndicationFormat::iterator::operator==(const SyndicationFormat::iterator &rhs) const {
    if (item_ == nullptr and rhs.item_ == nullptr)
        return true;
    if ((item_ == nullptr and rhs.item_ != nullptr) or (item_ != nullptr and rhs.item_ == nullptr))
        return false;
    return *item_ == *rhs.item_;
}


SyndicationFormat::SyndicationFormat(const std::string &xml_document, const AugmentParams &augment_params)
    : xml_parser_(XMLParser(xml_document, XMLParser::XML_STRING)),
      last_build_date_(TimeUtil::BAD_TIME_T), augment_params_(augment_params)
{
}


namespace {


enum SyndicationFormatType { TYPE_UNKNOWN, TYPE_RSS20, TYPE_RSS091, TYPE_ATOM, TYPE_RDF };


// set options to 0 so default setting ENABLE_UTF8 will be disabled
// if ENABLE_UTF8 is used, detection will fail for non-utf8-feeds
// even if the corresponding characters are NOT in the header!
const ThreadSafeRegexMatcher RSS20_MATCHER("<rss[^>]+version=['\"]2.0['\"]", /* options */ 0);
const ThreadSafeRegexMatcher RSS091_MATCHER("<rss[^>]+version=['\"]0.91['\"]", /* options */ 0);
const ThreadSafeRegexMatcher ATOM_MATCHER("<feed[^>]+2005/Atom['\"]", /* options */ 0);
const ThreadSafeRegexMatcher RDF_MATCHER("<rdf:RDF|<RDF", /* options */ 0);


SyndicationFormatType GetFormatType(const std::string &xml_document) {
    if (RSS20_MATCHER.match(xml_document))
        return TYPE_RSS20;

    if (RSS091_MATCHER.match(xml_document))
        return TYPE_RSS091;

    if (ATOM_MATCHER.match(xml_document))
        return TYPE_ATOM;

    if (RDF_MATCHER.match(xml_document))
        return TYPE_RDF;

    return TYPE_UNKNOWN;
}


static std::string ExtractText(XMLParser &parser, const std::string &closing_tag, const std::string &extra = "") {
    XMLParser::XMLPart part;
    if (unlikely(not parser.getNext(&part)))
        throw std::runtime_error("in ExtractText(SyndicationFormat.cc): parse error while looking for characters for \""
                                 + closing_tag + "\" tag!" + extra);
    std::string extracted_text;
    if (part.type_ == XMLParser::XMLPart::CHARACTERS)
        extracted_text = part.data_;
    else if (part.type_ == XMLParser::XMLPart::CLOSING_TAG) {
        if (unlikely(part.data_ != closing_tag))
            throw std::runtime_error("in ExtractText(SyndicationFormat.cc): unexpected closing tag \"" + part.data_
                                     + "\" while looking for the \"" + closing_tag + "\" closing tag!" + extra);
        return "";
    } else
        throw std::runtime_error("in ExtractText(SyndicationFormat.cc): unexpected "
                                 + XMLParser::XMLPart::TypeToString(part.type_) + " while looking for a closing \""
                                 + closing_tag + "\" tag!" + extra);
    if (not parser.getNext(&part)
        or part.type_ != XMLParser::XMLPart::CLOSING_TAG or part.data_ != closing_tag)
        throw std::runtime_error("in ExtractText(SyndicationFormat.cc): " + closing_tag + " closing tag not found!" + extra
                                 + " found instead: " + XMLParser::XMLPart::TypeToString(part.type_) + " '" + part.data_ + "'");

    return extracted_text;
}


// Handles RFC1123 datetimes as well as YYYY-MM-DD and YYYY.
bool ParseRFC1123DateTimeAndPrefixes(const std::string &datetime_candidate, time_t * const converted_time) {
    if (TimeUtil::ParseRFC1123DateTime(datetime_candidate, converted_time))
        return true;

    struct tm tm;
    const char *last_char(::strptime(datetime_candidate.c_str(), "%Y-%m-%d", &tm));
    if (last_char != nullptr and *last_char == '\0')  {
        *converted_time = TimeUtil::TimeGm(tm);
        return *converted_time != TimeUtil::BAD_TIME_T;
    }

    last_char = ::strptime(datetime_candidate.c_str(), "%Y", &tm);
    if (last_char != nullptr and *last_char == '\0')  {
        *converted_time = TimeUtil::TimeGm(tm);
        return *converted_time != TimeUtil::BAD_TIME_T;
    }

    return false;
}


} // unnamed namespace


std::unique_ptr<SyndicationFormat> SyndicationFormat::Factory(const std::string &xml_document, const AugmentParams &augment_params,
                                                              std::string * const err_msg)
{
    try {
        switch (GetFormatType(xml_document)) {
        case TYPE_UNKNOWN:
            *err_msg = "can't determine syndication format!";
            return nullptr;
        case TYPE_RSS20:
            return std::unique_ptr<SyndicationFormat>(new RSS20(xml_document, augment_params));
        case TYPE_RSS091:
            return std::unique_ptr<SyndicationFormat>(new RSS091(xml_document, augment_params));
        case TYPE_ATOM:
            return std::unique_ptr<SyndicationFormat>(new Atom(xml_document, augment_params));
        case TYPE_RDF:
            return std::unique_ptr<SyndicationFormat>(new RDF(xml_document, augment_params));
        }
    } catch (const std::runtime_error &x) {
        *err_msg = "Error while parsing syndication format: " + std::string(x.what());
        return nullptr;
    }

    __builtin_unreachable();
}


RSS20::RSS20(const std::string &xml_document, const AugmentParams &augment_params): SyndicationFormat(xml_document, augment_params) {
    XMLParser::XMLPart part;
    while (xml_parser_.getNext(&part)) {
        if (part.type_ == XMLParser::XMLPart::OPENING_TAG and part.data_ == "item")
            return;
        if (part.type_ == XMLParser::XMLPart::OPENING_TAG and part.data_ == "image") {
            if (unlikely(not xml_parser_.skipTo(XMLParser::XMLPart::CLOSING_TAG, "image")))
                throw std::runtime_error("in RSS20::RSS20: closing image tag not found!");
        }
        if (part.type_ == XMLParser::XMLPart::OPENING_TAG and part.data_ == "title")
            title_ = ExtractText(xml_parser_, "title", " (RSS20::RSS20)");
        if (part.type_ == XMLParser::XMLPart::OPENING_TAG and part.data_ == "link")
            link_ = ExtractText(xml_parser_, "link", " (RSS20::RSS20)");
        if (part.type_ == XMLParser::XMLPart::OPENING_TAG and part.data_ == "enclosure" and part.attributes_.find("url") != part.attributes_.end() and link_.empty())
            link_ = part.attributes_["url"];
        if (part.type_ == XMLParser::XMLPart::OPENING_TAG and part.data_ == "description")
            description_ = ExtractText(xml_parser_, "description", " (RSS20::RSS20)");
        if (part.type_ == XMLParser::XMLPart::OPENING_TAG and part.data_ == "lastBuildDate") {
            const std::string last_build_date(TextUtil::CollapseAndTrimWhitespace(ExtractText(xml_parser_,
                                              "lastBuildDate", " (RSS20::RSS20)")));
            if (augment_params_.strptime_format_.empty()) {
                if (not ParseRFC1123DateTimeAndPrefixes(last_build_date, &last_build_date_))
                    LOG_ERROR("failed to parse \"" + last_build_date + "\" as an RFC1123 datetime!");
            } else {
                struct tm parsed_date;
                if (not TimeUtil::StringToStructTm(&parsed_date, last_build_date, augment_params_.strptime_format_))
                    LOG_ERROR("failed to parse \"" + last_build_date + "\" with the given strptime_format \"" + augment_params_.strptime_format_ + "\"");
                last_build_date_ = TimeUtil::TimeGm(parsed_date);
            }
        }
    }
}


std::unique_ptr<SyndicationFormat::Item> RSS20::getNextItem() {
    std::string title, description, link, id;
    time_t pub_date(TimeUtil::BAD_TIME_T);
    XMLParser::XMLPart part;
    while (xml_parser_.getNext(&part)) {
        if (part.type_ == XMLParser::XMLPart::CLOSING_TAG and part.data_ == "item")
            return std::unique_ptr<SyndicationFormat::Item>(new Item(title, description, link, id, pub_date));
        else if (part.type_ == XMLParser::XMLPart::OPENING_TAG and part.data_ == "title")
            title = ExtractText(xml_parser_, "title", " (RSS20::getNextItem)");
        else if (part.type_ == XMLParser::XMLPart::OPENING_TAG and part.data_ == "description")
            description = ExtractText(xml_parser_, "description", " (RSS20::getNextItem)");
        else if (part.type_ == XMLParser::XMLPart::OPENING_TAG and part.data_ == "link") {
            link = ExtractText(xml_parser_, "link", " (RSS20::getNextItem)");
            if (link.empty() and part.attributes_.find("href") != part.attributes_.end())
                link = part.attributes_["href"];
        } else if (part.type_ == XMLParser::XMLPart::OPENING_TAG and part.data_ == "enclosure") {
            if (link.empty() and part.attributes_.find("url") != part.attributes_.end())
                link = part.attributes_["url"];
        } else if (part.type_ == XMLParser::XMLPart::OPENING_TAG and part.data_ == "guid")
            id = ExtractText(xml_parser_, "guid", " (RSS20::getNextItem)");
        else if (part.type_ == XMLParser::XMLPart::OPENING_TAG and part.data_ == "pubDate") {
            const std::string pub_date_string(TextUtil::CollapseAndTrimWhitespace(ExtractText(xml_parser_, "pubDate")));
            if (augment_params_.strptime_format_.empty()) {
                if (unlikely(not ParseRFC1123DateTimeAndPrefixes(pub_date_string, &pub_date)))
                    LOG_WARNING("couldn't parse \"" + pub_date_string + "\"!");
            } else {
                struct tm parsed_date;
                if (not TimeUtil::StringToStructTm(&parsed_date, pub_date_string, augment_params_.strptime_format_))
                    LOG_ERROR("failed to parse \"" + pub_date_string + "\" with the given strptime_format \"" + augment_params_.strptime_format_ + "\"");
                pub_date = TimeUtil::TimeGm(parsed_date);
            }
        }
    }
    return nullptr;
}


RSS091::RSS091(const std::string &xml_document, const AugmentParams &augment_params): SyndicationFormat(xml_document, augment_params) {
    XMLParser::XMLPart part;
    while (xml_parser_.getNext(&part)) {
        if (part.type_ == XMLParser::XMLPart::OPENING_TAG and part.data_ == "item")
            return;
        if (part.type_ == XMLParser::XMLPart::OPENING_TAG and part.data_ == "title")
            title_ = ExtractText(xml_parser_, "title", " (RSS091::RSS091)");
        if (part.type_ == XMLParser::XMLPart::OPENING_TAG and part.data_ == "link")
            link_ = ExtractText(xml_parser_, "link", " (RSS091::RSS091)");
        if (part.type_ == XMLParser::XMLPart::OPENING_TAG and part.data_ == "description")
            description_ = ExtractText(xml_parser_, "description", " (RSS091::RSS091)");
        if (part.type_ == XMLParser::XMLPart::OPENING_TAG and part.data_ == "lastBuildDate") {
            const std::string last_build_date(TextUtil::CollapseAndTrimWhitespace(ExtractText(xml_parser_,
                                              "lastBuildDate", " (RSS091::RSS091)")));
            if (augment_params_.strptime_format_.empty()) {
                if (not ParseRFC1123DateTimeAndPrefixes(last_build_date, &last_build_date_))
                    LOG_ERROR("failed to parse \"" + last_build_date + "\" as an RFC1123 datetime!");
            } else {
                struct tm parsed_date;
                if (not TimeUtil::StringToStructTm(&parsed_date, last_build_date, augment_params_.strptime_format_))
                    LOG_ERROR("failed to parse \"" + last_build_date + "\" with the given strptime_format \"" + augment_params_.strptime_format_ + "\"");
                last_build_date_ = TimeUtil::TimeGm(parsed_date);
            }
        }
    }
}


std::unique_ptr<SyndicationFormat::Item> RSS091::getNextItem() {
    std::string title, description, link;
    XMLParser::XMLPart part;
    while (xml_parser_.getNext(&part)) {
        if (part.type_ == XMLParser::XMLPart::CLOSING_TAG and part.data_ == "item")
            return std::unique_ptr<SyndicationFormat::Item>(new Item(title, description, link, /* id = */"", TimeUtil::BAD_TIME_T));
        else if (part.type_ == XMLParser::XMLPart::OPENING_TAG and part.data_ == "title")
            title = ExtractText(xml_parser_, "title");
        else if (part.type_ == XMLParser::XMLPart::OPENING_TAG and part.data_ == "description")
            description = ExtractText(xml_parser_, "description");
        else if (part.type_ == XMLParser::XMLPart::OPENING_TAG and part.data_ == "link") {
            link = ExtractText(xml_parser_, "link");
            if (link.empty() and part.attributes_.find("href") != part.attributes_.end())
                link = part.attributes_["href"];
        }
    }

    return nullptr;
}


Atom::Atom(const std::string &xml_document, const AugmentParams &augment_params): SyndicationFormat(xml_document, augment_params) {
    XMLParser::XMLPart part;
    while (xml_parser_.getNext(&part)) {
        if (part.type_ == XMLParser::XMLPart::OPENING_TAG and (part.data_ == "item" or part.data_ == "entry")) {
            item_tag_ = part.data_;
            return;
        }

        if (part.type_ == XMLParser::XMLPart::OPENING_TAG and part.data_ == "title")
            title_ = ExtractText(xml_parser_, "title", " (Atom::Atom)");
        if (part.type_ == XMLParser::XMLPart::OPENING_TAG and part.data_ == "link")
            link_ = ExtractText(xml_parser_, "link", " (Atom::Atom)");
        if (part.type_ == XMLParser::XMLPart::OPENING_TAG and part.data_ == "description")
            description_ = ExtractText(xml_parser_, "description", " (Atom::Atom)");
        if (part.type_ == XMLParser::XMLPart::OPENING_TAG and part.data_ == "updated") {
            const std::string last_build_date(TextUtil::CollapseAndTrimWhitespace(ExtractText(xml_parser_, "updated", " (Atom::Atom)")));
            if (augment_params_.strptime_format_.empty()) {
                if (not TimeUtil::ParseRFC3339DateTime(last_build_date, &last_build_date_))
                    LOG_ERROR("failed to parse \"" + last_build_date + "\" as an RFC3339 datetime!");
            } else {
                struct tm parsed_date;
                if (not TimeUtil::StringToStructTm(&parsed_date, last_build_date, augment_params_.strptime_format_))
                    LOG_ERROR("failed to parse \"" + last_build_date + "\" with the given strptime_format \"" + augment_params_.strptime_format_ + "\"");
                last_build_date_ = TimeUtil::TimeGm(parsed_date);
            }
        }
    }
}


std::unique_ptr<SyndicationFormat::Item> Atom::getNextItem() {
    std::string title, summary, link, id;
    time_t updated(TimeUtil::BAD_TIME_T);
    XMLParser::XMLPart part;
    while (xml_parser_.getNext(&part)) {
        if (part.type_ == XMLParser::XMLPart::CLOSING_TAG and part.data_ == item_tag_)
            return std::unique_ptr<SyndicationFormat::Item>(new Item(title, summary, link, id, updated));
        else if (part.type_ == XMLParser::XMLPart::OPENING_TAG and part.data_ == "title")
            title = ExtractText(xml_parser_, "title");
        else if (part.type_ == XMLParser::XMLPart::OPENING_TAG and part.data_ == "summary")
            summary = ExtractText(xml_parser_, "summary");
        else if (part.type_ == XMLParser::XMLPart::OPENING_TAG and part.data_ == "link") {
            link = ExtractText(xml_parser_, "link");
            if (link.empty() and part.attributes_.find("href") != part.attributes_.end())
                link = part.attributes_["href"];
        } else if (part.type_ == XMLParser::XMLPart::OPENING_TAG and part.data_ == "id")
            id = ExtractText(xml_parser_, "id", " (Atom::getNextItem)");
        else if (part.type_ == XMLParser::XMLPart::OPENING_TAG and part.data_ == "updated") {
            const std::string updated_string(TextUtil::CollapseAndTrimWhitespace(ExtractText(xml_parser_, "updated")));
            if (augment_params_.strptime_format_.empty()) {
                if (not TimeUtil::ParseRFC3339DateTime(updated_string, &updated))
                    throw std::runtime_error("can't convert date/time \"" + updated_string + "\" in Atom \"updated\" element to a time_t!");
            } else {
                struct tm parsed_date;
                if (not TimeUtil::StringToStructTm(&parsed_date, updated_string, augment_params_.strptime_format_))
                    LOG_ERROR("failed to parse \"" + updated_string + "\" with the given strptime_format \"" + augment_params_.strptime_format_ + "\"");
                updated = TimeUtil::TimeGm(parsed_date);
            }
        }
    }

    return nullptr;
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
static void ExtractNamespaces(XMLParser &parser, std::string * const rss_namespace,
                              std::string * const dc_namespace, std::string * const prism_namespace)
{
    XMLParser::XMLPart part;
    if (not parser.skipTo(XMLParser::XMLPart::OPENING_TAG, "rdf:RDF", &part)) {
        parser.rewind();
        if (not parser.skipTo(XMLParser::XMLPart::OPENING_TAG, "RDF", &part))
            throw std::runtime_error("in ExtractRSSNamespace(SyndicationFormat.cc): missing rdf:RDF opening tag!");
    }

    for (const auto &key_and_value : part.attributes_) {
        if (key_and_value.second == "http://purl.org/rss/1.0/")
            *rss_namespace = ExtractNamespacePrefix(key_and_value.first);
        else if (key_and_value.second == "http://purl.org/dc/elements/1.1/")
            *dc_namespace = ExtractNamespacePrefix(key_and_value.first);
        else if (key_and_value.second == "http://prismstandard.org/namespaces/2.0/basic/")
            *prism_namespace = ExtractNamespacePrefix(key_and_value.first);
    }
}


RDF::RDF(const std::string &xml_document, const AugmentParams &augment_params): SyndicationFormat(xml_document, augment_params) {
    ExtractNamespaces(xml_parser_, &rss_namespace_, &dc_namespace_, &prism_namespace_);

    XMLParser::XMLPart part;
    // peek instead of consuming the tag directly so that the first getNextItem() call
    // correctly parses the first <item> opening tag
    while (xml_parser_.peek(&part)) {
        if (part.type_ == XMLParser::XMLPart::OPENING_TAG and part.data_ == rss_namespace_ + "item")
            return;

        xml_parser_.getNext(&part);
        if (part.type_ == XMLParser::XMLPart::OPENING_TAG and part.data_ == rss_namespace_ + "image") {
            if (unlikely(not xml_parser_.skipTo(XMLParser::XMLPart::CLOSING_TAG, rss_namespace_ + "image")))
                throw std::runtime_error("in RDF::RDF: closing image tag not found!");
        } else if (part.type_ == XMLParser::XMLPart::OPENING_TAG and part.data_ == rss_namespace_ + "title")
            title_ = ExtractText(xml_parser_, rss_namespace_ + "title");
        else if (part.type_ == XMLParser::XMLPart::OPENING_TAG and part.data_ == rss_namespace_ + "link")
            link_ = ExtractText(xml_parser_, rss_namespace_ + "link");
        else if (part.type_ == XMLParser::XMLPart::OPENING_TAG and part.data_ == rss_namespace_ + "description")
            description_ = ExtractText(xml_parser_, rss_namespace_ + "description");
    }
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


void ExtractPrismData(XMLParser &xml_parser, const std::string &tag,
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
                LOG_WARNING("don't know what to do w/ \"" + tag + "\" tag attribute!");
        } else
            LOG_WARNING("don't know what to do w/ PRISM \"" + tag + "\" tag!");
        if (not xml_parser.skipTo(XMLParser::XMLPart::CLOSING_TAG, tag))
            throw std::runtime_error("in RDF::getNextItem: missing closing \"" + tag + "\" tag!");
    }
}


std::unique_ptr<SyndicationFormat::Item> RDF::getNextItem() {
    std::string title, description, link, id;
    time_t pub_date(TimeUtil::BAD_TIME_T);
    std::unordered_map<std::string, std::string> dc_and_prism_data;
    XMLParser::XMLPart part;
    while (xml_parser_.getNext(&part)) {
        if (part.type_ == XMLParser::XMLPart::OPENING_TAG and part.data_ == rss_namespace_ + "item") {
            const auto rdf_about(part.attributes_.find("rdf:about"));
            if (rdf_about != part.attributes_.cend())
                id = rdf_about->second;
        } else if (part.type_ == XMLParser::XMLPart::CLOSING_TAG and part.data_ == rss_namespace_ + "item")
            return std::unique_ptr<SyndicationFormat::Item>(new Item(title, description, link, id, pub_date, dc_and_prism_data));
        else if (part.type_ == XMLParser::XMLPart::OPENING_TAG) {
            if (part.data_ == rss_namespace_ + "title")
                title = ExtractText(xml_parser_, rss_namespace_ + "title");
            else if (part.data_ == rss_namespace_ + "description")
                description = ExtractText(xml_parser_, rss_namespace_ + "description");
            else if (part.data_ == rss_namespace_ + "link") {
                link = ExtractText(xml_parser_, rss_namespace_ + "link");
                if (link.empty() and part.attributes_.find("href") != part.attributes_.cend())
                    link = part.attributes_["href"];
            } else if (part.data_ == rss_namespace_ + "pubDate") {
                const std::string pub_date_string(TextUtil::CollapseAndTrimWhitespace(ExtractText(xml_parser_, rss_namespace_ + "pubDate")));
                if (augment_params_.strptime_format_.empty()) {
                    if (unlikely(not ParseRFC1123DateTimeAndPrefixes(pub_date_string, &pub_date)))
                        LOG_WARNING("couldn't parse \"" + pub_date_string + "\"!");
                } else {
                    struct tm parsed_date;
                    if (not TimeUtil::StringToStructTm(&parsed_date, pub_date_string, augment_params_.strptime_format_))
                        LOG_ERROR("failed to parse \"" + pub_date_string + "\" with the given strptime_format \"" + augment_params_.strptime_format_ + "\"");
                    pub_date = TimeUtil::TimeGm(parsed_date);
                }
            } else if (not dc_namespace_.empty() and StringUtil::StartsWith(part.data_, dc_namespace_)) {
                const std::string tag(part.data_);
                const std::string tag_suffix(tag.substr(dc_namespace_.length()));
                dc_and_prism_data["dc:" + tag_suffix] = ExtractText(xml_parser_, tag);
            } else if (not prism_namespace_.empty() and StringUtil::StartsWith(part.data_, prism_namespace_))
                ExtractPrismData(xml_parser_, part.data_, part.attributes_, prism_namespace_, &dc_and_prism_data);
        }
    }

    return nullptr;
}

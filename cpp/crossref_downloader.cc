/** \file    crossref_downloader.cc
 *  \brief   Downloads metadata from crossref.org and generates MARC-21 records.
 *  \author  Dr. Johannes Ruscheinski
 *
 *  \copyright (C) 2017, Library of the University of Tübingen
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
#include <iostream>
#include <stdexcept>
#include <vector>
#include <cstdlib>
#include <cstring>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>
#include "Compiler.h"
#include "Downloader.h"
#include "FileUtil.h"
#include "HttpHeader.h"
#include "MarcRecord.h"
#include "MarcWriter.h"
#include "MiscUtil.h"
#include "StringUtil.h"
#include "TextUtil.h"
#include "UrlUtil.h"
#include "util.h"


void Usage() {
    std::cerr << "Usage: " << ::progname << " [--timeout seconds] journal_list marc_output\n";
    std::exit(EXIT_FAILURE);
}


class CrossrefDate {
    unsigned year_, month_, day_;
public:
    CrossrefDate(const boost::property_tree::ptree &tree, const std::string &field);
    bool isValid() const { return year_ != 0; }
    unsigned getYear() const { return year_; }
    unsigned getMonth() const { return month_; }
    unsigned getDay() const { return day_; }
    std::string toString() const;
};


// Parses a JSON subtree that, should it exist looks like [[YYYY, MM, DD]] where the day as well as the
// month may be missing.
CrossrefDate::CrossrefDate(const boost::property_tree::ptree &tree, const std::string &field) {
    boost::property_tree::ptree::const_assoc_iterator tree_iter(tree.find(field));
    if (tree_iter == tree.not_found()) {
        year_ = month_ = day_ = 0;
        return;
    }

    auto nested_array_tree_iter(tree_iter->second.begin());
    if (unlikely(nested_array_tree_iter == tree_iter->second.end()))
        Error("in CrossrefDate::CrossrefDate: nested child of \"" + field + "\" does not exist!");
    auto nested_array_tree2_iter(nested_array_tree_iter->second.begin());
    if (unlikely(nested_array_tree_iter == nested_array_tree_iter->second.end()))
        Error("in CrossrefDate::CrossrefDate: inner nested child of \"" + field + "\" does not exist!");

    auto date_component_iter(nested_array_tree2_iter->second.begin());
    const auto &date_end(nested_array_tree2_iter->second.end());
    if (unlikely(date_component_iter == date_end))
        Error("in CrossrefDate::CrossrefDate: year is missing for the \"" + field + "\" date field!");

    const std::string year_candidate(date_component_iter->second.data());
    if (unlikely(not StringUtil::ToUnsigned(year_candidate, &year_)))
        Error("in CrossrefDate::CrossrefDate: cannot convert year component \"" + year_candidate
              + "\" to an unsigned integer!");
    if (unlikely(year_ < 1000 or year_ > 3000))
        Error("in CrossrefDate::CrossrefDate: year component \"" + year_candidate + "\" is unlikely to be a year!");

    ++date_component_iter;
    if (date_component_iter == date_end) {
        month_ = day_ = 0;
        return;
    }

    if (unlikely(not StringUtil::ToUnsigned(date_component_iter->second.data(), &month_)))
        Error("in CrossrefDate::CrossrefDate: cannot convert month component \"" + date_component_iter->first
              + "\" to an unsigned integer!");
    if (unlikely(month_ < 1 or month_ > 12))
        Error("in CrossrefDate::CrossrefDate: month component \"" + date_component_iter->first
              + "\" is not a month!");

    ++date_component_iter;
    if (date_component_iter == date_end) {
        day_ = 0;
        return;
    }

    if (unlikely(not StringUtil::ToUnsigned(date_component_iter->second.data(), &day_)))
        Error("in CrossrefDate::CrossrefDate: cannot convert day component \"" + date_component_iter->first
              + "\" to an unsigned integer!");
    if (unlikely(day_ < 1 or day_ > 31))
        Error("in CrossrefDate::CrossrefDate: day component \"" + date_component_iter->first
              + "\" is not a day!");
}


std::string CrossrefDate::toString() const {
    if (month_ == 0)
        return std::to_string(year_);

    std::string month_as_string;
    switch (month_) {
    case 1:
        month_as_string += "January";
        break;
    case 2:
        month_as_string += "February";
        break;
    case 3:
        month_as_string += "March";
        break;
    case 4:
        month_as_string += "April";
        break;
    case 5:
        month_as_string += "May";
        break;
    case 6:
        month_as_string += "June";
        break;
    case 7:
        month_as_string += "July";
        break;
    case 8:
        month_as_string += "August";
        break;
    case 9:
        month_as_string += "September";
        break;
    case 10:
        month_as_string += "October";
        break;
    case 11:
        month_as_string += "November";
        break;
    case 12:
        month_as_string += "December";
        break;
    default:
        Error("in CrossrefDate::toString: " + std::to_string(month_) + " is not a valid month!");
    }
    
    if (day_ == 0)
        return month_as_string + ", " + std::to_string(year_);

    return month_as_string + " " + std::to_string(day_) + ", " + std::to_string(year_);
}


/** \class MapDescriptor
 *  \brief Describes a mapping from a Crossref JSON field to a MARC-21 field.
 */
class MapDescriptor {
public:
    enum FieldType { STRING, AUTHOR_VECTOR, STRING_VECTOR, YEAR };
protected:
    std::string json_field_;
    FieldType field_type_;
    std::string marc_subfield_;
public:
    MapDescriptor(const std::string &json_field, const FieldType field_type, const std::string &marc_subfield)
        : json_field_(json_field), field_type_(field_type), marc_subfield_(marc_subfield) { }
    virtual ~MapDescriptor() { }
    
    inline const std::string &getJsonField() const { return json_field_; }
    inline FieldType getFieldType() const { return field_type_; }
    inline const std::string &getMarcSubfield() const { return marc_subfield_; }
    virtual void insertMarcData(const std::string &subfield_value, MarcRecord * const record);
};


void MapDescriptor::insertMarcData(const std::string &subfield_value, MarcRecord * const record) {
    const std::string tag(marc_subfield_.substr(0, DirectoryEntry::TAG_LENGTH));
    const char subfield_code(marc_subfield_.back());
    record->insertSubfield(tag, subfield_code, subfield_value);
}


class DOIMapDescriptor: public MapDescriptor {
public:
    DOIMapDescriptor(): MapDescriptor("DOI", MapDescriptor::STRING, "024a") { }
    virtual void insertMarcData(const std::string &subfield_value, MarcRecord * const record);
};


void DOIMapDescriptor::insertMarcData(const std::string &subfield_value, MarcRecord * const record) {
    const std::string tag(marc_subfield_.substr(0, DirectoryEntry::TAG_LENGTH));
    const char subfield_code(marc_subfield_.back());
    record->insertField(tag, "7 \x1F" + std::string(1, subfield_code) + subfield_value + "\x1F""2doi");
}


class YearMapDescriptor: public MapDescriptor {
public:
    YearMapDescriptor(const std::string &json_field, const std::string &marc_subfield)
        : MapDescriptor(json_field, MapDescriptor::YEAR, marc_subfield) { }
    virtual void insertMarcData(const std::string &subfield_value, MarcRecord * const record);
};


void YearMapDescriptor::insertMarcData(const std::string &subfield_value, MarcRecord * const record) {
    const std::string tag(marc_subfield_.substr(0, DirectoryEntry::TAG_LENGTH));
    const char subfield_code(marc_subfield_.back());
    record->insertField(tag, "  \x1F" + std::string(1, subfield_code) + subfield_value + "\x1F""2doi");
}


void InitCrossrefToMarcMapping(std::vector<MapDescriptor *> * const map_descriptors) {
    map_descriptors->emplace_back(new MapDescriptor("URL", MapDescriptor::STRING, "856u"));
    map_descriptors->emplace_back(new MapDescriptor("author", MapDescriptor::AUTHOR_VECTOR, "100a"));
    map_descriptors->emplace_back(new MapDescriptor("title", MapDescriptor::STRING, "245a"));
    map_descriptors->emplace_back(new MapDescriptor("publisher", MapDescriptor::STRING, "260a"));
    map_descriptors->emplace_back(new MapDescriptor("ISSN", MapDescriptor::STRING_VECTOR, "022a"));
    map_descriptors->emplace_back(new DOIMapDescriptor());
    map_descriptors->emplace_back(new YearMapDescriptor("issued", "936j"));
}


// Compares "s1" and "s2" while ignoring any occurences of characters found in "ignore_chars".
bool EqualIgnoreChars(const std::string &s1, const std::string &s2, const std::string &ignore_chars) {
    auto ch1(s1.cbegin());
    auto ch2(s2.cbegin());
    while (ch1 != s1.cend() and ch2 != s2.cend()) {
        if (ignore_chars.find(*ch1) != std::string::npos)
            ++ch1;
        else if (ignore_chars.find(*ch2) != std::string::npos)
            ++ch2;
        else if (*ch1 != *ch2)
            return false;
        else
            ++ch1, ++ch2;
    }

    while (ch1 != s1.cend() and ignore_chars.find(*ch1) != std::string::npos)
        ++ch1;
    while (ch2 != s2.cend() and ignore_chars.find(*ch2) != std::string::npos)
        ++ch2;

    return ch1 == s1.cend() and ch2 == s2.cend();
}


bool FuzzyTextMatch(const std::string &s1, const std::string &s2) {
    std::string lowercase_s1;
    if (unlikely(not TextUtil::UTF8ToLower(s1, &lowercase_s1)))
        Error("failed to convert supposed UTF-8 string \"" + s1 + "\" to a wide-character string! (1)");

    std::string lowercase_s2;
    if (unlikely(not TextUtil::UTF8ToLower(s2, &lowercase_s2)))
        Error("failed to convert supposed UTF-8 string \"" + s2 + "\" to a wide-character string! (2)");

    static const std::string IGNORE_CHARS(" :-");
    return EqualIgnoreChars(lowercase_s1, lowercase_s2, IGNORE_CHARS);
}


std::vector<std::string> ExtractString(const boost::property_tree::ptree &message_tree,
                                       const std::string &json_field_name)
{
    std::vector<std::string> extracted_values;
    const std::string value(message_tree.get<std::string>(json_field_name, ""));
    if (not value.empty())
        extracted_values.emplace_back(value);

    return extracted_values;
}


std::string ExtractAuthor(const boost::property_tree::ptree &author_ptree) {
    std::string author(author_ptree.get<std::string>("family"));
    const std::string given_name(author_ptree.get<std::string>("given", ""));
    if (not given_name.empty())
        author += ", " + given_name;

    return author;
}


std::vector<std::string> ExtractAuthorVector(const boost::property_tree::ptree &message_tree,
                                             const std::string &json_field_name)
{
    std::vector<std::string> extracted_values;

    boost::property_tree::ptree::const_assoc_iterator array_iter(message_tree.find(json_field_name));
    if (array_iter != message_tree.not_found()) {
        for (const auto &array_entry : array_iter->second)
            extracted_values.emplace_back(ExtractAuthor(array_entry.second));
    }

    return extracted_values;
}


std::vector<std::string> ExtractStringVector(const boost::property_tree::ptree &message_tree,
                                             const std::string &json_field_name)
{
    std::vector<std::string> extracted_values;

    boost::property_tree::ptree::const_assoc_iterator array_iter(message_tree.find(json_field_name));
    if (array_iter != message_tree.not_found()) {
        for (const auto &array_entry : array_iter->second)
            extracted_values.emplace_back(array_entry.second.data());
    }

    return extracted_values;
}


std::vector<std::string> ExtractYear(const boost::property_tree::ptree &message_tree,
                                     const std::string &json_field_name)
{
    std::vector<std::string> extracted_values;

    const CrossrefDate crossref_date(message_tree, json_field_name);
    extracted_values.emplace_back(std::to_string(crossref_date.getYear()));

    return extracted_values;
}


void CreateAndWriteMarcRecord(MarcWriter * const marc_writer, const boost::property_tree::ptree &message_tree,
                              const std::vector<MapDescriptor *> &map_descriptors)
{
    MarcRecord record;
    record.getLeader().setBibliographicLevel('a'); // We have an article.
    static unsigned control_number(0);
    record.insertField("001", std::to_string(++control_number));

    for (const auto &map_descriptor : map_descriptors) {
        std::vector<std::string> field_values;
        switch (map_descriptor->getFieldType()) {
        case MapDescriptor::STRING:
            field_values = ExtractString(message_tree, map_descriptor->getJsonField());
            break;
        case MapDescriptor::AUTHOR_VECTOR:
            field_values = ExtractAuthorVector(message_tree, map_descriptor->getJsonField());
            break;
        case MapDescriptor::STRING_VECTOR:
            field_values = ExtractStringVector(message_tree, map_descriptor->getJsonField());
            break;
        case MapDescriptor::YEAR:
            field_values = ExtractYear(message_tree, map_descriptor->getJsonField());
            break;
        default:
            Error("in CreateAndWriteMarcRecord: unexpected field type!");
        }

        for (const auto field_value : field_values)
            map_descriptor->insertMarcData(field_value, &record);
    }

    marc_writer->write(record);
}


// Converts the nnnn part of \unnnn to UTF-8. */
std::string UTF16EscapeToUTF8(std::string::const_iterator &cp, const std::string::const_iterator &end) {
    std::string hex_codes;
    for (unsigned i(0); i < 4; ++i) {
        if (unlikely(cp == end))
            Error("in UTF16EscapeToUTF8: unexpected end of input!");
        hex_codes += *cp++;
    }

    uint16_t u1;
    if (unlikely(not StringUtil::ToUnsignedShort(hex_codes, &u1, 16)))
            Error("in UTF16EscapeToUTF8: invalid hex sequence \\u" + hex_codes + "! (1)");

    if (TextUtil::IsValidSingleUTF16Char(u1))
        return TextUtil::UTF32ToUTF8(TextUtil::UTF16ToUTF32(u1));

    if (unlikely(not TextUtil::IsFirstHalfOfSurrogatePair(u1)))
        Error("in UTF16EscapeToUTF8: \\u" + hex_codes + " is neither a standalone UTF-8 character nor a valid "
              "first half of a UTF-16 surrogate pair!");

    if (unlikely(cp == end or *cp++ != '\\'))
        Error("in UTF16EscapeToUTF8: could not find expected '\\' as part of the 2nd half of a surrogate pair!");
    if (unlikely(cp == end or *cp++ != 'u'))
        Error("in UTF16EscapeToUTF8: could not find expected 'u' as part of the 2nd half of a surrogate pair!");
    
    hex_codes.clear();
    for (unsigned i(0); i < 4; ++i) {
        if (unlikely(cp == end))
            Error("in UTF16EscapeToUTF8: unexpected end of input while attempting to read a 2nd half of a surrogate "
                  "pair!");
        hex_codes += *cp++;
    }

    uint16_t u2;
    if (unlikely(not StringUtil::ToUnsignedShort(hex_codes, &u2, 16)))
            Error("in UTF16EscapeToUTF8: invalid hex sequence \\u" + hex_codes + "! (2)");
    if (unlikely(not TextUtil::IsSecondHalfOfSurrogatePair(u2)))
            Error("in UTF16EscapeToUTF8: invalid 2nd half of a surrogate pair: \\u" + hex_codes + "!");

    return TextUtil::UTF32ToUTF8(TextUtil::UTF16ToUTF32(u1, u2));
}


std::string UnescapeCrossRefJSON(const std::string &json_text) {
    std::string unescaped_string;
    bool in_text(false);
    auto cp(json_text.cbegin());
    while (cp != json_text.cend()) {
        if (in_text) {
            if (*cp == '\\') {
                if (unlikely(cp + 1 == json_text.cend()))
                    Error("in UnescapeCrossRefJSON: malformed JSON!");
                ++cp;
                if (*cp == '/')
                    unescaped_string += *cp++;
                else if (*cp == 'u')
                    unescaped_string += UTF16EscapeToUTF8(++cp, json_text.cend());
                else {
                    unescaped_string += '\\';
                    Warning("in UnescapeCrossRefJSON: unexpected escape \\" + std::string(1, *cp)
                            + "in JSON string constant!");
                    unescaped_string += *cp++;
                }
            } else {
                if (*cp == '"')
                    in_text = false;
                unescaped_string += *cp++;
            }
        } else {
            in_text = *cp == '"';
            unescaped_string += *cp++;
        }
    }

    return unescaped_string;
}


// Expects "line" to look like "XXXX-XXXX JJJ" where "XXXX-XXXX" is an ISSN and "JJJ" a journal title.
bool GetISSNAndJournalName(const std::string &line, std::string * const issn, std::string * const journal_name) {
    const size_t first_space_pos(line.find(' '));
    if (unlikely(first_space_pos == std::string::npos))
        return false;

    *issn = line.substr(0, first_space_pos);
    if (unlikely(not MiscUtil::IsPossibleISSN(*issn)))
        return false;

    *journal_name = StringUtil::TrimWhite(line.substr(first_space_pos + 1));
    return not journal_name->empty();
}


unsigned ProcessJournal(const unsigned timeout, const std::string &line, MarcWriter * const marc_writer,
                        const std::vector<MapDescriptor *> &map_descriptors)
{
    std::string issn, journal_name;
    if (unlikely(not GetISSNAndJournalName(line, &issn, &journal_name)))
        Error("bad input line \"" + line + "\"!");
    std::cout << "Processing " << journal_name << '\n';

    Downloader downloader("https://api.crossref.org/v1/journals/" + issn + "/works", Downloader::Params(),
                          timeout * 1000);
    if (downloader.anErrorOccurred()) {
        std::cerr << "Error while downloading metadata for ISSN " << issn << ": " << downloader.getLastErrorMessage()
                  << '\n';
        return 0;
    }

    // Check for rate limiting and error status codes:
    const HttpHeader http_header(downloader.getMessageHeader());
    if (http_header.getStatusCode() == 429)
        Error("we got rate limited!");
    else if (http_header.getStatusCode() != 200) {
        Warning("Crossref returned HTTP status code " + std::to_string(http_header.getStatusCode()) + "!");
        return 0;
    }

    const std::string json_document(UnescapeCrossRefJSON(downloader.getMessageBody()));
    std::stringstream query_input(json_document, std::ios_base::in);
    boost::property_tree::ptree full_tree;
    boost::property_tree::json_parser::read_json(query_input, full_tree);

    const boost::property_tree::ptree::const_assoc_iterator message_iter(full_tree.find("message"));
    if (unlikely(message_iter == full_tree.not_found()))
        return 0;

    const boost::property_tree::ptree::const_assoc_iterator items_iter(message_iter->second.find("items"));
    if (unlikely(items_iter == items_iter->second.not_found()))
        return 0;
    
    unsigned document_count(0);
    for (const auto &item : items_iter->second) {
        const boost::property_tree::ptree::const_assoc_iterator container_titles(
            item.second.find("container-title"));
        if (container_titles == item.second.not_found())
            continue;

        CreateAndWriteMarcRecord(marc_writer, item.second, map_descriptors);
        ++document_count;
    }

    return document_count;
}


int main(int argc, char *argv[]) {
    ::progname = argv[0];

    if (argc != 3 and argc != 5)
        Usage();

    const unsigned DEFAULT_TIMEOUT(20); // seconds
    unsigned timeout(DEFAULT_TIMEOUT);
    if (std::strcmp(argv[1], "--timeout") == 0) {
        if (not StringUtil::ToUnsigned(argv[2], &timeout))
            Error("bad timeout \"" + std::string(argv[2]) + "\"!");
        argc -= 2;
        argv += 2;
    }

    if (argc != 3)
        Usage();

    const std::string journal_list_filename(argv[1]);
    const std::string marc_output_filename(argv[2]);

    try {
        const std::unique_ptr<File> journal_list_file(FileUtil::OpenInputFileOrDie(journal_list_filename));
        const std::unique_ptr<MarcWriter> marc_writer(MarcWriter::Factory(marc_output_filename));

        std::vector<MapDescriptor *> map_descriptors;
        InitCrossrefToMarcMapping(&map_descriptors);

        unsigned success_count(0), total_article_count(0);
        while (not journal_list_file->eof()) {
            std::string line;
            journal_list_file->getline(&line);
            StringUtil::Trim(&line);
            if (not line.empty()) {
                const unsigned article_count(ProcessJournal(timeout, line, marc_writer.get(), map_descriptors));
                if (article_count > 0) {
                    ++success_count;
                    total_article_count += article_count;
                }
            }
        }

        std::cout << "Downloaded metadata for at least one article from " << success_count << " journals.\n";
        std::cout << "The total number of articles for which metadata was downloaded is " << total_article_count
                  << ".\n";
        return success_count == 0 ? EXIT_FAILURE : EXIT_SUCCESS;
    } catch (const std::exception &e) {
        Error("Caught exception: " + std::string(e.what()));
    }
}

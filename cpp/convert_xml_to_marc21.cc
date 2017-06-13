/** \brief Parses XML and generates MARC-21 data.
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
#include <iostream>
#include <list>
#include <map>
#include <cstdlib>
#include "Compiler.h"
#include "FileUtil.h"
#include "MarcReader.h"
#include "MarcRecord.h"
#include "MarcWriter.h"
#include "MarcXmlWriter.h"
#include "MiscUtil.h"
#include "RegexMatcher.h"
#include "SimpleXmlParser.h"
#include "StringUtil.h"
#include "util.h"


__attribute__((noreturn)) void Usage() {
    std::cerr << "Usage: " << ::progname
              << " [--verbose] --output-format=(marc_binary|marc_xml) config_file oai_pmh_dc_input marc_output\n";
    std::exit(EXIT_FAILURE);
}


class Matcher {
    std::map<std::string, std::string> required_attribs_and_values_;
    bool required_;
public:
    enum MatcherType { SINGLE_MATCH, MULTIPLE_MATCHES_AND_MAP };
protected:
    Matcher(const std::map<std::string, std::string> &required_attribs_and_values, const bool required)
        : required_attribs_and_values_(required_attribs_and_values), required_(required) { }
public:
    virtual ~Matcher() { }

    virtual MatcherType getType() const = 0;
    bool isRequired() const { return required_; }
    bool xmlTagAttribsAndValuesMatched(const std::map<std::string, std::string> &attrib_map) const;
};


bool Matcher::xmlTagAttribsAndValuesMatched(const std::map<std::string, std::string> &attrib_map) const {
    for (const auto &required_attrib_and_value : required_attribs_and_values_) {
        const auto iter(attrib_map.find(required_attrib_and_value.first));
        if (iter == attrib_map.cend())
            return false;
        if (iter->second != required_attrib_and_value.second)
            return false;
    }

    return true;
}


// An instance of this class specifies a rule for if and how to extract XML data and how to map it to a MARC-21
// field and subfield or leader positions.
class SingleMatchMatcher:public Matcher {
    std::string field_tag_;
    char subfield_code_, indicator1_, indicator2_;
    const RegexMatcher * const matching_regex_;
    const RegexMatcher * const extraction_regex_;
public:
    SingleMatchMatcher(const std::string &field_tag, const char subfield_code,
                       const std::map<std::string, std::string> &required_attribs_and_values, const bool required,
                       const RegexMatcher * const matching_regex = nullptr,
                       const RegexMatcher * const extraction_regex = nullptr, const char indicator1 = ' ',
                       const char indicator2 = ' ')
        : Matcher(required_attribs_and_values, required), field_tag_(field_tag), subfield_code_(subfield_code),
          indicator1_(indicator1), indicator2_(indicator2), matching_regex_(matching_regex),
          extraction_regex_(extraction_regex) { }

    virtual MatcherType getType() const { return SINGLE_MATCH; }

    inline bool matched(const std::string &character_data) const {
        return (matching_regex_ == nullptr) ? true : matching_regex_->matched(character_data);
    }

    /** \return That part of the character data that ought to be inserted into a MARC record. */
    std::string getInsertionData(const std::string &character_data) const;

    inline const std::string &getMarcFieldTag() const { return field_tag_; }
    inline char getMarcSubfieldCode() const { return subfield_code_; }
    inline char getIndicator1() const { return indicator1_; }
    inline char getIndicator2() const { return indicator2_; }
};


class MultipleMatchMatcher: public Matcher {
    const std::map<RegexMatcher *, std::string> regex_to_biblio_level_and_type_map_;
public:
    explicit MultipleMatchMatcher(const std::map<std::string, std::string> &required_attribs_and_values,
                                  const bool required,
                                  const std::map<RegexMatcher *, std::string> &regex_to_biblio_level_and_type_map)
        : Matcher(required_attribs_and_values, required),
          regex_to_biblio_level_and_type_map_(regex_to_biblio_level_and_type_map) { }

    virtual MatcherType getType() const { return MULTIPLE_MATCHES_AND_MAP; }

    const std::map<RegexMatcher *, std::string> &getRegexToBiblioLevelAndTypeMap()  const
        { return regex_to_biblio_level_and_type_map_; }
};


std::string SingleMatchMatcher::getInsertionData(const std::string &character_data) const {
    if (extraction_regex_ == nullptr)
        return character_data;

    if (not extraction_regex_->matched(character_data))
        return "";
    return (*extraction_regex_)[1];
}


// Expects the "string" that we extract to either contain no spaces or to be enclosed in double quotes.
// Optional comments start with a hash sign.
std::string ExtractOptionallyQuotedString(std::string::const_iterator &ch,
                                          const std::string::const_iterator &end,
                                          const std::set<char> &non_string_chars = { ' ' })
{
    if (ch == end)
        return "";

    std::string extracted_string;
    if (*ch == '"') { // Extract quoted string.
        ++ch;

        bool escaped(false);
        while (ch != end and *ch != '"') {
            if (unlikely(escaped)) {
                escaped = false;
                extracted_string += *ch;
            } else if (unlikely(*ch == '\\'))
                escaped = true;
            else
                extracted_string += *ch;
            ++ch;
        }

        if (unlikely(*ch != '"'))
            throw std::runtime_error("missing closing quote!");
        ++ch;
    } else { // Extract non-quoted string.
        while (ch != end and non_string_chars.find(*ch) == non_string_chars.cend())
            extracted_string += *ch++;
    }

    return extracted_string;
}


inline void SkipSpaces(std::string::const_iterator &ch, const std::string::const_iterator &end) {
    while (ch != end and *ch == ' ')
        ++ch;
}


void ParseOptionalRequiredAttributes(std::string::const_iterator &ch, const std::string::const_iterator &end,
                                     const unsigned line_no,
                                     std::map<std::string, std::string> * const required_attribs)
{
    required_attribs->clear();
    if (*ch != '(')
        return;
    ++ch;

    while (ch != end and *ch != ')') {
        if (not required_attribs->empty()) {
            if (unlikely(*ch) != ',')
                throw std::runtime_error("comma expected in attibute/value list on line " + std::to_string(line_no)
                                         + "!");
            ++ch; // Skip over the comma.
        }

        std::string attrib_name;
        while (ch != end and *ch != '=')
            attrib_name += *ch++;
        if (unlikely(ch == end))
            throw std::runtime_error("unexpected end-of-line while parsing an attibute name on line "
                                     + std::to_string(line_no) + "!");
        ++ch; // Skip over the equal-sign.

        const std::string attrib_value(ExtractOptionallyQuotedString(ch, end, { ')', ',' }));
        if (unlikely(ch == end))
            throw std::runtime_error("unexpected end-of-line while parsing an attibute value on line "
                                     + std::to_string(line_no) + "!");

        required_attribs->emplace(attrib_name, attrib_value);
    }

    if (unlikely(ch == end))
        throw std::runtime_error("unexpected end-of-line while parsing an attibute/value list on line "
                                 + std::to_string(line_no) + "!");

    ++ch; // Skip over the closing parenthesis.
}


void ParseMapBiblioLevelAndType(std::string::const_iterator &ch, const std::string::const_iterator &end,
                                const unsigned line_no, const bool required,
                                std::map<std::string, std::list<const Matcher *>> * const xml_tag_to_matchers_map)
{
    SkipSpaces(ch, end);
    const std::string xml_tag(ExtractOptionallyQuotedString(ch, end));
    if (unlikely(xml_tag.empty()))
        throw std::runtime_error("missing or empty XML tag on line " + std::to_string(line_no) + "!");

    std::map<std::string, std::string> required_attribs;
    ParseOptionalRequiredAttributes(ch, end, line_no, &required_attribs);

    std::map<RegexMatcher *, std::string> regex_to_biblio_level_and_type_map;
    SkipSpaces(ch, end);
    while (ch != end) {
        const std::string regex_and_level_and_type(ExtractOptionallyQuotedString(ch, end));
        const std::string::size_type first_colon_pos(regex_and_level_and_type.find(':'));
        if (unlikely(first_colon_pos == std::string::npos))
            throw std::runtime_error("colon missing in (regex, level-and-type-entry) pair!");

        const std::string regex_string(regex_and_level_and_type.substr(0, first_colon_pos));
        std::string err_msg;
        RegexMatcher *matching_regex(RegexMatcher::RegexMatcherFactory(regex_string, &err_msg));

        const std::string level_and_type(regex_and_level_and_type.substr(first_colon_pos + 1));
        if (unlikely(level_and_type.length() != 2))
            throw std::runtime_error("bad level-and-type-entry \"" + level_and_type + "\"!");

        regex_to_biblio_level_and_type_map.emplace(std::make_pair(matching_regex, level_and_type));

        SkipSpaces(ch, end);
    }

    if (unlikely(regex_to_biblio_level_and_type_map.empty()))
        throw std::runtime_error("missing regex and level-and-type entries!");

    xml_tag_to_matchers_map->emplace(xml_tag, std::list<const Matcher *>{
            new MultipleMatchMatcher(required_attribs, required, regex_to_biblio_level_and_type_map) });
}


void ParseSimpleMatchRequirement(const std::string &xml_tag, std::string::const_iterator &ch,
                                 const std::string::const_iterator &end, const unsigned line_no, const bool required,
                                 std::map<std::string, std::list<const Matcher *>> * const xml_tag_to_matchers_map)
{
    std::map<std::string, std::string> required_attribs;
    ParseOptionalRequiredAttributes(ch, end, line_no, &required_attribs);
    SkipSpaces(ch, end);

    const size_t LENGTH_WITHOUT_INDICATORS(DirectoryEntry::TAG_LENGTH + 1);
    const size_t LENGTH_WITH_INDICATORS(DirectoryEntry::TAG_LENGTH + 1 + 2);
    const std::string optional_indicators_marc_tag_and_subfield_code(
        ExtractOptionallyQuotedString(ch, end));
    const bool do_not_copy(optional_indicators_marc_tag_and_subfield_code == "do_not_copy");
    if (unlikely(not do_not_copy
                 and optional_indicators_marc_tag_and_subfield_code.length() != LENGTH_WITHOUT_INDICATORS
                 and optional_indicators_marc_tag_and_subfield_code.length() != LENGTH_WITH_INDICATORS))
        throw std::runtime_error("bad optional indicators, MARC tag and subfield code \""
                                 + optional_indicators_marc_tag_and_subfield_code + "\"!");
    SkipSpaces(ch, end);

    RegexMatcher *matching_regex(nullptr), *extraction_regex(nullptr);
    if (ch != end) {
        const std::string matching_regex_string(ExtractOptionallyQuotedString(ch, end));
        std::string err_msg;
        matching_regex = RegexMatcher::RegexMatcherFactory(matching_regex_string, &err_msg);
        if (unlikely(not err_msg.empty()))
            throw std::runtime_error("failed to compile regular expression for the matching regex! ("
                                     + err_msg + ")");

        SkipSpaces(ch, end);
        if (ch != end) {
            const std::string extraction_regex_string(ExtractOptionallyQuotedString(ch, end));
            extraction_regex = RegexMatcher::RegexMatcherFactory(extraction_regex_string, &err_msg);
            if (unlikely(not err_msg.empty()))
                throw std::runtime_error("failed to compile regular expression for the extraction regex! ("
                                         + err_msg + ")");
            if (unlikely(extraction_regex->getNoOfGroups() != 1))
                throw std::runtime_error("regular expression for the extraction regex needs exactly one "
                                         "capture group!");
        }

        SkipSpaces(ch, end);
        if (unlikely(ch != end))
            throw std::runtime_error("junk after regular expression!");
    }

    char indicator1(' '), indicator2(' ');
    std::string marc_tag;
    if (likely(not do_not_copy)) {
        if (optional_indicators_marc_tag_and_subfield_code.length() == LENGTH_WITH_INDICATORS) {
            indicator1 = optional_indicators_marc_tag_and_subfield_code[0];
            indicator2 = optional_indicators_marc_tag_and_subfield_code[1];
            marc_tag = optional_indicators_marc_tag_and_subfield_code.substr(2, DirectoryEntry::TAG_LENGTH);
        } else
            marc_tag = optional_indicators_marc_tag_and_subfield_code.substr(0, DirectoryEntry::TAG_LENGTH);
    }
    
    const SingleMatchMatcher * const new_matcher(
        new SingleMatchMatcher(do_not_copy ? "do_not_copy" : marc_tag,
                               optional_indicators_marc_tag_and_subfield_code.back(), required_attribs, required,
                               matching_regex, extraction_regex, indicator1, indicator2));
    const auto xml_tag_and_matchers(xml_tag_to_matchers_map->find(xml_tag));
    if (xml_tag_and_matchers == xml_tag_to_matchers_map->end())
        xml_tag_to_matchers_map->emplace(xml_tag, std::list<const Matcher *>{ new_matcher });
    else
        xml_tag_and_matchers->second.emplace_back(new_matcher);
}


// Loads a config file that specifies the mapping from XML elements to MARC fields.  An entry looks like this
//
//     ["required"] xml_tag_name [indicators]marc_field_and_subfield [match_regex [extraction_regex]]
//                                     or
//     "map_biblio_level_and_type" xml_tag_name match_regex1 level_and_type1 ... match_regexN level_and_typeN
//
// "xml_tag_name" is the tag for which the rule applies.  "marc_field_and_subfield" is the field which gets created
// when we have a match.  "optional_match_regex" when present has to match the character data following the tag for
// the rule to apply and "optional_extraction_regex" specifies which part of the data will be used (group 1).  The
// field and subfield code can also be substituted with "do_no_copy".  This is really only useful in conjunction with
// "required".  Please note that there can be no spaces between the optional indicators, if present, and the following
// MARC tag specification.
void LoadConfig(File * const input, std::map<std::string, std::list<const Matcher *>> * const xml_tag_to_matchers_map)
{
    xml_tag_to_matchers_map->clear();

    unsigned line_no(0);
    while (not input->eof()) {
        std::string line(input->getline());
        ++line_no;

        // Process optional comment:
        const std::string::size_type first_hash_pos(line.find('#'));
        if (first_hash_pos != std::string::npos)
            line.resize(first_hash_pos);

        StringUtil::TrimWhite(&line);
        if (line.empty())
            continue;

        try {
            auto ch(line.cbegin());
            const auto end(line.cend());
            SkipSpaces(ch, end);
            std::string xml_tag_or_keyword(ExtractOptionallyQuotedString(ch, end, { ' ', '(' }));
            if (unlikely(xml_tag_or_keyword.empty()))
                throw std::runtime_error("missing or empty XML tag (1)!");
            SkipSpaces(ch, end);

            const bool required(xml_tag_or_keyword == "required");
            if (required) {
                xml_tag_or_keyword = ExtractOptionallyQuotedString(ch, end, { ' ', '(' });
                if (unlikely(xml_tag_or_keyword.empty()))
                    throw std::runtime_error("missing or empty XML tag (2)!");
                SkipSpaces(ch, end);
            }

            if (xml_tag_or_keyword == "map_biblio_level_and_type") {
                ParseMapBiblioLevelAndType(ch, end, line_no, required, xml_tag_to_matchers_map);
                continue;
            }

            ParseSimpleMatchRequirement(xml_tag_or_keyword, ch, end, line_no, required, xml_tag_to_matchers_map);
        } catch (const std::exception &x) {
            throw std::runtime_error("error while parsing line #" + std::to_string(line_no) + " in \""
                                     + input->getPath() + "\"! (" + std::string(x.what()) + ")");
        }
    }
}


/** Generates a PPN by counting down from the largest possible PPN. */
std::string GeneratePPN() {
    static unsigned next_ppn(99999999);
    const std::string ppn_without_checksum_digit(StringUtil::ToString(next_ppn, /* radix = */10, /* width = */8));
    --next_ppn;
    return ppn_without_checksum_digit + MiscUtil::GeneratePPNChecksumDigit(ppn_without_checksum_digit);
}


unsigned CountRequiredMatchers(const std::map<std::string, std::list<const Matcher *>> &xml_tag_to_matchers_map) {
    unsigned required_matcher_count(0);

    for (const auto &tag_and_matchers : xml_tag_to_matchers_map) {
        for (const auto matcher : tag_and_matchers.second) {
            if (matcher->isRequired())
                ++required_matcher_count;
        }
    }

    return required_matcher_count;
}


enum OutputFormat { MARC_BINARY, MARC_XML };


void ProcessRecords(const bool verbose, File * const input, MarcWriter * const marc_writer,
                    const std::map<std::string, std::list<const Matcher *>> &xml_tag_to_matchers_map)
{
    const unsigned REQUIRED_CONDITIONS_COUNT(CountRequiredMatchers(xml_tag_to_matchers_map));

    SimpleXmlParser<File>::Type type;
    std::string data;
    std::map<std::string, std::string> attrib_map;
    SimpleXmlParser<File> xml_parser(input);
    MarcRecord record;
    unsigned record_count(0), written_record_count(0);
    bool collect_character_data(false);
    std::string character_data;
    unsigned met_required_conditions_count(0);
    std::vector<const Matcher *> matchers;
xml_parse_loop:
    while (xml_parser.getNext(&type, &attrib_map, &data)) {
        switch (type) {
        case SimpleXmlParser<File>::END_OF_DOCUMENT:
            if (verbose)
                std::cout << "Wrote " << written_record_count << " record(s) of " << record_count
                          << " record(s) which were found in the XML input stream.\n";
            return;
        case SimpleXmlParser<File>::OPENING_TAG:
            if (data == "record") {
                record = MarcRecord();
                record.insertField("001", GeneratePPN());
                collect_character_data = false;
                met_required_conditions_count = 0;
            } else {
                matchers.clear();
                character_data.clear();
                const auto tags_and_matchers(xml_tag_to_matchers_map.find(data));
                if (tags_and_matchers != xml_tag_to_matchers_map.cend()) {
                    for (const auto matcher : tags_and_matchers->second) {
                        if (matcher->Matcher::xmlTagAttribsAndValuesMatched(attrib_map))
                            matchers.emplace_back(matcher);
                    }
                }
                collect_character_data = not matchers.empty();
                if (matchers.empty() and verbose)
                    std::cerr << "No matcher found for XML tag \"" << data << "\".\n";
            }
            break;
        case SimpleXmlParser<File>::CLOSING_TAG:
            if (data == "record") {
                if (met_required_conditions_count == REQUIRED_CONDITIONS_COUNT) {
                    marc_writer->write(record);
                    ++written_record_count;
                }

                ++record_count;
            } else if (not character_data.empty()) {
                for (const auto &matcher : matchers) {
                    switch (matcher->getType()) {
                    case Matcher::SINGLE_MATCH: {
                        const SingleMatchMatcher * const single_match_matcher(
                            (const SingleMatchMatcher * const)(matcher));
                        if (single_match_matcher->matched(character_data)) {
                            if (matcher->isRequired())
                                ++met_required_conditions_count;
                            if (likely(single_match_matcher->getMarcFieldTag() != "do_not_copy"))
                                record.insertSubfield(single_match_matcher->getMarcFieldTag(),
                                                      single_match_matcher->getMarcSubfieldCode(),
                                                      single_match_matcher->getInsertionData(character_data),
                                                      single_match_matcher->getIndicator1(),
                                                      single_match_matcher->getIndicator2());
                        }
                        break;
                    }
                    case Matcher::MULTIPLE_MATCHES_AND_MAP: {
                        const MultipleMatchMatcher * const multiple_match_matcher(
                            (const MultipleMatchMatcher * const)(matcher));
                        const std::map<RegexMatcher *, std::string> &
                            map(multiple_match_matcher->getRegexToBiblioLevelAndTypeMap());
                        for (const auto &regex_and_values : map) {
                            if (regex_and_values.first->matched(character_data)) {
                                if (matcher->isRequired())
                                    ++met_required_conditions_count;
                                Leader &leader(record.getLeader());
                                leader.setRecordType(regex_and_values.second[0]);
                                leader.setBibliographicLevel(regex_and_values.second[1]);
                                goto xml_parse_loop;
                            }
                        }
                        Warning("found no match for \"" + character_data + "\"! (XML tag was " + data + ".)");
                        break;
                    }
                    }
                }
            }
            break;
        case SimpleXmlParser<File>::CHARACTERS:
            if (collect_character_data)
                character_data += data;
            break;
        default:
            /* Intentionally empty! */;
        }
    }

    Error("XML parsing error: " + xml_parser.getLastErrorMessage());
}


int main(int argc, char *argv[]) {
    ::progname = argv[0];

    if (argc != 5 and argc != 6)
        Usage();

    bool verbose(false);
    if (argc == 6) {
        if (std::strcmp(argv[1], "--verbose") != 0)
            Usage();
        verbose = true;
        --argc, ++argv;
    }

    OutputFormat output_format;
    if (std::strcmp(argv[1], "--output-format=marc_binary") == 0)
        output_format = MARC_BINARY;
    else if (std::strcmp(argv[1], "--output-format=marc_xml") == 0)
        output_format = MARC_XML;
    else
        Usage();

    const std::unique_ptr<File> config_input(FileUtil::OpenInputFileOrDie(argv[2]));
    const std::unique_ptr<File> input(FileUtil::OpenInputFileOrDie(argv[3]));
    const std::unique_ptr<MarcWriter> marc_writer(
        MarcWriter::Factory(argv[4], output_format == MARC_BINARY ? MarcWriter::BINARY : MarcWriter::XML));

    try {
        std::map<std::string, std::list<const Matcher *>> xml_tag_to_matchers_map;
        LoadConfig(config_input.get(), &xml_tag_to_matchers_map);
        ProcessRecords(verbose, input.get(), marc_writer.get(), xml_tag_to_matchers_map);
    } catch (const std::exception &x) {
        Error("caught exception: " + std::string(x.what()));
    }
}

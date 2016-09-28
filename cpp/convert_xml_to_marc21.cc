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
#include "MarcUtil.h"
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


// An instance of this class specifies a rule for if and how to extract XML data and how to map it to a MARC-21
// field and subfield.
class Matcher {
    std::string field_tag_;
    char subfield_code_;
    const RegexMatcher * const matching_regex_;
    const RegexMatcher * const extraction_regex_;
public:
    Matcher(const std::string &field_tag, const char subfield_code,
            const RegexMatcher * const matching_regex = nullptr,
            const RegexMatcher * const extraction_regex = nullptr)
        : field_tag_(field_tag), subfield_code_(subfield_code), matching_regex_(matching_regex),
          extraction_regex_(extraction_regex) { }

    inline bool matched(const std::string &character_data) const {
        return (matching_regex_ == nullptr) ? true : matching_regex_->matched(character_data);
    }

    /** \return That part of the character data that ought to be inserted into a MARC record. */
    std::string getInsertionData(const std::string &character_data) const;

    inline const std::string &getMarcFieldTag() const { return field_tag_; }
    inline char getMarcSubfieldCode() const { return subfield_code_; }
};


std::string Matcher::getInsertionData(const std::string &character_data) const {
    if (extraction_regex_ == nullptr)
        return character_data;

    if (not extraction_regex_->matched(character_data))
        return "";
    return (*extraction_regex_)[1];
}


// Expects the "string" that we extract to either contain no spaces or to be enclosed in double quotes.
// Optional comments start with a hash sign.
std::string ExtractOptionallyQuotedString(std::string::const_iterator &ch,
                                          const std::string::const_iterator &end)
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
        while (ch != end and *ch != ' ')
            extracted_string += *ch++;
    }

    return extracted_string;
}


void SkipSpaces(std::string::const_iterator &ch, const std::string::const_iterator &end) {
    while (ch != end and *ch == ' ')
        ++ch;
}


// Loads a config file that specifies the mapping from XML elements to MARC fields.  An entry looks like this
//
//     xml_tag_name marc_field_and_subfield optional_match_regex optional_extraction_regex
//
// "xml_tag_name" is the tag for which the rule applies.  "marc_field_and_subfield" is the field which gets created
// when we have a match.  "optional_match_regex" when present has to match the character data following the tag for
// the rule to apply and "optional_extraction_regex" specifies which part of the data will be used (group 1).
void LoadConfig(File * const input, std::map<std::string, std::list<Matcher>> * const xml_tag_to_marc_entry_map) {
    xml_tag_to_marc_entry_map->clear();

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
            const std::string xml_tag(ExtractOptionallyQuotedString(ch, end));
            if (unlikely(xml_tag.empty()))
                throw std::runtime_error("missing or empty XML tag!");
            SkipSpaces(ch, end);
            const std::string marc_tag_and_subfield_code(ExtractOptionallyQuotedString(ch, end));
            if (unlikely(marc_tag_and_subfield_code.length() != DirectoryEntry::TAG_LENGTH + 1))
                throw std::runtime_error("bad MARC tag and subfield code!");
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
            }

            Matcher new_matcher(marc_tag_and_subfield_code.substr(0, DirectoryEntry::TAG_LENGTH),
                                marc_tag_and_subfield_code[DirectoryEntry::TAG_LENGTH],
                                matching_regex, extraction_regex);
            const auto xml_tag_and_entries(xml_tag_to_marc_entry_map->find(xml_tag));
            if (xml_tag_and_entries == xml_tag_to_marc_entry_map->end())
                xml_tag_to_marc_entry_map->emplace(xml_tag, std::list<Matcher>{ new_matcher });
            else
                xml_tag_and_entries->second.push_back(new_matcher);
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


enum OutputFormat { MARC_BINARY, MARC_XML };


void ProcessRecords(const bool verbose, const OutputFormat output_format, File * const input,
                    File * const output, const std::map<std::string, std::list<Matcher>> &xml_tag_to_marc_entry_map)
{
    MarcXmlWriter *xml_writer;
    if (output_format == MARC_XML)
        xml_writer = new MarcXmlWriter(output);
    else
        xml_writer = nullptr;
    
    SimpleXmlParser::Type type;
    std::string data;
    std::map<std::string, std::string> attrib_map;
    SimpleXmlParser xml_parser(input);
    MarcUtil::Record record;
    unsigned record_count(0);
    bool collect_character_data;
    std::string character_data;
    while (xml_parser.getNext(&type, &attrib_map, &data)) {
        switch (type) {
        case SimpleXmlParser::END_OF_DOCUMENT:
            if (verbose)
                std::cout << "Found " << record_count << " record(s) in the XML input stream.\n";
            delete xml_writer;
            return;
        case SimpleXmlParser::OPENING_TAG:
            if (data == "record") {
                record = MarcUtil::Record();
                record.insertField("001", GeneratePPN());
                collect_character_data = false;
            } else if (xml_tag_to_marc_entry_map.find(data) != xml_tag_to_marc_entry_map.cend()) {
                character_data.clear();
                collect_character_data = true;
            } else
                collect_character_data = false;
            break;
        case SimpleXmlParser::CLOSING_TAG:
            if (data == "record") {
                (xml_writer == nullptr) ? record.write(output) : record.write(xml_writer);
                ++record_count;
            } else {
                const auto xml_tag_and_entries(xml_tag_to_marc_entry_map.find(data));
                if (xml_tag_and_entries == xml_tag_to_marc_entry_map.cend())
                    continue;
                for (auto matcher(xml_tag_and_entries->second.cbegin());
                     matcher != xml_tag_and_entries->second.cend(); ++matcher)
                {
                    if (matcher->matched(character_data)) {
                        record.insertSubfield(matcher->getMarcFieldTag(), matcher->getMarcSubfieldCode(),
                                              matcher->getInsertionData(character_data));
                        continue;
                    }
                }
            }
            break;
        case SimpleXmlParser::CHARACTERS:
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
    const std::unique_ptr<File> output(FileUtil::OpenOutputFileOrDie(argv[4]));

    try {
        std::map<std::string, std::list<Matcher>> xml_tag_to_marc_entry_map;
        LoadConfig(config_input.get(), &xml_tag_to_marc_entry_map);
        ProcessRecords(verbose, output_format, input.get(), output.get(), xml_tag_to_marc_entry_map);
    } catch (const std::exception &x) {
        Error("caught exception: " + std::string(x.what()));
    }
}

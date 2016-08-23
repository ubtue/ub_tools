/** \brief A MARC-21 filter utility that can remove records based on patterns for MARC subfields.
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
#include <memory>
#include <unordered_set>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "DirectoryEntry.h"
#include "FileUtil.h"
#include "Leader.h"
#include "MarcUtil.h"
#include "MarcXmlWriter.h"
#include "MediaTypeUtil.h"
#include "RegexMatcher.h"
#include "StringUtil.h"
#include "Subfields.h"
#include "util.h"


void Usage() {
    std::cerr << "usage: " << progname << " (--drop|--keep|--remove-fields) [--input-format=(marc-xml|marc-21)] [--output-format=(marc-xml|marc-21)] marc_input marc_output field_or_subfieldspec1:regex1 "
              << "[field_or_subfieldspec2:regex2 .. field_or_subfieldspecN:regexN]\n"
              << "       where \"field_or_subfieldspec\" must either be a MARC tag or a MARC tag followed by a\n"
              << "       single-character subfield code and \"regex\" is a Perl-compatible regular expression.\n"
              << "       If you don't specify an output format it will be the same as the input format.\n\n";

    std::exit(EXIT_FAILURE);
}


class CompiledPattern {
    std::string tag_;
    char subfield_code_;
    RegexMatcher matcher_;
public:
    static const char NO_SUBFIELD_CODE;
public:
    CompiledPattern(const std::string &tag, const char subfield_code,  const RegexMatcher &matcher)
        : tag_(tag), subfield_code_(subfield_code), matcher_(matcher) {}
    const std::string &getTag() const { return tag_; }
    bool hasSubfieldCode() const { return subfield_code_ != NO_SUBFIELD_CODE; }
    char getSubfieldCode() const { return subfield_code_; }
    bool fieldMatched(const std::string &field_contents) const;
    bool subfieldMatched(const std::string &subfield_contents) const;
};


const char CompiledPattern::NO_SUBFIELD_CODE('\0');


bool CompiledPattern::fieldMatched(const std::string &field_contents) const {
    std::string err_msg;
    const bool retval = matcher_.matched(field_contents, &err_msg);
    if (not retval and not err_msg.empty())
        Error("Unexpected error while trying to match a field in CompiledPattern::fieldMatched(): " + err_msg);

    return retval;
}


bool CompiledPattern::subfieldMatched(const std::string &subfield_contents) const {
    std::string err_msg;
    const bool retval = matcher_.matched(subfield_contents, &err_msg);
    if (not retval and not err_msg.empty())
        Error("Unexpected error while trying to match a subfield in CompiledPattern::subfieldMatched(): " + err_msg);

    return retval;
}


// Expects "patterns" to contain strings that look like TTTS:REGEX where TTT are 3 characters specifying a field,
// S is a subfield code and REGEX is a PCRE-style regex supporting UTF8 that should match subfield contents.
// Alteratively a pattern can look like TTT:REGEX where TTT is a tag and we have no subfield code.
bool CompilePatterns(const std::vector<std::string> &patterns, std::vector<CompiledPattern> * const compiled_patterns,
                     std::string * const err_msg)
{
    compiled_patterns->clear();
    compiled_patterns->reserve(patterns.size());

    for (const auto &pattern : patterns) {
        std::string tag;
        char subfield_code;
        std::string::size_type first_colon_pos = pattern.find(':');
        if (first_colon_pos == std::string::npos) {
            *err_msg = "missing colon!";
            return false;
        } else if (first_colon_pos == DirectoryEntry::TAG_LENGTH) {
            tag = pattern.substr(0, 3);
            subfield_code = CompiledPattern::NO_SUBFIELD_CODE;
        } else if (first_colon_pos == DirectoryEntry::TAG_LENGTH + 1) {
            tag = pattern.substr(0, 3);
            subfield_code = pattern[3];
        } else {
            *err_msg = "colon in wrong position (" + std::to_string(first_colon_pos) + ")! (Tag length must be "
                       + std::to_string(DirectoryEntry::TAG_LENGTH) + ".)";
            return false;
        }

        const std::string regex_string(pattern.substr(first_colon_pos + 1));
        RegexMatcher * const new_matcher(RegexMatcher::RegexMatcherFactory(regex_string, err_msg));
        if (new_matcher == nullptr) {
            *err_msg = "failed to compile regular expression: \"" + regex_string + "\"! (" + *err_msg +")";
            return false;
        }

        compiled_patterns->push_back(CompiledPattern(tag, subfield_code, std::move(*new_matcher)));
        delete new_matcher;
    }

    return true;
}


/** Returns true if we have at least one match. */
bool Matched(const MarcUtil::Record &record, const std::vector<DirectoryEntry> &dir_entries,
             const std::vector<std::string> &fields, const std::vector<CompiledPattern> &compiled_patterns,
             std::vector<size_t> * const matched_field_indices)
{
    matched_field_indices->clear();

    bool matched_at_least_one(false);
    for (const auto &compiled_pattern : compiled_patterns) {
        ssize_t index(record.getFieldIndex(compiled_pattern.getTag()));
        if (index == -1)
            continue;

        for (/* Intentionally empty! */;
             static_cast<size_t>(index) < fields.size() and dir_entries[index].getTag() == compiled_pattern.getTag();
             ++index)
        {
            if (compiled_pattern.hasSubfieldCode()) {
                const Subfields subfields(fields[index]);
                const auto begin_end(subfields.getIterators(compiled_pattern.getSubfieldCode()));
                for (auto subfield_code_and_value(begin_end.first); subfield_code_and_value != begin_end.second;
                     ++subfield_code_and_value)
                {
                    if (compiled_pattern.subfieldMatched(subfield_code_and_value->second)) {
                        matched_field_indices->emplace_back(index);
                        matched_at_least_one = true;
                    }
                }
            } else if (compiled_pattern.fieldMatched(fields[index])) {
                matched_field_indices->emplace_back(index);
                matched_at_least_one = true;
            }
        }
    }

    return matched_at_least_one;
}


namespace {


enum class OutputFormat { MARC_XML, MARC_21, SAME_AS_INPUT };
enum class OperationType { KEEP, DROP, REMOVE_FIELDS };


} // unnamed namespace


void Filter(const bool input_is_xml, const OutputFormat output_format, const std::vector<std::string> &patterns,
            const OperationType operation_type, File * const input, File * const output)
{
    MarcXmlWriter *xml_writer(nullptr);
    if ((output_format == OutputFormat::SAME_AS_INPUT and input_is_xml) or output_format == OutputFormat::MARC_XML)
        xml_writer = new MarcXmlWriter(output);

    std::vector<CompiledPattern> compiled_patterns;
    std::string err_msg;
    if (not CompilePatterns(patterns, &compiled_patterns, &err_msg))
        Error("Error while compiling patterns: " + err_msg);

    unsigned total_count(0), kept_count(0), modified_count(0);
    while (MarcUtil::Record record = input_is_xml ? MarcUtil::Record::XmlFactory(input)
                                                  : MarcUtil::Record::BinaryFactory(input))
    {
        record.setRecordWillBeWrittenAsXml(input_is_xml);
        ++total_count;

        const std::vector<DirectoryEntry> &dir_entries(record.getDirEntries());
        const std::vector<std::string> &fields(record.getFields());
        std::vector<size_t> matched_field_indices;
        if (Matched(record, dir_entries, fields, compiled_patterns, &matched_field_indices)) {
            if (operation_type == OperationType::KEEP) {
                xml_writer != nullptr ? record.write(xml_writer) : record.write(output);
                ++kept_count;
            } else if (operation_type == OperationType::REMOVE_FIELDS) {
                std::sort(matched_field_indices.begin(), matched_field_indices.end(), std::greater<size_t>());
                for (const auto field_index : matched_field_indices)
                    record.deleteField(field_index);
                xml_writer != nullptr ? record.write(xml_writer) : record.write(output);
                ++modified_count;
            }
        } else if (operation_type == OperationType::DROP or operation_type == OperationType::REMOVE_FIELDS) {
            xml_writer != nullptr ? record.write(xml_writer) : record.write(output);
            ++kept_count;
        }
    }

    if (not err_msg.empty())
        Error(err_msg);

    delete xml_writer;

    if (operation_type == OperationType::REMOVE_FIELDS)
        std::cerr << "Modified " << modified_count << " of " << total_count << " record(s).\n";
    else
        std::cerr << "Kept " << kept_count << " of " << total_count << " record(s).\n";
}


int main(int argc, char **argv) {
    ::progname = argv[0];

    if (argc < 5)
        Usage();
    
    OperationType operation_type;
    if (std::strcmp(argv[1], "--keep") == 0)
        operation_type = OperationType::KEEP;
    else if (std::strcmp(argv[1], "--drop") == 0)
        operation_type = OperationType::DROP;
    else if (std::strcmp(argv[1], "--remove-fields") == 0)
        operation_type = OperationType::REMOVE_FIELDS;
    else
        Error("expected --keep, --drop or --remove-field as the first argument!");

    bool input_is_xml(false), already_determined_input_format(false);
    if (std::strcmp("--input-format=marc-xml", argv[2]) == 0) {
        input_is_xml = true;
        --argc, ++argv;
        already_determined_input_format = true;
    } else if (std::strcmp("--input-format=marc-21", argv[2]) == 0) {
        --argc, ++argv;
        already_determined_input_format = true;
    }

    OutputFormat output_format(OutputFormat::SAME_AS_INPUT);
    if (StringUtil::StartsWith(argv[2], "--output-format=")) {
        if (std::strcmp(argv[2] + std::strlen("--output-format="), "marc-xml") == 0)
            output_format = OutputFormat::MARC_XML;
        else if (std::strcmp(argv[2] + std::strlen("--output-format="), "marc-21") == 0)
            output_format = OutputFormat::MARC_21;
        else
            Error("unknown output format \"" + std::string(argv[2] + 16)
                  + "\"!  Must be \"marc-xml\" or \"marc-21\".");
        ++argv, --argc;
    }

    const std::string input_filename(argv[2]);
    
    // Our input file is possibly a fifo, then avoid reading twice
    if (not already_determined_input_format) {
        const std::string media_type(MediaTypeUtil::GetFileMediaType(input_filename));
        if (unlikely(media_type.empty()))
            Error("can't determine media type of \"" + input_filename + "\"!");
        if (media_type != "application/xml" and media_type != "application/marc")
            Error("\"" + input_filename + "\" is neither XML nor MARC-21 data!");
        input_is_xml = (media_type == "application/xml");
     }

    std::unique_ptr<File> input(FileUtil::OpenInputFileOrDie(input_filename));
    std::unique_ptr<File> output(FileUtil::OpenOutputFileOrDie(argv[3]));

    std::vector<std::string> patterns;
    for (int arg_no(4); arg_no < argc; ++arg_no)
        patterns.emplace_back(argv[arg_no]);

    try {
        Filter(input_is_xml, output_format, patterns, operation_type, input.get(), output.get());
    } catch (const std::exception &x) {
        Error("caught exception: " + std::string(x.what()));
    }
}

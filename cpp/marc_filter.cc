/** \brief A MARC-21 filter utility that can remove records or fields based on patterns for MARC subfields.
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
    std::cerr << "usage: " << ::progname
              << " marc_input marc_output [[--input-format=(marc-xml|marc-21)] [--output-format=(marc-xml|marc-21)] op1 [op2 .. opN]\n"
              << "       where each operation must start with the operation type. Operation-type flags\n"
              << "       are --drop, --keep, --drop-biblio-level, --keep-biblio-level --remove-fields, --remove-subfields or\n"
              << "       --filter-chars.\n"
              << "       Arguments for --keep, --drop, --remove-field and --remove-subfields are field_or_subfieldspec1:regex1 "
              << "[field_or_subfieldspec2:regex2 .. field_or_subfieldspecN:regexN]\n"
              << "       where \"field_or_subfieldspec\" must either be a MARC tag or a MARC tag followed by a\n"
              << "       single-character subfield code and \"regex\" is a Perl-compatible regular expression.\n"
              << "       --drop-biblio-level and --keep-biblio-level arguments must be a single character.\n"
              << "       --filter-chars' arguments are subfield_spec1:subfield_spec2:...:subfield_specN  characters_to_delete\n"
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
bool CompilePatterns(const std::vector<std::string> &patterns,
                     std::vector<CompiledPattern *> * const compiled_patterns, std::string * const err_msg)
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

        compiled_patterns->push_back(new CompiledPattern(tag, subfield_code, std::move(*new_matcher)));
        delete new_matcher;
    }

    return true;
}


/** Returns true if we have at least one match. */
bool Matched(const MarcUtil::Record &record, const std::vector<DirectoryEntry> &dir_entries,
             const std::vector<std::string> &fields, const std::vector<CompiledPattern *> &compiled_patterns,
             std::vector<size_t> * const matched_field_indices)
{
    matched_field_indices->clear();

    bool matched_at_least_one(false);
    for (const auto &compiled_pattern : compiled_patterns) {
        ssize_t index(record.getFieldIndex(compiled_pattern->getTag()));
        if (index == -1)
            continue;

        for (/* Intentionally empty! */;
             static_cast<size_t>(index) < fields.size() and dir_entries[index].getTag() == compiled_pattern->getTag();
             ++index)
        {
            if (compiled_pattern->hasSubfieldCode()) {
                const Subfields subfields(fields[index]);
                const auto begin_end(subfields.getIterators(compiled_pattern->getSubfieldCode()));
                for (auto subfield_code_and_value(begin_end.first); subfield_code_and_value != begin_end.second;
                     ++subfield_code_and_value)
                {
                    if (compiled_pattern->subfieldMatched(subfield_code_and_value->second)) {
                        matched_field_indices->emplace_back(index);
                        matched_at_least_one = true;
                    }
                }
            } else if (compiled_pattern->fieldMatched(fields[index])) {
                matched_field_indices->emplace_back(index);
                matched_at_least_one = true;
            }
        }
    }

    return matched_at_least_one;
}


bool MatchedSubfield(const MarcUtil::Record &record, const std::vector<DirectoryEntry> &dir_entries,
             const std::vector<std::string> &fields, const std::vector<CompiledPattern *> &compiled_patterns,
             std::vector<std::pair<size_t,char>> * const matched_field_indices_and_subfields)
{
    matched_field_indices_and_subfields->clear();

    bool matched_at_least_one(false);
    for (const auto &compiled_pattern : compiled_patterns) {
        ssize_t index(record.getFieldIndex(compiled_pattern->getTag()));
        if (index == -1)
            continue;

        for (/* Intentionally empty! */;
             static_cast<size_t>(index) < fields.size() and dir_entries[index].getTag() == compiled_pattern->getTag();
             ++index)
        {
            if (compiled_pattern->hasSubfieldCode()) {
                const Subfields subfields(fields[index]);
                const auto begin_end(subfields.getIterators(compiled_pattern->getSubfieldCode()));
                for (auto subfield_code_and_value(begin_end.first); subfield_code_and_value != begin_end.second;
                     ++subfield_code_and_value)
                {
                    if (compiled_pattern->subfieldMatched(subfield_code_and_value->second)) {
                        matched_field_indices_and_subfields->emplace_back(index, subfield_code_and_value->first);
                        matched_at_least_one = true;
                    }
                }
            } 
        }
    }

    return matched_at_least_one;
}



namespace {


enum class OutputFormat { MARC_XML, MARC_21, SAME_AS_INPUT };
enum class FilterType { KEEP, DROP, KEEP_BIBLIOGRAPHIC_LEVEL, DROP_BIBLIOGRAPHIC_LEVEL, REMOVE_FIELDS, REMOVE_SUBFIELDS, FILTER_CHARS };


} // unnamed namespace


class FilterDescriptor {
private:
    FilterType filter_type_;
    std::vector<CompiledPattern *> compiled_patterns_;
    std::vector<std::string> subfield_specs_;
    std::string chars_to_delete_;
    char biblio_level_;
public:
    inline FilterType getFilterType() const { return filter_type_; }
    inline char getBiblioLevel() const { return biblio_level_; }

    /** \note Only call this if the filter type is not FILTER_CHARS! */
    inline const std::vector<CompiledPattern *> getCompiledPatterns() const { return compiled_patterns_; }

    /** \note Only call this if the filter type is FILTER_CHARS! */
    inline const std::vector<std::string> &getSubfieldSpecs() const { return subfield_specs_; }

    /** \note Only call this if the filter type is FILTER_CHARS! */
    inline const std::string &getCharsToDelete() const { return chars_to_delete_; }

    inline static FilterDescriptor MakeDropFilter(const std::vector<CompiledPattern *> &compiled_patterns) {
        return FilterDescriptor(FilterType::DROP, compiled_patterns);
    }
    inline static FilterDescriptor MakeKeepFilter(const std::vector<CompiledPattern *> &compiled_patterns) {
        return FilterDescriptor(FilterType::KEEP, compiled_patterns);
    }
    inline static FilterDescriptor MakeDropBiblioLevelFilter(const char biblio_level) {
        return FilterDescriptor(FilterType::DROP_BIBLIOGRAPHIC_LEVEL, biblio_level);
    }
    inline static FilterDescriptor MakeKeepBiblioLevelFilter(const char biblio_level) {
        return FilterDescriptor(FilterType::KEEP_BIBLIOGRAPHIC_LEVEL, biblio_level);
    }
    inline static FilterDescriptor MakeRemoveFieldsFilter(const std::vector<CompiledPattern *> &compiled_patterns) {
        return FilterDescriptor(FilterType::REMOVE_FIELDS, compiled_patterns);
    }
    inline static FilterDescriptor MakeRemoveSubfieldsFilter(const std::vector<CompiledPattern *> &compiled_patterns) {
        return FilterDescriptor(FilterType::REMOVE_SUBFIELDS, compiled_patterns);
    }

    inline static FilterDescriptor MakeFilterCharsFilter(const std::vector<std::string> &subfield_specs,
                                                         const std::string &chars_to_delete) {
        return FilterDescriptor(subfield_specs, chars_to_delete);
    }
private:
    FilterDescriptor(const FilterType filter_type, const std::vector<CompiledPattern *> &compiled_patterns)
        : filter_type_(filter_type), compiled_patterns_(compiled_patterns) { }
    FilterDescriptor(const std::vector<std::string> &subfield_specs, const std::string &chars_to_delete)
        : filter_type_(FilterType::FILTER_CHARS), subfield_specs_(subfield_specs),
          chars_to_delete_(chars_to_delete) { }
    FilterDescriptor(const FilterType filter_type, const char biblio_level)
        : filter_type_(filter_type), biblio_level_(biblio_level) { }
};


std::string GetSubfieldCodes(const std::string &tag, const std::vector<std::string> &subfield_specs) {
    std::string subfield_codes;

    for (const auto &subfield_spec : subfield_specs) {
        if (subfield_spec.substr(0, DirectoryEntry::TAG_LENGTH) == tag)
            subfield_codes += subfield_spec[DirectoryEntry::TAG_LENGTH];
    }

    return subfield_codes;
}


/** \brief Removes any of the characters in "chars_to_delete" from all of the subfields specified by "subfield_specs".
 *  \return True if at least one subfield has been modofied, else false.
 */
bool FilterCharacters(const std::vector<std::string> &subfield_specs, const std::string &chars_to_delete,
                      MarcUtil::Record * const record)
{
    const std::vector<DirectoryEntry> &dir_entries(record->getDirEntries());
    const std::vector<std::string> &fields(record->getFields());

        bool modified_at_least_one_field(false);
    for (std::vector<DirectoryEntry>::const_iterator dir_entry(dir_entries.cbegin());
         dir_entry != dir_entries.cend(); ++dir_entry)
    {
        const std::string subfield_codes(GetSubfieldCodes(dir_entry->getTag(), subfield_specs));
        if (subfield_codes.empty())
            continue;

        bool modified_at_least_one_subfield(false);
        const auto field_index(dir_entry - dir_entries.cbegin());
        Subfields subfields(fields[field_index]);
        for (const auto subfield_code : subfield_codes) {
            const auto begin_end(subfields.getIterators(subfield_code));
            for (auto subfield(begin_end.first); subfield != begin_end.second; ++subfield) {
                const auto old_length(subfield->second.length());
                StringUtil::RemoveChars(chars_to_delete, &(subfield->second));
                if (subfield->second.length() != old_length)
                    modified_at_least_one_subfield = true;
            }
        }

        if (modified_at_least_one_subfield) {
            modified_at_least_one_field = true;
            record->replaceField(field_index, subfields.toString());
        }
    }

    return modified_at_least_one_field;
}


void Filter(const bool input_is_xml, const OutputFormat output_format, const std::vector<FilterDescriptor> &filters,
            File * const input, File * const output)
{
    MarcXmlWriter *xml_writer(nullptr);
    if ((output_format == OutputFormat::SAME_AS_INPUT and input_is_xml) or output_format == OutputFormat::MARC_XML)
        xml_writer = new MarcXmlWriter(output);

    unsigned total_count(0), deleted_count(0), modified_count(0);
    while (MarcUtil::Record record = input_is_xml ? MarcUtil::Record::XmlFactory(input)
                                                  : MarcUtil::Record::BinaryFactory(input))
    {
        record.setRecordWillBeWrittenAsXml(input_is_xml);
        ++total_count;
        const std::vector<DirectoryEntry> &dir_entries(record.getDirEntries());
        const std::vector<std::string> &fields(record.getFields());
        bool deleted_record(false), modified_record(false);
        for (const auto &filter : filters) {
            if (filter.getFilterType() == FilterType::FILTER_CHARS) {
                if (FilterCharacters(filter.getSubfieldSpecs(), filter.getCharsToDelete(), &record))
                    modified_record = true;
            } else if (filter.getFilterType() == FilterType::DROP_BIBLIOGRAPHIC_LEVEL) {
                if (record.getLeader().getBibliographicLevel() == filter.getBiblioLevel()) {
                    deleted_record = true;
                    break;
                }
            } else if (filter.getFilterType() == FilterType::KEEP_BIBLIOGRAPHIC_LEVEL) {
                if (record.getLeader().getBibliographicLevel() != filter.getBiblioLevel()) {
                    deleted_record = true;
                    break;
                }
            } else if (filter.getFilterType() == FilterType::REMOVE_SUBFIELDS) {
               std::vector<std::pair<size_t, char>> matched_field_indices_and_subfields;
               if (MatchedSubfield(record, dir_entries, fields, filter.getCompiledPatterns(), &matched_field_indices_and_subfields)) {
                   std::sort(matched_field_indices_and_subfields.begin(), matched_field_indices_and_subfields.end());
                   for (const auto field_index_and_subfield : matched_field_indices_and_subfields) 
                        record.deleteSubfield(field_index_and_subfield.first, field_index_and_subfield.second);
                   modified_record = true;
                   break;
               }
           } else {
                std::vector<size_t> matched_field_indices;
                if (Matched(record, dir_entries, fields, filter.getCompiledPatterns(), &matched_field_indices)) {
                    if (filter.getFilterType() == FilterType::DROP) {
                        deleted_record = true;
                        break;
                    } else if (filter.getFilterType() == FilterType::REMOVE_FIELDS) {
                        std::sort(matched_field_indices.begin(), matched_field_indices.end(), std::greater<size_t>());
                        for (const auto field_index : matched_field_indices)
                            record.deleteField(field_index);
                        modified_record = true;
                    }
                } else if (filter.getFilterType() == FilterType::KEEP) {
                    deleted_record = true;
                    break;
                }
            }
        }

        if (deleted_record)
            ++deleted_count;
        else {
            if (modified_record)
                ++modified_count;
            xml_writer != nullptr ? record.write(xml_writer) : record.write(output);
        }
    }

    delete xml_writer;

    std::cerr << "Processed a total of " << total_count << " record(s).\n";
    std::cerr << "Kept " << (total_count - deleted_count) << " record(s).\n";
    std::cerr << "Modified " << modified_count << " record(s).\n";
    std::cerr << "Deleted " << deleted_count << " record(s).\n";
}


std::vector<CompiledPattern *> CollectAndCompilePatterns(char ***argvp) {
    const std::string operation_type(**argvp);
    ++*argvp;

    std::vector<std::string> specs_and_pattern;
    while (**argvp != nullptr and not StringUtil::StartsWith(**argvp, "--")) {
        specs_and_pattern.emplace_back(**argvp);
        ++*argvp;
    }

    if (specs_and_pattern.empty())
        Error("expected at least one field or subfield specification after \"" + operation_type + "\"!");

    std::vector<CompiledPattern *> compiled_patterns;
    std::string err_msg;
    if (not CompilePatterns(specs_and_pattern, &compiled_patterns, &err_msg))
        Error("bad field specification and or regular expression (" + err_msg + ")!");

    return compiled_patterns;
}


// Sanity check.
bool ArePlausibleSubfieldSpecs(const std::vector<std::string> &subfield_specs) {
    if (subfield_specs.empty())
        return false;

    for (const auto &subfield_spec : subfield_specs) {
        if (subfield_spec.length() != (DirectoryEntry::TAG_LENGTH + 1))
            return false;
    }

    return true;
}


char GetBiblioLevelArgument(char ***argvp) {
    ++*argvp;
    if (*argvp == nullptr)
        Error("missing bibliographic level after --drop-biblio-level or --keep-biblio-level flag!");
    const std::string bibliographic_level_candidate(**argvp);
    ++*argvp;

    if (bibliographic_level_candidate.length() != 1)
        Error("bad bibliographic level \"" + bibliographic_level_candidate + "\"!");
    return bibliographic_level_candidate[0];
}


void ProcessFilterArgs(char **argv, std::vector<FilterDescriptor> * const filters) {
    while (*argv != nullptr) {
        std::vector<CompiledPattern *> compiled_patterns;
        if (std::strcmp(*argv, "--drop") == 0)
            filters->emplace_back(FilterDescriptor::MakeDropFilter(CollectAndCompilePatterns(&argv)));
        else if (std::strcmp(*argv, "--keep") == 0)
            filters->emplace_back(FilterDescriptor::MakeKeepFilter(CollectAndCompilePatterns(&argv)));
        else if (std::strcmp(*argv, "--drop-biblio-level") == 0)
            filters->emplace_back(FilterDescriptor::MakeDropBiblioLevelFilter(GetBiblioLevelArgument(&argv)));
        else if (std::strcmp(*argv, "--keep-biblio-level") == 0)
            filters->emplace_back(FilterDescriptor::MakeKeepBiblioLevelFilter(GetBiblioLevelArgument(&argv)));
        else if (std::strcmp(*argv, "--remove-fields") == 0)
            filters->emplace_back(FilterDescriptor::MakeRemoveFieldsFilter(CollectAndCompilePatterns(&argv)));
        else if (std::strcmp(*argv, "--remove-subfields") == 0)
            filters->emplace_back(FilterDescriptor::MakeRemoveSubfieldsFilter(CollectAndCompilePatterns(&argv)));
        else if (std::strcmp(*argv, "--filter-chars") == 0) {
            ++argv;
            std::vector<std::string> subfield_specs;
            StringUtil::Split(*argv, ':', &subfield_specs);
            if (not ArePlausibleSubfieldSpecs(subfield_specs))
                Error("bad subfield specifications \"" + std::string(*argv) + "\"!");
            ++argv;
            if (argv == nullptr or StringUtil::StartsWith(*argv, "--"))
                Error("missing of bad \"characters_to_delete\" argument to \"--filter-chars\"!");
            filters->emplace_back(FilterDescriptor::MakeFilterCharsFilter(subfield_specs, *argv++));
        } else
            Error("unknown operation type \"" + std::string(*argv) + "\"!");
    }
}


int main(int argc, char **argv) {
    ::progname = argv[0];
    ++argv;

    if (argc < 5)
        Usage();

    const std::string input_filename(*argv++);
    std::unique_ptr<File> input(FileUtil::OpenInputFileOrDie(input_filename));
    std::unique_ptr<File> output(FileUtil::OpenOutputFileOrDie(*argv++));

    bool input_is_xml(false), already_determined_input_format(false);
    if (std::strcmp("--input-format=marc-xml", *argv) == 0) {
        input_is_xml = true;
        ++argv;
        already_determined_input_format = true;
    } else if (std::strcmp("--input-format=marc-21", *argv) == 0) {
        ++argv;
        already_determined_input_format = true;
    }

    OutputFormat output_format(OutputFormat::SAME_AS_INPUT);
    if (std::strcmp("--output-format=marc-xml", *argv) == 0) {
        output_format = OutputFormat::MARC_XML;
        ++argv;
    } else if (std::strcmp("--output-format=marc-21", *argv) == 0) {
        output_format = OutputFormat::MARC_21;
        ++argv;
    }

    if (not already_determined_input_format) {
        const std::string media_type(MediaTypeUtil::GetFileMediaType(input_filename));
        if (unlikely(media_type.empty()))
            Error("can't determine media type of \"" + input_filename + "\"!");
        if (media_type != "application/xml" and media_type != "application/marc")
            Error("\"" + input_filename + "\" is neither XML nor MARC-21 data!");
        input_is_xml = (media_type == "application/xml");
    }

    try {
        std::vector<FilterDescriptor> filters;
        ProcessFilterArgs(argv, &filters);

        Filter(input_is_xml, output_format, filters, input.get(), output.get());
    } catch (const std::exception &x) {
        Error("caught exception: " + std::string(x.what()));
    }
}

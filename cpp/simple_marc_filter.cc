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
#include <fstream>
#include <iostream>
#include <memory>
#include <unordered_set>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <getopt.h>
#include "DirectoryEntry.h"
#include "FileUtil.h"
#include "Leader.h"
#include "MarcUtil.h"
#include "MarcXmlWriter.h"
#include "RegexMatcher.h"
#include "StringUtil.h"
#include "Subfields.h"
#include "util.h"


void Usage() {
    std::cerr << "usage: " << progname << " (--drop|--keep) marc_input marc_output subfieldspec1:regex1 "
              << "[subfieldspec2:regex2 .. subfieldspecN:regexN]\n"
              << "       where \"subfieldspec\" must be a MARC tag followed by a single-character subfield code\n"
              << "       and \"regex\" is a Perl-compatible regular expression.\n\n";

    std::exit(EXIT_FAILURE);
}


class CompiledPattern {
    std::string tag_;
    char subfield_code_;
    RegexMatcher matcher_;
public:
    CompiledPattern(const std::string &tag, const char subfield_code,  const RegexMatcher &matcher)
        : tag_(tag), subfield_code_(subfield_code), matcher_(matcher) {}
    const std::string &getTag() const { return tag_; }
    char getSubfieldCode() const { return subfield_code_; }
    bool subfieldMatched(const std::string &subfield_contents) const;
};


bool CompiledPattern::subfieldMatched(const std::string &subfield_contents) const {
    std::string err_msg;
    const bool retval = matcher_.matched(subfield_contents, &err_msg);
    if (not retval and not err_msg.empty())
        Error("Unexpected error while trying to match a subfield in CompiledPattern::subfieldMatched(): " + err_msg);

    return retval;
}


// Expects "patterns" to contain strings that look like TTTS:REGEX where TTT are 3 characters specifying a field tah
// S is a subfield code and REGEX is a PCRE-style regex supporting UTF8 that should match subfield contents.
bool CompilePatterns(const std::vector<std::string> &patterns, std::vector<CompiledPattern> * const compiled_patterns,
                     std::string * const err_msg)
{
    compiled_patterns->clear();
    compiled_patterns->reserve(patterns.size());

    for (const auto &pattern : patterns) {
        std::string::size_type first_colon_pos = pattern.find(':');
        if (first_colon_pos == std::string::npos) {
            *err_msg = "missing colon!";
            return false;
        } else if (first_colon_pos != DirectoryEntry::TAG_LENGTH + 1) {
            *err_msg = "colon in wrong position! (Tag length must be "
                       + std::to_string(DirectoryEntry::TAG_LENGTH) + ".)";
            return false;
        }

        const std::string tag(pattern.substr(0, 3));
        const char subfield_code(pattern[3]);
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
             const std::vector<std::string> &fields, const std::vector<CompiledPattern> &compiled_patterns)
{
    for (const auto &compiled_pattern : compiled_patterns) {
        ssize_t index(record.getFieldIndex(compiled_pattern.getTag()));
        if (index == -1)
            continue;

        for (/* Intentionally empty! */;
             static_cast<size_t>(index) < fields.size() and dir_entries[index].getTag() == compiled_pattern.getTag();
             ++index)
        {
            const Subfields subfields(fields[index]);
            const auto begin_end(subfields.getIterators(compiled_pattern.getSubfieldCode()));
            for (auto subfield_code_and_value(begin_end.first); subfield_code_and_value != begin_end.second;
                 ++subfield_code_and_value)
            {
                if (compiled_pattern.subfieldMatched(subfield_code_and_value->second))
                    return true;
            }
        }
    }

    return false;
}


void Filter(const std::vector<std::string> &patterns, const bool keep, File * const input, File * const output) {
    MarcXmlWriter xml_writer(output);

    std::vector<CompiledPattern> compiled_patterns;
    std::string err_msg;
    if (not CompilePatterns(patterns, &compiled_patterns, &err_msg))
        Error("Error while compiling patterns: " + err_msg);

    unsigned total_count(0), kept_count(0);
    while (MarcUtil::Record record = MarcUtil::Record::XmlFactory(input)) {
        record.setRecordWillBeWrittenAsXml(true);
        ++total_count;

        const std::vector<DirectoryEntry> &dir_entries(record.getDirEntries());
        const std::vector<std::string> &fields(record.getFields());
        if (Matched(record, dir_entries, fields, compiled_patterns)) {
            if (keep) {
                record.write(&xml_writer);
                ++kept_count;
            }
        } else if (not keep) {
            record.write(&xml_writer);
            ++kept_count;
        }
    }

    if (not err_msg.empty())
        Error(err_msg);

    std::cerr << "Kept " << kept_count << " of " << total_count << " record(s).\n";
}


int main(int argc, char **argv) {
    ::progname = argv[0];

    if (argc < 5)
        Usage();

    bool keep;
    if (std::strcmp(argv[1], "--keep") == 0)
        keep = true;
    else if (std::strcmp(argv[1], "--drop") == 0)
        keep = false;
    else
        Error("expected --keep or --drop as the first argument!");

    std::unique_ptr<File> input(FileUtil::OpenInputFileOrDie(argv[2]));
    std::unique_ptr<File> output(FileUtil::OpenOutputFileOrDie(argv[3]));

    std::vector<std::string> patterns;
    for (int arg_no(4); arg_no < argc; ++arg_no)
        patterns.emplace_back(argv[arg_no]);

    try {
        Filter(patterns, keep, input.get(), output.get());
    } catch (const std::exception &x) {
        Error("caught exception: " + std::string(x.what()));
    }
}

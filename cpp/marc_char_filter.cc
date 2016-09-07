/** \brief A MARC-21 filter utility that can remove characters from a set of subfields.
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
    std::cerr << "usage: " << progname << " marc_input marc_output subfield_spec1:subfield_spec2:...:subfield_specN "
              << " characters_to_delete\n"
              << "       where \"subfieldspec\" must be a MARC tag followed by a single-character\n"
              << "       subfield code and \"characters_to_delete\" is list of characters that will be removed\n"
              << "       from the contents of the specified subfields.\n\n";

    std::exit(EXIT_FAILURE);
}


std::string GetSubfieldCodes(const std::string &tag, const std::vector<std::string> &subfield_specs) {
    std::string subfield_codes;
    
    for (const auto &subfield_spec : subfield_specs) {
        if (subfield_spec.substr(0, DirectoryEntry::TAG_LENGTH) == tag)
            subfield_codes += subfield_spec[DirectoryEntry::TAG_LENGTH];
    }

    return subfield_codes;
}


void Filter(File * const input, File * const output, const std::vector<std::string> &subfield_specs,
            const std::string &filter_chars)
{
    MarcXmlWriter xml_writer(output);

    unsigned total_count(0), modified_record_count(0), modified_field_count(0);
    while (MarcUtil::Record record = MarcUtil::Record::XmlFactory(input)) {
        ++total_count;
        record.setRecordWillBeWrittenAsXml(true);

        const std::vector<DirectoryEntry> &dir_entries(record.getDirEntries());
        const std::vector<std::string> &fields(record.getFields());

        for (std::vector<DirectoryEntry>::const_iterator dir_entry(dir_entries.cbegin());
             dir_entry != dir_entries.cend(); ++dir_entry)
        {
            const std::string subfield_codes(GetSubfieldCodes(dir_entry->getTag(), subfield_specs));
            if (subfield_codes.empty()) {
                record.write(&xml_writer);
                continue;
            }

            bool modified_at_least_one(false);
            const auto field_index(dir_entry - dir_entries.cbegin());
            Subfields subfields(fields[field_index]);
            for (const auto subfield_code : subfield_codes) {
                const auto begin_end(subfields.getIterators(subfield_code));
                for (auto subfield(begin_end.first); subfield != begin_end.second; ++subfield) {
                    const auto old_length(subfield->second.length());
                    StringUtil::RemoveChars(filter_chars, &(subfield->second));
                    if (subfield->second.length() != old_length) {
                        ++modified_field_count;
                        modified_at_least_one = true;
                    }
                }
            }

            if (modified_at_least_one) {
                record.replaceField(field_index, subfields.toString());
                ++modified_record_count;
            }

            record.write(&xml_writer);
        }
    }
    
    std::cerr << "Modified " << modified_record_count << " (" << modified_field_count << " fields) of "
              << total_count << " record(s).\n";
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


int main(int argc, char **argv) {
    ::progname = argv[0];

    if (argc != 5)
        Usage();

    std::unique_ptr<File> input(FileUtil::OpenInputFileOrDie(argv[1]));
    std::unique_ptr<File> output(FileUtil::OpenOutputFileOrDie(argv[2]));

    std::vector<std::string> subfield_specs;
    StringUtil::Split(argv[3], ':', &subfield_specs);
    if (not ArePlausibleSubfieldSpecs(subfield_specs))
        Error("bad subfield specifications!");

    const std::string filter_chars(argv[4]);
    if (filter_chars.empty())
        Error("missing characters to be filtered!");

    try {
        Filter(input.get(), output.get(), subfield_specs, filter_chars);
    } catch (const std::exception &x) {
        Error("caught exception: " + std::string(x.what()));
    }
}

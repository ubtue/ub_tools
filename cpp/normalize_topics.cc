/** \brief  A tool for normalizing the capitalization of keywords/topics
 *  \author Johannes Riedl
 *  \author Dr. Johannes Ruscheinski
 *
 *  \copyright 2017 Universitätsbibliothek Tübingen.  All rights reserved.
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
#include <string>
#include <vector>
#include <tuple>
#include <unordered_set>
#include <utility>
#include <cstdlib>
#include "Compiler.h"
#include "DirectoryEntry.h"
#include "Leader.h"
#include "MarcReader.h"
#include "MarcRecord.h"
#include "MarcWriter.h"
#include "MarcUtil.h"
#include "StringUtil.h"
#include "RegexMatcher.h"
#include "Subfields.h"
#include "util.h"


void Usage() {
    std::cerr << "Usage: " << ::progname << " marc_input marc_output\n";
    std::exit(EXIT_FAILURE);
}


const std::string FIELDS_TO_TRANSFORM("600adxz:610axyz:611axzdy:630adxyz:648adxyz:650adxyz:650adxyz:651adxyz:"
                                      "655adxyzx:689abctnp");


inline std::string GetTag(const std::string &tag_and_subfields_spec) {
    return tag_and_subfields_spec.substr(0, 3);
}


inline std::string GetSubfieldCodes(const std::string &tag_and_subfields_spec) {
    return tag_and_subfields_spec.substr(3);
}


bool ParseSpec(std::string spec_str, std::vector<std::string> * const field_specs,
               std::map<std::string, std::pair<std::string, std::string>> * const filter_specs = nullptr)
{
    std::vector<std::string> raw_field_specs;

    if (unlikely(StringUtil::Split(spec_str, ':', &raw_field_specs) == 0))
        Error("in ParseSpec: Need at least one field in \"" + spec_str + "\"!");

    if (filter_specs == nullptr) {
        *field_specs = raw_field_specs;
        return true;
    }

    // Iterate over all Field-specs and extract possible filters
    static RegexMatcher * const matcher(RegexMatcher::RegexMatcherFactory(
                                            "(\\d{1,3}[a-z]+)\\[(\\d{1,3}[a-z])=(.*)\\]"));

    for (auto field_spec : raw_field_specs) {
        if (matcher->matched(field_spec)) {
            filter_specs->emplace((*matcher)[1], std::make_pair((*matcher)[2], (*matcher)[3]));
            auto bracket = field_spec.find("[");
            field_spec = (bracket != std::string::npos) ? field_spec.erase(bracket, field_spec.length()) : field_spec;
        }
        field_specs->push_back(field_spec);
    }
    return true;
}


bool ProcessRecord(MarcRecord * const record, const std::vector<std::string> &tags_and_subfield_codes) {
    std::vector<std::string>::const_iterator field_spec;
    for (field_spec = tags_and_subfield_codes.begin(); field_spec != tags_and_subfield_codes.end(); ++field_spec) {

    }
}


void NormalizeFields(MarcReader * const marc_reader, MarcWriter * const marc_writer,
                     const std::vector<std::string> &tags_and_subfield_codes)
{
    unsigned count(0), modified_count(0);
    while (MarcRecord record = marc_reader->read()) {
        ++count;
        if (ProcessRecord(&record, tags_and_subfield_codes))
            ++modified_count;
        marc_writer->write(record);
    }

    std::cerr << "Processed " << count << " records of which " << modified_count << " were modified.\n";
}


int main(int argc, char **argv) {
    ::progname = argv[0];

    if (argc != 3)
        Usage();

    const std::string marc_input_filename(argv[1]);
    const std::string marc_output_filename(argv[2]);

    std::vector<std::string> tags_and_subfield_codes;
    std::map<std::string, std::pair<std::string, std::string>> filter_specs;

    if (unlikely(marc_input_filename == marc_output_filename))
        Error("Input file equals output file");

    try {
        std::unique_ptr<MarcReader> marc_reader(MarcReader::Factory(marc_input_filename));
        std::unique_ptr<MarcWriter> marc_writer(MarcWriter::Factory(marc_output_filename));
        if (not unlikely(ParseSpec(, &tags_and_subfield_codes, &filter_specs)))
            Error("Could not properly parse " + FIELDS_TO_TRANSFORM);

        NormalizeFields(marc_reader.get(), marc_writer.get(), tags_and_subfield_codes)

    } catch (const std::exception &x) {
        Error("caught exception: " + std::string(x.what()));
    }
}


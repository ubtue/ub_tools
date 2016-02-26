/** \file    create_author_synomym_map.cc
 *  \brief   Creates a SOLR-MARC syonym map for authors from norm data.
 *  \author  Dr. Johannes Ruscheinski
 */

/*
    Copyright (C) 2016, Library of the University of Tübingen

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU Affero General Public License as
    published by the Free Software Foundation, either version 3 of the
    License, or (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Affero General Public License for more details.

    You should have received a copy of the GNU Affero General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <iostream>
#include <vector>
#include <cstdlib>
#include "Compiler.h"
#include "MarcUtil.h"
#include "MediaTypeUtil.h"
#include "StringUtil.h"
#include "util.h"


void Usage() {
    std::cerr << "Usage: " << ::progname << " norm_data_marc_input synonym_map_output\n";
    std::exit(EXIT_FAILURE);
}


void RemoveCommasDuplicatesAndEmptyEntries(std::vector<std::string> * const vector) {
    std::vector<std::string> cleaned_up_vector;
    std::set<std::string> uniqe_entries;

    for (auto &entry : *vector) {
	StringUtil::RemoveChars(",", &entry);

        if (entry.empty())
            continue;

        bool isNewEntry = uniqe_entries.insert(entry).second;
        if (isNewEntry)
            cleaned_up_vector.emplace_back(std::move(entry));
    }

    vector->swap(cleaned_up_vector);
}


void ExtractSynonymsAndWriteSynonymMap(File * const marc_input, File * const synonym_output, const std::string &field_list) {
    std::set<std::string> synonyms;
    std::vector<std::string> fields;
    if (unlikely(StringUtil::Split(field_list, ':', &fields) < 2))
	Error("in ExtractSynonymsAndWriteSynonymMap: need at least two fields!");

    unsigned synomym_line_count(0), count(0);
    while (const MarcUtil::Record record = MarcUtil::Record::XmlFactory(marc_input)) {
        ++count;
	if (record.getFieldIndex(fields[0].substr(0, 3)) == -1)
	    continue;

	std::vector<std::string> subfield_values;
	record.extractSubfields(fields[0].substr(0, 3), fields[0].substr(3), &subfield_values);
        RemoveCommasDuplicatesAndEmptyEntries(&subfield_values);

	if (subfield_values.empty())
	    continue;

        std::sort(subfield_values.begin(), subfield_values.end());

	std::vector<std::string> alternatives;
	alternatives.emplace_back(StringUtil::Join(subfield_values, ' '));

	for (auto field(fields.cbegin() + 1); field != fields.cend(); ++field) {
	    record.extractSubfields(field->substr(0, 3), field->substr(3), &subfield_values);
            RemoveCommasDuplicatesAndEmptyEntries(&subfield_values);

	    if (subfield_values.empty())
                continue;

            std::sort(subfield_values.begin(), subfield_values.end());
            alternatives.emplace_back(StringUtil::Join(subfield_values, ' '));
	}

        RemoveCommasDuplicatesAndEmptyEntries(&alternatives);
        if (alternatives.size() <= 1)
            continue;

        std::string synonym(StringUtil::Join(alternatives, ','));
        if (not synonyms.insert(synonym).second)
            continue;

        (*synonym_output) << synonym << '\n';
	++synomym_line_count;
    }

    std::cout << "Created " << synomym_line_count << " lines in the synonym map while processing " << count
	      << " norm data records.\n";
}


std::unique_ptr<File> OpenInputFile(const std::string &filename) {
    std::string mode("r");
    mode += MediaTypeUtil::GetFileMediaType(filename) == "application/lz4" ? "u" : "m";
    std::unique_ptr<File> file(new File(filename, mode));
    if (file == nullptr)
        Error("can't open \"" + filename + "\" for reading!");

    return file;
}


int main(int argc, char **argv) {
    ::progname = argv[0];

    if (argc != 3)
	Usage();


    std::unique_ptr<File> marc_input(OpenInputFile(argv[1]));

    const std::string synonym_map_filename(argv[2]);
    File synonym_output(synonym_map_filename, "w");
    if (not synonym_output)
        Error("can't open \"" + synonym_map_filename + "\" for writing!");

    try {
	ExtractSynonymsAndWriteSynonymMap(marc_input.get(), &synonym_output, "100abcd:400abcd");
    } catch (const std::exception &x) {
	Error("caught exception: " + std::string(x.what()));
    }
}

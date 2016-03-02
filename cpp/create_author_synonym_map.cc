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
#include "Subfields.h"
#include "util.h"


void Usage() {
    std::cerr << "Usage: " << ::progname << " norm_data_marc_input synonym_map_output\n";
    std::exit(EXIT_FAILURE);
}


void RemoveCommasDuplicatesAndEmptyEntries(std::vector<std::string> * const vector) {
    std::vector<std::string> cleaned_up_vector;
    std::set<std::string> uniqe_entries;

    for (auto &entry : *vector) {
        std::cout << "- " << entry << "\n";
	StringUtil::RemoveChars(",", &entry);

        if (entry.empty())
            continue;

        const bool is_new_entry(uniqe_entries.emplace(entry).second);
        if (is_new_entry)
            cleaned_up_vector.emplace_back(std::move(entry));
    }

    for (auto entry : cleaned_up_vector) {
        std::cout << "+ " << entry << "\n";
    }

    vector->swap(cleaned_up_vector);
}


std::string ExtractNameFromSubfields(const std::string &field_contents, const std::string &subfield_codes) {
    const Subfields subfields(field_contents);
    std::vector<std::string> subfield_values;
    if (subfields.extractSubfields(subfield_codes, &subfield_values) == 0)
        return "";

    std::sort(subfield_values.begin(), subfield_values.end());
    return StringUtil::Join(subfield_values, ' ');
}


void ExtractSynonymsAndWriteSynonymMap(File * const marc_input, File * const synonym_output, const std::string &field_list) {
    std::set<std::string> synonyms;
    std::vector<std::string> tags_and_subfield_codes;
    if (unlikely(StringUtil::Split(field_list, ':', &tags_and_subfield_codes) < 2))
	Error("in ExtractSynonymsAndWriteSynonymMap: need at least two fields!");

    unsigned synomym_line_count(0), count(0);
    while (const MarcUtil::Record record = MarcUtil::Record::XmlFactory(marc_input)) {
        ++count;

	const int primary_name_field_index(record.getFieldIndex(tags_and_subfield_codes[0].substr(0, 3)));
	if (primary_name_field_index == -1)
	    continue;

	const std::vector<std::string> &fields(record.getFields());
	const std::string primary_field_subfield_codes(tags_and_subfield_codes[primary_name_field_index].substr(3));
	const std::string primary_name(ExtractNameFromSubfields(fields[primary_name_field_index], primary_field_subfield_codes));
	if (unlikely(primary_name.empty()))
	    continue;

	std::vector<std::string> alternatives;
	alternatives.emplace_back(primary_name);

	const std::vector<DirectoryEntry> &dir_entries(record.getDirEntries());

	for (unsigned i(1); i < tags_and_subfield_codes.size(); ++i) {
	    const std::string tag(tags_and_subfield_codes[i].substr(0, 3));
	    const std::string secondary_field_subfield_codes(tags_and_subfield_codes[i].substr(3));
	    int secondary_name_field_index(record.getFieldIndex(tag));
	    while (secondary_name_field_index != -1 and static_cast<size_t>(secondary_name_field_index) < dir_entries.size()
		   and dir_entries[secondary_name_field_index].getTag() == tag)
	    {
		const std::string secondary_name(ExtractNameFromSubfields(fields[secondary_name_field_index],
									  secondary_field_subfield_codes));
		if (not secondary_name.empty())
		    alternatives.emplace_back(secondary_name);
		++secondary_name_field_index;
	    }
	}

        RemoveCommasDuplicatesAndEmptyEntries(&alternatives);
        if (alternatives.size() <= 1)
            continue;

        std::string synonym(StringUtil::Join(alternatives, ','));

        const bool is_new_entry(synonyms.emplace(synonym).second);
        if (not is_new_entry)
            continue;

        (*synonym_output) << synonym << '\n';
	++synomym_line_count;

        if (synomym_line_count == 5)
            break;
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

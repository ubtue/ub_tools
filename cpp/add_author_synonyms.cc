/** \file    add_author_synonyms.cc
 *  \brief   Adds author synonyms to each record.
 *  \author  Oliver Obenland
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


static unsigned modified_count(0);
static unsigned record_count(0);


void Usage() {
    std::cerr << "Usage: " << ::progname << " master_marc_input norm_data_marc_input marc_output\n";
    std::exit(EXIT_FAILURE);
}


void RemoveCommasDuplicatesAndEmptyEntries(std::vector<std::string> * const vector) {
    std::vector<std::string> cleaned_up_vector;
    std::set<std::string> uniqe_entries;

    for (auto &entry : *vector) {
        StringUtil::RemoveChars(",", &entry);

        if (entry.empty())
            continue;

        const bool is_new_entry(uniqe_entries.emplace(entry).second);
        if (is_new_entry)
            cleaned_up_vector.emplace_back(std::move(entry));
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


void ExtractSynonyms(File * const marc_input, std::map<std::string, std::string> &author_to_synonyms_map,
                     const std::string &field_list)
{
    std::set<std::string> synonyms;
    std::vector<std::string> tags_and_subfield_codes;
    if (unlikely(StringUtil::Split(field_list, ':', &tags_and_subfield_codes) < 2))
        Error("in ExtractSynonymsAndWriteSynonymMap: need at least two fields!");
    unsigned count(0);
    while (const MarcUtil::Record record = MarcUtil::Record::XmlFactory(marc_input)) {
        ++count;

        const int primary_name_field_index(record.getFieldIndex(tags_and_subfield_codes[0].substr(0, 3)));
        if (primary_name_field_index == -1)
            continue;

        const std::vector<std::string> &fields(record.getFields());
        const std::string primary_name(ExtractNameFromSubfields(fields[primary_name_field_index],
								tags_and_subfield_codes[0].substr(3)));
        if (unlikely(primary_name.empty()))
            continue;

        std::vector<std::string> alternatives;
        alternatives.emplace_back(primary_name);
        if (author_to_synonyms_map.find(primary_name) != author_to_synonyms_map.end())
            continue;

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

        alternatives.erase(alternatives.begin());
        author_to_synonyms_map.emplace(primary_name, StringUtil::Join(alternatives, ','));
    }

    std::cout << "Found synonys for " << author_to_synonyms_map.size() << " authors while processing " << count
              << " norm data records.\n";
}


void ProcessRecord(MarcUtil::Record * const record, const std::map<std::string, std::string> &author_to_synonyms_map,
                   const std::string &primary_author_field)
{
    record->setRecordWillBeWrittenAsXml(true);
    const std::vector<DirectoryEntry> &dir_entries(record->getDirEntries());
    if (dir_entries.at(0).getTag() != "001")
        Error("First field of record is not \"001\"!");

    const int primary_name_field_index(record->getFieldIndex(primary_author_field.substr(0, 3)));
    if (primary_name_field_index == -1)
        return;

    const std::vector<std::string> &fields(record->getFields());
    const std::string primary_name(ExtractNameFromSubfields(fields[primary_name_field_index], primary_author_field.substr(3)));
    if (unlikely(primary_name.empty()))
        return;

    const auto synonyms_iterator = author_to_synonyms_map.find(primary_name);
    if (synonyms_iterator == author_to_synonyms_map.end())
        return;

    const std::string synonyms = synonyms_iterator->second;
    Subfields subfields(/* indicator1 = */' ', /* indicator2 = */' ');
    subfields.addSubfield('a', synonyms);

    if (not record->insertField("101", subfields.toString())) {
        Warning("Not enough room to add a 101 field! (Control number: " + fields[0] + ")");
        return;
    }
    ++modified_count;
}


void AddAuthorSynonyms(File * const marc_input, File * marc_output,
		       const std::map<std::string, std::string> &author_to_synonyms_map,
                       const std::string &primary_author_field)
{
    XmlWriter xml_writer(marc_output);
    xml_writer.openTag("marc:collection",
                       { std::make_pair("xmlns:marc", "http://www.loc.gov/MARC21/slim"),
                         std::make_pair("xmlns:xsi", "http://www.w3.org/2001/XMLSchema-instance"),
                         std::make_pair("xsi:schemaLocation", "http://www.loc.gov/standards/marcxml/schema/MARC21slim.xsd")});

    while (MarcUtil::Record record = MarcUtil::Record::XmlFactory(marc_input)) {
        ProcessRecord(&record, author_to_synonyms_map, primary_author_field);
        record.write(&xml_writer);
        ++record_count;
    }

    xml_writer.closeTag();

    std::cerr << "Modified " << modified_count << " of " << record_count << " record(s).\n";
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
    progname = argv[0];

    if (argc != 4)
        Usage();

    const std::string marc_input_filename(argv[1]);
    std::unique_ptr<File> marc_input(OpenInputFile(marc_input_filename));

    const std::string norm_data_marc_input_filename(argv[2]);
    std::unique_ptr<File> norm_data_marc_input(OpenInputFile(norm_data_marc_input_filename));

    const std::string marc_output_filename(argv[3]);
    if (unlikely(marc_input_filename == marc_output_filename))
        Error("Master input file name equals output file name!");
    if (unlikely(norm_data_marc_input_filename == marc_output_filename))
        Error("Auxillary input file name equals output file name!");

    std::string output_mode("w");
    if (marc_input->isCompressingOrUncompressing())
        output_mode += 'c';
    File marc_output(marc_output_filename, output_mode);
    if (not marc_output)
        Error("can't open \"" + marc_output_filename + "\" for writing!");

    try {
        std::map<std::string, std::string> author_to_synonyms_map;
        ExtractSynonyms(norm_data_marc_input.get(), author_to_synonyms_map, "100abcd:400abcd");
        AddAuthorSynonyms(marc_input.get(), &marc_output, author_to_synonyms_map, "100abcd");
    } catch (const std::exception &x) {
        Error("caught exception: " + std::string(x.what()));
    }
}

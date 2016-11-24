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
#include <map>
#include <vector>
#include <cstdlib>
#include "Compiler.h"
#include "FileUtil.h"
#include "MarcReader.h"
#include "MarcRecord.h"
#include "MarcWriter.h"
#include "StringUtil.h"
#include "Subfields.h"
#include "util.h"
#include "XmlWriter.h"


static unsigned modified_count(0);
static unsigned record_count(0);


void Usage() {
    std::cerr << "Usage: " << ::progname << " master_marc_input norm_data_marc_input marc_output\n";
    std::exit(EXIT_FAILURE);
}


void RemoveCommasDuplicatesAndEmptyEntries(std::vector<std::string> * const vector) {
    std::vector<std::string> cleaned_up_vector;
    std::set<std::string> unique_entries;

    for (auto &entry : *vector) {
        StringUtil::RemoveChars(",", &entry);

        if (entry.empty())
            continue;

        const bool is_new_entry(unique_entries.emplace(entry).second);
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


void ExtractSynonyms(MarcReader * const marc_reader, std::map<std::string, std::string> &author_to_synonyms_map,
                     const std::string &field_list)
{
    std::set<std::string> synonyms;
    std::vector<std::string> tags_and_subfield_codes;
    if (unlikely(StringUtil::Split(field_list, ':', &tags_and_subfield_codes) < 2))
        Error("in ExtractSynonymsAndWriteSynonymMap: need at least two fields!");
    unsigned count(0);
    while (const MarcRecord record = marc_reader->read()) {
        ++count;

        const size_t primary_name_field_index(record.getFieldIndex(tags_and_subfield_codes[0].substr(0, 3)));
        if (primary_name_field_index == MarcRecord::FIELD_NOT_FOUND)
            continue;

        const std::string primary_name(ExtractNameFromSubfields(record.getFieldData(primary_name_field_index),
                                                                tags_and_subfield_codes[0].substr(3)));
        if (unlikely(primary_name.empty()))
            continue;

        std::vector<std::string> alternatives;
        alternatives.emplace_back(primary_name);
        if (author_to_synonyms_map.find(primary_name) != author_to_synonyms_map.end())
            continue;

        for (unsigned i(1); i < tags_and_subfield_codes.size(); ++i) {
            const std::string tag(tags_and_subfield_codes[i].substr(0, 3));
            const std::string secondary_field_subfield_codes(tags_and_subfield_codes[i].substr(3));
            for (size_t secondary_name_field_index(record.getFieldIndex(tag));
                 secondary_name_field_index < record.getNumberOfFields() and record.getTag(secondary_name_field_index) == tag;
                 ++secondary_name_field_index)
            {
                const std::string secondary_name(ExtractNameFromSubfields(record.getFieldData(secondary_name_field_index), secondary_field_subfield_codes));
                if (not secondary_name.empty())
                    alternatives.emplace_back(secondary_name);
            }
        }
        RemoveCommasDuplicatesAndEmptyEntries(&alternatives);
        if (alternatives.size() <= 1)
            continue;

        alternatives.erase(alternatives.begin());
        author_to_synonyms_map.emplace(primary_name, StringUtil::Join(alternatives, ','));
    }

    std::cout << "Found synonyms for " << author_to_synonyms_map.size() << " authors while processing " << count
              << " norm data records.\n";
}


const std::string SYNOMYM_FIELD("101"); // This must be an o/w unused field!


void ProcessRecord(MarcRecord * const record, const std::map<std::string, std::string> &author_to_synonyms_map,
                   const std::string &primary_author_field)
{
    if (unlikely(record->getFieldIndex(SYNOMYM_FIELD) != MarcRecord::FIELD_NOT_FOUND))
        Error("field " + SYNOMYM_FIELD + " is apparently already in use in at least some title records!");

    const size_t primary_name_field_index(record->getFieldIndex(primary_author_field.substr(0, 3)));
    if (primary_name_field_index == MarcRecord::FIELD_NOT_FOUND)
        return;

    const std::string primary_name(ExtractNameFromSubfields(record->getFieldData(primary_name_field_index),
                                                            primary_author_field.substr(3)));
    if (unlikely(primary_name.empty()))
        return;

    const auto synonyms_iterator = author_to_synonyms_map.find(primary_name);
    if (synonyms_iterator == author_to_synonyms_map.end())
        return;

    const std::string synonyms = synonyms_iterator->second;
    Subfields subfields(/* indicator1 = */' ', /* indicator2 = */' ');
    subfields.addSubfield('a', synonyms);

    if (not record->insertField(SYNOMYM_FIELD, subfields)) {
        Warning("Not enough room to add a " + SYNOMYM_FIELD + " field! (Control number: "
                + record->getControlNumber() + ")");
        return;
    }
    ++modified_count;
}


void AddAuthorSynonyms(MarcReader * const marc_reader, MarcWriter * marc_writer,
                       const std::map<std::string, std::string> &author_to_synonyms_map,
                       const std::string &primary_author_field)
{
    while (MarcRecord record = marc_reader->read()) {
        ProcessRecord(&record, author_to_synonyms_map, primary_author_field);
        marc_writer->write(record);
        ++record_count;
    }

    std::cerr << "Modified " << modified_count << " of " << record_count << " record(s).\n";
}


int main(int argc, char **argv) {
    ::progname = argv[0];

    if (argc != 4)
        Usage();

    const std::string marc_input_filename(argv[1]);
    const std::string authority_data_marc_input_filename(argv[2]);
    const std::string marc_output_filename(argv[3]);
    if (unlikely(marc_input_filename == marc_output_filename))
        Error("Title input file name equals title output file name!");
    if (unlikely(authority_data_marc_input_filename == marc_output_filename))
        Error("Authority data input file name equals MARC output file name!");

    std::unique_ptr<MarcReader> marc_reader(MarcReader::Factory(marc_input_filename, MarcReader::BINARY));
    std::unique_ptr<MarcReader> authority_reader(MarcReader::Factory(authority_data_marc_input_filename,
                                                                     MarcReader::BINARY));
    std::unique_ptr<MarcWriter> marc_writer(MarcWriter::Factory(marc_output_filename, MarcWriter::BINARY));

    try {
        std::map<std::string, std::string> author_to_synonyms_map;
        ExtractSynonyms(authority_reader.get(), author_to_synonyms_map, "100abcd:400abcd");
        AddAuthorSynonyms(marc_reader.get(), marc_writer.get(), author_to_synonyms_map, "100abcd");
    } catch (const std::exception &x) {
        Error("caught exception: " + std::string(x.what()));
    }
}

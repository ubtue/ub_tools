/** \file    add_synonyms.cc
 *  \brief   Generic version for augmenting title data with synonyms found
             in the authority data
 *  \author  Johannes Riedl
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

/*  We offer a list of tags and subfields where the primary data resides along
    with a list of tags and subfields where the synonym data is found and
    a list of unused fields in the title data where the synonyms can be stored 
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


std::string GetTag(const std::string &tag_and_subfields_spec) {
    return tag_and_subfields_spec.substr(0, 3);
}


std::string GetSubfieldCodes(const std::string &tag_and_subfields_spec) {
    return tag_and_subfields_spec.substr(3);
}


void ExtractSynonyms(MarcReader * const authority_reader,
                     const std::set<std::string> &primary_tags_and_subfield_codes,
                     const std::set<std::string> &synonym_tags_and_subfield_codes,
                     std::vector<std::map<std::string, std::string>> * const synonym_maps)
{
    while (const MarcRecord record = authority_reader->read()) {
        std::set<std::string>::const_iterator primary;
        std::set<std::string>::const_iterator synonym;
        unsigned int i(0);
        for (primary = primary_tags_and_subfield_codes.begin(), synonym = synonym_tags_and_subfield_codes.begin();  
            primary != primary_tags_and_subfield_codes.end();
            ++primary, ++synonym, ++i) 
        {
            // Fill maps with synonyms
            std::vector<std::string> primary_values; 
            std::vector<std::string> synonym_values;              

            if (record.extractSubfields(GetTag(*primary), GetSubfieldCodes(*primary), &primary_values) and 
                record.extractSubfields(GetTag(*synonym), GetSubfieldCodes(*synonym), &synonym_values))
                    (*synonym_maps)[i].emplace(StringUtil::Join(primary_values, ','),
                                               StringUtil::Join(synonym_values, ','));
        }
    }
}


inline std::string GetMapValueOrEmptyString(const std::map<std::string, std::string> &map,
                                            const std::string &searchterm)
{
    auto value(map.find(searchterm));
    return (value != map.cend()) ? value->second : "";
}


void ProcessRecord(MarcRecord * const record, const std::vector<std::map<std::string, std::string>> &synonym_maps,
                   const std::set<std::string> &primary_tags_and_subfield_codes,
                   const std::set<std::string> &output_tags_and_subfield_codes) 
{
    std::set<std::string>::const_iterator primary;
    std::set<std::string>::const_iterator output;
    unsigned int i(0);

    if (primary_tags_and_subfield_codes.size() == output_tags_and_subfield_codes.size()) {
        for (primary = primary_tags_and_subfield_codes.begin(), output = output_tags_and_subfield_codes.begin();
            primary != primary_tags_and_subfield_codes.end();
            ++primary, ++output, ++i) 
        {
            std::vector<std::string> primary_values;
            std::set<std::string> synonym_values;
            if (record->extractSubfields(GetTag(*primary), GetSubfieldCodes(*primary), &primary_values)) {
               for (const auto &searchterm : primary_values) {
                    // First case: Look up synonyms only in one category
                    if (i < synonym_maps.size()) {
                        const auto &synonym_map(synonym_maps[i]);
                        const auto &synonym(GetMapValueOrEmptyString(synonym_map, searchterm));
                        if (not synonym.empty())
                            synonym_values.insert(synonym);
                    }
                    
                    // Second case: Look up synonyms in all categories
                    else {
                        for (auto &sm : synonym_maps) {
                            const auto &synonym(GetMapValueOrEmptyString(sm, searchterm));
                            if (not synonym.empty())
                                synonym_values.insert(synonym);
                        }
                    }
                }

                if (synonym_values.empty())
                    continue;
                
                const std::string synonyms(StringUtil::Join(synonym_values, ','));
 
                // Insert synonyms
                // Abort if field is already populated
                std::string tag(GetTag(*output));
                if (record->getFieldIndex(tag) != MarcRecord::FIELD_NOT_FOUND)
                    Error("Field with tag " + tag + " is not empty for PPN " + record->getControlNumber() + '\n');
                std::string subfield_spec = GetSubfieldCodes(*output);
                if (subfield_spec.size() != 1)
                    Error("We currently only support a single subfield and thus specifying " + subfield_spec
                          + " as output subfield is not valid\n");
                Subfields subfields(' ', ' '); // <- indicators must be set explicitly although empty
                subfields.addSubfield(subfield_spec.at(0), synonyms);
                if (not(record->insertField(tag, subfields.toString())))
                    Warning("Could not insert field " + tag + " for PPN " + record->getControlNumber() + '\n');
                ++modified_count;
            }   
        }
    }
}   


void InsertSynonyms(MarcReader * const marc_reader, MarcWriter * const marc_writer,
                    const std::set<std::string> &primary_tags_and_subfield_codes,
                    const std::set<std::string> &output_tags_and_subfield_codes,
                    std::vector<std::map<std::string, std::string>> &synonym_maps) 
{
    while (MarcRecord record = marc_reader->read()) {
        ProcessRecord(&record, synonym_maps, primary_tags_and_subfield_codes, output_tags_and_subfield_codes);
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
        Error("Title data input file name equals output file name!");
    if (unlikely(authority_data_marc_input_filename == marc_output_filename))
        Error("Authority data input file name equals output file name!");


    std::unique_ptr<MarcReader> marc_reader(MarcReader::Factory(marc_input_filename, MarcReader::BINARY));
    std::unique_ptr<MarcReader> authority_reader(MarcReader::Factory(authority_data_marc_input_filename,
                                                                     MarcReader::BINARY));
    std::unique_ptr<MarcWriter> marc_writer(MarcWriter::Factory(marc_output_filename, MarcWriter::BINARY));

    try {
        // Determine possible mappings
        const std::string AUTHORITY_DATA_PRIMARY_SPEC("100abcd:110abcd:111abcd:130abcd:150abcd:151abcd");
        const std::string AUTHORITY_DATA_SYNONYM_SPEC("400abcd:410abcd:411abcd:430abcd:450abcd:451abcd");
        const std::string TITLE_DATA_PRIMARY_SPEC("600abcd:610abcd:611abcd:630abcd:650abcd:651abcd:689abcd");
        const std::string TITLE_DATA_UNUSED_FIELDS_FOR_SYNONYMS("180a:181a:182a:183a:184a:185a:186a");

        // Determine fields to handle
        std::set<std::string> primary_tags_and_subfield_codes;
        std::set<std::string> synonym_tags_and_subfield_codes;
        std::set<std::string> input_tags_and_subfield_codes;
        std::set<std::string> output_tags_and_subfield_codes;

        if (unlikely(StringUtil::Split(AUTHORITY_DATA_PRIMARY_SPEC, ":", &primary_tags_and_subfield_codes) < 1))
            Error("Need at least one primary field");

        if (unlikely(StringUtil::Split(AUTHORITY_DATA_SYNONYM_SPEC, ":", &synonym_tags_and_subfield_codes) < 1))
            Error("Need at least one synonym field");

        if (unlikely(StringUtil::Split(TITLE_DATA_PRIMARY_SPEC, ":", &input_tags_and_subfield_codes) < 1))
            Error("Need at least one input field");

        if (unlikely(StringUtil::Split(TITLE_DATA_UNUSED_FIELDS_FOR_SYNONYMS, ":", &output_tags_and_subfield_codes)
                     < 1))
            Error("Need at least one output field");

        unsigned num_of_authority_entries(primary_tags_and_subfield_codes.size());

        if (synonym_tags_and_subfield_codes.size() != num_of_authority_entries)
            Error("Number of authority primary specs must match number of synonym specs");
        if (input_tags_and_subfield_codes.size() != output_tags_and_subfield_codes.size())
            Error("Number of fields title entry specs must match number of output specs");
             
        std::vector<std::map<std::string, std::string>> synonym_maps(num_of_authority_entries,
                                                                     std::map<std::string, std::string>());
        
        // Extract the synonyms from authority data
        ExtractSynonyms(authority_reader.get(), primary_tags_and_subfield_codes, synonym_tags_and_subfield_codes,
                        &synonym_maps);

        // Iterate over the title data
        InsertSynonyms(marc_reader.get(), marc_writer.get(), input_tags_and_subfield_codes,
                       output_tags_and_subfield_codes, synonym_maps);

    } catch (const std::exception &x) {
        Error("caught exception: " + std::string(x.what()));
    }
}

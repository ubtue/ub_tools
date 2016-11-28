/** \file    extract_normdata_translations.cc
 *  \brief   Extract IxTheo and MACS translations from the normdata file and write it to 
 *           language specific text files
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

/* The German term is found in Field 150 
   Currently there are two different kinds of translations:
   IxTheo-Translations with the following definitions:

   700: Person - fremdsprachige Äquivalenz
   710: Körperschaft - fremdsprachige Äquivalenz
   711: Konferenz - fremdsprachige Äquivalenz
   730: Titel - fremdsprachige Äquivalenz
   750: Sachbegriff - fremdsprachige Äquivalenz  
   751: Geografikum - fremdsprachige Äquivalenz    

   LoC/Rameau Translations:
   700: Person - Bevorzugter Name in einem anderen Datenbestand
   710: Körperschaft - Bevorzugter Name in einem anderen Datenbestand              
   711: Konferenz - Bevorzugte Benennung in einem anderen Datenbestand
   730: Einheitstitel - Bevorzugter Name in einem anderen Datenbestand
   750: Sachbegriff - Bevorzugte Benennung in einem anderen Datenbestand
   751: Geografikum - Bevorzugter Name in einem anderen Datenbestand  
*/ 
#include <iostream>
#include <map>
#include <vector>
#include <cstdlib>
#include "Compiler.h"
#include "MarcReader.h"
#include "MarcRecord.h"
#include "MarcWriter.h"
#include "MediaTypeUtil.h"
#include "StringUtil.h"
#include "Subfields.h"
#include "util.h"


// Languages to handle
const unsigned int NUMBER_OF_LANGUAGES(2);
const std::vector<std::string> languages_to_create{ "en", "fr" };
enum Languages { EN, FR };


void Usage() {
    std::cerr << "Usage: " << ::progname << " norm_data_marc_input extracted_translations\n";
    std::exit(EXIT_FAILURE);
}


void AugmentIxTheoTagWithLanguage(const MarcRecord &record, const std::string &tag,
                                  std::vector<std::string> * const translations)
{
    auto ixtheo_pos(std::find(translations->begin(), translations->end(), "IxTheo"));
    if (ixtheo_pos != translations->end()) {
        std::vector<std::string> ixtheo_lang_codes;
        record.extractSubfields(tag, "9", &ixtheo_lang_codes);
        bool already_found_ixtheo_translation(false);
        for (const auto &lang_code : ixtheo_lang_codes) {
            if (lang_code[0] != 'L')
                continue;
            if (already_found_ixtheo_translation)
                continue;
            if (lang_code.find("eng") != std::string::npos and *ixtheo_pos != "IxTheo_eng") {
                *ixtheo_pos += + "_eng";
                already_found_ixtheo_translation = true;
            } else if (lang_code.find("fra") != std::string::npos and *ixtheo_pos != "IxTheo_fra") {
                *ixtheo_pos += "_fra";
                already_found_ixtheo_translation = true;
            } else 
                Warning("Unsupported language code \"" + lang_code + "\" for PPN " + record.getControlNumber());
        }
    }
}


void ExtractTranslations(MarcReader * const marc_reader, const std::string &german_term_field_spec,
                         const std::string &translation_field_spec,
                         std::map<std::string, std::string> term_to_translation_maps[])
{
    std::set<std::string> german_tags_and_subfield_codes;
    if (unlikely(StringUtil::Split(german_term_field_spec, ':', &german_tags_and_subfield_codes) < 1))
        Error("ExtractTranslations: Need at least one translation field");

    std::set<std::string> translation_tags_and_subfield_codes;
    if (unlikely(StringUtil::Split(translation_field_spec, ':', &translation_tags_and_subfield_codes) < 1))
        Error("ExtractTranslations: Need at least one translation field");
    
    if (unlikely(german_tags_and_subfield_codes.size() != translation_tags_and_subfield_codes.size()))
        Error("ExtractTranslations: Number of German fields and number of translation fields must be equal");
    
    unsigned count(0);
    while (const MarcRecord record = marc_reader->read()) {
        std::map<std::string, std::vector<std::string>> all_translations;

        for (auto german_and_translations_it(std::make_pair(german_tags_and_subfield_codes.cbegin(),
                                                            translation_tags_and_subfield_codes.cbegin()));
             german_and_translations_it.first != german_tags_and_subfield_codes.cend();
             ++german_and_translations_it.first, ++german_and_translations_it.second) 
        {
             const std::string german_tag((*german_and_translations_it.first).substr(0, 3));
             const std::string german_subfields((*german_and_translations_it.first).substr(3));
             const std::string translation_tag((*german_and_translations_it.second).substr(0, 3));
             const std::string translation_subfields((*german_and_translations_it.second).substr(3));

            auto german_subfield_code_iterator(german_subfields.begin());
            auto translation_subfield_code_iterator(translation_subfields.begin());
             for (/* empty */; german_subfield_code_iterator != german_subfields.cend();
                             ++german_subfield_code_iterator, ++translation_subfield_code_iterator)
             {
                  std::vector<std::string> german_terms;
                  record.extractSubfield(german_tag, *german_subfield_code_iterator, &german_terms);
                 if (german_terms.empty())
                     continue;

                 // Always extract subfield 2 where "IxTheo" is located
                 std::vector<std::string> translations;
                 record.extractSubfields(translation_tag, std::string(1, *translation_subfield_code_iterator) + "2",
                                         &translations);
                 if (translations.empty())
                     continue;

                 // For IxTheo-Translations add the language code in the same field
                 AugmentIxTheoTagWithLanguage(record, translation_tag, &translations);
                 all_translations.insert(std::make_pair(StringUtil::Join(german_terms, ' '), translations));
            }
        }   
 
        for (auto all_translations_it = all_translations.begin(); all_translations_it != all_translations.end();
             ++all_translations_it)
        {
            const std::string german_term(all_translations_it->first);

            for (auto translation_vector_it(all_translations_it->second.begin());
                 translation_vector_it != all_translations_it->second.end();
                 ++translation_vector_it)
            {
                // FIXME: Die Reihenfolge scheint teilweise nicht zu stimmen. Die Schlüsselwörter können
                // FIXME: auch in falscher Reihenfolge kommen. Siehe PPN 208836667:
                // FIXME: Abakus: {Abacus, lcsh, Abaques (mathématiques), ram}
                // FIXME: - Obenland, 18.09.2016
                if (translation_vector_it + 1 == all_translations_it->second.end())
                    break;

                if (*translation_vector_it == "IxTheo_eng")
                    term_to_translation_maps[EN].emplace(german_term, *(++translation_vector_it));
                else if (*translation_vector_it == "IxTheo_fra")
                    term_to_translation_maps[FR].emplace(german_term, *(++translation_vector_it));
                else if (*translation_vector_it == "lcsh")
                    term_to_translation_maps[EN].emplace(german_term, *(++translation_vector_it));
                else if (*translation_vector_it == "ram")
                    term_to_translation_maps[FR].emplace(german_term, *(++translation_vector_it));
            }
        }
        ++count;
    }
    std::cerr << "Found EN: " << term_to_translation_maps[EN].size() << ", FR: "
              << term_to_translation_maps[FR].size() << " in " << count << " records.\n";
}


int main(int argc, char **argv) {
    ::progname = argv[0];

    if (argc != 3)
        Usage();

    const std::string authority_data_marc_input_filename(argv[1]);
    const std::string extracted_translations_filename(argv[2]);
    if (unlikely(authority_data_marc_input_filename == extracted_translations_filename))
        Error("Authority data input file name equals output file name!");
    std::unique_ptr<MarcReader> authority_data_reader(MarcReader::Factory(authority_data_marc_input_filename,
                                                                          MarcReader::BINARY));
    
    // Create a file for each language
    std::vector<std::string> output_file_components;
    if (unlikely(StringUtil::Split(extracted_translations_filename, ".", &output_file_components) < 1)) 
        Error("extracted_translations_filename " + extracted_translations_filename + " is not valid");
   
    File *lang_files[NUMBER_OF_LANGUAGES];
  
    // Derive output components from given input filename
    const std::string extension = (output_file_components.size() > 1) ? output_file_components.back() : "";
    std::string basename;
    if (not extension.empty())
        output_file_components.pop_back();
    basename = StringUtil::Join(output_file_components, ".");

    // Assemble output filename
    unsigned i(0);
    for (auto lang : languages_to_create) {
        lang = StringUtil::Trim(lang);
        const std::string lang_file_name_str(extension.empty() ? basename + "_" + lang : basename + "_" + lang + "."
                                             + extension);
        lang_files[i] = new File(lang_file_name_str, "w");
        if (lang_files[i]->fail())
            Error("can't open \"" + lang_file_name_str + "\" for writing!");
        ++i;
    }

    try {
        std::map<std::string, std::string> term_to_translation_maps[NUMBER_OF_LANGUAGES];
        ExtractTranslations(authority_data_reader.get(),
                            "100a:110a:111a:130a:150a:151a", 
                            "700a:710a:711a:730a:750a:751a",
                            term_to_translation_maps);
        for (const auto &line : term_to_translation_maps[EN]) 
            *(lang_files[EN]) << line.first << '|' << line.second << '\n';

        for (const auto &line : term_to_translation_maps[FR])
            *(lang_files[FR]) << line.first << '|' << line.second << '\n';

    } catch (const std::exception &x) {
        Error("caught exception: " + std::string(x.what()));
    }
}



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

   710: Körperschaft - fremdsprachige Äquivalenz        
   711: Konferenz - fremdsprachige Äquivalenz  
   700: Person - fremdsprachige Äquivalenz     
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
#include <vector>
#include <cstdlib>
#include "Compiler.h"
#include "MarcUtil.h"
#include "MediaTypeUtil.h"
#include "StringUtil.h"
#include "Subfields.h"
#include "util.h"


// Languages to handle
const unsigned int NUMBER_OF_LANGUAGES = 2;
const std::vector<std::string> languages_to_create{ "en", "fr" };
enum Languages { EN, FR };


void Usage() {
    std::cerr << "Usage: " << ::progname << " norm_data_marc_input extracted_translations\n";
    std::exit(EXIT_FAILURE);
}


void AugmentIxTheoTagWithLanguage(const MarcUtil::Record &record, const std::string &tag, std::vector<std::string> * const translations) {
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
                Warning("Unsupported language code \"" + lang_code + "\" for PPN " + record.getFields()[0]);
        }
    }
}


void ExtractTranslations(File* const marc_norm_input, 
                         const std::string german_term_field_spec,
                         const std::string translation_field_spec,
                         std::map<std::string, std::string> term_to_translation_maps[]) 
{
    std::set<std::string> german_tags_and_subfield_codes;
    if (unlikely(StringUtil::Split(german_term_field_spec, ':', &german_tags_and_subfield_codes) < 1))
        Error("ExtractTranslations: Need at least one translation field");

    std::set<std::string> translation_tags_and_subfield_codes;
    if (unlikely(StringUtil::Split(translation_field_spec, ':', &translation_tags_and_subfield_codes) < 1))
        Error("ExtractTranslations: Need at least one translation field");

    unsigned count(0);
    
    while (const MarcUtil::Record record = MarcUtil::Record::XmlFactory(marc_norm_input)) {
        std::vector<std::string> german_terms;

        // Determine the German term we will have translations for
        for (const auto tag_and_subfields : german_tags_and_subfield_codes) {
             const std::string tag(tag_and_subfields.substr(0, 3));
             const std::string subfields(tag_and_subfields.substr(3));
             
             std::vector<std::string> german_term_for_one_field;
             record.extractSubfields(tag, subfields, &german_term_for_one_field);

             // We may get the german term from only one field
             if (german_terms.size() > 1)  
                 Warning("We have german terms in more than one field for PPN: " + record.getFields()[0]);

             if (not german_term_for_one_field.empty()) 
                 german_terms = german_term_for_one_field;
        }        

        std::vector<std::string> all_translations;

        // Extract all additional translations
        for (auto tag_and_subfields : translation_tags_and_subfield_codes) {
            const std::string tag(tag_and_subfields.substr(0, 3));
            const std::string subfields(tag_and_subfields.substr(3));
            
            std::vector<std::string> translations;
            record.extractSubfields(tag, subfields, &translations);

            // For IxTheo-Translations add the language code in the same field
            AugmentIxTheoTagWithLanguage(record, tag, &translations);
            
            all_translations.insert(all_translations.end(), translations.begin(), translations.end());
        }
 
        for (auto it = all_translations.begin(); it != all_translations.end(); ++it) {
            if (*it == "IxTheo_eng") {
                term_to_translation_maps[EN].emplace(StringUtil::Join(german_terms, ' '), *(it + 1));
                ++it;
            } else if (*it == "IxTheo_fra") {
                term_to_translation_maps[FR].emplace(StringUtil::Join(german_terms, ' '), *(it + 1));
                ++it;
            } else if (*it == "lcsh") {
                term_to_translation_maps[EN].emplace(StringUtil::Join(german_terms, ' '), *(it + 1));
                ++it;
            } else if (*it == "ram") {
                term_to_translation_maps[FR].emplace(StringUtil::Join(german_terms, ' '), *(it + 1));
                ++it;
            }
        }
    }
    ++count;
}


std::unique_ptr<File> OpenInputFile(const std::string &filename) {
    std::string mode("r");
    if (MediaTypeUtil::GetFileMediaType(filename) == "application/lz4") 
        mode += "u";  
    std::unique_ptr<File> file(new File(filename, mode));
    if (file->fail())
        Error("can't open \"" + filename + "\" for reading!");

    return file;
}


int main(int argc, char **argv) {
    ::progname = argv[0];

    if (argc != 3)
        Usage();

    const std::string norm_data_marc_input_filename(argv[1]);
    std::unique_ptr<File> norm_data_marc_input(OpenInputFile(norm_data_marc_input_filename));

    const std::string extracted_translations_filename(argv[2]);
    if (unlikely(norm_data_marc_input_filename == extracted_translations_filename))
        Error("Norm data input file name equals output file name!");

    std::string output_mode("w");
    if (norm_data_marc_input->isCompressingOrUncompressing())
        output_mode += 'c';
    
    // Create a file for each language
    std::vector<std::string> output_file_components;
    if (unlikely(StringUtil::Split(extracted_translations_filename, ".", &output_file_components) < 1)) 
        Error("extracted_translations_filename " + extracted_translations_filename + " is not valid");
   
    File *lang_files[NUMBER_OF_LANGUAGES];
    unsigned i(0);
  
    // Derive output components from given input filename
    std::string extension = (output_file_components.size() > 1) ? output_file_components.back() : "";
    std::string basename;
    if (not extension.empty())
        output_file_components.pop_back();
    basename = StringUtil::Join(output_file_components, ".");
      
    // Assemble output filename
    for (auto lang : languages_to_create) {
         lang = StringUtil::Trim(lang);

         std::string lang_file_name_str = 
             (extension != "") ? basename + "_" + lang + "." + extension : basename + "_" + lang;

         lang_files[i] = new File(lang_file_name_str, output_mode);
         if (lang_files[i]->fail())
             Error("can't open \"" + lang_file_name_str + "\" for writing!");
         ++i;
    }

    try {
        std::map<std::string, std::string> term_to_translation_maps[NUMBER_OF_LANGUAGES];

        ExtractTranslations(norm_data_marc_input.get(), "100a:150a", "750a2", term_to_translation_maps);
        for (auto line : term_to_translation_maps[EN]) {
              
             *(lang_files[EN]) << line.first << "|" << line.second << "\n";
        }

        for (auto line : term_to_translation_maps[FR]) {
             *(lang_files[FR]) << line.first << "|" << line.second << "\n";
        }

    } catch (const std::exception &x) {
      Error("caught exception: " + std::string(x.what()));
    }
}



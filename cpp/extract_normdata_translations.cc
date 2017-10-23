/** \file    extract_normdata_translations.cc
 *  \brief   Extract IxTheo and MACS translations from the normdata file and write it to 
 *           language specific text files
 *  \author  Johannes Riedl
 */

/*
    Copyright (C) 2016, 2017 Library of the University of Tübingen

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
const unsigned int NUMBER_OF_LANGUAGES(9);
const std::vector<std::string> languages_to_create{ "en", "fr", "es", "it", "hans", "hant", "pt", "ru", "el"};
enum Languages { EN, FR, ES, IT, HANS, HANT, PT, RU, EL };


void Usage() {
    std::cerr << "Usage: " << ::progname << " norm_data_marc_input extracted_translations\n";
    std::exit(EXIT_FAILURE);
}

// We would like to determine the translation, the language and the the origin (ram, lcsh, ixtheo)
void ExtractOneTranslation(const Subfields &all_subfields, const std::string &translation_subfield_codes,
                           std::pair<std::string, std::string> * const language_translation_pair)
{
    language_translation_pair->first = "";
    language_translation_pair->second = "";

    std::vector<std::string> translation_origin;
    all_subfields.extractSubfields("2", &translation_origin);

    std::vector<std::string> translation_vector;
    all_subfields.extractSubfields(translation_subfield_codes, &translation_vector);

    const std::string language_and_type(all_subfields.getFirstSubfieldValue('9'));

    // Skip entry if we do not have IxTheo or MACS Mapping
    if (StringUtil::Join(translation_origin, ' ') != "IxTheo"
        and (not StringUtil::StartsWith(language_and_type, "v:MACS-Mapping"))) {
            return;
        }

    std::string language;
    std::string translation_type;

    const std::string language_prefix("L:");
    const std::string translation_type_prefix("Z:");

    // Try to find the correct field and extract the information
    auto lang(std::mismatch(language_prefix.cbegin(), language_prefix.cend(), language_and_type.cbegin()));
    auto type(std::mismatch(translation_type_prefix.cbegin(), translation_type_prefix.cend(), language_and_type.cbegin()));
    // Check if we matched the prefix
    if (lang.first == language_prefix.cend())
        language = std::string(lang.second, language_and_type.cend());
    else if (type.first == translation_type_prefix.cend()) {
        translation_type = std::string(type.second, language_and_type.cend());
        // We need a single translation so don't return synonyms
        if (translation_type == "VW")
            return;
    }

    if (translation_origin.size() == 1) {
        language_translation_pair->first = (translation_origin[0] == "IxTheo") ? translation_origin[0] + "_" + language : translation_origin[0];
        language_translation_pair->second = StringUtil::Join(translation_vector, ' ');
    } else
        logger->error("Incorrect translation origin translation " + StringUtil::Join(translation_vector, ' '));
}


void RemoveMACSIfIxTheoPresent(std::vector<std::string> * const translations) {

     if (std::find(translations->begin(), translations->end(), "IxTheo_eng") != translations->end()) {
         auto lcsh_it(std::find(translations->begin(), translations->end(), "lcsh"));
         if (lcsh_it != translations->end())
             translations->erase(lcsh_it, lcsh_it + 2);

     }

     if (std::find(translations->begin(), translations->end(), "IxTheo_fre") != translations->end()) {
         auto ram_it(std::find(translations->begin(), translations->end(), "ram"));
         if (ram_it != translations->end())
             translations->erase(ram_it, ram_it + 2);
     }
}


void ExtractTranslations(MarcReader * const marc_reader, const std::string &german_term_field_spec,
                         const std::string &translation_field_spec,
                         std::map<std::string, std::string> term_to_translation_maps[])
{
    std::vector<std::string> german_tags_and_subfield_codes;
    if (unlikely(StringUtil::Split(german_term_field_spec, ':', &german_tags_and_subfield_codes) < 1))
        logger->error("ExtractTranslations: Need at least one translation field");

    std::vector<std::string> translation_tags_and_subfield_codes;
    if (unlikely(StringUtil::Split(translation_field_spec, ':', &translation_tags_and_subfield_codes) < 1))
        logger->error("ExtractTranslations: Need at least one translation field");
    
    if (unlikely(german_tags_and_subfield_codes.size() != translation_tags_and_subfield_codes.size()))
        logger->error("ExtractTranslations: Number of German fields and number of translation fields must be equal");
    
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

                // Add additional specification in angle bracket if we can uniquely attribute it
                if (german_terms.size() == 1) {
                    std::vector<std::string> _9_subfields;
                    record.extractSubfield(german_tag, '9', &_9_subfields);
                    for (auto _9_subfield : _9_subfields)
                        if (StringUtil::StartsWith(_9_subfield, "g:")) {
                            german_terms[0] = german_terms[0] + " <" + _9_subfield.substr(2) + ">";
                        }
                }

                std::vector<std::string> translations;
                std::vector<size_t> translation_field_indices;
                record.getFieldIndices(translation_tag, &translation_field_indices);

                for (auto translation_field_index(translation_field_indices.cbegin());
                     translation_field_index != translation_field_indices.cend();
                     ++translation_field_index)
                {
                         Subfields all_subfields(record.getSubfields(*translation_field_index));
                         // Extract the translation in parameter given and subfields 2 and 9 where translation origin and translation type information
                         // is given
                         const std::string translation_subfield_codes(std::string(1, *translation_subfield_code_iterator));
                         std::pair<std::string, std::string> one_translation_and_metadata;
                         ExtractOneTranslation(all_subfields, translation_subfield_codes, &one_translation_and_metadata);
                         if (not (one_translation_and_metadata.first.empty() or one_translation_and_metadata.second.empty())) {
                            translations.push_back(StringUtil::Trim(one_translation_and_metadata.first, " \t\n"));
                            translations.push_back(StringUtil::Trim(one_translation_and_metadata.second," \t\n"));
                         }
                }

                if (translations.empty())
                    continue;

                // Make sure we use the more specific IxTheo translations if available
                RemoveMACSIfIxTheoPresent(&translations);
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
                if (translation_vector_it + 1 == all_translations_it->second.end())
                    break;
                if (*translation_vector_it == "IxTheo_eng")
                    term_to_translation_maps[EN].emplace(german_term, *(++translation_vector_it));
                else if (*translation_vector_it == "IxTheo_fre")
                    term_to_translation_maps[FR].emplace(german_term, *(++translation_vector_it));
                else if (*translation_vector_it == "IxTheo_spa")
                    term_to_translation_maps[ES].emplace(german_term, *(++translation_vector_it));
                else if (*translation_vector_it == "IxTheo_ita")
                    term_to_translation_maps[IT].emplace(german_term, *(++translation_vector_it));
                else if (*translation_vector_it == "IxTheo_hans")
                    term_to_translation_maps[HANS].emplace(german_term, *(++translation_vector_it));
                else if (*translation_vector_it == "IxTheo_hant")
                    term_to_translation_maps[HANT].emplace(german_term, *(++translation_vector_it));
                else if (*translation_vector_it == "lcsh")
                    term_to_translation_maps[EN].emplace(german_term, *(++translation_vector_it));
                else if (*translation_vector_it == "ram")
                    term_to_translation_maps[FR].emplace(german_term, *(++translation_vector_it));
                else if (*translation_vector_it == "IxTheo_por")
                    term_to_translation_maps[PT].emplace(german_term, *(++translation_vector_it));
                else if (*translation_vector_it == "IxTheo_rus")
                    term_to_translation_maps[RU].emplace(german_term, *(++translation_vector_it));
                else if (*translation_vector_it == "IxTheo_gre")
                    term_to_translation_maps[EL].emplace(german_term, *(++translation_vector_it));
            }
        }
        ++count;
    }
    std::cerr << "Found EN: " << term_to_translation_maps[EN].size()
              << ", FR: " << term_to_translation_maps[FR].size()
              << ", ES: " << term_to_translation_maps[ES].size()
              << ", IT: " << term_to_translation_maps[IT].size()
              << ", HANS: " << term_to_translation_maps[HANS].size()
              << ", HANT: " << term_to_translation_maps[HANT].size()
              << ", PT: " << term_to_translation_maps[PT].size()
              << ", RU: " << term_to_translation_maps[RU].size()
              << ", EL: " << term_to_translation_maps[EL].size()
              << " in " << count << " records.\n";
}


int main(int argc, char **argv) {
    ::progname = argv[0];

    if (argc != 3)
        Usage();

    const std::string authority_data_marc_input_filename(argv[1]);
    const std::string extracted_translations_filename(argv[2]);
    if (unlikely(authority_data_marc_input_filename == extracted_translations_filename))
        logger->error("Authority data input file name equals output file name!");
    std::unique_ptr<MarcReader> authority_data_reader(MarcReader::Factory(authority_data_marc_input_filename,
                                                                          MarcReader::BINARY));
    
    // Create a file for each language
    std::vector<std::string> output_file_components;
    if (unlikely(StringUtil::Split(extracted_translations_filename, ".", &output_file_components) < 1)) 
        logger->error("extracted_translations_filename " + extracted_translations_filename + " is not valid");
   
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
            logger->error("can't open \"" + lang_file_name_str + "\" for writing!");
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

        for (const auto &line : term_to_translation_maps[ES])
            *(lang_files[ES]) << line.first << '|' << line.second << '\n';

        for (const auto &line : term_to_translation_maps[IT])
            *(lang_files[IT]) << line.first << '|' << line.second << '\n';

        for (const auto &line : term_to_translation_maps[HANS])
            *(lang_files[HANS]) << line.first << '|' << line.second << '\n';

        for (const auto &line : term_to_translation_maps[HANT])
            *(lang_files[HANT]) << line.first << '|' << line.second << '\n';

        for (const auto &line : term_to_translation_maps[PT])
            *(lang_files[PT]) << line.first << '|' << line.second << '\n';

        for (const auto &line : term_to_translation_maps[RU])
            *(lang_files[RU]) << line.first << '|' << line.second << '\n';

        for (const auto &line : term_to_translation_maps[EL])
            *(lang_files[EL]) << line.first << '|' << line.second << '\n';
    } catch (const std::exception &x) {
        logger->error("caught exception: " + std::string(x.what()));
    }
}



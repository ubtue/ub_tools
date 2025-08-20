/** \file    extract_authority_translations.cc
 *  \brief   Extract IxTheo and MACS translations from the normdata file and write it to
 *           language specific text files
 *  \author  Johannes Riedl
 */

/*
    Copyright (C) 2016-2023 Library of the University of Tübingen

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
#include "MARC.h"
#include "StringUtil.h"
#include "TranslationUtil.h"
#include "util.h"


namespace {


// Languages to handle
const unsigned int NUMBER_OF_LANGUAGES(10);
const std::vector<std::string> languages_to_create{ "en", "fr", "es", "it", "hans", "hant", "pt", "pl", "ru", "el" };
enum Languages { EN, FR, ES, IT, HANS, HANT, PT, PL, RU, EL, LANGUAGES_END };


void Usage() {
    std::cerr << "Usage: " << ::progname << " norm_data_marc_input extracted_translations\n";
    std::exit(EXIT_FAILURE);
}


// Extract some translation specific information like language and primary or synonym type
void ExtractSubfield9Info(const std::vector<std::string> &_9Subfields, std::string * const language, std::string * const type,
                          std::string * const subfield_g_translation) {
    const std::string language_prefix("L:");
    const std::string ixtheo_type_prefix("Z:");
    const std::string translation_prefix("g:");

    for (const auto &_9Subfield : _9Subfields) {
        if (StringUtil::StartsWith(_9Subfield, language_prefix)) {
            *language = _9Subfield;
            // Strip the prefix
            StringUtil::ExtractHead(language, language_prefix);
        } else if (StringUtil::StartsWith(_9Subfield, ixtheo_type_prefix)) {
            *type = _9Subfield;
            StringUtil::ExtractHead(type, ixtheo_type_prefix);
        } else if (StringUtil::StartsWith(_9Subfield, translation_prefix)) {
            *subfield_g_translation = _9Subfield;
            StringUtil::ExtractHead(subfield_g_translation, translation_prefix);
        }
    }
}


// We would like to determine the translation, the language and the the origin (WikiData, GND/MACS, IxTheo)
void ExtractOneTranslation(const MARC::Subfields &all_subfields, const std::string &translation_subfield_codes,
                           std::pair<std::string, std::string> * const language_translation_pair) {
    language_translation_pair->first = "";
    language_translation_pair->second = "";

    std::vector<std::string> translation_origin(all_subfields.extractSubfields('2'));
    std::vector<std::string> translation_vector(all_subfields.extractSubfields(translation_subfield_codes));
    std::vector<std::string> _9Subfields(all_subfields.extractSubfields('9'));
    std::string language;
    std::string translation_type;
    std::string subfield_g_translation;

    ExtractSubfield9Info(_9Subfields, &language, &translation_type, &subfield_g_translation);
    if (not subfield_g_translation.empty())
        translation_vector.emplace_back("(" + subfield_g_translation + ")");

    // Skip entry if we do not have IxTheo or MACS (=lcsh, ram, embne, nsbncf) or WikiData translations
    const auto translation_origin_joined(StringUtil::Join(translation_origin, ' '));
    const std::set<std::string> admissible_translation_origins({ "IxTheo", "lcsh", "ram", "embne", "nsbncf", "WikiData" });
    if (admissible_translation_origins.find(translation_origin_joined) == admissible_translation_origins.end())
        return;

    const std::set<std::string> full_info_origins({ "IxTheo", "WikiData" });
    if (translation_origin.size() == 1) {
        language_translation_pair->first = (full_info_origins.find(translation_origin[0]) != full_info_origins.end())
                                               ? translation_origin[0] + "_" + language + "-" + translation_type
                                               : translation_origin[0];
        language_translation_pair->second = StringUtil::Join(translation_vector, ' ');
    } else
        LOG_ERROR("Incorrect translation origin translation " + StringUtil::Join(translation_vector, ' '));
}


void RemoveMACSIfIxTheoPresent(std::vector<std::string> * const translations) {
    if (std::find(translations->begin(), translations->end(), "IxTheo_eng-AF") != translations->end()) {
        auto lcsh_it(std::find(translations->begin(), translations->end(), "lcsh"));
        if (lcsh_it != translations->end())
            translations->erase(lcsh_it, lcsh_it + 2);
    }

    if (std::find(translations->begin(), translations->end(), "IxTheo_fre-AF") != translations->end()) {
        auto ram_it(std::find(translations->begin(), translations->end(), "ram"));
        if (ram_it != translations->end())
            translations->erase(ram_it, ram_it + 2);
    }

    if (std::find(translations->begin(), translations->end(), "IxTheo_spa-AF") != translations->end()) {
        auto embne_it(std::find(translations->begin(), translations->end(), "embne"));
        if (embne_it != translations->end())
            translations->erase(embne_it, embne_it + 2);
    }

    if (std::find(translations->begin(), translations->end(), "IxTheo_ita-AF") != translations->end()) {
        auto nsbncf_it(std::find(translations->begin(), translations->end(), "nsbncf"));
        if (nsbncf_it != translations->end())
            translations->erase(nsbncf_it, nsbncf_it + 2);
    }
}


void RemoveTranslationFromTranslations(std::vector<std::string> * const translations, const std::string &identifier) {
    auto it(std::find(translations->begin(), translations->end(), identifier));
    if (it != translations->end())
        translations->erase(it, it + 2);
}

bool HasTranslationOfType(std::vector<std::string> * const translations, const std::string type) {
    return std::find(translations->begin(), translations->end(), type) != translations->end();
}


void RemoveWikiDataIfIxTheoPresent(std::vector<std::string> * const translations) {
    for (const auto &short_lang : languages_to_create) {
        const std::string lang((short_lang == "hans" or short_lang == "hant")
                                   ? short_lang
                                   : TranslationUtil::MapInternational2LetterCodeToFake3LetterEnglishLanguageCode(short_lang));

        const std::string suffix("-AF");
        if (std::find(translations->begin(), translations->end(), "Ixtheo_" + lang + suffix) != translations->end())
            RemoveTranslationFromTranslations(translations, "WikiData_" + lang + suffix);
    }
}


void RemoveWikiDataIfMACSPresent(std::vector<std::string> * const translations) {
    const std::set<std::string> gnd_sources({ "lcsh", "ram", "embne", "nsbncd" });
    for (const auto &gnd_source : gnd_sources) {
        if (gnd_source == "lcsh" and HasTranslationOfType(translations, gnd_source)) {
            RemoveTranslationFromTranslations(translations, "WikiData_eng-AF");
            continue;
        }
        if (gnd_source == "ram" and HasTranslationOfType(translations, gnd_source)) {
            RemoveTranslationFromTranslations(translations, "WikiData_fre-AF");
            continue;
        }
        if (gnd_source == "embne" and HasTranslationOfType(translations, gnd_source)) {
            RemoveTranslationFromTranslations(translations, "WikiData_spa-AF");
            continue;
        }
        if (gnd_source == "nsbncd" and HasTranslationOfType(translations, gnd_source)) {
            RemoveTranslationFromTranslations(translations, "WikiData_ita-AF");
            continue;
        }
    }
}


void InsertTranslation(std::map<std::string, std::vector<std::string>> &term_to_translations_map, const std::string &german_term,
                       const std::string &translation, const std::string &type) {
    // Determine the type of the translation and make sure the so called "Ansetzungsformen" (i.e. the primary translation
    // in contrast to mere synonyms) are inserted in the front
    std::vector<std::string> term_translations(term_to_translations_map[german_term]);
    if (type == "AF")
        term_translations.insert(term_translations.begin(), translation);
    else
        term_translations.push_back(translation);
    term_to_translations_map[german_term] = term_translations;
}


bool IsTranslationForLanguage(const std::string &lang, const std::string &type) {
    const std::map<std::string, std::set<std::string>> origins{
        { "eng", { "IxTheo_eng", "lcsh", "WikiData_eng" } },  { "fre", { "IxTheo_fre", "ram", "WikiData_fre" } },
        { "spa", { "IxTheo_spa", "embne", "WikiData_spa" } }, { "ita", { "IxTheo_ita", "nsbncf", "WikiData_ita" } },
        { "hans", { "IxTheo_hans", "WikiData_hans" } },       { "hant", { "IxTheo_hant", "WikiData_hant" } },
        { "por", { "IxTheo_por", "WikiData_por" } },          { "pol", { "IxTheo_pol", "WikiData_pol" } },
        { "rus", { "IxTheo_rus", "WikiData_rus" } },          { "gre", { "IxTheo_gre", "WikiData_gre" } }
    };
    const auto lang_it(origins.find(lang));
    if (lang_it == origins.end())
        LOG_ERROR("Invalid language \"" + lang + "\"");
    return lang_it->second.find(type) != lang_it->second.end();
}


void ExtractTranslations(MARC::Reader * const marc_reader, const std::string &german_term_field_spec,
                         const std::string &translation_field_spec,
                         std::map<std::string, std::vector<std::string>> term_to_translation_maps[]) {
    std::vector<std::string> german_tags_and_subfield_codes;
    if (unlikely(StringUtil::Split(german_term_field_spec, ':', &german_tags_and_subfield_codes,
                                   /* suppress_empty_components = */ true)
                 < 1))
        LOG_ERROR("ExtractTranslations: Need at least one translation field");

    std::vector<std::string> translation_tags_and_subfield_codes;
    if (unlikely(StringUtil::Split(translation_field_spec, ':', &translation_tags_and_subfield_codes,
                                   /* suppress_empty_components = */ true)
                 < 1))
        LOG_ERROR("ExtractTranslations: Need at least one translation field");

    if (unlikely(german_tags_and_subfield_codes.size() != translation_tags_and_subfield_codes.size()))
        LOG_ERROR("ExtractTranslations: Number of German fields and number of translation fields must be equal");

    unsigned count(0);
    while (const MARC::Record record = marc_reader->read()) {
        std::map<std::string, std::vector<std::string>> all_translations;

        for (auto german_and_translations_it(
                 std::make_pair(german_tags_and_subfield_codes.cbegin(), translation_tags_and_subfield_codes.cbegin()));
             german_and_translations_it.first != german_tags_and_subfield_codes.cend();
             ++german_and_translations_it.first, ++german_and_translations_it.second)
        {
            const std::string german_tag((*german_and_translations_it.first).substr(0, 3));
            const std::string german_subfields((*german_and_translations_it.first).substr(3));
            const std::string translation_tag((*german_and_translations_it.second).substr(0, 3));
            const std::string translation_subfields((*german_and_translations_it.second).substr(3));

            auto german_subfield_code_iterator(german_subfields.begin());
            std::vector<std::string> german_terms;
            for (/* empty */; german_subfield_code_iterator != german_subfields.cend(); ++german_subfield_code_iterator) {
                std::vector<std::string> german_term(record.getSubfieldValues(german_tag, *german_subfield_code_iterator));
                german_terms.insert(german_terms.end(), german_term.begin(), german_term.end());
            }
            if (german_terms.empty())
                continue;

            // Add additional specification in angle bracket if we can uniquely attribute it
            std::vector<std::string> _9_subfields(record.getSubfieldValues(german_tag, '9'));
            std::vector<std::string> additional_specifications;
            for (auto _9_subfield : _9_subfields) {
                if (StringUtil::StartsWith(_9_subfield, "g:"))
                    additional_specifications.emplace_back("<" + _9_subfield.substr(2) + ">");
            }

            std::vector<std::string> translations;
            for (const auto &field : record.getTagRange(translation_tag)) {
                // Extract the translation in parameter given and subfields 2 and 9 where translation origin and translation type
                // information is given
                std::pair<std::string, std::string> one_translation_and_metadata;
                ExtractOneTranslation(field.getSubfields(), translation_subfields, &one_translation_and_metadata);
                if (not(one_translation_and_metadata.first.empty() or one_translation_and_metadata.second.empty())) {
                    translations.push_back(StringUtil::Trim(one_translation_and_metadata.first, " \t\n"));
                    translations.push_back(StringUtil::Trim(one_translation_and_metadata.second, " \t\n"));
                }
            }

            if (translations.empty())
                continue;

            // Make sure we use the most specific translation present
            RemoveWikiDataIfIxTheoPresent(&translations);
            RemoveWikiDataIfMACSPresent(&translations);
            RemoveMACSIfIxTheoPresent(&translations);

            const std::string final_german_term =
                StringUtil::Join(german_terms, " / ")
                + (not additional_specifications.empty() ? " " + StringUtil::Join(additional_specifications, ' ') : "");
            all_translations[final_german_term] = translations;
        }

        for (auto all_translations_it(all_translations.begin()); all_translations_it != all_translations.end(); ++all_translations_it) {
            const std::string german_term(all_translations_it->first);
            for (auto translation_vector_it(all_translations_it->second.begin());
                 translation_vector_it != all_translations_it->second.end(); ++translation_vector_it)
            {
                if (translation_vector_it + 1 == all_translations_it->second.end())
                    break;
                std::vector<std::string> splitType;

                // Extract the encoded type
                StringUtil::Split(*translation_vector_it, '-', &splitType, /* suppress_empty_components = */ true);
                const std::string origin_and_language = splitType[0];
                const std::string type = (splitType.size() == 2) ? splitType[1] : "";

                if (IsTranslationForLanguage("eng", origin_and_language))
                    InsertTranslation(term_to_translation_maps[EN], german_term, *(++translation_vector_it), type);
                else if (IsTranslationForLanguage("fre", origin_and_language))
                    InsertTranslation(term_to_translation_maps[FR], german_term, *(++translation_vector_it), type);
                else if (IsTranslationForLanguage("spa", origin_and_language))
                    InsertTranslation(term_to_translation_maps[ES], german_term, *(++translation_vector_it), type);
                else if (IsTranslationForLanguage("ita", origin_and_language))
                    InsertTranslation(term_to_translation_maps[IT], german_term, *(++translation_vector_it), type);
                else if (IsTranslationForLanguage("hans", origin_and_language))
                    InsertTranslation(term_to_translation_maps[HANS], german_term, *(++translation_vector_it), type);
                else if (IsTranslationForLanguage("hant", origin_and_language))
                    InsertTranslation(term_to_translation_maps[HANT], german_term, *(++translation_vector_it), type);
                else if (IsTranslationForLanguage("por", origin_and_language))
                    InsertTranslation(term_to_translation_maps[PT], german_term, *(++translation_vector_it), type);
                else if (IsTranslationForLanguage("pol", origin_and_language))
                    InsertTranslation(term_to_translation_maps[PL], german_term, *(++translation_vector_it), type);
                else if (IsTranslationForLanguage("rus", origin_and_language))
                    InsertTranslation(term_to_translation_maps[RU], german_term, *(++translation_vector_it), type);
                else if (IsTranslationForLanguage("gre", origin_and_language))
                    InsertTranslation(term_to_translation_maps[EL], german_term, *(++translation_vector_it), type);
            }
        }
        ++count;
    }
    std::cerr << "Found EN: " << term_to_translation_maps[EN].size() << ", FR: " << term_to_translation_maps[FR].size()
              << ", ES: " << term_to_translation_maps[ES].size() << ", IT: " << term_to_translation_maps[IT].size()
              << ", HANS: " << term_to_translation_maps[HANS].size() << ", HANT: " << term_to_translation_maps[HANT].size()
              << ", PT: " << term_to_translation_maps[PT].size() << ", RU: " << term_to_translation_maps[RU].size()
              << ", PL: " << term_to_translation_maps[PL].size() << ", EL: " << term_to_translation_maps[EL].size() << " in " << count
              << " records.\n";
}


} // unnamed namespace


int Main(int argc, char **argv) {
    if (argc != 3)
        Usage();

    const std::string authority_data_marc_input_filename(argv[1]);
    const std::string extracted_translations_filename(argv[2]);
    if (unlikely(authority_data_marc_input_filename == extracted_translations_filename))
        LOG_ERROR("Authority data input file name equals output file name!");
    std::unique_ptr<MARC::Reader> authority_data_reader(MARC::Reader::Factory(authority_data_marc_input_filename, MARC::FileType::BINARY));

    // Create a file for each language
    std::vector<std::string> output_file_components;
    if (unlikely(StringUtil::Split(extracted_translations_filename, '.', &output_file_components) < 1))
        LOG_ERROR("extracted_translations_filename " + extracted_translations_filename + " is not valid");

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
        const std::string lang_file_name_str(extension.empty() ? basename + "_" + lang : basename + "_" + lang + "." + extension);
        lang_files[i] = new File(lang_file_name_str, "w");
        if (lang_files[i]->fail())
            LOG_ERROR("can't open \"" + lang_file_name_str + "\" for writing!");
        ++i;
    }

    std::map<std::string, std::vector<std::string>> term_to_translation_maps[NUMBER_OF_LANGUAGES];
    ExtractTranslations(authority_data_reader.get(), "100abcd:110abcd:111a:130agp:150ax:151a", "700abcd:710abcd:711a:730a:750a:751a",
                        term_to_translation_maps);
    for (int lang(0); lang < LANGUAGES_END; ++lang) {
        for (const auto &line : term_to_translation_maps[lang])
            *(lang_files[lang]) << line.first << '|' << StringUtil::Join(line.second, "||") << '\n';
    }

    return EXIT_SUCCESS;
}

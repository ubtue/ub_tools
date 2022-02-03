/** \file    add_synonyms.cc
 *  \brief   Generic version for augmenting title data with synonyms found
             in the authority data
 *  \author  Johannes Riedl
 */

/*
    Copyright (C) 2016-2019, Library of the University of Tübingen

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

#include <fstream>
#include <iostream>
#include <map>
#include <vector>
#include <cstdlib>
#include "Compiler.h"
#include "MARC.h"
#include "RegexMatcher.h"
#include "StringUtil.h"
#include "util.h"


const unsigned FIELD_MIN_NON_DATA_SIZE(4); // Indicator 1 + 2, unit separator and subfield code
const unsigned int NUMBER_OF_LANGUAGES(9);
const std::vector<std::string> languages_to_translate{ "en", "fr", "es", "it", "hans", "hant", "pt", "ru", "el" };
enum Languages { EN, FR, ES, IT, HANS, HANT, PT, RU, EL, LANGUAGES_END };


[[noreturn]] void Usage() {
    ::Usage("master_marc_input norm_data_marc_input marc_output");
}


inline std::string GetTag(const std::string &tag_and_subfields_spec) {
    return tag_and_subfields_spec.substr(0, MARC::Record::TAG_LENGTH);
}


inline std::string GetSubfieldCodes(const std::string &tag_and_subfields_spec) {
    return tag_and_subfields_spec.substr(MARC::Record::TAG_LENGTH);
}


bool FilterPasses(const MARC::Record &record, const std::map<std::string, std::pair<std::string, std::string>> &filter_specs,
                  const std::string &field_spec) {
    auto filter_spec(filter_specs.find(field_spec));
    if (filter_spec == filter_specs.cend())
        return true; // No filter rule found!

    auto rule(filter_spec->second);
    // We have field_spec in key and rule to match in value
    const std::string subfield_codes(GetSubfieldCodes(rule.first));
    if (subfield_codes.length() != 1)
        LOG_ERROR("invalid subfield specification " + subfield_codes + " for filter!");

    const auto field(record.getFirstField(GetTag(rule.first)));
    if (field == record.end())
        return false;
    const std::vector<std::string> subfield_values(field->getSubfields().extractSubfields(subfield_codes[0]));
    if (subfield_values.empty())
        return false;

    return subfield_values[0] == rule.second;
}


void ExtractSynonyms(MARC::Reader * const authority_reader, const std::vector<std::string> &primary_tags_and_subfield_codes,
                     const std::vector<std::string> &synonym_tags_and_subfield_codes,
                     std::vector<std::map<std::string, std::string>> * const synonym_maps,
                     const std::map<std::string, std::pair<std::string, std::string>> &filter_spec) {
    while (const MARC::Record record = authority_reader->read()) {
        auto primary_tag_and_subfield_codes(primary_tags_and_subfield_codes.cbegin());
        auto synonym_tag_and_subfield_codes(synonym_tags_and_subfield_codes.cbegin());
        auto synonym_map(synonym_maps->begin());
        for (/*intentionally empty*/; primary_tag_and_subfield_codes != primary_tags_and_subfield_codes.cend();
             ++primary_tag_and_subfield_codes, ++synonym_tag_and_subfield_codes, ++synonym_map)
        {
            // Fill maps with synonyms
            std::vector<std::string> primary_values(record.getSubfieldAndNumericSubfieldValues(
                GetTag(*primary_tag_and_subfield_codes), GetSubfieldCodes(*primary_tag_and_subfield_codes)));
            std::vector<std::string> synonym_values(record.getSubfieldAndNumericSubfieldValues(
                GetTag(*synonym_tag_and_subfield_codes), GetSubfieldCodes(*synonym_tag_and_subfield_codes)));

            if (FilterPasses(record, filter_spec, *primary_tag_and_subfield_codes) and primary_values.size() and synonym_values.size()) {
                // Append if other synonyms for the same keyword exist
                const std::string key(StringUtil::Join(primary_values, ','));
                const std::string existing_synonyms((*synonym_map)[key]);
                std::string new_synonyms(StringUtil::Join(synonym_values, ','));
                if (not existing_synonyms.empty())
                    new_synonyms += "," + existing_synonyms;
                (*synonym_map)[key] = new_synonyms;
            }
        }
    }
}


inline std::string GetMapValueOrEmptyString(const std::map<std::string, std::string> &map, const std::string &searchterm) {
    auto value(map.find(searchterm));
    return (value != map.cend()) ? value->second : "";
}


inline std::vector<std::string> GetMapValueOrEmptyString(const std::map<std::string, std::vector<std::string>> &map,
                                                         const std::string &searchterm) {
    auto value(map.find(searchterm));
    return (value != map.cend()) ? value->second : std::vector<std::string>();
}

void WriteSynonymEntry(MARC::Record * const record, std::string tag, unsigned indicator2, const char subfield_code,
                       const std::string &synonyms) {
    if (record->hasTagWithIndicators(tag, '0', indicator2 + '0'))
        LOG_ERROR("in ProcessRecord: Could not insert field " + tag + " with indicators \'0\' and \'" + std::to_string(indicator2)
                  + "\' for PPN " + record->getControlNumber() + '!');
    record->insertField(tag, { MARC::Subfield(subfield_code, synonyms) }, '0', indicator2 + '0');
}


void ProcessRecordGermanSynonyms(MARC::Record * const record, const std::vector<std::map<std::string, std::string>> &synonym_maps,
                                 const std::vector<std::string> &primary_tags_and_subfield_codes,
                                 const std::vector<std::string> &output_tags_and_subfield_codes, bool *modified_record) {
    if (primary_tags_and_subfield_codes.size() != output_tags_and_subfield_codes.size())
        LOG_ERROR("Number of primary and output tags do not match");

    for (auto primary_tag_and_subfield_codes(primary_tags_and_subfield_codes.cbegin()),
         output_tag_and_subfield_code(output_tags_and_subfield_codes.cbegin());
         primary_tag_and_subfield_codes != primary_tags_and_subfield_codes.end();
         ++primary_tag_and_subfield_codes, ++output_tag_and_subfield_code)
    {
        std::vector<std::string> synonym_values;
        for (const auto &field : record->getTagRange(GetTag(*primary_tag_and_subfield_codes))) {
            const MARC::Subfields subfields(field.getSubfields());
            std::vector<std::string> primary_values(
                subfields.extractSubfieldsAndNumericSubfields(GetSubfieldCodes(*primary_tag_and_subfield_codes)));
            if (not primary_values.empty()) {
                std::string searchterm(StringUtil::Join(primary_values, ','));
                // Look up synonyms in all categories
                for (auto &synonym_map : synonym_maps) {
                    const auto &synonym_tag_and_subfield_codes(GetMapValueOrEmptyString(synonym_map, searchterm));
                    if (not synonym_tag_and_subfield_codes.empty()) {
                        synonym_values.push_back(synonym_tag_and_subfield_codes);
                    }
                }
            }
        }
        if (synonym_values.empty())
            continue;

        // Insert synonyms
        // Abort if field is already populated
        std::string tag(GetTag(*output_tag_and_subfield_code));
        if (unlikely(record->hasTag(tag)))
            LOG_ERROR("in ProcessRecord: Field with tag " + tag + " is not empty for PPN " + record->getControlNumber() + '!');
        std::string subfield_spec(GetSubfieldCodes(*output_tag_and_subfield_code));
        if (unlikely(subfield_spec.size() != 1))
            LOG_ERROR("in ProcessRecord: We currently only support a single subfield and thus specifying " + subfield_spec
                      + " as output subfield is not valid!");

        std::string synonyms;
        unsigned current_length(0);
        unsigned indicator2(0);

        for (auto synonym_it(synonym_values.cbegin()); synonym_it != synonym_values.cend();
             /*Intentionally empty*/)
        {
            if (indicator2 > 9)
                LOG_ERROR("Currently cannot handle synonyms with total length greater than "
                          + std::to_string(9 * (MARC::Record::MAX_VARIABLE_FIELD_DATA_LENGTH - FIELD_MIN_NON_DATA_SIZE)) + '\n' + "for PPN "
                          + record->getControlNumber());

            const size_t MARC_MAX_PAYLOAD_LENGTH(MARC::Record::MAX_VARIABLE_FIELD_DATA_LENGTH - FIELD_MIN_NON_DATA_SIZE);
            if (current_length + synonym_it->length() < MARC_MAX_PAYLOAD_LENGTH - 3 /* consider " , " */) {
                bool synonyms_empty(synonyms.empty());
                synonyms += synonyms_empty ? *synonym_it : " , " + *synonym_it;
                current_length += synonym_it->length() + (synonyms_empty ? 0 : 3);
                ++synonym_it;
            } else if (synonym_it->length() > MARC_MAX_PAYLOAD_LENGTH) {
                // Split the string at the longest possible word boundary and write back
                // We only support two bunches at the moment
                size_t last_admissible_word_offset((*synonym_it).rfind(" ", MARC_MAX_PAYLOAD_LENGTH));
                if (last_admissible_word_offset == std::string::npos)
                    LOG_ERROR("Could not properly split oversized synonym entry");
                const std::string first_part((*synonym_it).substr(0, last_admissible_word_offset));
                const std::string second_part((*synonym_it).substr(last_admissible_word_offset));
                if (second_part.length() > MARC_MAX_PAYLOAD_LENGTH)
                    LOG_ERROR("Could not properly split synonym list");
                synonyms.clear();
                WriteSynonymEntry(record, tag, indicator2, subfield_spec[0], first_part);
                ++indicator2;
                WriteSynonymEntry(record, tag, indicator2, subfield_spec[0], second_part);
                ++indicator2;
                *modified_record = true;
                ++synonym_it;
                continue;
            } else {
                WriteSynonymEntry(record, tag, indicator2, subfield_spec[0], synonyms);
                synonyms.clear();
                current_length = 0;
                ++indicator2;
                *modified_record = true;
            }
        }
        // Write rest of data
        if (not synonyms.empty()) {
            const char new_indicator2 = indicator2 + '0';
            if (record->hasTagWithIndicators(tag, '0', new_indicator2))
                logger->error("in ProcessRecord: Could not insert field " + tag + " with indicators \'0\' and \'" + new_indicator2
                              + "\' for PPN " + record->getControlNumber() + '!');
            record->insertField(tag, { MARC::Subfield(subfield_spec[0], synonyms) }, '0', new_indicator2);
            *modified_record = true;
        }
    }
}


// Write all occuring synonyms to the specific fields with one per language
void ProcessRecordTranslatedSynonyms(MARC::Record * const record, const std::vector<std::string> &primary_tags_and_subfield_codes,
                                     const std::vector<std::string> &translation_tags_and_subfield_codes,
                                     const std::vector<std::map<std::string, std::vector<std::string>>> &translation_maps,
                                     bool *modified_record) {
    auto output_tag_and_subfield_code(translation_tags_and_subfield_codes.begin());

    for (int lang(0); lang < LANGUAGES_END; ++lang, ++output_tag_and_subfield_code) {
        std::set<std::string> synonym_values;
        synonym_values.clear();
        for (auto primary_tag_and_subfield_codes(primary_tags_and_subfield_codes.begin());
             primary_tag_and_subfield_codes != primary_tags_and_subfield_codes.end(); ++primary_tag_and_subfield_codes)
        {
            std::vector<std::string> primary_values;
            std::vector<size_t> field_indices;
            for (const auto &field : record->getTagRange(GetTag(*primary_tag_and_subfield_codes))) {
                const MARC::Subfields subfields(field.getContents());
                primary_values = subfields.extractSubfields(GetSubfieldCodes(*primary_tag_and_subfield_codes));
                if (primary_values.size()) {
                    std::string searchterm = StringUtil::Join(primary_values, ',');
                    // Look up translation synonym_tag_and_subfield_codes for the respective language
                    const auto &translated_synonym_tag_and_subfield_codes(GetMapValueOrEmptyString(translation_maps[lang], searchterm));
                    if (not translated_synonym_tag_and_subfield_codes.empty())
                        // Only insert "real" synonyms without the primary translation
                        synonym_values.emplace(StringUtil::Join(translated_synonym_tag_and_subfield_codes, ','));
                }
            }
            if (synonym_values.empty())
                continue;
        }
        // Insert translated synonyms
        // Abort if field is already populated
        std::string tag(GetTag(*output_tag_and_subfield_code));
        if (record->hasTag(tag))
            LOG_ERROR("in ProcessRecord: Field with tag " + tag + " is not empty for PPN " + record->getControlNumber() + '!');
        std::string subfield_spec(GetSubfieldCodes(*output_tag_and_subfield_code));
        if (unlikely(subfield_spec.size() != 1))
            LOG_ERROR("in ProcessRecord: We currently only support a single subfield and thus specifying " + subfield_spec
                      + " as output subfield is not valid!");

        std::string synonyms(StringUtil::Join(synonym_values, ','));
        if (synonyms.size() > MARC::Record::MAX_VARIABLE_FIELD_DATA_LENGTH - 2)
            LOG_ERROR("Translated synonyms exceeded maximum length for PPN " + record->getControlNumber() + ": \"" + synonyms
                      + "\" has size " + std::to_string(synonyms.size()) + '\n');

        if (not synonyms.empty()) {
            if (record->hasTagWithIndicators(tag, '0', '0'))
                logger->error("in ProcessRecord: Could not insert field " + tag + " with indicators \'0\' and \'0\' " + " for PPN "
                              + record->getControlNumber() + '!');
            record->insertField(tag, { MARC::Subfield(subfield_spec[0], synonyms) }, '0', '0');
            *modified_record = true;
        }
    }
}


void InsertSynonyms(MARC::Reader * const marc_reader, MARC::Writer * const marc_writer,
                    const std::vector<std::string> &primary_tags_and_subfield_codes,
                    const std::vector<std::string> &output_tags_and_subfield_codes,
                    const std::vector<std::map<std::string, std::string>> &synonym_maps,
                    const std::vector<std::map<std::string, std::vector<std::string>>> &translation_maps,
                    const std::vector<std::string> &translated_tags_and_subfield_codes) {
    unsigned modified_count(0), record_count(0);
    while (MARC::Record record = marc_reader->read()) {
        bool modified_record(false);
        ProcessRecordGermanSynonyms(&record, synonym_maps, primary_tags_and_subfield_codes, output_tags_and_subfield_codes,
                                    &modified_record);
        ProcessRecordTranslatedSynonyms(&record, primary_tags_and_subfield_codes, translated_tags_and_subfield_codes, translation_maps,
                                        &modified_record);
        marc_writer->write(record);
        if (modified_record)
            ++modified_count;
        ++record_count;
    }

    LOG_INFO("Modified " + std::to_string(modified_count) + " of " + std::to_string(record_count) + " record(s).");
}


void ExtractTranslatedSynonyms(std::vector<std::map<std::string, std::vector<std::string>>> * const translation_maps) {
    const std::string TRANSLATION_FILES_BASE("normdata_translations");
    const std::string TRANSLATION_FILES_EXTENSION("txt");

    for (int lang(0); lang < LANGUAGES_END; ++lang) {
        const std::string lang_extension(languages_to_translate[lang]);
        const std::string translation_file_name(TRANSLATION_FILES_BASE + "_" + lang_extension + "." + TRANSLATION_FILES_EXTENSION);
        std::ifstream translation_file(translation_file_name);
        if (not translation_file.is_open())
            LOG_ERROR("Unable to open " + translation_file_name);
        std::string line;
        while (std::getline(translation_file, line)) {
            std::vector<std::string> german_and_translations;
            StringUtil::Split(line, '|', &german_and_translations, /* suppress_empty_components = */ true);
            if (german_and_translations.size() < 2)
                LOG_ERROR("invalid line \"" + line + "\" in \"" + translation_file_name + "\"!");
            std::string german_term = german_and_translations[0];
            (*translation_maps)[lang][german_term] =
                std::vector<std::string>(german_and_translations.cbegin() + 1, german_and_translations.cend());
        }
    }
}


bool ParseSpec(const std::string &spec_str, std::vector<std::string> * const field_specs,
               std::map<std::string, std::pair<std::string, std::string>> *filter_specs) {
    std::vector<std::string> raw_field_specs;

    if (unlikely(StringUtil::Split(spec_str, ':', &raw_field_specs, /* suppress_empty_components = */ true) == 0))
        LOG_ERROR("need at least one field!");

    // Iterate over all Field-specs and extract possible filters
    static RegexMatcher * const matcher(RegexMatcher::RegexMatcherFactory("(\\d{1,3}[a-z]+)\\[(\\d{1,3}[a-z])=(.*)\\]"));

    for (auto field_spec : raw_field_specs) {
        if (matcher->matched(field_spec)) {
            filter_specs->emplace((*matcher)[1], std::make_pair((*matcher)[2], (*matcher)[3]));
            const auto bracket_pos(field_spec.find('['));
            field_spec = (bracket_pos != std::string::npos) ? field_spec.erase(bracket_pos, field_spec.length()) : field_spec;
        }
        field_specs->push_back(field_spec);
    }
    return true;
}


int Main(int argc, char **argv) {
    if (argc != 4)
        Usage();

    const std::string marc_input_filename(argv[1]);
    const std::string authority_data_marc_input_filename(argv[2]);
    const std::string marc_output_filename(argv[3]);
    if (unlikely(marc_input_filename == marc_output_filename))
        LOG_ERROR("Title data input file name equals output file name!");
    if (unlikely(authority_data_marc_input_filename == marc_output_filename))
        LOG_ERROR("Authority data input file name equals output file name!");

    std::unique_ptr<MARC::Reader> marc_reader(MARC::Reader::Factory(marc_input_filename, MARC::FileType::BINARY));
    std::unique_ptr<MARC::Reader> authority_reader(MARC::Reader::Factory(authority_data_marc_input_filename, MARC::FileType::BINARY));
    std::unique_ptr<MARC::Writer> marc_writer(MARC::Writer::Factory(marc_output_filename, MARC::FileType::BINARY));

    // Determine possible mappings
    // Values in square brackets specify a positive criterion for values to be taken into account
    const std::string AUTHORITY_DATA_PRIMARY_SPEC(
        "100abcdpnt9g[079v=piz]:110abcdnpt9g:111abcdnpt9g:130abcdnpt9g:150abcdnpt9g:151abcdztnp9g:100a9g");
    const std::string AUTHORITY_DATA_SYNONYM_SPEC("400abcdpnt9g:410abcdnpt9g:411abcdnpt9g:430abcdnpt9g:450abcdnpt9g:451abcdznpt9g:700a9g");
    const std::string TITLE_DATA_PRIMARY_SPEC("600abcdpnt9g:610abcdnpt9g:611abcdnpt:630abcdnpt:650abcdnpt9g:651abcdnpt9g:689abcdpntz9g");
    const std::string TITLE_DATA_UNUSED_FIELDS_FOR_SYNONYMS("SYAa:SYBa:SYCa:SYDa:SYEa:SYFa:SYGa");
    const std::string TITLE_DATA_UNUSED_FIELD_FOR_TRANSLATED_SYNONYMS("STAa:STBa:STCa:STDa:STEa:STFa:STGa:STHa:STIa");

    // Determine fields to handle
    std::vector<std::string> primary_tags_and_subfield_codes;
    std::vector<std::string> synonym_tags_and_subfield_codes;
    std::vector<std::string> input_tags_and_subfield_codes;
    std::vector<std::string> output_tags_and_subfield_codes;
    std::vector<std::string> translation_tags_and_subfield_codes;

    std::map<std::string, std::pair<std::string, std::string>> filter_specs;

    if (not unlikely(ParseSpec(AUTHORITY_DATA_PRIMARY_SPEC, &primary_tags_and_subfield_codes, &filter_specs)))
        LOG_ERROR("Could not properly parse " + AUTHORITY_DATA_PRIMARY_SPEC);

    if (unlikely(StringUtil::Split(AUTHORITY_DATA_SYNONYM_SPEC, ':', &synonym_tags_and_subfield_codes,
                                   /* suppress_empty_components = */ true)
                 == 0))
        LOG_ERROR("Need at least one synonym_tag_and_subfield_codes field");

    if (unlikely(StringUtil::Split(TITLE_DATA_PRIMARY_SPEC, ':', &input_tags_and_subfield_codes,
                                   /* suppress_empty_components = */ true)
                 == 0))
        LOG_ERROR("Need at least one input field");

    if (unlikely(StringUtil::Split(TITLE_DATA_UNUSED_FIELDS_FOR_SYNONYMS, ':', &output_tags_and_subfield_codes,
                                   /* suppress_empty_components = */ true)
                 == 0))
        LOG_ERROR("Need at least one output field");

    if (unlikely(StringUtil::Split(TITLE_DATA_UNUSED_FIELD_FOR_TRANSLATED_SYNONYMS, ':', &translation_tags_and_subfield_codes,
                                   /* suppress_empty_components = */ true)
                 == 0))
        LOG_ERROR("Need at least as many output fields as supported languages: (currently " + std::to_string(languages_to_translate.size())
                  + ")");

    unsigned num_of_authority_entries(primary_tags_and_subfield_codes.size());

    if (synonym_tags_and_subfield_codes.size() != num_of_authority_entries)
        LOG_ERROR("Number of authority primary specs must match number of synonym_tag_and_subfield_codes specs");
    if (input_tags_and_subfield_codes.size() != output_tags_and_subfield_codes.size())
        LOG_ERROR("Number of fields title entry specs must match number of output specs");

    std::vector<std::map<std::string, std::string>> synonym_maps(num_of_authority_entries, std::map<std::string, std::string>());

    // Extract the synonyms from authority data
    ExtractSynonyms(authority_reader.get(), primary_tags_and_subfield_codes, synonym_tags_and_subfield_codes, &synonym_maps, filter_specs);

    // Extract translations from authority data
    std::vector<std::map<std::string, std::vector<std::string>>> translation_maps(NUMBER_OF_LANGUAGES,
                                                                                  std::map<std::string, std::vector<std::string>>());
    ExtractTranslatedSynonyms(&translation_maps);

    // Iterate over the title data
    InsertSynonyms(marc_reader.get(), marc_writer.get(), input_tags_and_subfield_codes, output_tags_and_subfield_codes, synonym_maps,
                   translation_maps, translation_tags_and_subfield_codes);

    return EXIT_SUCCESS;
}

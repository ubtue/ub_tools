/** \file    normalise_marc_contents.cc
 *  \brief   Replace variant entries in MARC subfields w/ a standardised form.
 *  \author  Dr. Johannes Ruscheinski
 */

/*
    Copyright (C) 2018,2019 Library of the University of Tübingen

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

#include <algorithm>
#include <iostream>
#include <map>
#include <unordered_set>
#include <utility>
#include <vector>
#include <cstring>
#include "Compiler.h"
#include "IniFile.h"
#include "MARC.h"
#include "StringUtil.h"
#include "TextUtil.h"
#include "UBTools.h"
#include "util.h"


namespace {


[[noreturn]] void Usage() {
    std::cerr << "Usage: " << ::progname << " marc_input marc_output\n";
    std::exit(EXIT_FAILURE);
}


typedef std::map<std::string, std::string> VariantsToCanonicalNameMap;


std::string NormaliseSubfieldContents(std::string subfield_contents) {
    TextUtil::UTF8ToLower(&subfield_contents);
    return TextUtil::CollapseAndTrimWhitespace(&subfield_contents);
}


void LoadTagAndSubfieldCodesGroupsFromGlobalSection(
    const IniFile &ini_file, std::map<std::string, std::vector<std::string>> * const subfields_name_to_subfields_map) {
    const auto global_section(ini_file.getSection(""));
    if (global_section == ini_file.end())
        LOG_ERROR("missing gobal section!");

    for (const auto &entry : *global_section) {
        if (unlikely(subfields_name_to_subfields_map->find(entry.name_) != subfields_name_to_subfields_map->cend()))
            LOG_ERROR("duplicate subfields name \"" + entry.name_ + "\"!");
        if (unlikely(entry.value_.empty()))
            LOG_ERROR("missing subfields spec for \"" + entry.name_ + "\"!");

        std::vector<std::string> tags_and_subfield_codes;
        StringUtil::Split(entry.value_, ':', &tags_and_subfield_codes, /* suppress_empty_components */ true);
        for (const auto &tag_and_subfield_code : tags_and_subfield_codes) {
            if (unlikely(tag_and_subfield_code.length() != MARC::Record::TAG_LENGTH + 1))
                LOG_ERROR("bad subfields spec for \"" + entry.name_ + "\"!");
        }

        (*subfields_name_to_subfields_map)[entry.name_] = tags_and_subfield_codes;
    }
}


void InsertVariantsIntoMap(const std::vector<std::string> &subfield_specs, const std::unordered_set<std::string> &variants,
                           const std::string &canonical_name,
                           std::vector<std::pair<std::string, VariantsToCanonicalNameMap>> * const maps) {
    for (const auto &subfield_spec : subfield_specs) {
        auto subfield_spec_and_replacement_map(
            std::find_if(maps->begin(), maps->end(),
                         [&subfield_spec](std::pair<std::string, VariantsToCanonicalNameMap> &subfield_spec_and_replacement_map1) {
                             return subfield_spec == subfield_spec_and_replacement_map1.first;
                         }));
        if (subfield_spec_and_replacement_map == maps->end()) {
            maps->emplace_back(subfield_spec, VariantsToCanonicalNameMap{});
            subfield_spec_and_replacement_map = maps->end() - 1;
        }

        for (const auto &variant : variants)
            subfield_spec_and_replacement_map->second.insert(std::make_pair(variant, canonical_name));
    }
}


// The structure of the config file is as follows:
// In the gobal section at the top there must be one or more string entries which have values that consist of colon-separated
// subfield references, e.g.
//              authors    = "100a:700a:710a"
//              publishers = "400d:422d"
//
// The named sections have the following structure:
//   The name of the section itself is the canonical name, i.e. what we want to use to replace the variants.
//   There must be one entry named "subfields" whose value is one of the entries in the global section.
//   All other entries must have names starting with "variant".  These variants will be replaced with the
//   canonical name if found in a relevent subfield.  An example might look like
//
//   [Fred & Johnson]
//   subfields = "publishers"
//   variant1 = "Fred and Johnson"
//   variant2 = "F. & J."
//
void LoadConfigFile(std::vector<std::pair<std::string, VariantsToCanonicalNameMap>> * const maps) {
    const IniFile ini_file(UBTools::GetTuelibPath() + "normalise_marc_contents.conf");

    std::map<std::string, std::vector<std::string>> subfields_name_to_subfields_map;
    LoadTagAndSubfieldCodesGroupsFromGlobalSection(ini_file, &subfields_name_to_subfields_map);

    for (const auto &section : ini_file) {
        if (section.getSectionName().empty())
            continue;

        const std::string canonical_name(section.getSectionName());
        const std::vector<std::string> *subfield_specs(nullptr);
        std::unordered_set<std::string> variants;
        for (const auto &entry : section) {
            if (entry.name_ == "subfields") {
                const auto subfields_name_and_specs(subfields_name_to_subfields_map.find(entry.value_));
                if (unlikely(subfields_name_and_specs == subfields_name_to_subfields_map.cend()))
                    LOG_ERROR("unknown \"subfields\": \"" + entry.value_ + "\"!");
                subfield_specs = &(subfields_name_and_specs->second);
            } else {
                if (unlikely(not StringUtil::StartsWith(entry.name_, "variant")))
                    LOG_ERROR("unknown entry \"" + entry.name_ + "\" entry in section \"" + section.getSectionName() + "\"!");
                variants.emplace(NormaliseSubfieldContents(entry.value_));
            }
        }

        if (unlikely(variants.empty()))
            LOG_ERROR("missing variants entries in the \"" + section.getSectionName() + "\" section!");
        if (unlikely(subfield_specs == nullptr))
            LOG_ERROR("missing \"subfields\" entry for the \"" + section.getSectionName() + "\" section!");
        InsertVariantsIntoMap(*subfield_specs, variants, canonical_name, maps);
    }

    // Sort into ascending order of subfield specs:
    std::sort(maps->begin(), maps->end(),
              [](const std::pair<std::string, VariantsToCanonicalNameMap> &a, const std::pair<std::string, VariantsToCanonicalNameMap> &b) {
                  return a.first < b.first;
              });

    LOG_INFO("loaded " + std::to_string(maps->size()) + " substitution maps.");
}


void ProcessRecords(MARC::Reader * const marc_reader, MARC::Writer * const marc_writer,
                    const std::vector<std::pair<std::string, VariantsToCanonicalNameMap>> &maps) {
    unsigned total_count(0), modified_count(0);
    while (MARC::Record record = marc_reader->read()) {
        bool replaced_at_least_one_field(false);
        ++total_count;

        auto subfield_spec_and_replacement_map(maps.cbegin());
        for (auto &field : record) {
            while (subfield_spec_and_replacement_map != maps.cend()
                   and subfield_spec_and_replacement_map->first < field.getTag().toString())
                ++subfield_spec_and_replacement_map;
            if (subfield_spec_and_replacement_map == maps.cend())
                break;

            if (subfield_spec_and_replacement_map->first.substr(0, MARC::Record::TAG_LENGTH) == field.getTag().toString()) {
                const char SUBFIELD_CODE(subfield_spec_and_replacement_map->first[MARC::Record::TAG_LENGTH]);
                MARC::Subfields subfields(field.getSubfields());

                bool replaced_at_least_one_subfield(false);
                for (auto &subfield : subfields) {
                    if (subfield.code_ == SUBFIELD_CODE) {
                        const std::string normalised_subfield_contents(NormaliseSubfieldContents(subfield.value_));
                        auto variant_and_canonical_value(subfield_spec_and_replacement_map->second.find(normalised_subfield_contents));
                        if (variant_and_canonical_value != subfield_spec_and_replacement_map->second.cend()) {
                            subfield.value_ = variant_and_canonical_value->second;
                            replaced_at_least_one_subfield = true;
                        }
                    }
                }

                if (replaced_at_least_one_subfield) {
                    field.setSubfields(subfields);
                    replaced_at_least_one_field = true;
                }
            }
        }

        if (replaced_at_least_one_field)
            ++modified_count;

        marc_writer->write(record);
    }
    LOG_INFO("Processed " + std::to_string(total_count) + " records and modified " + std::to_string(modified_count) + " record(s).");
}


} // unnamed namespace


int Main(int argc, char **argv) {
    ::progname = argv[0];

    if (argc != 3)
        Usage();

    std::unique_ptr<MARC::Reader> marc_reader(MARC::Reader::Factory(argv[1]));
    std::unique_ptr<MARC::Writer> marc_writer(MARC::Writer::Factory(argv[2]));

    std::vector<std::pair<std::string, VariantsToCanonicalNameMap>> maps;
    LoadConfigFile(&maps);
    ProcessRecords(marc_reader.get(), marc_writer.get(), maps);

    return EXIT_SUCCESS;
}

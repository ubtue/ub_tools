/** \file   normalize_and_deduplicate_lang.cc
 *  \brief  Normalizes language codes and removes duplicates from specific MARC record fields
 *  \author Madeeswaran Kannan (madeeswaran.kannan@uni-tuebingen.de)
 *
 *  \copyright 2018 Universitätsbibliothek Tübingen.  All rights reserved.
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU Affero General Public License as
 *  published by the Free Software Foundation, either version 3 of the
 *  License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Affero General Public License for more details.
 *
 *  You should have received a copy of the GNU Affero General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#include <iostream>
#include <unordered_map>
#include <unordered_set>
#include <cinttypes>
#include <cstring>
#include <unistd.h>
#include "IniFile.h"
#include "MARC.h"
#include "StringUtil.h"
#include "util.h"


namespace {


[[noreturn]] void Usage() {
    std::cerr << "Usage: " << ::progname << " [--verbosity=min_verbosity] marc_input marc_output \n"
              << "\n";
    std::exit(EXIT_FAILURE);
}


const std::string CONFIG_FILE_PATH("/usr/local/var/lib/tuelib/normalize_and_deduplicate_lang.conf");


const std::string LANGUAGE_CODE_OVERRIDE_SECTION("Overrides");


struct LanguageCodeParams {
    static constexpr size_t MAX_LANGUAGE_CODE_LENGTH = 3;

    std::unordered_map<std::string, std::string> variant_to_canonical_form_map_;
    std::unordered_set<std::string> valid_language_codes_;
public:
    inline bool isCanonical(const std::string &language_code) { return valid_language_codes_.find(language_code) != valid_language_codes_.end(); }
    std::string getCanonicalCode(const std::string &language_code, bool fallback_to_original = true);
};


bool IsValidLanguageCodeLength(const std::string &language_code) {
    return language_code.length() == LanguageCodeParams::MAX_LANGUAGE_CODE_LENGTH;
}


std::string LanguageCodeParams::getCanonicalCode(const std::string &language_code, bool fallback_to_original) {
    if (isCanonical(language_code))
        return language_code;

    const auto match(variant_to_canonical_form_map_.find(language_code));
    if (match != variant_to_canonical_form_map_.cend())
        return match->second;
    else if (fallback_to_original) {
        LOG_WARNING("No canonical language code found for variant '" + language_code + "'");
        return language_code;
    } else
        LOG_ERROR("Unknown language code variant '" + language_code + "'!");
}


void LoadLanguageCodesFromConfig(const IniFile &config, LanguageCodeParams * const params) {
    std::vector<std::string> raw_language_codes;
    StringUtil::Split(config.getString("", "canonical_language_codes"), ",", &raw_language_codes);
    if (raw_language_codes.empty())
        LOG_ERROR("Couldn't read canonical language codes from config file!");

    for (const auto& language_code : raw_language_codes) {
        if (not IsValidLanguageCodeLength(language_code))
            LOG_ERROR("Invalid length for language code '" + language_code + "'!");
        else if (params->isCanonical(language_code))
            LOG_WARNING("Duplicate canonical language code '" + language_code + "' found!");
        else
            params->valid_language_codes_.insert(language_code);
    }

    for (const auto& variant : config.getSectionEntryNames(LANGUAGE_CODE_OVERRIDE_SECTION)) {
        const auto canonical_name(config.getString(LANGUAGE_CODE_OVERRIDE_SECTION, variant));
        if (not IsValidLanguageCodeLength(variant))
            LOG_ERROR("Invalid length for language code '" + variant + "'!");
        else if (not IsValidLanguageCodeLength(canonical_name))
            LOG_ERROR("Invalid length for language code '" + canonical_name + "'!");
        else if (not params->isCanonical(canonical_name))
            LOG_ERROR("Unknown canonical language code '" + canonical_name + "' for variant '" + variant + "'!");

        params->variant_to_canonical_form_map_.insert(std::make_pair(variant, canonical_name));
    }
}


} // unnamed namespace


int Main(int argc, char *argv[]) {
    ::progname = argv[0];

    if (argc < 2)
        Usage();

    IniFile config_file(CONFIG_FILE_PATH);
    LanguageCodeParams params;

    std::unique_ptr<MARC::Reader> reader(MARC::Reader::Factory(argv[1]));
    std::unique_ptr<MARC::Writer> writer(MARC::Writer::Factory(argv[2]));

    LoadLanguageCodesFromConfig(config_file, &params);
    int num_records(0);
    while (MARC::Record record = reader->read()) {
        num_records++;
        const auto ppn(record.findTag("001")->getContents());
        const auto PrintInfo = [&ppn, &num_records](const std::string &message) {
            LOG_INFO("Record '" + ppn + "' [" + std::to_string(num_records) + "]: " + message);
        };

        const auto tag_008(record.findTag("008"));
        const auto tag_041(record.findTag("041"));
        auto language_code_008(tag_008->getContents().substr(35, 3));

        StringUtil::Trim(&language_code_008);
        if (language_code_008.empty() or language_code_008 == "|||") {
    //        PrintInfo("No language code found in control field");
            language_code_008.clear();      // to indicate absence in the case of '|||'
        } else {
            const auto language_code_008_normalized(params.getCanonicalCode(language_code_008));
            if (language_code_008 != language_code_008_normalized) {
                PrintInfo("Normalized control field 008 language code: '" + language_code_008
                         + "' => " + language_code_008_normalized + "'");

                auto old_content(tag_008->getContents());
                old_content.replace(35, 3, language_code_008_normalized);
                tag_008->setContents(old_content);
                language_code_008 = language_code_008_normalized;
            }
        }

        if (tag_041 == record.end()) {
            if (not language_code_008.empty()) {
                PrintInfo("Copying language code '" + language_code_008 + "' from 008 => 041");
                record.insertField("041", { { 'a', language_code_008 } });
            }
        } else {
            // normalize the existing records
            for (auto& subfield : tag_041->getSubfields()) {
                const auto normalized_language_code(params.getCanonicalCode(subfield.value_));
                if (normalized_language_code != subfield.value_) {
                    PrintInfo("Normalized subfield 041$" + std::string(1, subfield.code_) +
                             " language code: '" + subfield.value_ + "' => '" + normalized_language_code + "'");

                    subfield.value_ = normalized_language_code;
                }
            }

            // remove duplicates
            std::unordered_set<std::string> unique_language_codes;
            for (auto subfield(tag_041->getSubfields().begin()); subfield != tag_041->getSubfields().end();) {
                if (unique_language_codes.find(subfield->value_) != unique_language_codes.end()) {
                    PrintInfo("Removing duplicate subfield entry 041$" + std::string(1, subfield->code_) +
                             " '" + subfield->value_ + "'");
                    subfield = tag_041->getSubfields().deleteSubfield(subfield);
                    // ### WTF!
                    continue;
                }

                 unique_language_codes.insert(subfield->value_);
                ++subfield;
            }
        }

        writer->write(record);
    }

    return EXIT_SUCCESS;
}

/** \file   normalise_and_deduplicate_language.cc
 *  \brief  Normalises language codes and removes duplicates from specific MARC record fields
 *  \author Madeeswaran Kannan (madeeswaran.kannan@uni-tuebingen.de)
 *
 *  \copyright 2018,2019 Universitätsbibliothek Tübingen.  All rights reserved.
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
#include "UBTools.h"
#include "util.h"


namespace {


[[noreturn]] void Usage() {
    std::cerr << "Usage: " << ::progname << " [--min-log-level=min_verbosity] marc_input marc_output\n"
              << "      Normalises language codes and removes their duplicates from specific MARC "
                 "record fields (008 and 041).\n";
    std::exit(EXIT_FAILURE);
}


const std::string CONFIG_FILE_PATH(UBTools::GetTuelibPath() + "normalise_and_deduplicate_language.conf");


const std::string LANGUAGE_CODE_OVERRIDE_SECTION("Overrides");


struct LanguageCodeParams {
    static constexpr size_t LANGUAGE_CODE_LENGTH = 3;

    std::unordered_map<std::string, std::string> variant_to_canonical_form_map_;
    std::unordered_set<std::string> valid_language_codes_;
public:
    inline bool isCanonical(const std::string &language_code) { return valid_language_codes_.find(language_code) != valid_language_codes_.end(); }
    bool getCanonicalCode(const std::string &language_code, std::string * const canonical_code, const bool fallback_to_original = true);
};


bool HasValidLanguageCodeLength(const std::string &language_code) {
    return language_code.length() == LanguageCodeParams::LANGUAGE_CODE_LENGTH;
}


bool LanguageCodeParams::getCanonicalCode(const std::string &language_code, std::string * const canonical_code, const bool fallback_to_original) {
    if (isCanonical(language_code)) {
        *canonical_code = language_code;
        return true;
    }

    const auto match(variant_to_canonical_form_map_.find(language_code));
    if (match != variant_to_canonical_form_map_.cend()) {
        *canonical_code = match->second;
        return true;
    } else {
        if (fallback_to_original)
            *canonical_code = language_code;
        return false;
    }
}


void LoadLanguageCodesFromConfig(const IniFile &config, LanguageCodeParams * const params) {
    std::vector<std::string> raw_language_codes;
    StringUtil::Split2(config.getString("", "canonical_language_codes"), ',', &raw_language_codes);
    if (raw_language_codes.empty())
        LOG_ERROR("Couldn't read canonical language codes from config file '" + CONFIG_FILE_PATH + "'!");

    for (const auto &language_code : raw_language_codes) {
        if (not HasValidLanguageCodeLength(language_code))
            LOG_ERROR("Invalid length for language code '" + language_code + "'!");
        else if (params->isCanonical(language_code))
            LOG_WARNING("Duplicate canonical language code '" + language_code + "' found!");
        else
            params->valid_language_codes_.insert(language_code);
    }

    for (const auto &variant : config.getSectionEntryNames(LANGUAGE_CODE_OVERRIDE_SECTION)) {
        const auto canonical_name(config.getString(LANGUAGE_CODE_OVERRIDE_SECTION, variant));
        if (not HasValidLanguageCodeLength(variant))
            LOG_ERROR("Invalid length for language code '" + variant + "'!");
        else if (not HasValidLanguageCodeLength(canonical_name))
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
        ++num_records;
        const auto ppn(record.findTag("001")->getContents());
        const auto LogOutput = [&ppn, &num_records](const std::string &message, bool warning = false) {
            const auto msg("Record '" + ppn + "' [" + std::to_string(num_records) + "]: " + message);
            if (not warning)
                LOG_INFO(msg);
            else
                LOG_WARNING(msg);
        };

        const auto tag_008(record.findTag("008"));
        const auto tag_041(record.findTag("041"));
        auto language_code_008(tag_008->getContents().substr(35, 3));

        StringUtil::Trim(&language_code_008);
        if (language_code_008.empty() or language_code_008 == "|||")
            language_code_008.clear();      // to indicate absence in the case of '|||'
        else {
            std::string language_code_008_normalized;
            if (not params.getCanonicalCode(language_code_008, &language_code_008_normalized))
                LogOutput("Unknown language code variant '" + language_code_008 + "' in control field 008", true);
            if (language_code_008 != language_code_008_normalized) {
                LogOutput("Normalized control field 008 language code: '" + language_code_008
                         + "' => " + language_code_008_normalized + "'");

                auto old_content(tag_008->getContents());
                old_content.replace(35, 3, language_code_008_normalized);
                tag_008->setContents(old_content);
                language_code_008 = language_code_008_normalized;
            }
        }

        if (tag_041 == record.end()) {
            if (not language_code_008.empty()) {
                LogOutput("Copying language code '" + language_code_008 + "' from 008 => 041");
                record.insertField("041", { { 'a', language_code_008 } });
            }
        } else {
            // normalize and remove the existing records
            MARC::Record::Field modified_tag_041(*tag_041);
            MARC::Subfields modified_subfields;
            bool propagate_changes(false);
            std::unordered_set<std::string> unique_language_codes;
            const auto indicator1(modified_tag_041.getIndicator1()), indicator2(modified_tag_041.getIndicator2());

            for (auto &subfield : tag_041->getSubfields()) {
                if (unique_language_codes.find(subfield.value_) != unique_language_codes.end()) {
                    LogOutput("Removing duplicate subfield entry 041$" + std::string(1, subfield.code_) +
                              " '" + subfield.value_ + "'");
                    propagate_changes = true;
                    continue;
                }

                std::string normalized_language_code;
                if (not params.getCanonicalCode(subfield.value_, &normalized_language_code)) {
                    LogOutput("Unknown language code variant '" + subfield.value_ + "' in subfield 041$" +
                              std::string(1, subfield.code_), true);
                }

                if (normalized_language_code != subfield.value_) {
                    LogOutput("Normalized subfield 041$" + std::string(1, subfield.code_) +
                              " language code: '" + subfield.value_ + "' => '" + normalized_language_code + "'");

                    subfield.value_ = normalized_language_code;
                    propagate_changes = true;
                }

                unique_language_codes.insert(subfield.value_);
                modified_subfields.addSubfield(subfield.code_, subfield.value_);
            }

            if (propagate_changes)
                tag_041->setContents(modified_subfields, indicator1, indicator2);
        }

        writer->write(record);
    }

    return EXIT_SUCCESS;
}

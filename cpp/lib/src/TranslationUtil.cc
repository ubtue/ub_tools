/** \file   TranslationUtil.cc
 *  \brief  Implementation of the DbConnection class.
 *  \author Dr. Johannes Ruscheinski (johannes.ruscheinski@uni-tuebingen.de)
 *
 *  \copyright 2016-2020 Universitätsbibliothek Tübingen.  All rights reserved.
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
#include "TranslationUtil.h"
#include <map>
#include "Compiler.h"
#include "File.h"
#include "StringUtil.h"
#include "util.h"


namespace TranslationUtil {


std::string GetId(DbConnection * const connection, const std::string &german_text) {
    const std::string SELECT_EXISTING("SELECT id FROM translations WHERE text=\"" + connection->escapeString(german_text)
                                      + "\" AND language_code=\"deu\"");
    if (not connection->query(SELECT_EXISTING))
        LOG_ERROR("SELECT failed: " + SELECT_EXISTING + " (" + connection->getLastErrorMessage() + ")");
    DbResultSet id_result_set(connection->getLastResultSet());
    std::string id;
    if (not id_result_set.empty())
        return id_result_set.getNextRow()["id"];
    else { // We don't have any entries for this German term yet.
        const std::string SELECT_MAX_ID("SELECT MAX(id) FROM translations");
        if (not connection->query(SELECT_MAX_ID))
            logger->error("in TranslationUtil::GetId: SELECT failed: " + SELECT_MAX_ID + " (" + connection->getLastErrorMessage() + ")");
        DbResultSet max_id_result_set(connection->getLastResultSet());
        if (max_id_result_set.empty())
            return "1";

        const std::string max_id(max_id_result_set.getNextRow()["MAX(id)"]); // Apparently SQL NULL can be  returned
                                                                             // which leads to an empty string here.
        return std::to_string(max_id.empty() ? 1 : StringUtil::ToUnsigned(max_id) + 1);
    }
}


const std::map<std::string, std::string> international_2letter_code_to_german_3or4letter_code{
    { "af", "afr" },  { "an", "arg" },    { "ar", "ara" }, { "bg", "bul" }, { "br", "bre" }, { "ca", "cat" }, { "cs", "cze" },
    { "cy", "cym" },  { "da", "dan" },    { "de", "deu" }, { "el", "gre" }, { "en", "eng" }, { "es", "spa" }, { "et", "est" },
    { "eu", "eus" },  { "fi", "fin" },    { "fr", "fra" }, { "ga", "gaa" }, { "gl", "glg" }, { "he", "heb" }, { "ht", "hat" },
    { "hr", "hrv" },  { "hu", "hun" },    { "id", "ind" }, { "is", "ice" }, { "it", "ita" }, { "la", "lat" }, { "lv", "lav" },
    { "lt", "lit" },  { "ms", "msa" },    { "mt", "mlt" }, { "nl", "nld" }, { "no", "nor" }, { "oc", "oci" }, { "pl", "pol" },
    { "pt", "por" },  { "ro", "rum" },    { "ru", "rus" }, { "sk", "slo" }, { "sl", "slv" }, { "so", "som" }, { "sq", "sqi" },
    { "sr", "srp" },  { "sv", "swe" },    { "sw", "swa" }, { "tl", "tgl" }, { "tr", "tur" }, { "uk", "ukr" }, { "vi", "vie" },
    { "zh", "hant" }, { "zh-cn", "hans" }
};


std::string MapInternational2LetterCodeToGerman3Or4LetterCode(const std::string &international_2letter_code) {
    const auto _2letter_and_3letter_codes(international_2letter_code_to_german_3or4letter_code.find(international_2letter_code));
    if (unlikely(_2letter_and_3letter_codes == international_2letter_code_to_german_3or4letter_code.cend()))
        logger->error(
            "in TranslationUtil::MapInternational2LetterCodeToGerman3LetterCode: unknown international "
            "2-letter code \""
            + international_2letter_code + "\"!");
    return _2letter_and_3letter_codes->second;
}


bool IsValidInternational2LetterCode(const std::string &international_2letter_code_candidate) {
    if (international_2letter_code_candidate.length() != 2)
        return false;

    for (const auto &_2letter_and_3letter_codes : international_2letter_code_to_german_3or4letter_code) {
        if (_2letter_and_3letter_codes.first == international_2letter_code_candidate)
            return true;
    }

    return false;
}


std::string MapGerman3Or4LetterCodeToInternational2LetterCode(const std::string &german_3or4letter_code) {
    for (const auto &_2letter_and_3letter_codes : international_2letter_code_to_german_3or4letter_code) {
        if (_2letter_and_3letter_codes.second == german_3or4letter_code)
            return _2letter_and_3letter_codes.first;
    }
    LOG_ERROR("unknown German 3-letter code \"" + german_3or4letter_code + "\"!");
}


bool IsValidGerman3Or4LetterCode(const std::string &german_3or4letter_code_candidate) {
    if (german_3or4letter_code_candidate.length() != 3 and german_3or4letter_code_candidate.length() != 4)
        return false;

    for (const auto &_2letter_and_3letter_codes : international_2letter_code_to_german_3or4letter_code) {
        if (_2letter_and_3letter_codes.second == german_3or4letter_code_candidate)
            return true;
    }

    return false;
}


void ReadIniFile(const std::string &ini_filename,
                 std::unordered_map<std::string, std::pair<unsigned, std::string>> * const token_to_line_no_and_other_map) {
    File input(ini_filename, "r");
    if (not input)
        throw std::runtime_error("can't open \"" + ini_filename + "\" for reading!");

    unsigned line_no(0);
    while (not input.eof()) {
        ++line_no;
        std::string line;
        if (input.getline(&line) == 0 or line.empty())
            continue;

        const std::string::size_type first_equal_pos(line.find('='));
        if (unlikely(first_equal_pos == std::string::npos))
            throw std::runtime_error("missing equal-sign in \"" + ini_filename + "\" on line " + std::to_string(line_no) + "!");

        const std::string key(StringUtil::Trim(line.substr(0, first_equal_pos)));
        if (unlikely(key.empty()))
            throw std::runtime_error("missing token or English key in \"" + ini_filename + "\" on line " + std::to_string(line_no) + "!");

        const std::string rest(StringUtil::Trim(StringUtil::Trim(line.substr(first_equal_pos + 1)), '"'));
        if (unlikely(rest.empty()))
            throw std::runtime_error("missing translation in \"" + ini_filename + "\" on line " + std::to_string(line_no) + "!");
        (*token_to_line_no_and_other_map)[key] = std::make_pair(line_no, rest);
    }
}


static std::map<std::string, std::string> german_to_3or4letter_english_codes{
    { "afr", "afr" }, { "ara", "ara" },   { "arg", "arg" },  { "ast", "ast" }, { "bre", "bre" }, { "bul", "bul" }, { "cat", "cat" },
    { "cym", "wel" }, { "cze", "ces" },   { "dan", "dan" },  { "deu", "ger" }, { "eng", "eng" }, { "est", "est" }, { "eus", "baq" },
    { "fin", "fin" }, { "fra", "fre" },   { "gaa", "gaa" },  { "glg", "glg" }, { "gre", "gre" }, { "hat", "hat" }, { "heb", "heb" },
    { "hrv", "hrv" }, { "hun", "hun" },   { "ice", "isl" },  { "ind", "ind" }, { "ita", "ita" }, { "lat", "lat" }, { "lav", "lav" },
    { "lit", "lit" }, { "mlt", "mlt" },   { "msa", "may" },  { "nld", "dut" }, { "nor", "nor" }, { "oci", "oci" }, { "pol", "pol" },
    { "por", "por" }, { "rum", "ron" },   { "rus", "rus" },  { "slo", "slk" }, { "slv", "slv" }, { "som", "som" }, { "spa", "spa" },
    { "sqi", "alb" }, { "srp", "srp" },   { "swa", "swa" },  { "swe", "swe" }, { "tgl", "tgl" }, { "tur", "tur" }, { "ukr", "ukr" },
    { "vie", "vie" }, { "hans", "hans" }, { "hant", "hant" }
};


std::string MapGermanLanguageCodesToFake3LetterEnglishLanguagesCodes(const std::string &german_code) {
    const auto ger_and_eng_code(german_to_3or4letter_english_codes.find(german_code));
    if (ger_and_eng_code == german_to_3or4letter_english_codes.cend())
        return "???";
    return ger_and_eng_code->second;
}


std::string MapFake3LetterEnglishLanguagesCodesToGermanLanguageCodes(const std::string &english_3or4letter_code) {
    for (const auto &ger_and_eng_code : german_to_3or4letter_english_codes) {
        if (ger_and_eng_code.second == english_3or4letter_code)
            return ger_and_eng_code.first;
    }

    return "???";
}


bool IsValidFake3Or4LetterEnglishLanguagesCode(const std::string &english_3or4letter_code_candidate) {
    if (english_3or4letter_code_candidate.length() != 3 and english_3or4letter_code_candidate.length() != 4)
        return false;

    for (const auto &german_and_english_codes : german_to_3or4letter_english_codes) {
        if (german_and_english_codes.second == english_3or4letter_code_candidate)
            return true;
    }
    return false;
}


} // namespace TranslationUtil

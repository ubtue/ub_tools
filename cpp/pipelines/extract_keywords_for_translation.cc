/** \file    extract_keywords_for_translation.cc
 *  \brief   A tool for extracting keywords that need to be translated.  The keywords and any possibly pre-existing
 *           translations will be stored in a SQL database.
 *  \author  Dr. Johannes Ruscheinski
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

#include <iostream>
#include <span>
#include <string>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>
#include <cstdlib>
#include <nlohmann/json.hpp>
#include "Compiler.h"
#include "DbConnection.h"
#include "Downloader.h"
#include "FileUtil.h"
#include "IniFile.h"
#include "MARC.h"
#include "StringUtil.h"
#include "TimeUtil.h"
#include "TranslationUtil.h"
#include "UBTools.h"
#include "util.h"


[[noreturn]] void Usage() {
    ::Usage("[--insert-only-non-existing] [--download-full-wikidata] norm_data_input");
}


static unsigned keyword_count, translation_count, additional_hits, synonym_count, german_term_count;
static DbConnection *shared_connection;
static std::unordered_set<std::string> *ppns_already_present;
enum Status { RELIABLE, UNRELIABLE, UNRELIABLE_CAT2, RELIABLE_SYNONYM, UNRELIABLE_SYNONYM };
const std::string CONF_FILE_PATH(UBTools::GetTuelibPath() + "translations.conf");
const unsigned WIKIDATA_FULL_DOWNLOAD_BATCH_SIZE(3000);


struct WikidataTranslation {
    std::string translation_;
    std::string language_;
    std::string wiki_id_;

public:
    WikidataTranslation(const std::string &translation, const std::string &language, const std::string wiki_id)
        : translation_(translation), language_(language), wiki_id_(wiki_id) { }
};

typedef std::unordered_multimap<std::string, WikidataTranslation> wikidata_translation_lookup_table;


std::string StatusToString(const Status status) {
    switch (status) {
    case RELIABLE:
        return "reliable";
    case UNRELIABLE:
        return "unreliable";
    case UNRELIABLE_CAT2:
        return "unreliable_cat2";
    case RELIABLE_SYNONYM:
        return "reliable_synonym";
    case UNRELIABLE_SYNONYM:
        return "unreliable_synonym";
    }

    LOG_ERROR("in StatusToString: we should *never* get here!");
}


using TextLanguageCodeWikiIDStatusAndOriginTag = std::tuple<std::string, std::string, std::string, Status, std::string, bool>;


void ExtractGermanTerms(
    const MARC::Record &record,
    std::vector<TextLanguageCodeWikiIDStatusAndOriginTag> * const text_language_codes_wiki_ids_statuses_and_origin_tags) {
    bool updated_german(false);
    for (const auto &_150_field : record.getTagRange("150")) {
        const MARC::Subfields _150_subfields(_150_field.getSubfields());
        // $a is non-repeatable in 150 and necessary
        if (likely(_150_subfields.hasSubfield('a'))) {
            std::string complete_keyword_phrase;
            for (auto subfield : _150_subfields) {
                if (subfield.code_ == 'a')
                    complete_keyword_phrase = StringUtil::RemoveChars("<>", &(subfield.value_));
                // $x and $g are repeatable and possibly belong to each other
                if (subfield.code_ == 'x') {
                    complete_keyword_phrase += " / " + subfield.value_;
                    updated_german = true;
                }
                if (subfield.code_ == '9') {
                    std::string _9_subfield(subfield.value_);
                    if (StringUtil::StartsWith(_9_subfield, "g:")) {
                        _9_subfield = _9_subfield.substr(2);
                        complete_keyword_phrase += " <" + StringUtil::RemoveChars("<>", &_9_subfield) + ">";
                    }
                }
            }

            text_language_codes_wiki_ids_statuses_and_origin_tags->emplace_back(
                complete_keyword_phrase, TranslationUtil::MapGermanLanguageCodesToFake3LetterEnglishLanguagesCodes("deu"), "", RELIABLE,
                "150", updated_german);
            ++german_term_count;
        }
    }
}


void ExtractGermanSynonyms(
    const MARC::Record &record,
    std::vector<TextLanguageCodeWikiIDStatusAndOriginTag> * const text_language_codes_wiki_ids_statuses_and_origin_tags) {
    for (const auto &_450_field : record.getTagRange("450")) {
        const MARC::Subfields _450_subfields(_450_field.getSubfields());
        if (_450_subfields.hasSubfield('a')) {
            text_language_codes_wiki_ids_statuses_and_origin_tags->emplace_back(
                _450_subfields.getFirstSubfieldWithCode('a'),
                TranslationUtil::MapGermanLanguageCodesToFake3LetterEnglishLanguagesCodes("deu"), "", RELIABLE_SYNONYM, "450",
                false /*updated_german*/);
            ++synonym_count;
        }
    }
}


bool IsSynonym(const MARC::Subfields &_750_subfields) {
    for (const auto &_9_subfield : _750_subfields.extractSubfields('9')) {
        if (_9_subfield == "Z:VW")
            return true;
    }
    return false;
}


void ExtractNonGermanTranslations(
    const MARC::Record &record,
    std::vector<TextLanguageCodeWikiIDStatusAndOriginTag> * const text_language_codes_wiki_ids_statuses_and_origin_tags) {
    for (const auto &_750_field : record.getTagRange("750")) {
        const MARC::Subfields _750_subfields(_750_field.getSubfields());
        std::vector<std::string> _9_subfields(_750_subfields.extractSubfields('9'));
        std::string language_code;
        for (const auto &_9_subfield : _9_subfields) {
            if (StringUtil::StartsWith(_9_subfield, "L:"))
                language_code = _9_subfield.substr(2);
        }
        if (language_code.empty() and _750_subfields.hasSubfield('2')) {
            const std::string _750_2(_750_subfields.getFirstSubfieldWithCode('2'));
            if (_750_2 == "lcsh")
                language_code = "eng";
            else if (_750_2 == "ram")
                language_code = "fra";
            else if (_750_2 == "embne")
                language_code = "spa";
            else if (_750_2 == "nsbncf")
                language_code = "ita";

            if (not language_code.empty())
                ++additional_hits;
        }

        language_code = TranslationUtil::MapGermanLanguageCodesToFake3LetterEnglishLanguagesCodes(language_code);
        if (language_code != "???") {
            const bool is_synonym(IsSynonym(_750_subfields));
            Status status;
            if (_750_subfields.getFirstSubfieldWithCode('2') == "IxTheo")
                status = is_synonym ? RELIABLE_SYNONYM : RELIABLE;
            else
                status = is_synonym ? UNRELIABLE_SYNONYM : UNRELIABLE;
            ++translation_count;
            text_language_codes_wiki_ids_statuses_and_origin_tags->emplace_back(_750_subfields.getFirstSubfieldWithCode('a'), language_code,
                                                                                "", status, "750", false);
        }
    }
}


std::string GetWikidataPostQuery(const std::span<std::string> &gnd_codes, const std::vector<std::string> languages) {
    std::vector<std::string> quoted_gnds;
    std::transform(gnd_codes.begin(), gnd_codes.end(), std::back_inserter(quoted_gnds), [](const std::string &s) { return '"' + s + '"'; });
    const std::string joined_gnds(StringUtil::Join(quoted_gnds, " "));
    std::vector<std::string> quoted_langs;
    std::transform(languages.begin(), languages.end(), std::back_inserter(quoted_langs),
                   [](const std::string &s) { return '"' + s + '"'; });
    const std::string joined_langs(StringUtil::Join(quoted_langs, ", "));

    return std::string() + R"(query=PREFIX schema: <http://schema.org/>)" + R"(SELECT DISTINCT ?item ?title ?lang ?gnd WHERE {)"
           + R"(VALUES ?gnd {)" + joined_gnds + "}" + R"(?item wdt:P227 ?gnd .)"
           + R"([ schema:about ?item ; schema:name ?title; schema:inLanguage ?lang; ] .)" + R"(FILTER (?lang IN ()" + joined_langs + R"()))"
           + R"(})" + R"(ORDER BY ?gnd ?lang)";
}


int GetRetryAfterSeconds(const HttpHeader &http_header) {
    const std::string retry_after(http_header.getRetryAfter());
    if (not retry_after.empty() and std::isdigit(retry_after[0]))
        return StringUtil::ToInt(retry_after);
    const double diff_time(
        TimeUtil::DiffStructTm(TimeUtil::StringToStructTm(retry_after, TimeUtil::RFC822_FORMAT), TimeUtil::GetCurrentTimeGMT()));
    if (diff_time < 0)
        LOG_ERROR("Invalid Time difference");
    return std::round(diff_time);
}

const unsigned MAX_429_ITERATIONS(5);

void DownloadWikidataTranslations(const std::string &query, std::string * const results) {
    Downloader::Params params;
    params.additional_headers_ = { "Accept: application/sparql-results+json" };
    Downloader downloader(params);
    const Url wikidata_url("https://query.wikidata.org/sparql");
    downloader.postData(wikidata_url, query, Downloader::DEFAULT_TIME_LIMIT * 2);
    unsigned response_code(downloader.getResponseCode());
    if (response_code != 200) {
        LOG_WARNING("Could not download Wikidata Translations for query \"" + query + "\"(Error Code " + std::to_string(response_code)
                    + ")");
        if (response_code == 429) {
            for (unsigned iteration = 0; iteration < MAX_429_ITERATIONS; ++iteration) {
                const unsigned wait_seconds(GetRetryAfterSeconds(downloader.getMessageHeaderObject()));
                TimeUtil::Millisleep(wait_seconds * 1000 + 1);
                downloader.postData(wikidata_url, query);
                response_code = downloader.getResponseCode();
                if (response_code == 200)
                    break;
            }
            if (response_code != 200)
                LOG_ERROR("Failed to download Wikidata Translations for query \"" + query + "\"(Error Code "
                          + std::to_string(response_code));
        } else
            LOG_ERROR("Aborting because of Error Code " + std::to_string(response_code));
    }
    *results = downloader.getMessageBody();
}


std::vector<std::string> GetAllTranslatorLanguages(const IniFile &ini_file) {
    static std::vector<std::string> all_translator_languages;
    static std::once_flag flag;
    std::call_once(flag, [&]() {
        std::string all_translator_languages_entry;
        ini_file.lookup("Languages", "all", &all_translator_languages_entry);
        if (all_translator_languages_entry.empty())
            LOG_ERROR("Could not determine translator languages from IniFile \"" + ini_file.getFilename() + "\"");
        StringUtil::SplitThenTrimWhite(all_translator_languages_entry, ',', &all_translator_languages);
        std::for_each(all_translator_languages.begin(), all_translator_languages.end(), [](std::string &lang) {
            lang = TranslationUtil::MapGerman3Or4LetterCodeToInternational2LetterCode(
                TranslationUtil::MapFake3LetterEnglishLanguagesCodesToGermanLanguageCodes(lang));
        });
        for (const auto &lang : all_translator_languages)
            std::cerr << "LANG: " << lang << '\n';
    });

    return all_translator_languages;
}


void AddWikidataTranslationsToLookupTable(const std::string batch_results, wikidata_translation_lookup_table * const wikidata_info) {
    nlohmann::json wikidata_json(nlohmann::json::parse(batch_results));
    for (const auto &result : wikidata_json["results"]["bindings"]) {
        const std::string gnd_code(result["gnd"]["value"]);
        const std::string translation(result["title"]["value"]);
        const std::string lang(result["lang"]["value"]);
        const std::string wiki_id(FileUtil::GetLastPathComponent(result["item"]["value"]));
        if (StringUtil::StartsWith(translation, "Category:"))
            continue;
        wikidata_info->emplace(gnd_code, WikidataTranslation(translation, lang, wiki_id));
    }
}


void GetAllSubjectKeywordsGNDs(MARC::Reader * const authority_reader, std::unordered_set<std::string> * const all_subject_keyword_gnds) {
    while (const MARC::Record record = authority_reader->read()) {
        if (not record.hasTag("150"))
            continue;
        std::string gnd_code;
        MARC::GetGNDCode(record, &gnd_code);
        if (gnd_code.empty())
            continue;
        all_subject_keyword_gnds->emplace(gnd_code);
    }
    authority_reader->rewind();
}


void GetWikidataTranslationsForASingleRecord(const IniFile &ini_file, const std::string &gnd_code,
                                             wikidata_translation_lookup_table * const wikidata_translations) {
    std::string results;
    std::vector<std::string> gnd_codes({ gnd_code });
    DownloadWikidataTranslations(GetWikidataPostQuery(std::span{ gnd_codes }, GetAllTranslatorLanguages(ini_file)), &results);
    AddWikidataTranslationsToLookupTable(results, wikidata_translations);
}


void GetAllWikidataTranslations(MARC::Reader * const authority_reader, const IniFile &ini_file,
                                wikidata_translation_lookup_table * const wikidata_translations) {
    std::unordered_set<std::string> all_subject_keywords_gnds_set;
    GetAllSubjectKeywordsGNDs(authority_reader, &all_subject_keywords_gnds_set);
    const unsigned batch_size(WIKIDATA_FULL_DOWNLOAD_BATCH_SIZE);
    std::vector<std::string> all_subject_keywords_gnds_vector;
    std::copy(all_subject_keywords_gnds_set.begin(), all_subject_keywords_gnds_set.end(),
              std::back_inserter(all_subject_keywords_gnds_vector));
    auto all_subject_keywords_gnds(std::span(all_subject_keywords_gnds_vector.begin(), all_subject_keywords_gnds_vector.end()));
    const unsigned all_iterations(std::ceil(all_subject_keywords_gnds.size() / batch_size));
    for (unsigned iteration = 0; iteration <= all_iterations; ++iteration) {
        auto gnd_batch(
            all_subject_keywords_gnds.subspan(batch_size * iteration, (iteration == all_iterations) ? std::dynamic_extent : batch_size));
        const auto joined_gnds(StringUtil::Join(gnd_batch.begin(), gnd_batch.end(), "\n"));
        std::string batch_results;
        DownloadWikidataTranslations(GetWikidataPostQuery(gnd_batch, GetAllTranslatorLanguages(ini_file)), &batch_results);
        AddWikidataTranslationsToLookupTable(batch_results, wikidata_translations);
    }
    for (const auto &[key, value] : *wikidata_translations)
        std::cout << key << ": " << value.translation_ << "| " << value.language_ << "| " << value.wiki_id_ << '\n';
}


void ExtractWikidataTranslations(
    const MARC::Record &record,
    std::vector<TextLanguageCodeWikiIDStatusAndOriginTag> * const text_language_codes_wiki_ids_statuses_and_origin_tags,
    wikidata_translation_lookup_table * const wikidata_translations) {
    std::string gnd_code;
    MARC::GetGNDCode(record, &gnd_code);
    auto all_wikidata_translations_for_gnd_range(wikidata_translations->equal_range(gnd_code));
    std::unordered_multimap<std::string, WikidataTranslation> all_wikidata_translations_for_gnd;
    std::set<std::string> languages_seen;
    // In some cases we have several translations, so skip all but the first
    std::copy_if(all_wikidata_translations_for_gnd_range.first, all_wikidata_translations_for_gnd_range.second,
                 std::inserter(all_wikidata_translations_for_gnd, all_wikidata_translations_for_gnd.end()),
                 [&languages_seen](std::pair<std::string, WikidataTranslation> gnd_and_wikidata_translation) {
                     const auto lang(gnd_and_wikidata_translation.second.language_);
                     if (languages_seen.contains(lang))
                         return false;
                     languages_seen.emplace(lang);
                     return true;
                 });
    for (const auto &one_wikidata_translation : all_wikidata_translations_for_gnd) {
        const std::string translation(one_wikidata_translation.second.translation_);
        const std::string wiki_id(FileUtil::GetLastPathComponent(one_wikidata_translation.second.wiki_id_));
        const std::string language_code(TranslationUtil::MapGermanLanguageCodesToFake3LetterEnglishLanguagesCodes(
            TranslationUtil::MapInternational2LetterCodeToGerman3Or4LetterCode(one_wikidata_translation.second.language_)));
        std::cerr << "WIKI ID: " << wiki_id << ": " << translation << " (" << language_code << ")\n";
        if (language_code == "ger")
            continue;
        text_language_codes_wiki_ids_statuses_and_origin_tags->emplace_back(translation, language_code, wiki_id, UNRELIABLE_CAT2, "WIK",
                                                                            false);
    }
}


// Helper function for ExtractTranslations().
void FlushToDatabase(std::string &insert_statement) {
    DbTransaction transaction(shared_connection);
    // Remove trailing comma and space:
    insert_statement.resize(insert_statement.size() - 2);
    insert_statement += ';';
    shared_connection->queryRetryOrDie(insert_statement);
}


// Returns a string that looks like "(language_code='deu' OR language_code='eng')" etc.
std::string GenerateLanguageCodeWhereClause(
    const std::vector<TextLanguageCodeWikiIDStatusAndOriginTag> &text_language_codes_wiki_ids_statuses_and_origin_tags) {
    std::string partial_where_clause;

    partial_where_clause += '(';
    std::set<std::string> already_seen;
    for (const auto &text_language_code_status_and_origin : text_language_codes_wiki_ids_statuses_and_origin_tags) {
        const std::string &language_code(std::get<1>(text_language_code_status_and_origin));
        if (already_seen.find(language_code) == already_seen.cend()) {
            already_seen.insert(language_code);
            if (partial_where_clause.length() > 1)
                partial_where_clause += " OR ";
            partial_where_clause += "language_code='" + language_code + "'";
        }
    }
    partial_where_clause += ')';

    return partial_where_clause;
}


static unsigned no_gnd_code_count;


std::string GetPseudoGNDSigil(const IniFile &ini_file) {
    static const std::string pseudo_gnd_sigil(
        ini_file.lookup("Configuration", "pseudo_gnd_sigil", nullptr) ? ini_file.getString("Configuration", "pseudo_gnd_sigil") : "");
    return pseudo_gnd_sigil;
}


bool HasPseudoGNDCode(const MARC::Record &record, const IniFile &ini_file, std::string * const pseudo_gnd_code) {
    const std::string pseudo_gnd_sigil(GetPseudoGNDSigil(ini_file));
    if (pseudo_gnd_sigil.empty())
        return false;
    pseudo_gnd_code->clear();
    for (const auto &_035_field : record.getTagRange("035")) {
        const MARC::Subfields _035_subfields(_035_field.getSubfields());
        const std::string _035a_field(_035_subfields.getFirstSubfieldWithCode('a'));
        if (StringUtil::StartsWith(_035a_field, pseudo_gnd_sigil)) {
            *pseudo_gnd_code = _035a_field.substr(pseudo_gnd_sigil.length());
            return not pseudo_gnd_code->empty();
        }
    }
    return false;
}


std::string IsPriorityEntry(const MARC::Record &record) {
    return record.hasTag("PRI") ? "true" : "false";
}


bool KeywordPPNAlreadyInDatabase(const std::string &ppn) {
    return (ppns_already_present->find(ppn) != ppns_already_present->end());
}


bool ExtractTranslationsForASingleRecord(const MARC::Record * const record, const IniFile &ini_file,
                                         wikidata_translation_lookup_table * const wikidata_translations,
                                         const bool insert_only_non_existing = false, const bool download_full_wikidata = false) {
    // Skip records that are not GND records:
    std::string gnd_code;
    std::string pseudo_gnd_code;
    if (not MARC::GetGNDCode(*record, &gnd_code) and not HasPseudoGNDCode(*record, ini_file, &pseudo_gnd_code))
        return true;

    const std::string ppn(record->getControlNumber());
    if (insert_only_non_existing and KeywordPPNAlreadyInDatabase(ppn))
        return true;

    if (not record->hasTag("150"))
        return true;

    if (not download_full_wikidata)
        GetWikidataTranslationsForASingleRecord(ini_file, gnd_code, wikidata_translations);

    // Extract all synonyms and translations:
    std::vector<TextLanguageCodeWikiIDStatusAndOriginTag> text_language_codes_wiki_ids_statuses_and_origin_tags;
    ExtractGermanTerms(*record, &text_language_codes_wiki_ids_statuses_and_origin_tags);
    ExtractGermanSynonyms(*record, &text_language_codes_wiki_ids_statuses_and_origin_tags);
    ExtractNonGermanTranslations(*record, &text_language_codes_wiki_ids_statuses_and_origin_tags);
    ExtractWikidataTranslations(*record, &text_language_codes_wiki_ids_statuses_and_origin_tags, wikidata_translations);
    if (text_language_codes_wiki_ids_statuses_and_origin_tags.empty())
        return true;

    ++keyword_count;

    // Remove entries for which authoritative translation were shipped to us from the BSZ:
    // if prev_version_id!=null -> it's a successor
    // if next_version_id!=null -> it has been modified by translation tool (stored proc.)
    {
        DbTransaction transaction(shared_connection);
        shared_connection->queryRetryOrDie("DELETE FROM keyword_translations WHERE ppn=\"" + ppn + "\" AND "
                                           + "prev_version_id IS NULL AND next_version_id IS NULL AND translator IS NULL AND "
                                           + GenerateLanguageCodeWhereClause(text_language_codes_wiki_ids_statuses_and_origin_tags));
    }

    if (not MARC::GetGNDCode(*record, &gnd_code)) {
        ++no_gnd_code_count;
        gnd_code = "0";
    }

    std::vector<std::string> gnd_systems;
    for (const auto &_065_field : record->getTagRange("065")) {
        std::vector<std::string> _065a_subfields(_065_field.getSubfields().extractSubfields('a'));
        gnd_systems.insert(gnd_systems.begin(), _065a_subfields.begin(), _065a_subfields.end());
    }
    std::string gnd_system(StringUtil::Join(gnd_systems, ","));
    const std::string INSERT_STATEMENT_START(
        "INSERT IGNORE INTO keyword_translations (ppn, gnd_code, wikidata_id, language_code,"
        " translation, status, origin, gnd_system, german_updated, priority_entry) VALUES ");
    std::string insert_statement(INSERT_STATEMENT_START);

    size_t row_counter(0);
    const size_t MAX_ROW_COUNT(1000);

    // Update the database:
    for (const auto &text_language_code_status_and_origin : text_language_codes_wiki_ids_statuses_and_origin_tags) {
        const std::string language_code(shared_connection->escapeString(std::get<1>(text_language_code_status_and_origin)));
        const std::string translation(shared_connection->escapeString(std::get<0>(text_language_code_status_and_origin)));
        const std::string wiki_id(shared_connection->escapeString(std::get<2>(text_language_code_status_and_origin)));
        const std::string status(StatusToString(std::get<3>(text_language_code_status_and_origin)));
        const std::string &origin(std::get<4>(text_language_code_status_and_origin));
        const bool updated_german(std::get<5>(text_language_code_status_and_origin));

        // check if there is an existing entry, insert ignore does not work here
        // any longer due to the deleted unique key for the history functionality.
        // Unsure if it worked before due to translator=null not affecting a ukey in mysql
        // Also prevent inserting reliable translations if a tranlator bases translation already exists in the database
        // prevent ambiguous associations when inserting new translations
        const std::string CHECK_EXISTING("SELECT ppn FROM keyword_translations WHERE ppn='" + ppn + "' AND language_code='" + language_code
                                         + "' AND (status='" + status + "'" + (status == StatusToString(RELIABLE) ? " OR status='new'" : "")
                                         + ")");
        {
            DbTransaction transaction(shared_connection);
            shared_connection->queryRetryOrDie(CHECK_EXISTING);
            DbResultSet result(shared_connection->getLastResultSet());
            if (not result.empty()) {
                continue;
            }
        }
        insert_statement += "('" + ppn + "', '" + gnd_code + "', '" + wiki_id + "', '" + language_code + "', '" + translation + "', '"
                            + status + "', '" + origin + "', '" + gnd_system + "', " + (updated_german ? "true" : "false") + ", "
                            + IsPriorityEntry(*record) + "), ";
        if (++row_counter > MAX_ROW_COUNT) {
            FlushToDatabase(insert_statement);
            insert_statement = INSERT_STATEMENT_START;
            row_counter = 0;
        }
    }
    if (row_counter > 0)
        FlushToDatabase(insert_statement);

    return true;
}


void ExtractTranslationsForAllRecords(MARC::Reader * const authority_reader, const IniFile &ini_file,
                                      const bool insert_only_non_existing = false, const bool download_full_wikidata = false) {
    wikidata_translation_lookup_table wikidata_translations;
    if (download_full_wikidata) {
        GetAllWikidataTranslations(authority_reader, ini_file, &wikidata_translations);
    }

    // std::exit(EXIT_SUCCESS);

    while (const MARC::Record record = authority_reader->read()) {
        if (not ExtractTranslationsForASingleRecord(&record, ini_file, &wikidata_translations, insert_only_non_existing,
                                                    download_full_wikidata))
            LOG_ERROR("error while extracting translations from \"" + authority_reader->getPath() + "\"");
    }
    std::cerr << "Added " << keyword_count << " keywords to the translation database.\n";
    std::cerr << "Found " << german_term_count << " german terms.\n";
    std::cerr << "Found " << translation_count << " translations in the norm data. (" << additional_hits
              << " due to 'ram', 'lcsh', 'embne' and 'nsbncf' entries.)\n";
    std::cerr << "Found " << synonym_count << " synonym entries.\n";
    std::cerr << no_gnd_code_count << " authority records had no GND code.\n";
}


void GetAllKeywordPPNsFromDatabase(std::unordered_set<std::string> * const keyword_ppns_in_database) {
    const std::string query("SELECT DISTINCT ppn FROM keyword_translations");
    shared_connection->queryRetryOrDie(query);
    DbResultSet result(shared_connection->getLastResultSet());
    while (const auto row = result.getNextRow())
        keyword_ppns_in_database->emplace(row["ppn"]);
}


int Main(int argc, char **argv) {
    if (argc < 2 or argc > 4)
        Usage();


    bool insert_only_non_existing(false);
    if (argc >= 3 and std::strcmp(argv[1], "--insert-only-non-existing") == 0) {
        insert_only_non_existing = true;
        --argc;
        ++argv;
    }

    bool download_full_wikidata(false);
    if (argc == 3 and std::strcmp(argv[1], "--download-full-wikidata") == 0) {
        download_full_wikidata = true;
        --argc;
        ++argv;
    }


    std::unique_ptr<MARC::Reader> authority_marc_reader(MARC::Reader::Factory(argv[1], MARC::FileType::BINARY));
    try {
        const IniFile ini_file(CONF_FILE_PATH);
        const std::string sql_database(ini_file.getString("Database", "sql_database"));
        const std::string sql_username(ini_file.getString("Database", "sql_username"));
        const std::string sql_password(ini_file.getString("Database", "sql_password"));
        DbConnection db_connection(DbConnection::MySQLFactory(sql_database, sql_username, sql_password));
        shared_connection = &db_connection;

        std::unordered_set<std::string> keyword_ppns_in_database;
        GetAllKeywordPPNsFromDatabase(&keyword_ppns_in_database);
        if (insert_only_non_existing) {
            ppns_already_present = &keyword_ppns_in_database;
        }

        if (not download_full_wikidata and keyword_ppns_in_database.size() < 5000) {
            LOG_INFO(
                "Few items in DB => expected number of single Wikidata queries too high \
                      - forcing download-full-wikidata");
            download_full_wikidata = true;
        }

        if (not download_full_wikidata and not insert_only_non_existing) {
            LOG_INFO(
                "insert-only-not-existing missing => expected number of single Wikidata queries too high \
                     - forcing donwload-full-wikidata");
            download_full_wikidata = true;
        }

        ExtractTranslationsForAllRecords(authority_marc_reader.get(), ini_file, insert_only_non_existing, download_full_wikidata);
    } catch (const std::exception &x) {
        LOG_ERROR("caught exception: " + std::string(x.what()));
    }

    return EXIT_SUCCESS;
}

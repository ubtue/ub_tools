/** \file    PhaseEnrichKeywordsWithTitleWords.cc
 *  \brief   A tool for adding keywords extracted from titles to MARC records.
 *  \author  Dr. Johannes Ruscheinski
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

#include "PhaseEnrichKeywordsWithTitleWords.h"

#include <algorithm>
#include <fstream>
#include <iostream>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <unordered_set>
#include <cassert>
#include <cstdlib>
#include <cstring>
#include "DirectoryEntry.h"
#include "Leader.h"
#include "MarcUtil.h"
#include "RegexMatcher.h"
#include "Stemmer.h"
#include "StringUtil.h"
#include "Subfields.h"
#include "TextUtil.h"
#include "util.h"


// The following constant is used to reject cases where a key phrase consists of exactly one word and
// that single word is not as least as long as the constant.  This is used to try to increase precision
// but, of course, decreases recall.  Part of the reason why this seems necessary is the crappy stemmer.
constexpr auto MIN_SINGLE_STEMMED_KEYWORD_LENGTH(7);

static std::unordered_map <std::string, std::set<std::string>> stemmed_keyword_to_stemmed_keyphrases_map__GLOBAL;
static std::unordered_map <std::string, std::string> stemmed_keyphrases_to_unstemmed_keyphrases_map__GLOBAL;
static std::map <std::string, std::unordered_set<std::string>> language_codes_to_stopword_sets__GLOBAL;

unsigned records_with_keywords_count(0), keywords_count(0), augmented_record_count(0);


void LoadStopwords(File * const input, std::unordered_set <std::string> * const stopwords_set) {
    unsigned count(0);
    while (not input->eof()) {
        const std::string line(input->getline());
        if (line.empty() or line[0] == ';') // Empty or comment line?
            continue;

        stopwords_set->insert(StringUtil::ToLower(line));
        ++count;
    }
}


void FilterOutStopwords(const std::unordered_set <std::string> &stopwords, std::vector <std::string> * const words) {
    std::vector <std::string> filtered_words;
    bool removed_at_least_one_word(false);
    for (const auto &word : *words) {
        if (stopwords.find(word) == stopwords.end())
            filtered_words.emplace_back(word);
        else
            removed_at_least_one_word = true;
    }
    if (removed_at_least_one_word)
        words->swap(filtered_words);
}


std::string VectorToString(const std::vector <std::string> &v) {
    std::string vector_as_string;
    for (std::vector<std::string>::const_iterator entry(v.cbegin()); entry != v.cend(); ++entry) {
        vector_as_string += *entry;
        if ((entry + 1) != v.cend())
            vector_as_string += ' ';
    }
    return vector_as_string;
}


auto constexpr MIN_WORD_LENGTH(3); // At least this many characters have to be in a word for to consider it
// to be "interesting".


inline std::string FilterOutNonwordChars(const std::string &phrase) {
    std::vector <std::string> phrase_as_vector;
    TextUtil::ChopIntoWords(phrase, &phrase_as_vector, MIN_WORD_LENGTH);
    return VectorToString(phrase_as_vector);
}


// Lowercases and stems "keyword_phrase" and chops it into `words'.  Populates
// "stemmed_keyword_to_stemmed_keyphrases_map" and "stemmed_keyphrases_to_unstemmed_keyphrases_map".
// The former maps from each individual stemmed word to the entire cleaned up and stemmed key phrase and the
// latter maps from the cleaned up and stemmed key phrase to the original key phrase.
void ProcessKeywordPhrase(
        const std::string &keyword_phrase, const Stemmer * const stemmer,
        std::unordered_map <std::string, std::set<std::string>> * const stemmed_keyword_to_stemmed_keyphrases_map,
        std::unordered_map <std::string, std::string> * const stemmed_keyphrases_to_unstemmed_keyphrases_map) {
    std::string cleaned_up_phrase(keyword_phrase);

    // Convert "surname, first_name" to "first_name surname" assuming we only have a comma if the keyphrase
    // consist of a name:
    const size_t comma_pos(keyword_phrase.find(','));
    if (comma_pos != std::string::npos)
        cleaned_up_phrase = cleaned_up_phrase.substr(comma_pos + 1) + " "
                + cleaned_up_phrase.substr(0, comma_pos);

    cleaned_up_phrase = FilterOutNonwordChars(cleaned_up_phrase);

    const std::string stemmed_phrase(stemmer == nullptr ? cleaned_up_phrase : stemmer->stem(cleaned_up_phrase));
    std::string lowercase_stemmed_phrase;
    TextUtil::UTF8ToLower(stemmed_phrase, &lowercase_stemmed_phrase);
    (*stemmed_keyphrases_to_unstemmed_keyphrases_map)[lowercase_stemmed_phrase] = keyword_phrase;
    std::vector <std::string> stemmed_words;
    StringUtil::Split(lowercase_stemmed_phrase, ' ', &stemmed_words);
    for (const auto &stemmed_word : stemmed_words) {
        auto iter(stemmed_keyword_to_stemmed_keyphrases_map->find(stemmed_word));
        if (iter == stemmed_keyword_to_stemmed_keyphrases_map->end())
            (*stemmed_keyword_to_stemmed_keyphrases_map)[stemmed_word] =
                    std::set < std::string > {lowercase_stemmed_phrase,};
        else
            (*stemmed_keyword_to_stemmed_keyphrases_map)[stemmed_word].insert(lowercase_stemmed_phrase);
    }
}


// Replace patterns like "Jahrhundert XX" w/ "XX. Jahrhundert" etc.  If we don't have a match we
// return the original string.
std::string CanonizeCentury(const std::string &century_candidate) {
    static RegexMatcher *matcher(RegexMatcher::RegexMatcherFactory("[jJ]ahrhundert (\\d+)\\.?"));
    if (not matcher->matched(century_candidate))
        return century_candidate;

    std::string ordinal(century_candidate.substr(12));
    if (ordinal[ordinal.size() - 1] != '.')
        ordinal += '.';

    return ordinal + " " + century_candidate.substr(0, 12);
}


size_t ExtractKeywordsFromKeywordChainFields(
        const MarcUtil::Record &record,
        const Stemmer * const stemmer,
        std::unordered_map <std::string, std::set<std::string>> * const stemmed_keyword_to_stemmed_keyphrases_map,
        std::unordered_map <std::string, std::string> * const stemmed_keyphrases_to_unstemmed_keyphrases_map) {
    size_t keyword_count(0);
    // TODO: Update to new MarcUtil API.
    const std::vector <DirectoryEntry> &dir_entries(record.getDirEntries());
    const std::vector <std::string> &fields(record.getFields());
    const auto _689_iterator(DirectoryEntry::FindField("689", dir_entries));
    if (_689_iterator != dir_entries.end()) {
        size_t field_index(_689_iterator - dir_entries.begin());
        while (field_index < fields.size() and dir_entries[field_index].getTag() == "689") {
            const Subfields subfields(fields[field_index]);
            const std::string subfield_a_value(subfields.getFirstSubfieldValue('a'));
            if (not subfield_a_value.empty()) {
                std::string keyphrase(subfield_a_value);
                const std::string subfield_c_value(subfields.getFirstSubfieldValue('c'));
                if (not subfield_c_value.empty())
                    keyphrase += " " + subfield_c_value;
                ProcessKeywordPhrase(CanonizeCentury(keyphrase), stemmer, stemmed_keyword_to_stemmed_keyphrases_map,
                                     stemmed_keyphrases_to_unstemmed_keyphrases_map
                );
                ++keyword_count;
            }

            ++field_index;
        }
    }

    return keyword_count;
}


size_t ExtractKeywordsFromIndividualKeywordFields(
        const MarcUtil::Record &record,
        const Stemmer * const stemmer,
        std::unordered_map <std::string, std::set<std::string>> * const stemmed_keyword_to_stemmed_keyphrases_map,
        std::unordered_map <std::string, std::string> * const stemmed_keyphrases_to_unstemmed_keyphrases_map) {
    size_t keyword_count(0);
    std::vector <std::string> keyword_phrases;
    static const std::string SUBFIELD_IGNORE_LIST("02"); // Do not extract $0 and $2.
    record.extractAllSubfields("600:610:611:630:650:653:656", &keyword_phrases, SUBFIELD_IGNORE_LIST);
    for (const auto &keyword_phrase : keyword_phrases) {
        ProcessKeywordPhrase(CanonizeCentury(keyword_phrase), stemmer, stemmed_keyword_to_stemmed_keyphrases_map,
                             stemmed_keyphrases_to_unstemmed_keyphrases_map
        );
        ++keyword_count;
    }

    return keyword_count;
}


size_t ExtractAllKeywords(
        const MarcUtil::Record &record,
        std::unordered_map <std::string, std::set<std::string>> * const stemmed_keyword_to_stemmed_keyphrases_map,
        std::unordered_map <std::string, std::string> * const stemmed_keyphrases_to_unstemmed_keyphrases_map)
{
    const std::string language_code(record.getLanguage());
    const Stemmer * const stemmer(language_code.empty() ? nullptr : Stemmer::StemmerFactory(language_code));

    size_t extracted_count(ExtractKeywordsFromKeywordChainFields(record, stemmer,
                                                                 stemmed_keyword_to_stemmed_keyphrases_map,
                                                                 stemmed_keyphrases_to_unstemmed_keyphrases_map
    ));
    return extracted_count;
}


// Checks to see if "value" is in any of the sets in "key_to_set_map".
bool ContainedInMapValues(const std::string &value, const std::unordered_map <std::string, std::set<std::string>> &key_to_set_map) {
    for (const auto &key_and_set : key_to_set_map) {
        for (const auto &set_entry : key_and_set.second) {
            if (set_entry == value)
                return true;
        }
    }

    return false;
}


PipelinePhaseState PhaseEnrichKeywordsWithTitleWords::preprocess(const MarcUtil::Record &record, std::string * const) {
    const size_t extracted_count(
            ExtractAllKeywords(record, &stemmed_keyword_to_stemmed_keyphrases_map__GLOBAL, &stemmed_keyphrases_to_unstemmed_keyphrases_map__GLOBAL));
    if (extracted_count > 0) {
        ++records_with_keywords_count;
        keywords_count += extracted_count;
    }
    return SUCCESS;
};


PipelinePhaseState PhaseEnrichKeywordsWithTitleWords::process(MarcUtil::Record &record, std::string * const) {
    // Look for a title...
    // TODO: Update to new MarcUtil API.
    const std::vector <DirectoryEntry> &dir_entries(record.getDirEntries());
    const auto entry_iterator(DirectoryEntry::FindField("245", dir_entries));
    if (entry_iterator == dir_entries.end())
        return SUCCESS;

    // ...in subfields 'a', 'b', 'c' and 'p':
    const size_t title_index(entry_iterator - dir_entries.begin());
    const std::vector <std::string> &fields(record.getFields());
    Subfields subfields(fields[title_index]);
    if (not subfields.hasSubfield('a'))
        return SUCCESS;

    std::string title;
    for (const char subfield_code : "abp") {
        const auto begin_end = subfields.getIterators(subfield_code);
        if (begin_end.first != begin_end.second)
            title += " " + begin_end.first->second;
    }
    assert(not title.empty());

    std::string lowercase_title;
    TextUtil::UTF8ToLower(title, &lowercase_title);
    std::vector <std::string> title_words;
    TextUtil::ChopIntoWords(lowercase_title, &title_words, MIN_WORD_LENGTH);

    // Remove language-appropriate stop words from the title words:
    const std::string language_code(record.getLanguage());
    const auto code_and_stopwords(language_codes_to_stopword_sets__GLOBAL.find(language_code));
    if (code_and_stopwords != language_codes_to_stopword_sets__GLOBAL.end())
        FilterOutStopwords(code_and_stopwords->second, &title_words);
    if (language_code != "eng") // Hack because people suck at cataloging!
        FilterOutStopwords(language_codes_to_stopword_sets__GLOBAL.find("eng")->second, &title_words);

    if (title_words.empty())
        return SUCCESS;

    // If we have an appropriate stemmer, replace the title words w/ stemmed title words:
    const Stemmer * const stemmer(language_code.empty() ? nullptr : Stemmer::StemmerFactory(language_code));
    if (stemmer != nullptr) {
        std::vector <std::string> stemmed_title_words;
        for (const auto &title_word : title_words)
            stemmed_title_words.emplace_back(stemmer->stem(title_word));
        title_words.swap(stemmed_title_words);
    }

    std::unordered_map <std::string, std::set<std::string>> local_stemmed_keyword_to_stemmed_keyphrases_map;
    std::unordered_map <std::string, std::string> local_stemmed_keyphrases_to_unstemmed_keyphrases_map;
    ExtractAllKeywords(record, &local_stemmed_keyword_to_stemmed_keyphrases_map,
                       &local_stemmed_keyphrases_to_unstemmed_keyphrases_map
    );

    // Find title phrases that match stemmed keyphrases:
    std::unordered_set <std::string> new_keyphrases;
    for (const auto &title_word : title_words) {
        const auto word_and_set(stemmed_keyword_to_stemmed_keyphrases_map__GLOBAL.find(title_word));
        if (word_and_set == stemmed_keyword_to_stemmed_keyphrases_map__GLOBAL.end())
            continue;

        for (const std::string &stemmed_phrase : word_and_set->second) {
            if (ContainedInMapValues(stemmed_phrase, local_stemmed_keyword_to_stemmed_keyphrases_map))
                continue; // We already have this in our MARC record.

            std::vector <std::string> stemmed_phrase_as_vector;
            StringUtil::Split(stemmed_phrase, ' ', &stemmed_phrase_as_vector);
            if (stemmed_phrase_as_vector.size() == 1 and stemmed_phrase_as_vector[0].length() < MIN_SINGLE_STEMMED_KEYWORD_LENGTH)
                continue;

            if (TextUtil::FindSubstring(title_words, stemmed_phrase_as_vector) != title_words.end()) {
                const auto stemmed_and_unstemmed_keyphrase(stemmed_keyphrases_to_unstemmed_keyphrases_map__GLOBAL.find(stemmed_phrase));
                new_keyphrases.insert(stemmed_and_unstemmed_keyphrase->second);
            }
        }
    }

    if (new_keyphrases.empty())
        return SUCCESS;

    // Augment the record with new keywords derived from title words:
    for (const auto &new_keyword : new_keyphrases) {
        // TODO: use Subfield class.
        const std::string field_contents("  ""\x1F""a" + new_keyword);
        record.insertField("601", field_contents);
    }

    ++augmented_record_count;
    return SUCCESS;
};

const std::string stopword_file_path("/usr/local/ub_tools/cpp/data/");

void LoadStopwords(const std::string &language_code) {
    File stopwords(stopword_file_path + "stopwords." + language_code, "rm");
    if (not stopwords)
        Error("can't open \"" + stopword_file_path + "stopwords." + language_code + "\" for reading!");
    std::unordered_set<std::string> stopwords_set;
    LoadStopwords(&stopwords, &stopwords_set);
    language_codes_to_stopword_sets__GLOBAL[language_code] = stopwords_set;
}

PhaseEnrichKeywordsWithTitleWords::PhaseEnrichKeywordsWithTitleWords() {
    LoadStopwords("dut");
    LoadStopwords("eng");
    LoadStopwords("fre");
    LoadStopwords("ger");
    LoadStopwords("ita");
    LoadStopwords("spa");
    LoadStopwords("swe");
}


PhaseEnrichKeywordsWithTitleWords::~PhaseEnrichKeywordsWithTitleWords() {
    std::cerr << "Enrich keywords with title words:\n";
    std::cerr << "\t" << records_with_keywords_count << " records had keywords.\n";
    std::cerr << "\t" << keywords_count << " keywords were extracted of which " << stemmed_keyword_to_stemmed_keyphrases_map__GLOBAL.size() << " were unique.\n";
    std::cerr << "\t" << augmented_record_count << " records were augmented w/ additional keywords.\n";
}
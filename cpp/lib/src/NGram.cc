/** \file    NGram.cc
 *  \brief   Implementation of ngram related utility functions.
 *  \author  Dr. Johannes Ruscheinski
 */

/*
 *  Copyright 2003-2009 Project iVia.
 *  Copyright 2003-2009 The Regents of The University of California.
 *  Copyright 2019 Universitätsbibliothek Tübingen.
 *
 *  This file is part of the libiViaCore package.
 *
 *  The libiViaCore package is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public License as published
 *  by the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  libiViaCore is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public License
 *  along with libiViaCore; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
#include "NGram.h"
#include <algorithm>
#include <iostream>//XXX
#include <fstream>
#include <unordered_set>
#include <climits>
#include "BinaryIO.h"
#include "FileUtil.h"
#include "TextUtil.h"
#include "UBTools.h"
#include "util.h"


namespace {


void Split(const std::wstring &s, std::vector<std::wstring> * const words) {
    bool in_word(false);
    std::wstring new_word;
    for (const auto ch : s) {
        if (std::iswspace(ch)) {
            if (in_word) {
                words->push_back(new_word);
                in_word = false;
            }
        } else if (in_word)
            new_word += ch;
        else {
            in_word = true;
            new_word = ch;
        }
    }

    if (in_word)
        words->push_back(new_word);
}


// LoadLanguageModel -- loads a language model from "path_name" into "ngram_counts".
//
void LoadLanguageModel(const std::string &path_name, NGram::NGramCounts * const ngram_counts) {
    const auto input(FileUtil::OpenInputFileOrDie(path_name));
    if (input->fail())
        LOG_ERROR("can't open language model file \"" + path_name + "\" for reading!");

    size_t entry_count;
    BinaryIO::ReadOrDie(*input, &entry_count);
    ngram_counts->reserve(entry_count);
std::cerr << path_name << " constains " << entry_count << " entries.\n";

    for (unsigned i(0); i < entry_count; ++i) {
        std::wstring ngram;
std::cerr << "About to read the ngram\n";
        BinaryIO::ReadOrDie(*input, &ngram);

std::cerr << "About to read the score\n";
        double score;
        BinaryIO::ReadOrDie(*input, &score);

        (*ngram_counts)[ngram] = score;
    }
std::cerr << "Done.\n\n\n";
}


struct IndexAndRelFrequency {
    int index_;
    double rel_frequency_;
    double max_distance_;
public:
    IndexAndRelFrequency() = default;
    IndexAndRelFrequency(const IndexAndRelFrequency &other) = default;
    IndexAndRelFrequency(const int index, const double rel_frequency): index_(index), rel_frequency_(rel_frequency) { }
};


class LanguageModel: public std::unordered_map<std::wstring, IndexAndRelFrequency> {
    std::string language_;
    const NGram::DistanceType distance_type_;
    const int topmost_use_count_;
    double max_distance_;
public:
    LanguageModel(const std::string &language, const NGram::NGramCounts &ngram_counts,
                  const NGram::DistanceType distance_type, const unsigned topmost_use_count);
    std::string getLanguage() const { return language_; }
    double distance(const std::wstring &ngram, const int position) const;
};


LanguageModel::LanguageModel(const std::string &language, const NGram::NGramCounts &ngram_counts,
                             const NGram::DistanceType distance_type, const unsigned topmost_use_count)
    : language_(language), distance_type_(distance_type), topmost_use_count_(topmost_use_count)
{
    if (topmost_use_count > ngram_counts.size())
        LOG_ERROR("request to use more ngrams than are available cannot be satisfied!");

    int position(1);
    double total_distance(0.0);
    for (auto ngram_count(ngram_counts.begin()); ngram_count != ngram_counts.end(); ++ngram_count, ++position) {
        emplace(ngram_count->first, IndexAndRelFrequency(position, ngram_count->second));
        total_distance += ngram_count->second;
    }

    max_distance_ = (distance_type == NGram::SIMPLE_DISTANCE) ? topmost_use_count : total_distance;
}


double LanguageModel::distance(const std::wstring &ngram, const int position) const {
    const_iterator iter(find(ngram));
    if (iter == end() or iter->second.index_ >= topmost_use_count_)
        return max_distance_;
    else if (distance_type_ == NGram::WEIGHTED_DISTANCE) {
        double total_distance(0.0);
        for (int i(std::min(position, iter->second.index_)); i < std::max(position, iter->second.index_); ++i)
            total_distance += iter->second.rel_frequency_;
        return total_distance;
    } else
        return std::abs(iter->second.index_ - position);
}


// LoadLanguageModels -- returns true if at least one language model was loaded
//                       from "language_models_directory"
//
bool LoadLanguageModels(const std::string &language_models_directory, const NGram::DistanceType distance_type,
                        const unsigned topmost_use_count, std::vector<LanguageModel> * const language_models)
{
    FileUtil::Directory directory(language_models_directory, ".+\\.lm");
    bool found_at_least_one_language_model(false);
    for (const auto &dir_entry : directory) {
        NGram::NGramCounts language_model;
        LoadLanguageModel(language_models_directory + "/" + dir_entry.getName(), &language_model);

        const std::string language(dir_entry.getName().substr(0, dir_entry.getName().length() - 3));
        language_models->push_back(LanguageModel(language, language_model, distance_type, topmost_use_count));
        found_at_least_one_language_model = true;
    }

    return found_at_least_one_language_model;
}


// Filters out anything except for letters and space characters and converts UTF8 to wide characters.
std::wstring PreprocessText(const std::string &utf8_string) {
    std::wstring wchar_string;
    if (unlikely(not TextUtil::UTF8ToWCharString(utf8_string, &wchar_string)))
        LOG_ERROR("failed to convert UTF* to wide characters!");

    std::wstring filtered_string;
    filtered_string.reserve(wchar_string.size());
    for (const auto ch : wchar_string) {
        if (std::iswalpha(ch) or std::iswspace(ch))
            filtered_string += ch;
    }

    return filtered_string;
}


} // unnamed namespace


namespace NGram {


SortedNGramCounts::SortedNGramCounts(const NGramCounts &ngram_counts, const SortOrder sort_order) {
    resize(ngram_counts.size());
    unsigned i = 0;
    for (NGramCounts::const_iterator entry(ngram_counts.begin()); entry != ngram_counts.end();
         ++entry, ++i)
        (*this)[i] = std::pair<std::wstring, double>(entry->first, entry->second);
    bool (*sort_func)(const std::pair<std::wstring, double> &lhs, const std::pair<std::wstring, double> &rhs);
    if (sort_order == ASCENDING_ORDER)
        sort_func = IsLessThan;
    else
        sort_func = IsGreaterThan;
    std::sort(begin(), end(), sort_func);
}


bool SortedNGramCounts::IsLessThan(const std::pair<std::wstring, double> &lhs, const std::pair<std::wstring, double> &rhs) {
    if (lhs.second == rhs.second)
        return lhs.first > rhs.first ;
    return lhs.second < rhs.second;
}


bool SortedNGramCounts::IsGreaterThan(const std::pair<std::wstring, double> &lhs, const std::pair<std::wstring, double> &rhs) {
    if (lhs.second == rhs.second)
        return lhs.first < rhs.first ;
    return lhs.second > rhs.second;
}


void CreateLanguageModel(std::istream &input, NGramCounts * const ngram_counts,
			 SortedNGramCounts * const top_ngrams, const unsigned ngram_number_threshold,
			 const unsigned topmost_use_count)
{
    const std::string file_contents(std::istreambuf_iterator<char>(input), {});
    const std::wstring filtered_text(PreprocessText(file_contents));

    std::vector<std::wstring> words;
    Split(filtered_text, &words);

    unsigned total_ngram_count(0);
    for (std::vector<std::wstring>::const_iterator word(words.begin()); word != words.end(); ++word) {
        const std::wstring funny_word(L"_" + *word + L"_");
        const std::wstring::size_type funny_word_length(funny_word.length());
        std::wstring::size_type length(funny_word_length);
        for (unsigned i = 0; i < funny_word_length; ++i, --length) {
            if (length > 4) {
                const std::wstring ngram(funny_word.substr(i, 5));
                NGramCounts::iterator ngram_count = ngram_counts->find(ngram);
                if (ngram_count == ngram_counts->end())
                    (*ngram_counts)[ngram] = 1.0;
                else
                    ++(*ngram_counts)[ngram];
                ++total_ngram_count;
            }

            if (length > 3) {
                const std::wstring ngram(funny_word.substr(i, 4));
                NGramCounts::iterator ngram_count = ngram_counts->find(ngram);
                if (ngram_count == ngram_counts->end())
                    (*ngram_counts)[ngram] = 1.0;
                else
                    ++(*ngram_counts)[ngram];
                ++total_ngram_count;
            }

            if (length > 2) {
                const std::wstring ngram(funny_word.substr(i, 3));
                NGramCounts::iterator ngram_count = ngram_counts->find(ngram);
                if (ngram_count == ngram_counts->end())
                    (*ngram_counts)[ngram] = 1.0;
                else
                    ++(*ngram_counts)[ngram];
                ++total_ngram_count;
            }

            if (length > 1) {
                const std::wstring ngram(funny_word.substr(i, 2));
                NGramCounts::iterator ngram_count = ngram_counts->find(ngram);
                if (ngram_count == ngram_counts->end())
                    (*ngram_counts)[ngram] = 1.0;
                else
                    ++(*ngram_counts)[ngram];
                ++total_ngram_count;
            }

            const std::wstring ngram(funny_word.substr(i, 1));
            NGramCounts::iterator ngram_count = ngram_counts->find(ngram);
            if (ngram_count == ngram_counts->end())
                (*ngram_counts)[ngram] = 1.0;
            else
                ++(*ngram_counts)[ngram];
            ++total_ngram_count;
        }
    }

    // Normalise the ngram frequencies:
    for (NGramCounts::iterator ngram_count(ngram_counts->begin()); ngram_count != ngram_counts->end(); ++ngram_count)
        ngram_count->second /= total_ngram_count;

    // Remove entries that occur less than "ngram_number_threshold" times:
    for (NGramCounts::iterator entry(ngram_counts->begin()); entry != ngram_counts->end(); ++entry)
        if (entry->second < ngram_number_threshold)
            ngram_counts->erase(entry->first);

    // Create a sorted list of winners from "ngram_counts":
    *top_ngrams = SortedNGramCounts(*ngram_counts, SortedNGramCounts::DESCENDING_ORDER);

    // If "topmost_use_count" has been set, return at most "topmost_use_count" winners:
    if (topmost_use_count != 0 and top_ngrams->size() > topmost_use_count)
        top_ngrams->resize(topmost_use_count);
}


void ClassifyLanguage(std::istream &input, std::vector<std::string> * const top_languages, const std::set<std::string> &considered_languages,
                      const DistanceType distance_type, const unsigned ngram_number_threshold, const unsigned topmost_use_count,
                      const double alternative_cutoff_factor, const std::string &override_language_models_directory)
{
    // Determine the language models directroy:
    const std::string language_models_directory(override_language_models_directory.empty() ? UBTools::GetTuelibPath() + "/language_models"
                                                                                           : override_language_models_directory);

    NGramCounts unknown_language_model;
    SortedNGramCounts sorted_unknown_language_model;
    CreateLanguageModel(input, &unknown_language_model, &sorted_unknown_language_model, ngram_number_threshold, topmost_use_count);

    static bool models_already_loaded(false);
    static std::vector<LanguageModel> language_models;
    if (not models_already_loaded) {
        models_already_loaded = true;
        if (not LoadLanguageModels(language_models_directory, distance_type, topmost_use_count, &language_models))
            LOG_ERROR("no language models available in \"" + language_models_directory + "\"!");
    }

    // Verify that we do have models for all requested languages:
    if (not considered_languages.empty()) {
        std::unordered_set<std::string> all_languages;
        for (const auto &language_model : language_models)
            all_languages.emplace(language_model.getLanguage());

        for (const auto &requested_language : considered_languages) {
            if (unlikely(all_languages.find(requested_language) == all_languages.cend()))
                LOG_ERROR("considered language \"" + requested_language + "\" is not supported!");
        }
    }

    std::vector<std::pair<std::string, double>> languages_and_scores;
    for (const auto &language_model : language_models) {
        if (not considered_languages.empty() and considered_languages.find(language_model.getLanguage()) == considered_languages.cend())
            continue;

        // Compare the known language model with the unknown language model:
        double distance(0.0);
        for (unsigned i(0); i < sorted_unknown_language_model.size(); ++i)
            distance += language_model.distance(sorted_unknown_language_model[i].first, i);

        languages_and_scores.emplace_back(language_model.getLanguage(), distance);
    }
    std::sort(languages_and_scores.begin(), languages_and_scores.end(),
              [](const std::pair<std::string, double> &a, const std::pair<std::string, double> &b){ return a.second > b.second; });

    // Select the top scoring language and anything that's close (as defined by alternative_cutoff_factor):
    const double high_score(languages_and_scores[0].second);
    top_languages->push_back(languages_and_scores[0].first);
    for (unsigned i(1); i < languages_and_scores.size() and (languages_and_scores[i].second < alternative_cutoff_factor * high_score); ++i)
        top_languages->push_back(languages_and_scores[i].first);
}


} // namespace NGram

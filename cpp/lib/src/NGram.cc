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
#include <fstream>
#include <climits>
#include "FileUtil.h"
#include "UBTools.h"
#include "util.h"


namespace {


// GetWhiteSpace -- appends all characters considered to be whitespace
//                  characters in the currently selected locale to
//                  "whitespace".
//
void GetWhiteSpace(std::string * const whitespace) {
    for (int ch = CHAR_MIN; ch <= CHAR_MAX; ++ch) {
        if (isspace(ch))
            *whitespace += static_cast<char>(ch);
    }
}


void Split(const std::string &s, const std::string &non_word_chars, std::vector<std::string> * const words) {
    bool in_word(false);
    std::string new_word;
    for (const auto ch : s) {
        if (non_word_chars.find(ch) != std::string::npos) {
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
    std::ifstream input(path_name.c_str());
    if (input.fail())
        LOG_ERROR("can't open language model file \"" + path_name + "\" for reading!");

    std::string non_ngram_chars("1234567890");
    GetWhiteSpace(&non_ngram_chars);

    unsigned rank(1);
    while (not input.eof()) {
        std::string line;
        std::getline(input, line);

        std::string ngram;
        for (std::string::const_iterator ch(line.begin());
             ch != line.end() and non_ngram_chars.find(*ch) == std::string::npos; ++ch)
            ngram += *ch;

        if (ngram.empty())
            continue;

        (*ngram_counts)[ngram] = rank;
        ++rank;
    }
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


class LanguageModel: public std::unordered_map<std::string, IndexAndRelFrequency> {
    std::string language_;
    const NGram::DistanceType distance_type_;
    const int topmost_use_count_;
    double max_distance_;
public:
    LanguageModel(const std::string &language, const NGram::NGramCounts &ngram_counts,
                  const NGram::DistanceType distance_type, const unsigned topmost_use_count);
    std::string getLanguage() const { return language_; }
    double distance(const std::string &ngram, const int position) const;
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


double LanguageModel::distance(const std::string &ngram, const int position) const {
    const_iterator iter = find(ngram);
    if (iter == end() or iter->second.index_ >= topmost_use_count_)
        return max_distance_;
    else if (distance_type_ == NGram::WEIGHTED_DISTANCE) {
        double total_distance(0.0);
        for (int i = std::min(position, iter->second.index_);
             i < std::max(position, iter->second.index_); ++i)
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
    FileUtil::Directory directory(language_models_directory, "*.lm");
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


} // unnamed namespace


namespace NGram {


SortedNGramCounts::SortedNGramCounts(const NGramCounts &ngram_counts, const SortOrder sort_order) {
    resize(ngram_counts.size());
    unsigned i = 0;
    for (NGramCounts::const_iterator entry(ngram_counts.begin()); entry != ngram_counts.end();
         ++entry, ++i)
        (*this)[i] = std::pair<std::string, double>(entry->first, entry->second);
    bool (*sort_func)(const std::pair<std::string, double> &lhs, const std::pair<std::string, double> &rhs);
    if (sort_order == ASCENDING_ORDER)
        sort_func = IsLessThan;
    else
        sort_func = IsGreaterThan;
    std::sort(begin(), end(), sort_func);
}


bool SortedNGramCounts::IsLessThan(const std::pair<std::string, double> &lhs, const std::pair<std::string, double> &rhs) {
    if (lhs.second == rhs.second)
        return lhs.first > rhs.first ;
    return lhs.second < rhs.second;
}


bool SortedNGramCounts::IsGreaterThan(const std::pair<std::string, double> &lhs, const std::pair<std::string, double> &rhs) {
    if (lhs.second == rhs.second)
        return lhs.first < rhs.first ;
    return lhs.second > rhs.second;
}


void CreateLanguageModel(std::istream &input, NGramCounts * const ngram_counts,
			 SortedNGramCounts * const top_ngrams, const unsigned ngram_number_threshold,
			 const unsigned topmost_use_count)
{
    std::string file_contents(std::istreambuf_iterator<char>(input), {});

    // Replace anything but letters and quotes with spaces.
    // xlate_map is used to convert all non-letters with the exception of single quotes
    // into spaces:
    static char xlate_map_table[256];
    static char * const xlate_map = xlate_map_table - CHAR_MIN;
    static bool initialized_xlate_map(false);
    if (not initialized_xlate_map) {
        initialized_xlate_map = true;

        for (int c = CHAR_MIN; c <= CHAR_MAX; ++c)
            xlate_map[c] = isalpha(c) ? c : ' ';
        xlate_map[unsigned('\'')] = '\'';
    }

    for (auto &ch : file_contents)
        ch = static_cast<char>(xlate_map[unsigned(ch)]);

    static std::string whitespace;
    static bool whitespace_is_initialized(false);
    if (not whitespace_is_initialized) {
        whitespace_is_initialized = true;
        GetWhiteSpace(&whitespace);
    }

    std::vector<std::string> words;
    Split(file_contents, whitespace, &words);

    unsigned total_ngram_count(0);
    for (std::vector<std::string>::const_iterator word(words.begin()); word != words.end(); ++word) {
        const std::string funny_word("_" + *word + "_");
        const std::string::size_type funny_word_length(funny_word.length());
        std::string::size_type length(funny_word_length);
        for (unsigned i = 0; i < funny_word_length; ++i, --length) {
            if (length > 4) {
                const std::string ngram(funny_word.substr(i, 5));
                NGramCounts::iterator ngram_count = ngram_counts->find(ngram);
                if (ngram_count == ngram_counts->end())
                    (*ngram_counts)[ngram] = 1.0;
                else
                    ++(*ngram_counts)[ngram];
                ++total_ngram_count;
            }

            if (length > 3) {
                const std::string ngram(funny_word.substr(i, 4));
                NGramCounts::iterator ngram_count = ngram_counts->find(ngram);
                if (ngram_count == ngram_counts->end())
                    (*ngram_counts)[ngram] = 1.0;
                else
                    ++(*ngram_counts)[ngram];
                ++total_ngram_count;
            }

            if (length > 2) {
                const std::string ngram(funny_word.substr(i, 3));
                NGramCounts::iterator ngram_count = ngram_counts->find(ngram);
                if (ngram_count == ngram_counts->end())
                    (*ngram_counts)[ngram] = 1.0;
                else
                    ++(*ngram_counts)[ngram];
                ++total_ngram_count;
            }

            if (length > 1) {
                const std::string ngram(funny_word.substr(i, 2));
                NGramCounts::iterator ngram_count = ngram_counts->find(ngram);
                if (ngram_count == ngram_counts->end())
                    (*ngram_counts)[ngram] = 1.0;
                else
                    ++(*ngram_counts)[ngram];
                ++total_ngram_count;
            }

            const std::string ngram(funny_word.substr(i, 1));
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


void ClassifyLanguage(std::istream &input, std::vector<std::string> * const top_languages,
		      const DistanceType distance_type, const unsigned ngram_number_threshold,
		      const unsigned topmost_use_count, const double alternative_cutoff_factor,
		      const std::string &override_language_models_directory)
{
    // Determine the language models directroy:
    const std::string language_models_directory(override_language_models_directory.empty()
                                                ? UBTools::GetTuelibPath() + "/language_models"
                                                : override_language_models_directory);

    NGramCounts unknown_language_model;
    SortedNGramCounts sorted_unknown_language_model;
    CreateLanguageModel(input, &unknown_language_model, &sorted_unknown_language_model,
                        ngram_number_threshold, topmost_use_count);

    static bool models_already_loaded(false);
    static std::vector<LanguageModel> language_models;
    if (not models_already_loaded) {
        models_already_loaded = true;
        if (not LoadLanguageModels(language_models_directory, distance_type, topmost_use_count, &language_models))
            LOG_ERROR("no language models available in \"" + language_models_directory + "\"!");
    }

    NGramCounts results;
    for (std::vector<LanguageModel>::const_iterator language_model(language_models.begin());
         language_model != language_models.end(); ++language_model)
    {
        // Compare the known language model with the unknown language model:
        double distance = 0.0;
        for (unsigned i = 0; i < sorted_unknown_language_model.size(); ++i)
            distance += language_model->distance(sorted_unknown_language_model[i].first, i);

        results[language_model->getLanguage()] = distance;
    }

    SortedNGramCounts sorted_results(results, SortedNGramCounts::ASCENDING_ORDER);

    // Select the top scoring language and anything that's close
    // (as defined by alternative_cutoff_factor):
    const double high_score = sorted_results[0].second;
    top_languages->push_back(sorted_results[0].first);
    for (unsigned i = 1; i < sorted_results.size() and
             (sorted_results[i].second < alternative_cutoff_factor * high_score); ++i)
        top_languages->push_back(sorted_results[i].first);
}


} // namespace NGram

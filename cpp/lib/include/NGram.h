/** \file    NGram.h
 *  \brief   Declarations of ngram related utility functions.
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
#pragma once


#include <ostream>
#include <set>
#include <sstream>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>


// Forward declaration:
class File;


namespace NGram {


// Language extraction parameters.  These defaults are the same as the reference
// implementation (i.e. the perl "text_cat" program) except where noted.
const unsigned DEFAULT_NGRAM_NUMBER_THRESHOLD = 0; // 0 means no threshold.
const unsigned DEFAULT_TOPMOST_USE_COUNT = 400;
const double DEFAULT_ALTERNATIVE_CUTOFF_FACTOR = 1.0; // textcat = 1.05


typedef std::vector<std::pair<std::wstring, double>> NGramCounts;


class UnitVector : public NGramCounts {
public:
    UnitVector() = default;
    explicit UnitVector(const NGramCounts &ngram_counts);
    double dotProduct(const UnitVector &rhs) const;
    void prettyPrint(std::ostream &output) const;
};


class LanguageModel : public UnitVector {
    std::string language_;

public:
    LanguageModel() = default;
    LanguageModel(const std::string &language, const NGramCounts &ngram_counts): UnitVector(ngram_counts), language_(language) { }
    inline const std::string &getLanguage() const { return language_; }
    inline void setLanguage(const std::string &language) { language_ = language; }
    inline double similarity(const NGram::UnitVector &rhs) const { return dotProduct(rhs); }
    void serialise(File &output) const;
    void deserialise(File &input);
};


/** \brief Loads a language model from a file.
 *  \override_language_models_directory  If empty the default directory for language models will be used.
 */
void LoadLanguageModel(const std::string &language, LanguageModel * const language_model,
                       const std::string &override_language_models_directory = "");


/** \brief  Create a language model from the input.
 *  \param  input                   Where to read the input text from.
 *  \param  language_model          Nomen est omen.
 *  \param  ngram_number_threshold  Don't used ngrams that occur less than this many times.
 *                                  A value of 0 means: use all ngrams.
 *  \param  topmost_use_count       The topmost number of ngrams that should be used.
 */
void CreateLanguageModel(std::istream &input, LanguageModel * const language_model,
                         const unsigned ngram_number_threshold = DEFAULT_NGRAM_NUMBER_THRESHOLD,
                         const unsigned topmost_use_count = DEFAULT_TOPMOST_USE_COUNT);


/** \brief  Create a language model from the input.
 *  \param  input_text              The input text.
 *  \param  language_model          Nomen est omen.
 *  \param  ngram_number_threshold  Don't used ngrams that occur less than this many times.
 *                                  A value of 0 means: use all ngrams.
 *  \param  topmost_use_count       The topmost number of ngrams that should be used.
 */
inline void CreateLanguageModel(const std::string &input_text, LanguageModel * const language_model,
                                const unsigned ngram_number_threshold = DEFAULT_NGRAM_NUMBER_THRESHOLD,
                                const unsigned topmost_use_count = DEFAULT_TOPMOST_USE_COUNT) {
    std::istringstream input(input_text);
    CreateLanguageModel(input, language_model, ngram_number_threshold, topmost_use_count);
}


struct DetectedLanguage {
    std::string language_;
    double score_;

public:
    DetectedLanguage() = default;
    DetectedLanguage(const std::string &language, const double score): language_(language), score_(score){};
};


/** \brief  Tell which language(s) "input" might contain.
 *  \param  input                      Where to read the to be classified text from.
 *  \param  top_languages              The list of most likely languages with the most likely language first.
 *  \param  considered_languages       If non-empty only the specified languages will be used for classification o/w all
 *                                     languages will be considered.
 *  \param  alternative_cutoff_factor  Include languages in "top_languages" that received scores no less
 *                                     than "alternative_cutoff_factor" * alternative_language's_score/
 *                                     score_of_highest_scoring_language.
 *  \param  override_language_models_directory  If set, it specifies alternative location of language models.
 *
 *  \note    By default, the language models are located in the libiViCore share/language_models directory.
 */
void ClassifyLanguage(std::istream &input, std::vector<DetectedLanguage> * const top_languages,
                      const std::set<std::string> &considered_languages = {},
                      const double alternative_cutoff_factor = DEFAULT_ALTERNATIVE_CUTOFF_FACTOR,
                      const std::string &override_language_models_directory = "");


/** \brief  Tell which language(s) "input_text" might be.
 *  \param  input_text                 The text to classify.
 *  \param  top_languages              The list of most likely languages with the most likely language first.
 *  \param  considered_languages       If non-empty only the specified languages will be used for classification o/w all
 *                                     languages will be considered.
 *  \param  alternative_cutoff_factor  Include languages in "top_languages" that received scores no less
 *                                     than "alternative_cutoff_factor" * alternative_language's_score/
 *                                     score_of_highest_scoring_language.
 *  \param  override_language_models_directory  If set, it specifies alternative location of language models.
 *
 *  \note   By default, the language models are located in the libiViaCore share/language_models directory.
 */
inline void ClassifyLanguage(const std::string &input_text, std::vector<DetectedLanguage> * const top_languages,
                             const std::set<std::string> &considered_languages = {},
                             const double alternative_cutoff_factor = DEFAULT_ALTERNATIVE_CUTOFF_FACTOR,
                             const std::string &override_language_models_directory = "") {
    std::istringstream input(input_text);
    ClassifyLanguage(input, top_languages, considered_languages, alternative_cutoff_factor, override_language_models_directory);
}


/** \brief  Tell which language(s) "input_text" might be.
 *  \param  input                   Where to read the to be classified text from.
 *  \param  output_path             Where to write the model.
 *  \param  ngram_number_threshold  Don't used ngrams that occur less than this many times.
 *                                  A value of 0 means: use all ngrams.
 *  \param  topmost_use_count       The topmost number of ngrams that should be used.
 */
void CreateAndWriteLanguageModel(std::istream &input, const std::string &output_path,
                                 const unsigned ngram_number_threshold = DEFAULT_NGRAM_NUMBER_THRESHOLD,
                                 const unsigned topmost_use_count = DEFAULT_TOPMOST_USE_COUNT);


/** \brief  Tell which language(s) "input_text" might be.
 *  \param  input_text              The input text.
 *  \param  output_path             Where to write the model.
 *  \param  ngram_number_threshold  Don't used ngrams that occur less than this many times.
 *                                  A value of 0 means: use all ngrams.
 *  \param  topmost_use_count       The topmost number of ngrams that should be used.
 */
inline void CreateAndWriteLanguageModel(const std::string &input_text, const std::string &output_path,
                                        const unsigned ngram_number_threshold = DEFAULT_NGRAM_NUMBER_THRESHOLD,
                                        const unsigned topmost_use_count = DEFAULT_TOPMOST_USE_COUNT) {
    std::istringstream input(input_text);
    CreateAndWriteLanguageModel(input, output_path, ngram_number_threshold, topmost_use_count);
}


} // namespace NGram

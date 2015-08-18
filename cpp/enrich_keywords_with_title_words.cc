// A tool for adding keywords extracted from titles to MARC records.
#include <algorithm>
#include <fstream>
#include <iostream>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <unordered_set>
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


void Usage() {
    std::cerr << "Usage: " << progname << " [--verbose] marc_input marc_output [stopwords_files]\n";
    std::cerr << "       The MARC-21 output will have enriched keywords based on title words that were\n";
    std::cerr << "       similar to keywords found in the MARC-21 input file.\n";
    std::cerr << "       Stopword files must be named \"stopwords.xxx\" where xxx has to be a 3-letter\n";
    std::cerr << "       language code.\n";
    std::exit(EXIT_FAILURE);
}


void LoadStopwords(const bool verbose, FILE * const input, const std::string &language_code,
                   std::unordered_set<std::string> * const stopwords_set)
{
    if (verbose)
        std::cout << "Starting loading of stopwords for language: " << language_code << "\n";

    unsigned count(0);
    while (not std::feof(input)) {
        char buf[1024];
        if (std::fgets(buf, sizeof buf, input) == nullptr)
            break;
        if (buf[0] == '\0' or buf[0] == ';') // Empty or comment line?
            continue;

        size_t len(std::strlen(buf));
        if (buf[len - 1] == '\n')
            --len;

        stopwords_set->insert(StringUtil::ToLower(std::string(buf, len)));
        ++count;
    }

    if (std::ferror(input))
        Error("Read error while trying to read the stopwords file.");

    if (verbose)
        std::cerr << "Read " << count << " stopwords.\n";
}


void FilterOutStopwords(const std::unordered_set<std::string> &stopwords,
                        std::vector<std::string> * const words)
{
    std::vector<std::string> filtered_words;
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


std::string VectorToString(const std::vector<std::string> &v) {
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
    std::vector<std::string> phrase_as_vector;
    TextUtil::ChopIntoWords(phrase, &phrase_as_vector, MIN_WORD_LENGTH);
    return VectorToString(phrase_as_vector);
}


std::string ppn;
// Lowercases and stems "keyword_phrase" and chops it into `words'.  Populates
// "stemmed_keyword_to_stemmed_keyphrases_map" and "stemmed_keyphrases_to_unstemmed_keyphrases_map".
// The former maps from each individual stemmed word to the entire cleaned up and stemmed key phrase and the
// latter maps from the cleaned up and stemmed key phrase to the original key phrase.
void ProcessKeywordPhrase(
    const std::string &keyword_phrase, const Stemmer * const stemmer,
    std::unordered_map<std::string, std::set<std::string>> * const stemmed_keyword_to_stemmed_keyphrases_map,
    std::unordered_map<std::string, std::string> * const stemmed_keyphrases_to_unstemmed_keyphrases_map)
{
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
    std::vector<std::string> stemmed_words;
    StringUtil::Split(lowercase_stemmed_phrase, ' ', &stemmed_words);
    for (const auto &stemmed_word : stemmed_words) {
	auto iter(stemmed_keyword_to_stemmed_keyphrases_map->find(stemmed_word));
	if (iter == stemmed_keyword_to_stemmed_keyphrases_map->end())
	    (*stemmed_keyword_to_stemmed_keyphrases_map)[stemmed_word] =
		std::set<std::string>{ lowercase_stemmed_phrase, };
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
    const std::vector<DirectoryEntry> &dir_entries, const std::vector<std::string> &field_data,
    const Stemmer * const stemmer,
    std::unordered_map<std::string, std::set<std::string>> * const stemmed_keyword_to_stemmed_keyphrases_map,
    std::unordered_map<std::string, std::string> * const stemmed_keyphrases_to_unstemmed_keyphrases_map)
{
    size_t keyword_count(0);
    const auto _689_iterator(DirectoryEntry::FindField("689", dir_entries));
    if (_689_iterator != dir_entries.end()) {
	size_t field_index(_689_iterator - dir_entries.begin());
	while (field_index < field_data.size() and dir_entries[field_index].getTag() == "689") {
	    const Subfields subfields(field_data[field_index]);
	    const std::string subfield_a_value(subfields.getFirstSubfieldValue('a'));
	    if (not subfield_a_value.empty()) {
		std::string keyphrase(subfield_a_value);
		const std::string subfield_c_value(subfields.getFirstSubfieldValue('c'));
		if (not subfield_c_value.empty())
		    keyphrase += " " + subfield_c_value;
		ProcessKeywordPhrase(CanonizeCentury(keyphrase), stemmer, stemmed_keyword_to_stemmed_keyphrases_map,
				     stemmed_keyphrases_to_unstemmed_keyphrases_map);
		++keyword_count;
	    }

	    ++field_index;
	}
    }

    return keyword_count;
}


size_t ExtractKeywordsFromIndividualKeywordFields(
    const std::vector<DirectoryEntry> &dir_entries,
    const std::vector<std::string> &field_data,
    const Stemmer * const stemmer,
    std::unordered_map<std::string, std::set<std::string>> * const stemmed_keyword_to_stemmed_keyphrases_map,
    std::unordered_map<std::string, std::string> * const stemmed_keyphrases_to_unstemmed_keyphrases_map)
{
    size_t keyword_count(0);
    std::vector<std::string> keyword_phrases;
    static const std::string SUBFIELD_IGNORE_LIST("02"); // Do not extract $0 and $2.
    MarcUtil::ExtractAllSubfields("600:610:611:630:650:653:656", dir_entries, field_data, &keyword_phrases,
				  SUBFIELD_IGNORE_LIST);
    for (const auto &keyword_phrase : keyword_phrases) {
	ProcessKeywordPhrase(CanonizeCentury(keyword_phrase), stemmer, stemmed_keyword_to_stemmed_keyphrases_map,
			     stemmed_keyphrases_to_unstemmed_keyphrases_map);
	++keyword_count;
    }

    return keyword_count;
}


size_t ExtractAllKeywords(
    const std::vector<DirectoryEntry> &dir_entries, const std::vector<std::string> &field_data,
    std::unordered_map<std::string, std::set<std::string>> * const stemmed_keyword_to_stemmed_keyphrases_map,
    std::unordered_map<std::string, std::string> * const stemmed_keyphrases_to_unstemmed_keyphrases_map)
{
    const std::string language_code(MarcUtil::GetLanguage(dir_entries, field_data));
    const Stemmer * const stemmer(language_code.empty() ? nullptr : Stemmer::StemmerFactory(language_code));

    size_t extracted_count(ExtractKeywordsFromKeywordChainFields(dir_entries, field_data, stemmer,
								 stemmed_keyword_to_stemmed_keyphrases_map,
								 stemmed_keyphrases_to_unstemmed_keyphrases_map));
    extracted_count += ExtractKeywordsFromIndividualKeywordFields(dir_entries, field_data, stemmer,
								  stemmed_keyword_to_stemmed_keyphrases_map,
								  stemmed_keyphrases_to_unstemmed_keyphrases_map);
    return extracted_count;
}


void ExtractStemmedKeywords(
    const bool verbose, FILE * const input,
    std::unordered_map<std::string, std::set<std::string>> * const stemmed_keyword_to_stemmed_keyphrases_map,
    std::unordered_map<std::string, std::string> * const stemmed_keyphrases_to_unstemmed_keyphrases_map)
{
    if (verbose)
        std::cerr << "Starting extraction and stemming of pre-existing keywords.\n";

    Leader *raw_leader;
    std::vector<DirectoryEntry> dir_entries;
    std::vector<std::string> field_data;
    unsigned total_count(0), records_with_keywords_count(0), keywords_count(0);
    std::string err_msg;
    while (MarcUtil::ReadNextRecord(input, &raw_leader, &dir_entries, &field_data, &err_msg)) {
        ++total_count;
        std::unique_ptr<Leader> leader(raw_leader);

ppn=field_data[0];
	const size_t extracted_count(
            ExtractAllKeywords(dir_entries, field_data, stemmed_keyword_to_stemmed_keyphrases_map,
			       stemmed_keyphrases_to_unstemmed_keyphrases_map));
	if (extracted_count > 0) {
	    ++records_with_keywords_count;
	    keywords_count += extracted_count;
	}
    }

    if (verbose) {
        std::cerr << total_count << " records processed.\n";
        std::cerr << records_with_keywords_count << " records had keywords.\n";
	std::cerr << keywords_count << " keywords were extracted of which "
		  << stemmed_keyword_to_stemmed_keyphrases_map->size() << " were unique.\n";
    }
}


// Checks to see if "value" is in any of the sets in "key_to_set_map".
bool ContainedInMapValues(const std::string &value,
			  const std::unordered_map<std::string, std::set<std::string>> &key_to_set_map)
{
    for (const auto &key_and_set : key_to_set_map) {
	for (const auto &set_entry : key_and_set.second) {
	    if (set_entry == value)
		return true;
	}
    }

    return false;
}


// The following constant is used to reject cases where a key phrase consists of exactly one word and
// that single word is not as least as long as the constant.  This is used to try to increase precision
// but, of course, decreases recall.  Part of the reason why this seems necessary is the crappy stemmer.
constexpr auto MIN_SINGLE_STEMMED_KEYWORD_LENGTH(7);


void AugmentRecordsWithTitleKeywords(
    const bool verbose, FILE * const input, FILE * const output,
    const std::unordered_map<std::string, std::set<std::string>> &stemmed_keyword_to_stemmed_keyphrases_map,
    const std::unordered_map<std::string, std::string> &stemmed_keyphrases_to_unstemmed_keyphrases_map,
    const std::map<std::string, std::unordered_set<std::string>> &language_codes_to_stopword_sets)
{
    if (verbose)
        std::cerr << "Starting augmentation of stopwords.\n";

    Leader *raw_leader;
    std::vector<DirectoryEntry> dir_entries;
    std::vector<std::string> field_data;
    unsigned total_count(0), augmented_record_count(0);
    std::string err_msg;
    while (MarcUtil::ReadNextRecord(input, &raw_leader, &dir_entries, &field_data, &err_msg)) {
        ++total_count;
        std::unique_ptr<Leader> leader(raw_leader);

	// Look for a title...
        const auto entry_iterator(DirectoryEntry::FindField("245", dir_entries));
        if (entry_iterator == dir_entries.end()) {
            MarcUtil::ComposeAndWriteRecord(output, dir_entries, field_data, leader.get());
            continue;
        }

	// ...in subfields 'a' and 'b':
        const size_t title_index(entry_iterator - dir_entries.begin());
        Subfields subfields(field_data[title_index]);
        if (not subfields.hasSubfield('a')) {
            MarcUtil::ComposeAndWriteRecord(output, dir_entries, field_data, leader.get());
            continue;
        }
        const auto begin_end_a = subfields.getIterators('a');
        std::string title(begin_end_a.first->second);
        const auto begin_end_b = subfields.getIterators('b'); // optional additional title part.
        if (begin_end_b.first != begin_end_b.second)
            title += " " + begin_end_b.first->second;

	std::string lowercase_title;
	TextUtil::UTF8ToLower(title, &lowercase_title);
        std::vector<std::string> title_words;
        TextUtil::ChopIntoWords(lowercase_title, &title_words, MIN_WORD_LENGTH);

	// Remove language-appropriate stop words from the title words:
        const std::string language_code(MarcUtil::GetLanguage(dir_entries, field_data));
        const auto code_and_stopwords(language_codes_to_stopword_sets.find(language_code));
        if (code_and_stopwords != language_codes_to_stopword_sets.end())
            FilterOutStopwords(code_and_stopwords->second, &title_words);
        if (language_code != "eng") // Hack because people suck at cataloging!
            FilterOutStopwords(language_codes_to_stopword_sets.find("eng")->second, &title_words);

        if (title_words.empty()) {
            MarcUtil::ComposeAndWriteRecord(output, dir_entries, field_data, leader.get());
            continue;
        }

	// If we have an appropriate stemmer, replace the title words w/ stemmed title words:
	const Stemmer * const stemmer(language_code.empty() ? nullptr : Stemmer::StemmerFactory(language_code));
	if (stemmer != nullptr) {
	    std::vector<std::string> stemmed_title_words;
	    for (const auto &title_word : title_words)
		stemmed_title_words.emplace_back(stemmer->stem(title_word));
	    title_words.swap(stemmed_title_words);
	}

	std::unordered_map<std::string, std::set<std::string>> local_stemmed_keyword_to_stemmed_keyphrases_map;
	std::unordered_map<std::string, std::string> local_stemmed_keyphrases_to_unstemmed_keyphrases_map;
	ExtractAllKeywords(dir_entries, field_data, &local_stemmed_keyword_to_stemmed_keyphrases_map,
			   &local_stemmed_keyphrases_to_unstemmed_keyphrases_map);

	// Find title phrases that match stemmed keyphrases:
	std::unordered_set<std::string> new_keyphrases;
	for (const auto &title_word : title_words) {
	    const auto word_and_set(stemmed_keyword_to_stemmed_keyphrases_map.find(title_word));
	    if (word_and_set == stemmed_keyword_to_stemmed_keyphrases_map.end())
		continue;

            for (const std::string &stemmed_phrase : word_and_set->second) {
		if (ContainedInMapValues(stemmed_phrase, local_stemmed_keyword_to_stemmed_keyphrases_map))
		    continue; // We already have this in our MARC record.

		std::vector<std::string> stemmed_phrase_as_vector;
		StringUtil::Split(stemmed_phrase, ' ', &stemmed_phrase_as_vector);
		if (stemmed_phrase_as_vector.size() == 1
		    and stemmed_phrase_as_vector[0].length() < MIN_SINGLE_STEMMED_KEYWORD_LENGTH)
		    continue;

		if (TextUtil::FindSubstring(title_words, stemmed_phrase_as_vector) != title_words.end()) {
		    const auto stemmed_and_unstemmed_keyphrase(
                        stemmed_keyphrases_to_unstemmed_keyphrases_map.find(stemmed_phrase));
		    new_keyphrases.insert(stemmed_and_unstemmed_keyphrase->second);
		}
	    }
	}

        if (new_keyphrases.empty()) {
            MarcUtil::ComposeAndWriteRecord(output, dir_entries, field_data, leader.get());
            continue;
        }

	// Augment the record with new keywords derived from title words:
	for (const auto &new_keyword : new_keyphrases) {
	    const std::string field_contents("  ""\x1F""a" + new_keyword);
	    MarcUtil::InsertField(field_contents, "601", leader.get(), &dir_entries, &field_data);
	}

	MarcUtil::ComposeAndWriteRecord(output, dir_entries, field_data, leader.get());
        ++augmented_record_count;
    }

    if (verbose)
        std::cerr << augmented_record_count << " records of " << total_count
                  << " were augmented w/ additional keywords.\n";
}


int main(int argc, char **argv) {
    progname = argv[0];

    if (argc < 3)
        Usage();

    const bool verbose(std::strcmp(argv[1], "--verbose") == 0);
    if (verbose and argc < 4)
        Usage();

    const std::string marc_input_filename(argv[verbose ? 2 : 1]);
    FILE *marc_input = std::fopen(marc_input_filename.c_str(), "rm");
    if (marc_input == nullptr)
        Error("can't open \"" + marc_input_filename + "\" for reading!");

    const std::string marc_output_filename(argv[verbose ? 3 : 2]);
    FILE *marc_output = std::fopen(marc_output_filename.c_str(), "wb");
    if (marc_output == nullptr)
        Error("can't open \"" + marc_output_filename + "\" for writing!");

    if (unlikely(marc_input_filename == marc_output_filename))
        Error("MARC input file name equals MARC output file name!");

    // Read optional stopword lists:
    std::map<std::string, std::unordered_set<std::string>> language_codes_to_stopword_sets;
    for (int arg_no(verbose ? 4 : 3); arg_no < argc; ++arg_no) {
	const RegexMatcher * const matcher(RegexMatcher::RegexMatcherFactory("stopwords\\....$"));
        const std::string stopwords_filename(argv[arg_no]);
	std::string err_msg;
        if (not matcher->matched(stopwords_filename, &err_msg))
            Error("Invalid stopwords filename \"" + stopwords_filename + "\"!");
        const std::string language_code(stopwords_filename.substr(stopwords_filename.length() - 3));
        FILE *stopwords = std::fopen(stopwords_filename.c_str(), "rm");
        if (stopwords == nullptr)
            Error("can't open \"" + stopwords_filename + "\" for reading!");
        std::unordered_set<std::string> stopwords_set;
        LoadStopwords(verbose, stopwords, language_code, &stopwords_set);
        language_codes_to_stopword_sets[language_code] = stopwords_set;
        std::fclose(stopwords);
    }

    // We always need English because librarians suck at specifying English:
    if (language_codes_to_stopword_sets.find("eng") == language_codes_to_stopword_sets.end())
        Error("You always need to provide \"stopwords.eng\"!");

    std::unordered_map<std::string, std::set<std::string>> stemmed_keyword_to_stemmed_keyphrases_map;
    std::unordered_map<std::string, std::string> stemmed_keyphrases_to_unstemmed_keyphrases_map;
    ExtractStemmedKeywords(verbose, marc_input, &stemmed_keyword_to_stemmed_keyphrases_map,
			   &stemmed_keyphrases_to_unstemmed_keyphrases_map);

    std::rewind(marc_input);
    AugmentRecordsWithTitleKeywords(verbose, marc_input, marc_output, stemmed_keyword_to_stemmed_keyphrases_map,
				    stemmed_keyphrases_to_unstemmed_keyphrases_map, language_codes_to_stopword_sets);

    std::fclose(marc_input);
    std::fclose(marc_output);
}

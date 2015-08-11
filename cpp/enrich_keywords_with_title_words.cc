// A tool for adding keywords extracted from titles to MARC records.
#include <algorithm>
#include <fstream>
#include <iostream>
#include <map>
#include <memory>
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


void LoadStopwords(const bool verbose, FILE * const input,
                   std::unordered_set<std::string> * const stopwords_set)
{
    if (verbose)
        std::cout << "Starting loading of stopwords.\n";

    unsigned count(0);
    while (not std::feof(input)) {
        char buf[1024];
        if (std::fgets(buf, sizeof buf, input) == NULL)
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


void LowercaseSet(std::unordered_set<std::string> * const words) {
    std::unordered_set<std::string> lowercase_set;
    for (const auto &word : *words)
        lowercase_set.insert(StringUtil::ToLower(word));
    std::swap(lowercase_set, *words);
}


void FilterOutStopwords(const std::unordered_set<std::string> &stopwords,
                        std::unordered_set<std::string> * const words)
{
    for (std::unordered_set<std::string>::iterator word(words->begin()); word != words->end(); /* Empty! */) {
        if (stopwords.find(*word) != stopwords.end())
            word = words->erase(word);
        else
            ++word;
    }
}


std::string ConcatSet(const std::unordered_set<std::string> &words) {
    std::string retval;
    for (const auto &word : words)
        retval += word + " ";
    return retval;
}


void ExtractKeywordsFromKeywordChainFields(
    const std::vector<DirectoryEntry> &dir_entries, const std::vector<std::string> &field_data,
    const Stemmer &stemmer, std::unordered_map<std::string, std::string> * const stemmed_to_unstemmed_keywords_map)
{
    const auto _689_iterator(DirectoryEntry::FindField("689", dir_entries));
    if (_689_iterator != dir_entries.end()) {
	size_t field_index(_689_iterator - dir_entries.begin());
	while (field_index < field_data.size() and dir_entries[field_index].getTag() == "689") {
	    const Subfields subfields(field_data[field_index]);
	    const std::string subfield_a_value(subfields.getFirstSubfieldValue('a'));
	    if (not subfield_a_value.empty() and subfield_a_value.find(' ') == std::string::npos) {
		const std::string stemmed_keyword(stemmer.stem(subfield_a_value));
		(*stemmed_to_unstemmed_keywords_map)[stemmed_keyword] = subfield_a_value;
	    }

	    ++field_index;
	}
    }
}


void ExtractKeywordsFromIndividualKeywordFields(
    const std::vector<DirectoryEntry> &dir_entries,
    const std::vector<std::string> &field_data,
    const Stemmer &stemmer, std::unordered_map<std::string, std::string> * const stemmed_to_unstemmed_keywords_map)
{
    std::vector<std::string> keywords;
    MarcUtil::ExtractAllSubfields("600:610:611:630:650:653:656", dir_entries, field_data, &keywords);
    for (const auto &keyword : keywords) {
	if (keyword.find(' ') != std::string::npos)
	    continue;
	const std::string stemmed_keyword(stemmer.stem(keyword));
	(*stemmed_to_unstemmed_keywords_map)[stemmed_keyword] = keyword;
    }
}


void ExtractStemmedKeywords(const bool verbose, FILE * const input,
			    std::unordered_map<std::string, std::string> * const stemmed_to_unstemmed_keywords_map)
{
    if (verbose)
        std::cerr << "Starting extraction and stemming of pre-existing keywords.\n";

    Stemmer stemmer("deu");
    Leader *raw_leader;
    std::vector<DirectoryEntry> dir_entries;
    std::vector<std::string> field_data;
    unsigned total_count(0), records_with_keywords_count(0), keywords_count(0);
    std::string err_msg;
    while (MarcUtil::ReadNextRecord(input, &raw_leader, &dir_entries, &field_data, &err_msg)) {
        ++total_count;
        std::unique_ptr<Leader> leader(raw_leader);

	const size_t old_size(stemmed_to_unstemmed_keywords_map->size());

	ExtractKeywordsFromKeywordChainFields(dir_entries, field_data, stemmer,
					      stemmed_to_unstemmed_keywords_map);
	ExtractKeywordsFromIndividualKeywordFields(dir_entries, field_data, stemmer,
						   stemmed_to_unstemmed_keywords_map);

	const size_t new_size(stemmed_to_unstemmed_keywords_map->size());
	if (new_size > old_size) {
	    ++records_with_keywords_count;
	    keywords_count += new_size - old_size;
	}
    }

    if (verbose) {
        std::cerr << total_count << " records processed.\n";
        std::cerr << records_with_keywords_count << " records had keywords.\n";
	std::cerr << keywords_count << " keywords were extracted of which "
		  << stemmed_to_unstemmed_keywords_map->size() << " were unique.\n";
    }
}


/*
void AugmentStopwordsWithTitleWords(
    const bool verbose, FILE * const input, FILE * const output,
    const std::map<std::string, std::unordered_set<std::string>> &language_codes_to_stopword_sets)
{
    if (verbose)
        std::cerr << "Starting augmentation of stopwords.\n";

    Leader *raw_leader;
    std::vector<DirectoryEntry> dir_entries;
    std::vector<std::string> field_data;
    unsigned total_count(0), augment_count(0), title_count(0);
    std::string err_msg;
    while (MarcUtil::ReadNextRecord(input, &raw_leader, &dir_entries, &field_data, &err_msg)) {
        ++total_count;
        std::unique_ptr<Leader> leader(raw_leader);

        const auto entry_iterator(DirectoryEntry::FindField("245", dir_entries));
        if (entry_iterator == dir_entries.end()) {
            MarcUtil::ComposeAndWriteRecord(output, dir_entries, field_data, leader.get());
            continue;
        }

        const size_t title_index(entry_iterator - dir_entries.begin());
        Subfields subfields(field_data[title_index]);
        if (not subfields.hasSubfield('a')) {
            MarcUtil::ComposeAndWriteRecord(output, dir_entries, field_data, leader.get());
            continue;
        }

        const auto begin_end_a = subfields.getIterators('a');
        std::string title(begin_end_a.first->second);
        const auto begin_end_b = subfields.getIterators('b');
        if (begin_end_b.first != begin_end_b.second) {
            title += " " + begin_end_b.first->second;
        }

        ++title_count;

        std::unordered_set<std::string> title_words;
        TextUtil::ChopIntoWords(title, &title_words, / * min_word_length = * / 3);
        LowercaseSet(&title_words);

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

        for (const auto &word : title_words)
            std::cout << word << ' ' << language_code << '\n';

        ++augment_count;
    }

    if (verbose) {
        std::cerr << title_count << " records had titles in 245a.\n";
        std::cerr << "Augmented " << augment_count << " records of " << total_count
                  << " records with title words.\n";
    }
}
*/


int main(int argc, char **argv) {
    progname = argv[0];

    if (argc < 3)
        Usage();

    const bool verbose(std::strcmp(argv[1], "--verbose") == 0);
    if (verbose and argc < 4)
        Usage();

    const std::string marc_input_filename(argv[verbose ? 2 : 1]);
    FILE *marc_input = std::fopen(marc_input_filename.c_str(), "rm");
    if (marc_input == NULL)
        Error("can't open \"" + marc_input_filename + "\" for reading!");

    const std::string marc_output_filename(argv[verbose ? 3 : 2]);
    FILE *marc_output = std::fopen(marc_output_filename.c_str(), "wb");
    if (marc_output == NULL)
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
        if (stopwords == NULL)
            Error("can't open \"" + stopwords_filename + "\" for reading!");
        std::unordered_set<std::string> stopwords_set;
        LoadStopwords(verbose, stopwords, &stopwords_set);
        language_codes_to_stopword_sets[language_code] = stopwords_set;
        std::fclose(stopwords);
    }

    // We always need English because librarians suck at specifying English:
    if (language_codes_to_stopword_sets.find("eng") == language_codes_to_stopword_sets.end())
        Error("You always need to provide \"stopwords.eng\"!");

    std::unordered_map<std::string, std::string> stemmed_to_unstemmed_keywords_map;
    ExtractStemmedKeywords(verbose, marc_input, &stemmed_to_unstemmed_keywords_map);

    std::fclose(marc_input);
    std::fclose(marc_output);
}

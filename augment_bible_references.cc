// A tool for flagging and extracting bible references.
#include <algorithm>
#include <fstream>
#include <iostream>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <unordered_set>
#include <utility>
#include <cstdlib>
#include <cstring>
#include "BibleReferenceParser.h"
#include "DirectoryEntry.h"
#include "Leader.h"
#include "MarcUtil.h"
#include "RegexMatcher.h"
#include "StringUtil.h"
#include "Subfields.h"
#include "TextUtil.h"
#include "util.h"


void Usage() {
    std::cerr << "Usage: " << progname << " [--verbose] ix_theo_titles ix_theo_norm augmented_ix_theo_titles "
	      << "bible_norm\n";
    std::exit(EXIT_FAILURE);
}


const std::unordered_set<std::string> books_of_the_bible { // Found in 130$a
    "Matthäusevangelium", // -- start New Testament --
    "Markusevangelium",
    "Lukasevangelium",
    "Johannesevangelium",
    "Apostelgeschichte",
    "Römerbrief",
    "Korintherbrief", // 2 records "I." and "II." in $n
    "Galaterbrief",
    "Epheserbrief",
    "Philipperbrief",
    "Kolosserbrief",
    "Thessalonicherbrief", // 2 records "I." and "II." in $n
    "Timotheusbrief", // 2 records "I." and "II." in $n
    "Titusbrief",
    "Philemonbrief",
    "Hebräerbrief",
    "Jakobusbrief",
    "Petrusbrief", // 2 records "I." and "II." in $n
    "Johannesbrief", // 3 records "I.", "II." and "III." in $n
    "Judasbrief",
    "Johannes-Apokalypse", // a.k.a. "Offenbarung des Johannes"
    "Genesis", // -- start Old Testament --
    "Exodus",
    "Leviticus",
    "Numeri",
    "Deuteronomium",
    "Josua", // $9g:Buch
    "Richter", // $9g:Buch
    "Rut", // $9g:Buch
    "Samuel", // $9g:Buch, 2 records "I." and "II." in $n
    "Könige", // $9g:Buch, 2 records "I." and "II." in $n
    "Chronik", // $9g:Buch, 2 records "I." and "II." in $n
    "Esra", // $9g:Buch, $9g:gBuch, III., $9g:gBuch, IV. und $9g:gBuch, IV. 1-2
    "Nehemia", // $9g:Buch
    "Tobit", // $9g:Buch
    "Judit", // $9g:Buch
    "Ester", // $9g:Buch
    "Makkabäer", // $9g:Buch, 4 records "I.", "II.", "III." and "IV." in $n
    "Ijob", // $9g:Buch
    "Psalmen",
    "Sprichwörter", // $9g:Bibel
    "Kohelet",
    "Hoheslied",
    "Weisheit", // $9g:Buch
    "Sirach", // $9g:Buch
    "Jesaja", // $9g:Buch
    "Jeremia", // $9g:Buch
    "Klagelieder Jeremias", // a.k.a. "Klagelieder"
    "Baruch", // $9g:Buch
    "Jeremiabrief", // a.k.a. "Epistola Jeremiae"
    "Ezechiel", // $9g:Buch
    "Daniel", // $9g:Buch
    "Hosea", // $9g:Buch
    "Joel", // $9g:Buch
    "Amos", // $9g:Buch
    "Obadja", // $9g:Buch
    "Jona", // $9g:Buch
    "Micha", // $9g:Buch
    "Nahum", // $9g:Buch
    "Habakuk", // $9g:Buch
    "Zefanja", // $9g:Buch
    "Haggai", // $9g:Buch
    "Sacharja", // $9g:Buch
    "Maleachi", // $9g:Buch
};


// Books of the bible that are flagged as "g:Buch.*" in 530$9:
const std::unordered_set<std::string> explicit_books {
    "Josua", "Richter", "Rut", "Samuel", "Könige", "Chronik", "Esra", "Nehemia", "Tobit", "Judit", "Ester",
    "Makkabäer", "Ijob", "Weisheit", "Sirach", "Jesaja", "Jeremia", "Baruch", "Ezechiel", "Daniel", "Hosea", "Joel",
    "Amos", "Obadja", "Jona", "Micha", "Nahum", "Habakuk", "Zefanja", "Haggai", "Sacharja", "Maleachi"
};


// Books of the bible that have ordinal Roman numerals in $530$n:
const std::unordered_set<std::string> books_with_ordinals {
    "Korintherbrief", "Thessalonicherbrief", "Timotheusbrief", "Petrusbrief", "Johannesbrief", "Samuel", "Könige",
    "Chronik", "Esra", "Makkabäer"
};


bool StartsWithSmallRomanOrdinal(const std::string &roman_ordinal_candidate) {
    return StringUtil::StartsWith(roman_ordinal_candidate, "I.")
	   or StringUtil::StartsWith(roman_ordinal_candidate, "II.")
	   or StringUtil::StartsWith(roman_ordinal_candidate, "III.")
	   or StringUtil::StartsWith(roman_ordinal_candidate, "IV.");
}


// Extracts the Roman ordinals and converts them cardinal numbers.
void ExtractRomanOrdinals(const std::string &ordinals, std::set<unsigned> * extracted_set) {
    std::string scanned_text;
    for (auto ch : ordinals) {
	switch (ch) {
	case 'I':
	case 'V':
	    scanned_text += ch;
	    break;
	case '.':
	    if (scanned_text == "I")
		extracted_set->insert(1);
	    else if (scanned_text == "II")
		extracted_set->insert(2);
	    else if (scanned_text == "III")
		extracted_set->insert(3);
	    else if (scanned_text == "IV")
		extracted_set->insert(4);
	    scanned_text.clear();
	    break;
	default:
	    scanned_text.clear();
	}
    }
}


bool EndsWithLowercaseChar(const std::string &s) {
    return std::islower(s[s.length() - 1]);
}


/** \brief Tries to find a book of the bible in a subfield.
 *  \param fields_and_subfields  A colon separated list of fields plus subfield codes used to try to locate the name
 *                               of one of the books of the bible, e.g. "130a:100t".
 *  \param dir_entries           The directory entries of a MARC-21 record.
 *  \param field_data            The field data of a MARC-21 record.
 *  \param book_candidate        After a successful return this will contain the name of a book of the bible.
 *  \param field                 After a successful return this will contain the name of the field where the book of
 *                               the bible was found.
 *  \return True if a book of the bible was found in one of the subfields, else false.
 */
bool findBibleBookInField(const std::string &fields_and_subfields, const std::vector<DirectoryEntry> &dir_entries,
			  const std::vector<std::string> &field_data, std::string * const book_candidate,
			  std::string * const field)
{
    std::vector<std::string> fields;
    StringUtil::Split(fields_and_subfields, ':', &fields);
    for (const auto &field_and_subfield : fields) {
	*field = field_and_subfield.substr(0, 3);
	const auto field_iter(DirectoryEntry::FindField(*field, dir_entries));
	if (field_iter == dir_entries.end())
	    continue;
	const Subfields subfields(field_data[field_iter - dir_entries.begin()]);
	*book_candidate = subfields.getFirstSubfieldValue(field_and_subfield[3]);
	if (books_of_the_bible.find(*book_candidate) != books_of_the_bible.end())
	    return true;
    }

    return false;
}


void LoadNormData(const bool verbose, FILE * const norm_input,
		  std::unordered_map<std::string, std::string> * const gnd_codes_to_bible_ref_codes_map) {
    gnd_codes_to_bible_ref_codes_map->clear();
    if (verbose)
	std::cerr << "Starting loading of norm data.\n";

    const std::string bible_book_map_filename("books_of_the_bible_to_code_map.js");
    std::ofstream bible_book_map(bible_book_map_filename, std::ofstream::out | std::ofstream::trunc);
    if (bible_book_map.fail())
	Error("Failed to open \"" + bible_book_map_filename + "\" for writing!");
    bible_book_map << "var book_name_to_code_map = {};\n\n";

    Leader *raw_leader;
    std::vector<DirectoryEntry> dir_entries;
    std::vector<std::string> field_data;
    unsigned count(0), bible_ref_count(0);
    std::string err_msg;
    unsigned bible_book_code(0);
    std::unordered_map<std::string, std::string> bible_book_to_code_map;
    while (MarcUtil::ReadNextRecord(norm_input, &raw_leader, &dir_entries, &field_data, &err_msg)) {
	++count;

	const auto _001_iter(DirectoryEntry::FindField("001", dir_entries));
	if (_001_iter == dir_entries.end())
	    continue;
	const std::string &control_number(field_data[_001_iter - dir_entries.begin()]);

	const auto _065_begin_end(DirectoryEntry::FindFields("065", dir_entries));
	bool found_a_bible_indicator(false);
	for (auto _065_iter(_065_begin_end.first); _065_iter != _065_begin_end.second; ++_065_iter) {
	    const Subfields _065_subfields(field_data[_065_iter - dir_entries.begin()]);
	    const std::string _065_a(_065_subfields.getFirstSubfieldValue('a'));
	    if (StringUtil::StartsWith(_065_a, "3.2aa") or StringUtil::StartsWith(_065_a, "3.2ba")) {
		found_a_bible_indicator = true;
		break;
	    }
	}
	if (not found_a_bible_indicator)
	    continue;

	const auto _079_iter(DirectoryEntry::FindField("079", dir_entries));
	if (_079_iter == dir_entries.end())
	    continue;
	const Subfields _079_subfields(field_data[_079_iter - dir_entries.begin()]);
	if (_079_subfields.getFirstSubfieldValue('v') != "wit")
	    continue;

	std::string book_candidate, book_field;
	if (not findBibleBookInField("430a:130a:100a:100t", dir_entries, field_data, &book_candidate, &book_field))
	    continue;

	// Ensure that we have a GND-Code in 035$a:
	const auto _035_iter(DirectoryEntry::FindField("035", dir_entries));
        if (_035_iter == dir_entries.end())
            continue;
	const Subfields _035_subfields(field_data[_035_iter - dir_entries.begin()]);
	const std::string _035a_field(_035_subfields.getFirstSubfieldValue('a'));
	if (not StringUtil::StartsWith(_035a_field, "(DE-588)")) {
	    if (verbose)
		std::cerr << "Missing GND code for control number " << control_number << ".\n";
	    continue;
	}
	const std::string gnd_code(_035a_field.substr(8));
	if (gnd_code.empty()) {
	    if (verbose)
		std::cerr << "Empty GND code for control number " << control_number << ".\n";
	    continue;
	}

	std::string _065n_field;
	const auto _065_iter(DirectoryEntry::FindField("065", dir_entries));
	if (_065_iter != dir_entries.end()) {
	    const Subfields _065_subfields(field_data[_065_iter - dir_entries.begin()]);
	    _065n_field = _065_subfields.getFirstSubfieldValue('n');
	    if (not _065n_field.empty()) {
		if (not CanParseBibleReference(_065n_field))
		    std::cerr << "Bad bible chapter/verse ref? " << _065n_field << ", PPN: "
			      << control_number << ", GND: " << gnd_code << ", book candidate: "
			      << book_candidate << '\n';
	    }
	}

	// Filter records that looks like bible books but would have to have a 530$9 subfield starting
	// with "g:Buch" in order to qualify:
	if (explicit_books.find(book_candidate) != explicit_books.end()) {
	    const auto _530_begin_end(DirectoryEntry::FindFields("530", dir_entries));
	    bool found_at_least_one(false);
	    for (auto _530_iter(_530_begin_end.first); _530_iter != _530_begin_end.second; ++_530_iter) {
		const Subfields _530_subfields(field_data[_530_iter - dir_entries.begin()]);
		const auto _530a_begin_end(_530_subfields.getIterators('9'));
		for (auto code_and_value(_530a_begin_end.first); code_and_value != _530a_begin_end.second;
		     ++code_and_value)
		{
		    if (StringUtil::StartsWith(code_and_value->second, "g:Buch")) {
			found_at_least_one = true;
			break;
		    }
		}
		if (found_at_least_one)
		    break;
	    }

	    if (not found_at_least_one)
		continue;
	}

	// Filter records that looks like bible books but would have to have a $n subfield containing
	// a roman ordinal number in order to qualify:
	std::set<unsigned> book_ordinals;
	if (books_with_ordinals.find(book_candidate) != books_with_ordinals.end()) {
	    const auto book_field_begin_end(DirectoryEntry::FindFields(book_field, dir_entries));
	    for (auto book_field_iter(book_field_begin_end.first); book_field_iter != book_field_begin_end.second;
		 ++book_field_iter)
        {
		const Subfields book_field_subfields(field_data[book_field_iter - dir_entries.begin()]);
		const auto book_field_n_begin_end(book_field_subfields.getIterators('n'));
		for (auto code_and_contents(book_field_n_begin_end.first);
		     code_and_contents != book_field_n_begin_end.second; ++code_and_contents)
		{
		    ExtractRomanOrdinals(code_and_contents->second, &book_ordinals);
		    if (not book_ordinals.empty())
			break;
		}
		if (not book_ordinals.empty())
		    break;
	    }
	    if (book_ordinals.empty())
		continue;
	}

	// Generate the mapping from books of the bible to numeric codes:
	std::string current_book_code;
	if (book_ordinals.empty()) {
	    const auto book_and_code(bible_book_to_code_map.find(book_candidate));
	    if (book_and_code != bible_book_to_code_map.end())
		current_book_code = book_and_code->second;
	    else {
		++bible_book_code;
		current_book_code = StringUtil::PadLeading(std::to_string(bible_book_code), 2, '0');
		bible_book_to_code_map[book_candidate] = current_book_code;
		bible_book_map << "book_name_to_code_map[\"" << StringUtil::ToLower(book_candidate)
			       << "\"] = \"" << current_book_code << "\";\n";
	    }
	} else {
	    for (const auto ordinal : book_ordinals) {
		std::string augmented_book_name(std::to_string(ordinal) + book_candidate);
		const auto book_and_code(bible_book_to_code_map.find(augmented_book_name));
		if (book_and_code != bible_book_to_code_map.end())
		    current_book_code = book_and_code->second;
		else {
		    ++bible_book_code;
		    current_book_code = StringUtil::PadLeading(std::to_string(bible_book_code), 2, '0');
		    bible_book_to_code_map[augmented_book_name] = current_book_code;
		    bible_book_map << "book_name_to_code_map[\"" << StringUtil::ToLower(augmented_book_name)
				   << "\"] = \"" << current_book_code << "\";\n";
		}
	    }
	}

	std::string book_field_9, book_field_n;
	const auto book_field_begin_end(DirectoryEntry::FindFields(book_field, dir_entries));
	for (auto book_field_iter(book_field_begin_end.first); book_field_iter != book_field_begin_end.second;
	     ++book_field_iter)
        {
	    const Subfields book_field_subfields(field_data[book_field_iter - dir_entries.begin()]);

	    const auto _9_begin_end(book_field_subfields.getIterators('9'));
	    for (auto code_and_value(_9_begin_end.first); code_and_value != _9_begin_end.second; ++code_and_value) {
		if (not book_field_9.empty())
		    book_field_9 += ';';
		book_field_9 += code_and_value->second;
	    }

	    const auto _n_begin_end(book_field_subfields.getIterators('n'));
	    for (auto code_and_value(_n_begin_end.first); code_and_value != _n_begin_end.second; ++code_and_value) {
		if (not book_field_n.empty())
		    book_field_n += ';';
		book_field_n += code_and_value->second;
	    }
	}

//	if (_065n_field != book_field_n)
//	    std::cerr << "_065n_field(" << _065n_field << ") and book_field_n(" << book_field_n << ") differ!\n";

	if (not book_field_9.empty())
	    book_field_9 = " " + book_field + "|" + book_field_9;
	if (not book_field_n.empty())
	    book_field_n = " " + book_field + "$n|" + book_field_n;
	if (not _065n_field.empty())
	    _065n_field = " 065$n|" + _065n_field;

	std::cout << control_number << "| " << book_candidate << ' ' << book_field_9 << book_field_n
		  << _065n_field << '\n';
	++bible_ref_count;
    }

    if (not err_msg.empty())
	Error("Read error while trying to read the norm data file: " + err_msg);

    if (verbose) {
	std::cerr << "Read " << count << " norm data records.\n";
	std::cerr << "Found " << bible_ref_count << " reference records.\n";
    }
}


void AugmentBibleRefs(const bool verbose, FILE * const input, FILE * const output,
		      const std::unordered_set<std::string> &gnd_codes)
{
    if (verbose)
	std::cerr << "Starting augmentation of title records.\n";

    Leader *raw_leader;
    std::vector<DirectoryEntry> dir_entries;
    std::vector<std::string> field_data;
    unsigned total_count(0), augment_count(0);
    std::string err_msg;
    while (MarcUtil::ReadNextRecord(input, &raw_leader, &dir_entries, &field_data, &err_msg)) {
	++total_count;
	std::unique_ptr<Leader> leader(raw_leader);

	const auto entry_iterator(DirectoryEntry::FindField("689", dir_entries));
	if (entry_iterator == dir_entries.end()) {
	    MarcUtil::ComposeAndWriteRecord(output, dir_entries, field_data, leader.get());
	    continue;
	}

	const size_t keyword_index(entry_iterator - dir_entries.begin());
	Subfields subfields(field_data[keyword_index]);
	if (not subfields.hasSubfield('0')) {
	    MarcUtil::ComposeAndWriteRecord(output, dir_entries, field_data, leader.get());
	    continue;
	}

	if (not subfields.hasSubfield('t') or not subfields.hasSubfield('9')) {
	    MarcUtil::ComposeAndWriteRecord(output, dir_entries, field_data, leader.get());
	    continue;
	}

	const auto begin_end = subfields.getIterators('0');
	bool found_a_matching_reference(false);
	for (auto norm_ref(begin_end.first); norm_ref != begin_end.second; ++norm_ref) {
	    if (not StringUtil::StartsWith(norm_ref->second, "(DE-588)"))
		continue;

	    if (gnd_codes.find(norm_ref->second) != gnd_codes.end()) {
		found_a_matching_reference = true;
		std::cerr << subfields.getFirstSubfieldValue('t') << ' ' << subfields.getFirstSubfieldValue('9')
			  << '\n';
	    }
	}

	if (not found_a_matching_reference) {
	    std::cout << "Norm data ref missing\n";
	    MarcUtil::ComposeAndWriteRecord(output, dir_entries, field_data, leader.get());
	    continue;
	}

	++augment_count;
    }

    if (verbose)
	std::cerr << "Augmented " << augment_count << " records of " << total_count << ".\n";
}


int main(int argc, char **argv) {
    progname = argv[0];

    if (argc < 5)
	Usage();

    const bool verbose(std::strcmp(argv[1], "--verbose") == 0);
    if (verbose ? (argc != 6) : (argc != 5))
	Usage();

    const std::string title_input_filename(argv[verbose ? 2 : 1]);
    FILE *title_input = std::fopen(title_input_filename.c_str(), "rbm");
    if (title_input == NULL)
	Error("can't open \"" + title_input_filename + "\" for reading!");

    const std::string norm_input_filename(argv[verbose ? 3 : 2]);
    FILE *norm_input = std::fopen(norm_input_filename.c_str(), "rbm");
    if (norm_input == NULL)
	Error("can't open \"" + norm_input_filename + "\" for writing!");

    const std::string title_output_filename(argv[verbose ? 4 : 3]);
    FILE *title_output = std::fopen(title_output_filename.c_str(), "wb");
    if (title_output == NULL)
	Error("can't open \"" + title_output_filename + "\" for writing!");

    const std::string bible_norm_output_filename(argv[verbose ? 5 : 6]);
    FILE *bible_norm_output = std::fopen(bible_norm_output_filename.c_str(), "wb");
    if (bible_norm_output == NULL)
	Error("can't open \"" + bible_norm_output_filename + "\" for writing!");

    if (unlikely(title_input_filename == title_output_filename))
	Error("Title input file name equals title output file name!");

    if (unlikely(norm_input_filename == title_output_filename))
	Error("Norm data input file name equals title output file name!");

    std::unordered_map<std::string, std::string> gnd_codes_to_bible_ref_codes_map;
    LoadNormData(verbose, norm_input, &gnd_codes_to_bible_ref_codes_map);
//    AugmentBibleRefs(verbose, title_input, title_output, gnd_codes);

    std::fclose(title_input);
    std::fclose(norm_input);
    std::fclose(title_output);
    std::fclose(bible_norm_output);
}

#include <fstream>
#include <iostream>
#include <memory>
#include <unordered_set>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <getopt.h>
#include "DirectoryEntry.h"
#include "Leader.h"
#include "MarcUtil.h"
#include "RegexMatcher.h"
#include "StringUtil.h"
#include "util.h"


void Usage() {
    std::cerr << "Usage: " << progname << "[((--dump|-d) [dump_filename] input_filename) \n"
	"       \t| ((--filter|-f) [--verbose|-v] input_filename output_filename pattern1 ... patternN)]\n"
	"       \t| ((--dump-edit-format|-E) [dump_filename] input_filename)]\n"
	"       \t| ((--verbose|-v)]\n"
	"       \t| ((--delete-if-not-matched|-D) input_filename output_filename tags_to_delete"
	"pattern1 ... patternN)]\n";
    std::exit(EXIT_FAILURE);
}


const struct option longopts[] = {
    {
	.name    = "dump",
	.has_arg = no_argument,
	.flag    = NULL,
	.val     = 'd'
    },
    {
	.name    = "filter",
	.has_arg = no_argument,
	.flag    = NULL,
	.val     = 'f'
    },
    {
	.name    = "dump-edit-format",
	.has_arg = no_argument,
	.flag    = NULL,
	.val     = 'E'
    },
    {
	.name    = "delete-if-not-matched",
	.has_arg = no_argument,
	.flag    = NULL,
	.val     = 'D'
    },
    {
	.name    = "verbose",
	.has_arg = no_argument,
	.flag    = NULL,
	.val     = 'v'
    },
    {
	.name    = NULL,
	.has_arg = 0,
	.flag    = NULL,
	.val     = '\0'
    },
};

		    
void Dump(const std::string &input_filename, const std::string &output_filename) {
    std::ofstream output(output_filename);
    if (!output)
	Error("can't open \"" + output_filename + "\" for writing!");

    FILE *input = std::fopen(input_filename.c_str(), "rb");
    if (input == NULL)
	Error("can't open \"" + input_filename + "\" for reading!");

    Leader *raw_leader;
    std::vector<DirectoryEntry> dir_entries;
    std::vector<std::string> field_data;
    std::string err_msg;
    unsigned count(0);
    while (MarcUtil::ReadNextRecord(input, &raw_leader, &dir_entries, &field_data, &err_msg)) {
	++count;
	std::unique_ptr<Leader> leader(raw_leader);

	output << "NEW RECORD\n";

	output << "  Leader:\n";
	output << "\trecord length: " << leader->getRecordLength() << '\n';
	output << "\trecord status: " << leader-> getRecordStatus() << '\n';
	output << "\trecord type: " << leader->getRecordType() << '\n';
	output << "\tcharacter coding scheme: " << leader->getCharacterCodingScheme() << '\n';

	output << "  Directory:\n";
	for (const auto &entry : dir_entries) {
	    output << "\ttag: " << entry.getTag() << '\n';
	    output << "\tfield length: " << entry.getFieldLength() << '\n';
	    output << "\tstarting character position: " << entry.getFieldOffset() << '\n';
	}
    }

    if (not err_msg.empty())
	Error(err_msg);
    std::cerr << "Read " << count << " records.\n";

    std::fclose(input);
}

		    
class CompiledPattern {
    std::string tag_;
    RegexMatcher matcher_;
public:
    CompiledPattern(const std::string &tag,  const RegexMatcher &matcher): tag_(tag), matcher_(matcher) {}
    bool tagMatched(const std::string &tag) const { return tag == tag_; }
    bool fieldMatched(const std::string &field_contents) const;
};


bool CompiledPattern::fieldMatched(const std::string &field_contents) const {
    std::string err_msg;
    const bool retval = matcher_.matched(field_contents, &err_msg);
    if (not retval and not err_msg.empty())
	Error("Unexpected error while trying to match a field in CompiledPattern::fieldMatched(): " + err_msg);

    return retval;
}


// Expects "patterns" to contain strings that look like XXX:REGEX where XXX are 3 characters specifying a field
// and REGEX is a PCRE-style regex supporting UTF8 that should match field contents.
bool CompilePatterns(const std::vector<std::string> &patterns, std::vector<CompiledPattern> * const compiled_patterns,
		     std::string * const err_msg)
{
    compiled_patterns->clear();
    compiled_patterns->reserve(patterns.size());

    for (const auto &pattern : patterns) {
	std::string::size_type first_colon_pos = pattern.find(':');
	if (first_colon_pos == std::string::npos) {
	    *err_msg = "missing colon!";
	    return false;
	} else if (first_colon_pos != DirectoryEntry::TAG_LENGTH) {
	    *err_msg = "colon in wrong position! (Tag length must be "
		       + std::to_string(DirectoryEntry::TAG_LENGTH) + ".)";
	    return false;
	}

	const std::string field(pattern.substr(0, first_colon_pos));
	const std::string regex_string(pattern.substr(first_colon_pos + 1));

	RegexMatcher *new_matcher = RegexMatcher::RegexMatcherFactory(regex_string, err_msg);
	if (new_matcher == NULL) {
	    *err_msg = "failed to compile regular expression: \"" + regex_string + "\"! (" + *err_msg +")";
	    return false;
	}

	compiled_patterns->push_back(CompiledPattern(field, std::move(*new_matcher)));
	delete new_matcher;
    }

    return true;
}


void Filter(const std::string &input_filename, const std::string &output_filename,
	    std::vector<std::string> &patterns, const bool verbose) {
    FILE *input = std::fopen(input_filename.c_str(), "rb");
    if (input == NULL)
	Error("can't open \"" + input_filename + "\" for reading!");

    FILE *output = std::fopen(output_filename.c_str(), "wb");
    if (output == NULL)
	Error("can't open \"" + output_filename + "\" for writing!");

    std::vector<CompiledPattern> compiled_patterns;
    std::string err_msg;
    if (not CompilePatterns(patterns, &compiled_patterns, &err_msg))
	Error("Error while compiling patterns: " + err_msg);

    Leader *raw_leader;
    std::vector<DirectoryEntry> dir_entries;
    std::vector<std::string> field_data;
    unsigned count(0), matched_count(0);
    while (MarcUtil::ReadNextRecord(input, &raw_leader, &dir_entries, &field_data, &err_msg)) {
	++count;
	std::unique_ptr<Leader> leader(raw_leader);
	bool matched(false);
	for (unsigned i(0); i < dir_entries.size(); ++i) {
	    for (const auto &compiled_pattern : compiled_patterns) {
		if (compiled_pattern.tagMatched(dir_entries[i].getTag())) {
		    if (compiled_pattern.fieldMatched(field_data[i])) {
			if (verbose) std::cerr << '=' << dir_entries[i].getTag() << "  " << field_data[i] << '\n';
			matched = true;
			goto found;
		    }
		}
	    }
	}
    found:
	if (matched) {
	    ++matched_count;	
	    const std::string record(MarcUtil::ComposeRecord(dir_entries, field_data, leader.get()));
	    if ((std::fwrite(record.data(), record.size(), 1, output)) != record.size())
		Error("failed to write to \"" + output_filename + "\"!");
	}
    }

    if (not err_msg.empty())
	Error(err_msg);
    std::cerr << "Read " << count << " records.\n";
    std::cerr << "Matched " << matched_count << " records.\n";

    std::fclose(input);
    std::fclose(output);
}

		    
void DumpEditFormat(const std::string &input_filename, const std::string &output_filename) {
    std::ofstream output(output_filename);
    if (!output)
	Error("can't open \"" + output_filename + "\" for writing!");

    FILE *input = std::fopen(input_filename.c_str(), "rb");
    if (input == NULL)
	Error("can't open \"" + input_filename + "\" for reading!");

    Leader *raw_leader;
    std::vector<DirectoryEntry> dir_entries;
    std::vector<std::string> field_data;
    std::string err_msg;
    unsigned count(0);
    while (MarcUtil::ReadNextRecord(input, &raw_leader, &dir_entries, &field_data, &err_msg)) {
	++count;
	std::unique_ptr<Leader> leader(raw_leader);
	output << "=LDR  ....." << leader->toString().substr(5) << '\n';

	unsigned i(0);
	for (const auto &entry : dir_entries) {
	    output << '=' << entry.getTag() << "  ";
	    if (not entry.isControlFieldEntry()) {
		if (field_data[i][0] == ' ') field_data[i][0] = '\\';
		if (field_data[i][1] == ' ') field_data[i][1] = '\\';
	    }
		output << field_data[i] << '\n';
	    
	    ++i;
	}

	output << '\n';
    }

    if (not err_msg.empty())
	Error(err_msg);

    std::cerr << "Read " << count << " records.\n";

    std::fclose(input);
}


// Removes any tags and corresponding fields where the tag is contained in "drop_tags".
void FilterTagsAndFields(const std::unordered_set<std::string> &drop_tags,
			 std::vector<DirectoryEntry> * const dir_entries,
			 std::vector<std::string> * const field_data)
{
    if (dir_entries->empty() or dir_entries->size() != field_data->size())
	Error("Incorrect input data for FilterTagsAndFields()!");

    std::vector<size_t> matched_slots;
    matched_slots.reserve(dir_entries->size());

    unsigned slot_no(0);
    for (const auto &dir_entry : *dir_entries) {
	if (drop_tags.find(dir_entry.getTag()) != drop_tags.end())
	    matched_slots.push_back(slot_no);
	++slot_no;
    }

    if (matched_slots.empty())
	return;
    const size_t new_size(dir_entries->size() - matched_slots.size());

    std::vector<DirectoryEntry> old_dir_entries;
    dir_entries->swap(old_dir_entries);
    dir_entries->reserve(new_size);

    std::vector<std::string> old_field_data;
    field_data->swap(old_field_data);
    field_data->reserve(new_size);

    std::vector<size_t>::const_iterator matched_slot(matched_slots.begin());
    for (unsigned slot_no (0); slot_no < old_dir_entries.size(); ++slot_no) {
	if (matched_slot != matched_slots.end() and *matched_slot == slot_no)
	    ++matched_slot; // skip tag and field
	else {
	    dir_entries->push_back(std::move(old_dir_entries[slot_no]));
	    field_data->push_back(std::move(old_field_data[slot_no]));
	}
    }
}


void DeleteMatched(const std::string &input_filename, const std::string &output_filename,
		   const std::string &tags_list, const std::vector<std::string> &patterns, const bool invert)
{
    FILE *input = std::fopen(input_filename.c_str(), "rb");
    if (input == NULL)
	Error("can't open \"" + input_filename + "\" for reading!");

    FILE *output = std::fopen(output_filename.c_str(), "wb");
    if (output == NULL)
	Error("can't open \"" + output_filename + "\" for writing!");

    std::vector<CompiledPattern> compiled_patterns;
    std::string err_msg;
    if (not CompilePatterns(patterns, &compiled_patterns, &err_msg))
	Error("Error while compiling patterns: " + err_msg);

    std::vector<std::string> tags;
    if (StringUtil::Split(tags_list, ':', &tags) == 0)
	Error("Empty list of tags to remove!");
    const std::unordered_set<std::string> drop_tags(tags.begin(), tags.end());

    for (const auto &tag : tags) {
	if (tag.length() != DirectoryEntry::TAG_LENGTH)
	    Error("Tags need to be " + std::to_string(DirectoryEntry::TAG_LENGTH)
		  + " characters in length! (Bad tag is \"" + tag +"\")");
    }

    Leader *leader;
    std::vector<DirectoryEntry> dir_entries;
    std::vector<std::string> field_data;
    unsigned count(0), modified_count(0);
    while (MarcUtil::ReadNextRecord(input, &leader, &dir_entries, &field_data, &err_msg)) {
	++count;

	bool matched(false);
	for (unsigned i(0); i < dir_entries.size(); ++i) {
	    for (const auto &compiled_pattern : compiled_patterns) {
		if (compiled_pattern.tagMatched(dir_entries[i].getTag())) {
		    if (compiled_pattern.fieldMatched(field_data[i])) {
			matched = true;
			goto found;
		    }
		}
	    }
	}
    found:
	if (invert)
	    matched = not matched;
	if (matched) {
	    ++modified_count;
	    FilterTagsAndFields(drop_tags, &dir_entries, &field_data);
	}

	const std::string record(MarcUtil::ComposeRecord(dir_entries, field_data, leader));
	if (not MarcUtil::RecordSeemsCorrect(record, &err_msg))
	    Error("bad record! (" + err_msg + ")");

	const size_t write_count = std::fwrite(record.data(), 1, record.size(), output);
	if (write_count != record.size())
	    Error("failed to write " + std::to_string(record.size()) + " bytes to \"" + output_filename + "\"!");

	delete leader;
    }

    if (not err_msg.empty())
	Error(err_msg);

    std::cerr << "Read " << count << " records.\n";
    std::cerr << "Modified " << modified_count << " records.\n";

    std::fclose(input);
    std::fclose(output);
}


int main(int argc, char **argv) {
    progname = argv[0];

    int opt;
    bool dump(false), filter(false), dump_edit_format(false), delete_if_not_matched(false), verbose(false);
    std::string output_filename("/dev/stdout");
    int option_index(0);
    while ((opt = getopt_long(argc, argv, "dfEDv", longopts, &option_index)) != -1) {
	switch (opt) {
	case 'd':
	    dump = true;
	    break;
	case 'f':
	    filter = true;
	    break;
	case 'E':
	    dump_edit_format = true;
	    break;
	case 'D':
	    delete_if_not_matched = true;
	    break;
	case 'v':
	    verbose = true;
	    break;
	default:
	    std::cerr << progname << ": unknown command-line option!\n";
	    Usage();
	}
    }
    argc -= optind;
    argv += optind;

    if (dump) {
	if (argc == 0) {
	    std::cerr<< progname << ": missing input filename!\n";
	    Usage();
	} else if (argc > 2) {
	    std::cerr<< progname << ": trailing unknown command-line arguments!\n";
	    Usage();
	}
	if (argc == 2)
	    output_filename = argv[1];

	Dump(argv[0], output_filename);
    } else if (filter) {
	if (argc < 3) // progname input_filename output_filename pattern1
	    Usage();

	std::vector<std::string> patterns;
	for (int arg_no(2); arg_no < argc; ++arg_no)
	    patterns.push_back(argv[arg_no]);
	Filter(argv[0], argv[1], patterns, verbose);
    } else if (dump_edit_format) {
	if (argc == 0) {
	    std::cerr<< progname << ": missing input filename!\n";
	    Usage();
	} else if (argc > 2) {
	    std::cerr<< progname << ": trailing unknown command-line arguments!\n";
	    Usage();
	}
	if (argc == 2)
	    output_filename = argv[1];

	DumpEditFormat(argv[0], output_filename);
    } else if (delete_if_not_matched) {
	if (argc < 1) {
	    std::cerr<< progname << ": missing input filename!\n";
	    Usage();
	}
	if (argc < 2) {
	    std::cerr<< progname << ": missing input filename!\n";
	    Usage();
	}
	if (argc < 3) {
	    std::cerr<< progname << ": missing tags list!\n";
	    Usage();
	}

	std::vector<std::string> patterns;
	for (int arg_no(3); arg_no < argc; ++arg_no)
	    patterns.push_back(argv[arg_no]);

	DeleteMatched(argv[0], argv[1], argv[2], patterns, /* invert = */ true);
    } else
	Usage();
}

/** \brief A MARC-21 filter uliity that can remove records based on ISILs and replace URNs in 856u-fields with URLs.
 *  \author Dr. Johannes Ruscheinski (johannes.ruscheinski@uni-tuebingen.de)
 *
 *  \copyright 2015,2016 Universitätsbiblothek Tübingen.  All rights reserved.
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
#include "Subfields.h"
#include "util.h"
#include "XmlWriter.h"


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
        if (new_matcher == nullptr) {
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
    File input(input_filename, "rm");
    if (not input)
        Error("can't open \"" + input_filename + "\" for reading!");

    File output(output_filename, "wb");
    if (not output)
        Error("can't open \"" + output_filename + "\" for writing!");
    XmlWriter xml_writer(&output);
    xml_writer.openTag("marc:collection",
                       { std::make_pair("xmlns:marc", "http://www.loc.gov/MARC21/slim"),
                         std::make_pair("xmlns:xsi", "http://www.w3.org/2001/XMLSchema-instance"),
                         std::make_pair("xsi:schemaLocation", "http://www.loc.gov/standards/marcxml/schema/MARC21slim.xsd")});

    std::vector<CompiledPattern> compiled_patterns;
    std::string err_msg;
    if (not CompilePatterns(patterns, &compiled_patterns, &err_msg))
        Error("Error while compiling patterns: " + err_msg);

    unsigned count(0), matched_count(0);
    while (MarcUtil::Record record = MarcUtil::Record::XmlFactory(&input)) {
	record.setRecordWillBeWrittenAsXml(true);
        ++count;

	const std::vector<DirectoryEntry> &dir_entries(record.getDirEntries());
	const std::vector<std::string> &fields(record.getFields());
        bool matched(false);
        for (unsigned i(0); i < dir_entries.size(); ++i) {
            for (const auto &compiled_pattern : compiled_patterns) {
                if (compiled_pattern.tagMatched(dir_entries[i].getTag())) {
                    if (compiled_pattern.fieldMatched(fields[i])) {
                        if (verbose) std::cerr << '=' << dir_entries[i].getTag() << "  " << fields[i] << '\n';
                        matched = true;
                        goto found;
                    }
                }
            }
        }
    found:
        if (matched) {
            ++matched_count;
	    record.write(&xml_writer);
        }
    }
    xml_writer.closeTag();

    if (not err_msg.empty())
        Error(err_msg);
    std::cerr << "Read " << count << " records.\n";
    std::cerr << "Matched " << matched_count << " records.\n";
}


void DumpEditFormat(const std::string &input_filename, const std::string &output_filename) {
    std::ofstream output(output_filename);
    if (not output)
        Error("can't open \"" + output_filename + "\" for writing!");

    File input(input_filename, "rm");
    if (not input)
        Error("can't open \"" + input_filename + "\" for reading!");

    std::shared_ptr<Leader> leader;
    std::vector<DirectoryEntry> dir_entries;
    std::vector<std::string> field_data;
    std::string err_msg;
    unsigned count(0);
    while (const MarcUtil::Record record = MarcUtil::Record::XmlFactory(&input)) {
        ++count;

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
}


// Performs a few sanity checks.
bool RecordSeemsCorrect(const std::string &record, std::string * const err_msg) {
    if (record.size() < Leader::LEADER_LENGTH) {
        *err_msg = "record too small to contain leader!";
        return false;
    }

    std::shared_ptr<Leader> leader;
    if (not Leader::ParseLeader(record.substr(0, Leader::LEADER_LENGTH), leader.get(), err_msg))
        return false;

    if (leader->getRecordLength() != record.length()) {
        *err_msg = "leader's record length (" + std::to_string(leader->getRecordLength())
                   + ") does not equal actual record length (" + std::to_string(record.length()) + ")!";
        return false;
    }

    if (leader->getBaseAddressOfData() <= Leader::LEADER_LENGTH) {
        *err_msg = "impossible base address of data!";
        return false;
    }

    const size_t directory_length(leader->getBaseAddressOfData() - Leader::LEADER_LENGTH - 1);
    if ((directory_length % DirectoryEntry::DIRECTORY_ENTRY_LENGTH) != 0) {
        *err_msg = "directory length is not a multiple of "
                   + std::to_string(DirectoryEntry::DIRECTORY_ENTRY_LENGTH) + "!";
        return false;
    }

    if (record[leader->getBaseAddressOfData() - 1] != '\x1E') {
        *err_msg = "directory is not terminated with a field terminator!";
        return false;
    }

    if (record[record.size() - 1] != '\x1D') {
        *err_msg = "record is not terminated with a record terminator!";
        return false;
    }

    return true;
}


void DeleteMatched(const std::string &tags_list, const std::vector<std::string> &patterns, const bool invert,
                   File * const input, File * const output)
{
    XmlWriter xml_writer(output);
    xml_writer.openTag("marc:collection",
                       { std::make_pair("xmlns:marc", "http://www.loc.gov/MARC21/slim"),
                         std::make_pair("xmlns:xsi", "http://www.w3.org/2001/XMLSchema-instance"),
                         std::make_pair("xsi:schemaLocation", "http://www.loc.gov/standards/marcxml/schema/MARC21slim.xsd")});

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

    unsigned count(0), modified_count(0);
    while (MarcUtil::Record record = MarcUtil::Record::XmlFactory(input)) {
	record.setRecordWillBeWrittenAsXml(true);
        ++count;

        bool matched(false);
	const std::vector<DirectoryEntry> &dir_entries(record.getDirEntries());
	const std::vector<std::string> &fields(record.getFields());
        for (unsigned i(0); i < dir_entries.size(); ++i) {
            for (const auto &compiled_pattern : compiled_patterns) {
                if (compiled_pattern.tagMatched(dir_entries[i].getTag())) {
                    if (compiled_pattern.fieldMatched(fields[i])) {
                        matched = true;
                        goto found_match;
                    }
                }
            }
        }

found_match:
        if (invert)
            matched = not matched;
        if (matched) {
            ++modified_count;
            record.filterTags(drop_tags);
        }

        record.write(&xml_writer);
    }
    xml_writer.closeTag();

    if (not err_msg.empty())
        Error(err_msg);

    std::cerr << "Read " << count << " records.\n";
    std::cerr << "Modified " << modified_count << " record(s).\n";
}


inline bool IsHttpOrHttpsURL(const std::string &url_candidate) {
    return StringUtil::StartsWith(url_candidate, "http://") or StringUtil::StartsWith(url_candidate, "https://");
}


void NormaliseURLs(const bool verbose, File * const input, File * const output) {
    XmlWriter xml_writer(output);
    xml_writer.openTag("marc:collection", { std::make_pair("xmlns", "http://www.loc.gov/MARC21/slim") });

    unsigned count(0), modified_count(0), duplicate_skip_count(0);
    while (MarcUtil::Record record = MarcUtil::Record::XmlFactory(input)) {
	record.setRecordWillBeWrittenAsXml(true);
        ++count;

	const std::vector<DirectoryEntry> &dir_entries(record.getDirEntries());
	const std::vector<std::string> &fields(record.getFields());
        bool modified_record(false);
	std::unordered_set<std::string> already_seen_links;
        for (unsigned field_no(0); field_no < dir_entries.size(); /* Intentionally empty! */) {
            if (dir_entries[field_no].getTag() != "856") {
		++field_no;
                continue;
	    }

            Subfields _856_subfields(fields[field_no]);
	    bool duplicate_link(false);
            if (_856_subfields.hasSubfield('u')) {
		const std::string u_subfield(_856_subfields.getFirstSubfieldValue('u'));

		if (IsHttpOrHttpsURL(u_subfield)) {
		    if (already_seen_links.find(u_subfield) == already_seen_links.cend())
			already_seen_links.insert(u_subfield);
		    else
			duplicate_link = true;
		} else {
		    std::string new_http_replacement_link;
		    if (StringUtil::StartsWith(u_subfield, "urn:"))
			new_http_replacement_link = "https://nbn-resolving.org/" + u_subfield;
		    else if (StringUtil::StartsWith(u_subfield, "10900/"))
			new_http_replacement_link = "https://publikationen.uni-tuebingen.de/xmlui/handle/" + u_subfield;
		    else
			new_http_replacement_link = "http://" + u_subfield;
		    if (already_seen_links.find(new_http_replacement_link) == already_seen_links.cend()) {
			_856_subfields.replace('u', u_subfield, new_http_replacement_link);
			if (verbose)
			    std::cout << "Replaced \"" << u_subfield << "\" with \"" << new_http_replacement_link
				      << "\". (PPN: " << fields[0] << ")\n";
			already_seen_links.insert(new_http_replacement_link);
			modified_record = true;
		    } else
			duplicate_link = true;
		}
	    }

	    if (not duplicate_link)
		++field_no;
	    else {
		++duplicate_skip_count;
		if (verbose)
		    std::cout << "Skipping duplicate, control numbers is " << fields[0] << ".\n";
		record.deleteField(field_no);
		modified_record = true;
	    }
        }

        if (modified_record)
            ++modified_count;

	record.write(&xml_writer);
    }
    xml_writer.closeTag();

    std::cerr << "Read " << count << " records.\n";
    std::cerr << "Modified " << modified_count << " record(s).\n";
    std::cerr << "Skipped " << duplicate_skip_count << " duplicate links.\n";
}


void Usage() {
    std::cerr << "Usage: " << progname << "[(--verbose|-v)]"
              << "[(--bibliotheks-sigel-filtern|-f) input_filename output_filename]\n"
              << "\t[(--normalise-urls|-n) input_filename output_filename]\n";

    std::exit(EXIT_FAILURE);
}


#pragma GCC diagnostic ignored "-Wpedantic"
const struct option longopts[] = {
    {
        .name    = "bibliotheks-sigel-filtern",
        .has_arg = no_argument,
        .flag    = nullptr,
        .val     = 'f'
    },
    {
        .name    = "verbose",
        .has_arg = no_argument,
        .flag    = nullptr,
        .val     = 'v'
    },
    {
        .name    = "normalise-urls",
        .has_arg = no_argument,
        .flag    = nullptr,
        .val     = 'n'
    },
    {
        .name    = nullptr,
        .has_arg = 0,
        .flag    = nullptr,
        .val     = '\0'
    },
};
#pragma GCC diagnostic warning "-Wpedantic"


int main(int argc, char **argv) {
    ::progname = argv[0];

    int opt;
    bool bibliotheks_sigel_filtern(false), normalise_urls(false), verbose(false);
    int option_index(0);
    while ((opt = getopt_long(argc, argv, "fvn", longopts, &option_index)) != -1) {
        switch (opt) {
        case 'f':
            bibliotheks_sigel_filtern = true;
            break;
        case 'n':
            normalise_urls = true;
            break;
        case 'v':
            verbose = true;
            break;
        default:
            std::cerr << ::progname << ": unknown command-line option!\n";
            Usage();
        }
    }
    argc -= optind;
    argv += optind;

    if (argc < 1) {
        std::cerr<< ::progname << ": missing input filename!\n";
        Usage();
    }
    if (argc < 2) {
        std::cerr<< ::progname << ": missing input filename!\n";
        Usage();
    }

    const std::string input_filename(argv[0]);
    File input(input_filename, "rm");
    if (not input)
        Error("can't open \"" + input_filename + "\" for reading!");

    const std::string output_filename(argv[1]);
    File output(output_filename, "w");
    if (not output)
        Error("can't open \"" + output_filename + "\" for writing!");

    try {
	if (bibliotheks_sigel_filtern) {
	    std::vector<std::string> patterns = { "LOK:^.*[a]DE-21 *$|^.*[a]DE-21-24 *$|^.*[a]DE-21-110 *$" };
	    DeleteMatched("LOK", patterns, /* invert = */ true, &input, &output);
	} else if (normalise_urls)
	    NormaliseURLs(verbose, &input, &output);
	else
	    Usage();
    } catch (const std::exception &x) {
	Error("caught exception: " + std::string(x.what()));
    }
}

/** \brief A MARC-21 filter uliity that can remove records based on ISILs and replace URNs in 856u-fields with URLs.
 *  \author Dr. Johannes Ruscheinski (johannes.ruscheinski@uni-tuebingen.de)
 *
 *  \copyright 2015 Universitätsbiblothek Tübingen.  All rights reserved.
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
    FILE *input = std::fopen(input_filename.c_str(), "rb");
    if (input == nullptr)
        Error("can't open \"" + input_filename + "\" for reading!");

    FILE *output = std::fopen(output_filename.c_str(), "wb");
    if (output == nullptr)
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
    if (input == nullptr)
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


// Performs a few sanity checks.
bool RecordSeemsCorrect(const std::string &record, std::string * const err_msg) {
    if (record.size() < Leader::LEADER_LENGTH) {
        *err_msg = "record too small to contain leader!";
        return false;
    }

    Leader *raw_leader;
    if (not Leader::ParseLeader(record.substr(0, Leader::LEADER_LENGTH), &raw_leader, err_msg))
        return false;
    const std::unique_ptr<Leader> leader(raw_leader);

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


void DeleteMatched(const std::string &tags_list, const std::vector<std::string> &patterns, const bool invert,
                   FILE * const input, FILE * const output, const std::string &output_filename)
{
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
            FilterTagsAndFields(drop_tags, &dir_entries, &field_data);
        }

        const std::string record(MarcUtil::ComposeRecord(dir_entries, field_data, leader));
        if (not RecordSeemsCorrect(record, &err_msg))
            Error("bad record! (" + err_msg + ")");

        const size_t write_count = std::fwrite(record.data(), 1, record.size(), output);
        if (write_count != record.size())
            Error("failed to write " + std::to_string(record.size()) + " bytes to \"" + output_filename + "\"!");

        delete leader;
    }

    if (not err_msg.empty())
        Error(err_msg);

    std::cerr << "Read " << count << " records.\n";
    std::cerr << "Modified " << modified_count << " record(s).\n";
}


struct Record856uEntry {
    unsigned index_;
    std::string link_;
public:
    Record856uEntry(unsigned index, const std::string &link): index_(index), link_(link) { }
};


class MatchAnyLink {
    std::string link_to_match_;
public:
    explicit MatchAnyLink(const std::string &link_to_match): link_to_match_(link_to_match) { }
    inline bool operator()(const Record856uEntry &entry) const { return entry.link_ == link_to_match_; }
};


inline bool IsHttpOrHttpsURL(const std::string &url_candidate) {
    return StringUtil::StartsWith(url_candidate, "http://") or StringUtil::StartsWith(url_candidate, "https://");
}


bool All856uLinksAreHttpOrHttpsLinks(const std::vector<Record856uEntry> &entries) {
    for (auto const &entry : entries) {
        if (not IsHttpOrHttpsURL(entry.link_))
            return false;
    }

    return true;
}


void SelectHttpAndHttpsURLs(const std::vector<Record856uEntry> &entries,
                            std::unordered_set<std::string> * const http_urls)
{
    http_urls->clear();
    for (auto const &entry : entries) {
        if (IsHttpOrHttpsURL(entry.link_))
            http_urls->insert(entry.link_);
    }       
}


void SelectNonHttpAndHttpsLinkEntries(const std::vector<Record856uEntry> &entries,
                                      std::vector<Record856uEntry> * const non_http_link_entries)
{
    non_http_link_entries->clear();
    for (auto const &entry : entries) {
        if (not IsHttpOrHttpsURL(entry.link_))
            non_http_link_entries->push_back(entry);
    }       
}


void NormaliseURLs(const bool verbose, FILE * const input, FILE * const output, const std::string &output_filename) {
    Leader *leader;
    std::vector<DirectoryEntry> dir_entries;
    std::vector<std::string> field_data;
    unsigned count(0), modified_count(0);
    std::string err_msg;
    while (MarcUtil::ReadNextRecord(input, &leader, &dir_entries, &field_data, &err_msg)) {
        ++count;

        std::vector<Record856uEntry> _856u_entries;
        for (unsigned i(0); i < dir_entries.size(); ++i) {
            if (dir_entries[i].getTag() != "856")
                continue;

            std::vector<std::string> links;
            Subfields _856_subfields(field_data[i]);
            if (_856_subfields.hasSubfield('u')) {
                const std::pair<Subfields::ConstIterator, Subfields::ConstIterator> begin_end(
                    _856_subfields.getIterators('u'));
                
                for (Subfields::ConstIterator code_and_value(begin_end.first);
                     code_and_value != begin_end.second; ++code_and_value)
                {
                    _856u_entries.push_back(Record856uEntry(i, code_and_value->second));
                    break;
                }
            }
        }

        bool modified_record(false);
        if (not All856uLinksAreHttpOrHttpsLinks(_856u_entries)) {
            std::unordered_set<std::string> http_urls;
            SelectHttpAndHttpsURLs(_856u_entries, &http_urls);
            std::vector<Record856uEntry> non_http_link_entries;
            SelectNonHttpAndHttpsLinkEntries(_856u_entries, &non_http_link_entries);
            for (const auto &non_http_link_entry : non_http_link_entries) {
                bool is_suffix(false);
                for (const auto &http_url : http_urls) {
                    if (StringUtil::EndsWith(http_url, non_http_link_entry.link_)) {
                        is_suffix = true;
                        if (verbose)
                            std::cout << "Deleting tag " << dir_entries[non_http_link_entry.index_].getTag()
                                      << " with link \"" << field_data[non_http_link_entry.index_]
                                      << "\" because it is probably a duplicate of \"" << http_url << "\".\n";
                        dir_entries.erase(dir_entries.begin() + non_http_link_entry.index_);
                        field_data.erase(field_data.begin() + non_http_link_entry.index_);
                        modified_record = true;
                        break;
                    }
                }
                if (not is_suffix) { // Replace non a HTTP URL with an HTTP URL.
                    Subfields subfields(field_data[non_http_link_entry.index_]);

                    std::string new_http_url;
                    if (StringUtil::StartsWith(non_http_link_entry.link_, "urn:"))
                        new_http_url = "https://nbn-resolving.org/" + non_http_link_entry.link_;
                    else // Ever the optimist.
                        new_http_url = "http://" + non_http_link_entry.link_;
                    if (verbose)
                        std::cout << "Replacing \"" << non_http_link_entry.link_ << "\" with \""
                                  << new_http_url << "\".\n";
                    subfields.replace('u', non_http_link_entry.link_, new_http_url);

                    const size_t orig_length = field_data[non_http_link_entry.index_].size();
                    field_data[non_http_link_entry.index_] = subfields.toString();
                    const size_t new_length = field_data[non_http_link_entry.index_].size();
                    const ssize_t delta = new_length - orig_length;
                    leader->setRecordLength(leader->getRecordLength() + delta);
                    dir_entries[non_http_link_entry.index_].setFieldLength(
                        dir_entries[non_http_link_entry.index_].getFieldLength() + delta);

                    modified_record = true;
                }
            }
        }

        if (modified_record)
            ++modified_count;

        const std::string record(MarcUtil::ComposeRecord(dir_entries, field_data, leader));
        if (not RecordSeemsCorrect(record, &err_msg))
            Error("bad record! (" + err_msg + ")");

        const size_t write_count = std::fwrite(record.data(), 1, record.size(), output);
        if (write_count != record.size())
            Error("failed to write " + std::to_string(record.size()) + " bytes to \"" + output_filename + "\"!");

        delete leader;
    }

    if (not err_msg.empty())
        Error(err_msg);

    std::cerr << "Read " << count << " records.\n";
    std::cerr << "Modified " << modified_count << " record(s).\n";
}


void Usage() {
    std::cerr << "Usage: " << progname << "[(--verbose|-v)]"
              << "[(--bibliotheks-sigel-filtern|-f) input_filename output_filename]\n"
              << "\t[(--normalise-urls|-n) input_filename output_filename]\n";

    std::exit(EXIT_FAILURE);
}


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
    FILE *input = std::fopen(input_filename.c_str(), "rb");
    if (input == nullptr)
        Error("can't open \"" + input_filename + "\" for reading!");

    const std::string output_filename(argv[1]);
    FILE *output = std::fopen(output_filename.c_str(), "wb");
    if (output == nullptr)
        Error("can't open \"" + output_filename + "\" for writing!");

    if (bibliotheks_sigel_filtern) {
        std::vector<std::string> patterns = { "LOK:^.*[a]DE-21 *$|^.*[a]DE-21-24 *$|^.*[a]DE-21-110 *$" };
        DeleteMatched("LOK", patterns, /* invert = */ true, input, output, output_filename);
    } else if (normalise_urls)
        NormaliseURLs(verbose, input, output, output_filename);
    else
        Usage();

    std::fclose(input);
    std::fclose(output);
}

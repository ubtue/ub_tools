/** \brief  A MARC-21 filter uliity that replace URNs in 856u-fields with URLs
 *  \author Oliver Obenland (oliver.obenland@uni-tuebingen.de)
 *
 *  \copyright 2016,2017 Universitätsbiblothek Tübingen.  All rights reserved.
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

#include <iostream>
#include <string>
#include <vector>

#include "MarcReader.h"
#include "MarcRecord.h"
#include "MarcWriter.h"


void Usage() {
    std::cerr << "Usage: " << ::progname << " [-v|--verbose] marc_input marc_output\n";
    std::exit(EXIT_FAILURE);
}


inline bool IsHttpOrHttpsURL(const std::string &url_candidate) {
    return StringUtil::StartsWith(url_candidate, "http://") or StringUtil::StartsWith(url_candidate, "https://");
}


// Returns the number of extracted 856u subfields.
size_t ExtractAllHttpOrHttps856uSubfields(const MarcRecord &record, std::vector<std::string> * const _856u_urls) {
    std::vector<size_t> field_indices;
    if (record.getFieldIndices("856", &field_indices) == 0)
        return 0;

    for (const auto field_index : field_indices) {
        std::string _856u_subfield_value(record.extractFirstSubfield(field_index, 'u'));
        if (IsHttpOrHttpsURL(_856u_subfield_value))
            _856u_urls->emplace_back(_856u_subfield_value);
    }

    return _856u_urls->size();
}


// Returns true if "test_string" is the suffix of "url" after stripping off the schema and domain name as well as
// a single slash after the domain name.
bool IsSuffixOfURL(const std::string &url, const std::string &test_string) {
    const bool starts_with_http(StringUtil::StartsWith(url, "http://"));
    if (not starts_with_http and not StringUtil::StartsWith(url, "https://"))
        return false;

    size_t next_slash_pos;
    if (starts_with_http) {
        next_slash_pos = url.find('/', std::strlen("http://"));
        if (unlikely(next_slash_pos == std::string::npos))
            return false;
    } else { // We assume that "url" started with "https://"
        next_slash_pos = url.find('/', std::strlen("https://"));
        if (unlikely(next_slash_pos == std::string::npos))
            return false;
    }
    if (unlikely(next_slash_pos + 1 >= url.size()))
        return false; // We have no path component in our URL.

    return url.substr(next_slash_pos + 1) == test_string;
}


// Returns true if "test_string" is a proper suffix of any of the URLs contained in "urls".
bool IsSuffixOfAnyURL(const std::vector<std::string> &urls, const std::string &test_string) {
    for (const auto &url : urls) {
        if (IsSuffixOfURL(url, test_string))
            return true;
    }

    return false;
}


void NormaliseURLs(const bool verbose, MarcReader * const reader, MarcWriter * const writer) {
    unsigned count(0), modified_count(0), duplicate_skip_count(0);
    while (MarcRecord record = reader->read()) {
        ++count;

        std::vector<std::string> _856u_urls;
        ExtractAllHttpOrHttps856uSubfields(record, &_856u_urls);

        bool modified_record(false);
        std::unordered_set<std::string> already_seen_links;

        std::vector<size_t> erase_field_indices;
        for (size_t field_no(record.getFieldIndex("856")); record.getTag(field_no) == "856"; /* empty */) {
            Subfields _856_subfields(record.getSubfields(field_no));
            bool duplicate_link(false);
            if (_856_subfields.hasSubfield('u')) {
                const std::string u_subfield(StringUtil::Trim(_856_subfields.getFirstSubfieldValue('u')));

                if (IsHttpOrHttpsURL(u_subfield)) {
                    if (already_seen_links.find(u_subfield) == already_seen_links.cend())
                        already_seen_links.insert(u_subfield);
                    else
                        duplicate_link = true;
                } else if (IsSuffixOfAnyURL(_856u_urls, u_subfield) )
                    erase_field_indices.emplace_back(field_no);
                else {
                    std::string new_http_replacement_link;
                    if (StringUtil::StartsWith(u_subfield, "urn:"))
                        new_http_replacement_link = "https://nbn-resolving.org/" + u_subfield;
                    else if (StringUtil::StartsWith(u_subfield, "10900/"))
                        new_http_replacement_link = "https://publikationen.uni-tuebingen.de/xmlui/handle/"
                                                    + u_subfield;
                    else
                        new_http_replacement_link = "http://" + u_subfield;
                    if (already_seen_links.find(new_http_replacement_link) == already_seen_links.cend()) {
                        _856_subfields.replace('u', u_subfield, new_http_replacement_link);
                        if (verbose)
                            std::cout << "Replaced \"" << u_subfield << "\" with \"" << new_http_replacement_link
                                      << "\". (PPN: " << record.getControlNumber() << ")\n";
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
                    std::cout << "Skipping duplicate, control numbers is " << record.getControlNumber() << ".\n";
                record.deleteField(field_no);
                modified_record = true;
            }
        }

        if (not erase_field_indices.empty()) {
            // We need to remove fields starting from the end of the record.
            std::sort(erase_field_indices.begin(), erase_field_indices.end(), std::greater<size_t>());

            for (const size_t field_index : erase_field_indices)
                record.deleteField(field_index);
            
            modified_record = true;
        }
        
        if (modified_record)
            ++modified_count;

        writer->write(record);
    }

    std::cerr << "Read " << count << " records.\n";
    std::cerr << "Modified " << modified_count << " record(s).\n";
    std::cerr << "Skipped " << duplicate_skip_count << " duplicate links.\n";
}


int main(int argc, char **argv) {
    ::progname = argv[0];

    if (argc != 3 and argc != 4)
        Usage();

    bool verbose(std::strcmp("-v", argv[1]) == 0 or std::strcmp("--verbose", argv[1]) == 0);
    if (verbose)
        ++argv;
    std::unique_ptr <MarcReader> marc_reader(MarcReader::Factory(argv[1]));
    std::unique_ptr <MarcWriter> marc_writer(MarcWriter::Factory(argv[2]));
    try {
        NormaliseURLs(verbose, marc_reader.get(), marc_writer.get());
    } catch (const std::exception &x) {
        Error("caught exception: " + std::string(x.what()));
    }
}

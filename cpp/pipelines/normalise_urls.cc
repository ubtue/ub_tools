/** \brief  A MARC-21 filter utility that replace URNs in 856u-fields with URL's.
 *  \author Oliver Obenland (oliver.obenland@uni-tuebingen.de)
 *  \author Dr. Johannes Ruscheinski
 *
 *  \copyright 2016-2019 Universitätsbibliothek Tübingen.  All rights reserved.
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
#include "MARC.h"
#include "StringUtil.h"
#include "util.h"


namespace {


[[noreturn]] void Usage() {
    std::cerr << "Usage: " << ::progname << " [-v|--verbose] marc_input marc_output\n";
    std::exit(EXIT_FAILURE);
}


inline bool IsHttpOrHttpsURL(const std::string &url_candidate) {
    return StringUtil::StartsWith(url_candidate, "http://") or StringUtil::StartsWith(url_candidate, "https://");
}


// Returns the number of extracted 856u subfields.
size_t ExtractAllHttpOrHttps856uSubfields(const MARC::Record &record, std::vector<std::string> * const _856u_urls) {
    for (const auto &_856_field : record.getTagRange("856")) {
        const std::string _856u_subfield_value(_856_field.getSubfields().getFirstSubfieldWithCode('u'));
        if (IsHttpOrHttpsURL(_856u_subfield_value))
            _856u_urls->emplace_back(_856u_subfield_value);
    }

    return _856u_urls->size();
}


inline std::string StripSchema(const std::string &url) {
    const auto colon_and_double_slash_start(url.find("://"));
    return (colon_and_double_slash_start == std::string::npos) ? url : url.substr(colon_and_double_slash_start + 3);
}


// Returns true if "test_string" is the suffix of "url" after stripping off the schema and domain name as well as
// a single slash after the domain name.
bool IsSuffixOfURL(const std::string &url, const std::string &test_string) {
    const bool starts_with_http(StringUtil::StartsWith(url, "http://"));
    if (not starts_with_http and not StringUtil::StartsWith(url, "https://"))
        return false;

    const std::string stripped_url(StripSchema(url));
    const std::string stripped_test_string(StripSchema(test_string));

    return StringUtil::EndsWith(stripped_url, stripped_test_string) or StringUtil::EndsWith(stripped_test_string, stripped_url);
}


// Returns true if "test_string" is a proper suffix of any of the URL's contained in "urls or vice versa".
bool IsSuffixOfAnyURL(const std::unordered_set<std::string> &urls, const std::string &test_string) {
    for (const auto &url : urls) {
        if (IsSuffixOfURL(url, test_string) or IsSuffixOfURL(test_string, url))
            return true;
    }

    return false;
}


bool SkipDOIBecauseOfEmbargo(MARC::Record * const record, const std::string &doi) {
    auto local_block_starts(record->findStartOfAllLocalDataBlocks());
    for (const auto local_block_start : local_block_starts) {
        for (const auto &_local_866_field : record->getLocalTagRange("866", local_block_start)) {
            const std::string _local_866a_content(_local_866_field.getFirstSubfieldWithCode('x'));
            if (_local_866a_content.find(doi) != std::string::npos)
                return true;
        }
    }
    return false;
}


bool CreateUrlsFrom024(MARC::Record * const record) {
    std::vector<std::string> _024_dois;
    for (const auto &_024_field : record->getTagRange("024")) {
        if (_024_field.getFirstSubfieldWithCode('2') == "doi") {
            const std::string doi(_024_field.getFirstSubfieldWithCode('a'));
            if (not doi.empty() and not SkipDOIBecauseOfEmbargo(record, doi))
                _024_dois.emplace_back(doi);
        }
    }

    for (const auto &_024_doi : _024_dois)
        record->insertFieldAtEnd("856", { { 'u', "https://doi.org/" + _024_doi }, { 'x', "doi" } });

    return not _024_dois.empty();
}


void NormaliseURLs(const bool verbose, MARC::Reader * const reader, MARC::Writer * const writer) {
    unsigned count(0), modified_count(0), duplicate_skip_count(0);
    while (MARC::Record record = reader->read()) {
        ++count;

        bool modified_record(false);
        if (CreateUrlsFrom024(&record))
            modified_record = true;

        std::vector<std::string> _856u_urls;
        ExtractAllHttpOrHttps856uSubfields(record, &_856u_urls);

        std::unordered_set<std::string> already_seen_links;

        auto _856_field(record.findTag("856"));
        while (_856_field != record.end() and _856_field->getTag() == "856") {
            MARC::Subfields _856_subfields(_856_field->getSubfields());
            bool duplicate_link(false);
            if (_856_subfields.hasSubfield('u')) {
                std::string u_subfield(StringUtil::Trim(_856_subfields.getFirstSubfieldWithCode('u')));
                if (already_seen_links.find(u_subfield) != already_seen_links.end()) {
                    if (verbose)
                        std::cout << "Found duplicate URL \"" << u_subfield << "\".\n";
                    duplicate_link = true;
                } else if (IsSuffixOfAnyURL(already_seen_links, u_subfield)) {
                    if (verbose)
                        std::cout << "Dropped field w/ duplicate URL suffix. (" << u_subfield << ")\n";
                    duplicate_link = true;
                    already_seen_links.emplace(u_subfield);
                } else if (IsHttpOrHttpsURL(u_subfield))
                    already_seen_links.emplace(u_subfield);
                else {
                    std::string new_http_replacement_link;
                    if (StringUtil::StartsWith(u_subfield, "urn:"))
                        new_http_replacement_link = "https://nbn-resolving.org/" + u_subfield;
                    else if (StringUtil::StartsWith(u_subfield, "10900/"))
                        new_http_replacement_link = "https://publikationen.uni-tuebingen.de/xmlui/handle/" + u_subfield;
                    else
                        new_http_replacement_link = "http://" + u_subfield;
                    if (already_seen_links.find(new_http_replacement_link) == already_seen_links.cend()) {
                        _856_subfields.replaceFirstSubfield('u', new_http_replacement_link);
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
                ++_856_field;
            else {
                ++duplicate_skip_count;
                if (verbose)
                    std::cout << "Skipping duplicate, control numbers is " << record.getControlNumber() << ".\n";
                _856_field = record.erase(_856_field);
                modified_record = true;
            }
        }

        if (modified_record)
            ++modified_count;

        writer->write(record);
    }

    LOG_INFO("Read " + std::to_string(count) + " records.");
    LOG_INFO("Modified " + std::to_string(modified_count) + " record(s).");
    LOG_INFO("Skipped " + std::to_string(duplicate_skip_count) + " duplicate links.");
}


} // unnamed namespace


int Main(int argc, char **argv) {
    if (argc < 2)
        Usage();

    const bool verbose(std::strcmp("-v", argv[1]) == 0 or std::strcmp("--verbose", argv[1]) == 0);
    if (verbose)
        --argc, ++argv;

    if (argc < 2)
        Usage();

    if (argc != 3)
        Usage();

    auto marc_reader(MARC::Reader::Factory(argv[1]));
    auto marc_writer(MARC::Writer::Factory(argv[2]));
    NormaliseURLs(verbose, marc_reader.get(), marc_writer.get());

    return EXIT_SUCCESS;
}

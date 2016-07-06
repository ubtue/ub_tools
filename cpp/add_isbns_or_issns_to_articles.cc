/** \file    add_isbns_or_issns_to_articles.cc
 *  \brief   A tool for adding missing ISBN's (field 020$a) or ISSN's (field 773$x)to articles entries,
 *           in MARC-21 data.
 *  \author  Dr. Johannes Ruscheinski
 */

/*
    Copyright (C) 2015, Library of the University of Tübingen

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU Affero General Public License as
    published by the Free Software Foundation, either version 3 of the
    License, or (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Affero General Public License for more details.

    You should have received a copy of the GNU Affero General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <fstream>
#include <iostream>
#include <memory>
#include <unordered_map>
#include <cstdlib>
#include <cstring>
#include "DirectoryEntry.h"
#include "FileUtil.h"
#include "Leader.h"
#include "MarcUtil.h"
#include "RegexMatcher.h"
#include "StringUtil.h"
#include "Subfields.h"
#include "util.h"
#include "XmlWriter.h"


void Usage() {
    std::cerr << "Usage: " << progname << " [-v|--verbose] master_marc_input marc_output\n";
    std::cerr << "  Adds host/parent/journal ISBNs and ISSNs to article entries found in the\n";
    std::cerr << "  master_marc_input and writes this augmented file as marc_output.  The ISBNs and ISSNs are\n";
    std::cerr << "  extracted from superior entries found in master_marc_input.\n";
    std::exit(EXIT_FAILURE);
}


bool IsPossibleISSN(const std::string &issn_candidate) {
    static RegexMatcher *matcher(nullptr);
    std::string err_msg;
    if (unlikely(matcher == nullptr)) {
        matcher = RegexMatcher::RegexMatcherFactory("\\d{4}\\-\\d{3}[\\dX]", &err_msg);
        if (matcher == nullptr)
            Error(err_msg);
    }

    const bool is_possible_issn(matcher->matched(issn_candidate, &err_msg));
    if (unlikely(not err_msg.empty()))
        Error(err_msg);

    return is_possible_issn;
}


void PopulateParentIdToISBNAndISSNMap(
    const bool verbose, File * const input,
    std::unordered_map<std::string, std::string> * const parent_id_to_isbn_and_issn_map)
{
    if (verbose)
        std::cout << "Starting extraction of ISBN's and ISSN's.\n";

    unsigned count(0), extracted_isbn_count(0), extracted_issn_count(0);
    std::string err_msg;
    while (const MarcUtil::Record record = MarcUtil::Record::XmlFactory(input)) {
        ++count;

        const Leader &leader(record.getLeader());
        if (not leader.isSerial())
            continue;

        const std::vector<DirectoryEntry> &dir_entries(record.getDirEntries());
        if (dir_entries[0].getTag() != "001")
            Error("First field is not \"001\"!");

        const std::vector<std::string> &fields(record.getFields());
        const std::string isbn(record.extractFirstSubfield("020", 'a'));
        if (not isbn.empty()) {
            (*parent_id_to_isbn_and_issn_map)[fields[0]] = isbn;
            ++extracted_isbn_count;
        }

        const std::string issn(record.extractFirstSubfield("022", 'a'));
        if (not issn.empty()) {
            (*parent_id_to_isbn_and_issn_map)[fields[0]] = issn;
            ++extracted_issn_count;
        }
    }

    if (not err_msg.empty())
        Error(err_msg);

    if (verbose) {
        std::cerr << "Read " << count << " records.\n";
        std::cerr << "Extracted " << extracted_isbn_count << " ISBNs.\n";
        std::cerr << "Extracted " << extracted_issn_count << " ISSNs.\n";
    }
}


void AddMissingISBNsOrISSNsToArticleEntries(const bool verbose, File * const input, File * const output,
                                            const std::unordered_map<std::string,
                                            std::string> &parent_id_to_isbn_and_issn_map)
{
    if (verbose)
        std::cout << "Starting augmentation of article entries.\n";

    XmlWriter xml_writer(output);
    unsigned count(0), isbns_added(0), issns_added(0), missing_host_record_ctrl_num_count(0),
             missing_isbn_or_issn_count(0);
    xml_writer.openTag("marc:collection",
                       { std::make_pair("xmlns:marc", "http://www.loc.gov/MARC21/slim"),
                         std::make_pair("xmlns:xsi", "http://www.w3.org/2001/XMLSchema-instance"),
                         std::make_pair("xsi:schemaLocation", "http://www.loc.gov/standards/marcxml/schema/MARC21slim.xsd")});
    while (MarcUtil::Record record = MarcUtil::Record::XmlFactory(input)) {
        record.setRecordWillBeWrittenAsXml(true);
        ++count;

        const Leader &leader(record.getLeader());
        if (not leader.isArticle()) {
            record.write(&xml_writer);
            continue;
        }

        const std::vector<DirectoryEntry> &dir_entries(record.getDirEntries());
        if (dir_entries[0].getTag() != "001")
            Error("First field is not \"001\"!");

        auto entry_iterator(DirectoryEntry::FindField("773", dir_entries));
        if (entry_iterator == dir_entries.end()) {
            record.write(&xml_writer);
            continue;
        }

        const size_t index_773(entry_iterator - dir_entries.begin());
        const std::vector<std::string> &fields(record.getFields());
        Subfields subfields(fields[index_773]);
        if (subfields.hasSubfield('x')) {
            record.write(&xml_writer);
            continue;
        }

        auto begin_end = subfields.getIterators('w'); // Record control number of Host Item Entry.
        if (begin_end.first == begin_end.second) {
            record.write(&xml_writer);
            ++missing_host_record_ctrl_num_count;
            continue;
        }

        std::string host_id(begin_end.first->second);
        if (StringUtil::StartsWith(host_id, "(DE-576)"))
            host_id = host_id.substr(8);
        auto const parent_isbn_or_issn_iter(parent_id_to_isbn_and_issn_map.find(host_id));
        if (parent_isbn_or_issn_iter == parent_id_to_isbn_and_issn_map.end()) {
            record.write(&xml_writer);
            ++missing_isbn_or_issn_count;
            continue;
        }

        if (IsPossibleISSN(parent_isbn_or_issn_iter->second)) {
            subfields.addSubfield('x', parent_isbn_or_issn_iter->second);
            record.updateField(index_773, subfields.toString());
            ++issns_added;
        } else { // Deal with ISBNs.
            if (not record.extractFirstSubfield("020", 'a').empty())
                continue; // We already have an ISBN.
            std::string new_field_020("  ""\x1F""a" + parent_isbn_or_issn_iter->second);
            record.insertField("020", new_field_020);
            ++isbns_added;
        }

        record.write(&xml_writer);
    }
    xml_writer.closeTag();

    if (verbose) {
        std::cerr << "Read " << count << " records.\n";
        std::cerr << "Added ISBN's to " << isbns_added << " article record(s).\n";
        std::cerr << "Added ISSN's to " << issns_added << " article record(s).\n";
        std::cerr << missing_host_record_ctrl_num_count << " articles had missing host record control number(s).\n";
        std::cerr << "For " << missing_isbn_or_issn_count << " articles no host ISBN nor ISSN was found.\n";
    }
}


int main(int argc, char **argv) {
    progname = argv[0];

    if ((argc != 3 and argc != 4) or (argc == 4 and std::strcmp(argv[1], "-v") != 0 and std::strcmp(argv[1], "--verbose") != 0))
        Usage();
    const bool verbose(argc == 4);

    const std::string marc_input_filename(argv[argc == 3 ? 1 : 2]);
    std::unique_ptr<File> marc_input(FileUtil::OpenInputFileOrDie(marc_input_filename));

    const std::string marc_output_filename(argv[argc == 3 ? 2 : 3]);
    if (unlikely(marc_input_filename == marc_output_filename))
        Error("Master input file name equals output file name!");
    std::string output_mode("w");
    if (marc_input->isCompressingOrUncompressing())
        output_mode += "c";
    File marc_output(marc_output_filename, output_mode);
    if (not marc_output)
        Error("can't open \"" + marc_output_filename + "\" for writing!");

    try {
        std::unordered_map<std::string, std::string> parent_id_to_isbn_and_issn_map;
        PopulateParentIdToISBNAndISSNMap(verbose, marc_input.get(), &parent_id_to_isbn_and_issn_map);
        marc_input->close();
        
        std::unique_ptr<File> marc_input2(FileUtil::OpenInputFileOrDie(marc_input_filename));
        AddMissingISBNsOrISSNsToArticleEntries(verbose, marc_input2.get(), &marc_output, parent_id_to_isbn_and_issn_map);
    } catch (const std::exception &x) {
        Error("caught exception: " + std::string(x.what()));
    }
}

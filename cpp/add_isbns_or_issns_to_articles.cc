/** \file    add_isbns_or_issns_to_articles.cc
 *  \brief   A tool for adding missing ISBN's (field 020$a) or ISSN's (field 773$x)to articles entries,
 *           in MARC-21 data.
 *  \author  Dr. Johannes Ruscheinski
 */

/*
    Copyright (C) 2015-2017, Library of the University of Tübingen

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
#include "MarcReader.h"
#include "MarcRecord.h"
#include "MarcWriter.h"
#include "MiscUtil.h"
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


void PopulateParentIdToISBNAndISSNMap(
    const bool verbose, MarcReader * const marc_reader,
    std::unordered_map<std::string, std::string> * const parent_id_to_isbn_and_issn_map)
{
    if (verbose)
        std::cout << "Starting extraction of ISBN's and ISSN's.\n";

    unsigned count(0), extracted_isbn_count(0), extracted_issn_count(0);
    std::string err_msg;
    while (const MarcRecord record = marc_reader->read()) {
        ++count;

        const Leader &leader(record.getLeader());
        if (not leader.isSerial() and not leader.isMonograph())
            continue;

        // Try to see if we have an ISBN:
        const std::string isbn(record.extractFirstSubfield("020", 'a'));
        if (not isbn.empty()) {
            (*parent_id_to_isbn_and_issn_map)[record.getControlNumber()] = isbn;
            ++extracted_isbn_count;
            continue;
        }

        std::string issn;

        // 1. First try to get an ISSN from 029$a, (according to the BSZ's PICA-to-MARC mapping
        // documentation this contains the "authorised" ISSN) but only if the indicators are correct:
        std::vector<size_t> _029_field_indices;
        record.getFieldIndices("029", &_029_field_indices);
        for (const auto &_029_field_index : _029_field_indices) {
            const Subfields subfields(record.getSubfields(_029_field_index));

            // We only want fields with indicators 'x' and 'a':
            if (subfields.getIndicator1() != 'x' or subfields.getIndicator2() != 'a')
                continue;

            issn = subfields.getFirstSubfieldValue('a');
            if (not issn.empty()) {
                (*parent_id_to_isbn_and_issn_map)[record.getControlNumber()] = issn;
                ++extracted_issn_count;
            }
        }

        // 2. If we don't already have an ISSN check 022$a as a last resort:
        if (issn.empty()) {
            issn = record.extractFirstSubfield("022", 'a');
            if (not issn.empty()) {
                (*parent_id_to_isbn_and_issn_map)[record.getControlNumber()] = issn;
                ++extracted_issn_count;
            }
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


void AddMissingISBNsOrISSNsToArticleEntries(const bool verbose, MarcReader * const marc_reader,
                                            MarcWriter * const marc_writer, const std::unordered_map<std::string,
                                            std::string> &parent_id_to_isbn_and_issn_map)
{
    if (verbose)
        std::cout << "Starting augmentation of article entries.\n";

    unsigned count(0), isbns_added(0), issns_added(0), missing_host_record_ctrl_num_count(0),
             missing_isbn_or_issn_count(0);
    while (MarcRecord record = marc_reader->read()) {
        ++count;

        const Leader &leader(record.getLeader());
        if (not leader.isArticle()) {
            marc_writer->write(record);
            continue;
        }

        const size_t _773_index(record.getFieldIndex("773"));
        if (_773_index == MarcRecord::FIELD_NOT_FOUND) {
            marc_writer->write(record);
            continue;
        }

        Subfields subfields(record.getSubfields(_773_index));
        if (subfields.hasSubfield('x')) {
            marc_writer->write(record);
            continue;
        }

        const auto &begin_end(subfields.getIterators('w')); // Record control number of Host Item Entry.
        if (begin_end.first == begin_end.second) {
            marc_writer->write(record);
            ++missing_host_record_ctrl_num_count;
            continue;
        }

        std::string host_id(begin_end.first->value_);
        if (StringUtil::StartsWith(host_id, "(DE-576)"))
            host_id = host_id.substr(8);
        const auto &parent_isbn_or_issn_iter(parent_id_to_isbn_and_issn_map.find(host_id));
        if (parent_isbn_or_issn_iter == parent_id_to_isbn_and_issn_map.end()) {
            marc_writer->write(record);
            ++missing_isbn_or_issn_count;
            continue;
        }

        if (MiscUtil::IsPossibleISSN(parent_isbn_or_issn_iter->second)) {
            subfields.addSubfield('x', parent_isbn_or_issn_iter->second);
            record.updateField(_773_index, subfields.toString());
            ++issns_added;
        } else { // Deal with ISBNs.
            if (not record.extractFirstSubfield("020", 'a').empty())
                continue; // We already have an ISBN.

            record.insertSubfield("020", 'a', parent_isbn_or_issn_iter->second);
            ++isbns_added;
        }
        marc_writer->write(record);
    }

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

    if ((argc != 3 and argc != 4)
        or (argc == 4 and std::strcmp(argv[1], "-v") != 0 and std::strcmp(argv[1], "--verbose") != 0))
        Usage();
    const bool verbose(argc == 4);

    const std::string marc_input_filename(argv[argc == 3 ? 1 : 2]);
    const std::string marc_output_filename(argv[argc == 3 ? 2 : 3]);
    if (unlikely(marc_input_filename == marc_output_filename))
        Error("Master input file name equals output file name!");

    std::unique_ptr<MarcReader> marc_reader(MarcReader::Factory(marc_input_filename, MarcReader::BINARY));
    std::unique_ptr<MarcWriter> marc_writer(MarcWriter::Factory(marc_output_filename, MarcWriter::BINARY));

    try {
        std::unordered_map<std::string, std::string> parent_id_to_isbn_and_issn_map;
        PopulateParentIdToISBNAndISSNMap(verbose, marc_reader.get(), &parent_id_to_isbn_and_issn_map);
        marc_reader->rewind();
        
        AddMissingISBNsOrISSNsToArticleEntries(verbose, marc_reader.get(), marc_writer.get(),
                                               parent_id_to_isbn_and_issn_map);
    } catch (const std::exception &x) {
        Error("caught exception: " + std::string(x.what()));
    }
}

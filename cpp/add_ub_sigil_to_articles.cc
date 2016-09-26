/** \file    add_ub_sigil_to_articles.cc
 *  \brief   A tool for adding the DE-21 sigil to articles entries if the containing work has the DE-21 entry.
 *  \author  Dr. Johannes Ruscheinski
 */

/*
    Copyright (C) 2016, Library of the University of Tübingen

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
#include <unordered_set>
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
#include "MarcXmlWriter.h"


void Usage() {
    std::cerr << "Usage: " << ::progname << " [-v|--verbose] master_marc_input marc_output\n";
    std::cerr << "  Adds DE-21 sigils, as appropriate, to article entries found in the\n";
    std::cerr << "  master_marc_input and writes this augmented file as marc_output.\n\n";
    std::exit(EXIT_FAILURE);
}


void CollectParentIDs(const bool verbose, File * const input,
                      std::unordered_set<std::string> * const parent_ids)
{
    if (verbose)
        std::cout << "Starting extraction of parent IDs.\n";

    unsigned count(0);
    std::string err_msg;
    while (const MarcUtil::Record record = MarcUtil::Record::XmlFactory(input)) {
        ++count;

        const Leader &leader(record.getLeader());
        if (not leader.isSerial() and not leader.isMonograph())
            continue;

        std::vector<size_t> lok_field_indices;
        record.getFieldIndices("LOK", &lok_field_indices);
        const std::vector<std::string> &fields(record.getFields());
        for (const auto lok_field_index : lok_field_indices) {
            const Subfields subfields(fields[lok_field_index]);
            if (subfields.getFirstSubfieldValue('0') == "852" and subfields.getFirstSubfieldValue('a') == "DE-21")
                parent_ids->insert(record.getControlNumber());
        }
    }

    if (not err_msg.empty())
        Error(err_msg);

    if (verbose) {
        std::cerr << "Read " << count << " records.\n";
        std::cerr << "Found " << parent_ids->size() << " relevant parent ID's.\n";
    }
}


void AddMissingSigilsToArticleEntries(const bool verbose, File * const input, File * const output,
                                      const std::unordered_set<std::string> &parent_ids)
{
    if (verbose)
        std::cout << "Starting augmentation of article entries.\n";

    MarcXmlWriter xml_writer(output);
    unsigned count(0), ids_added(0), missing_host_record_ctrl_num_count(0),
             missing_parent_id_count(0);
    while (MarcUtil::Record record = MarcUtil::Record::XmlFactory(input)) {
        record.setRecordWillBeWrittenAsXml(true);
        ++count;

        const Leader &leader(record.getLeader());
        if (not leader.isArticle()) {
            record.write(&xml_writer);
            continue;
        }

        const std::vector<DirectoryEntry> &dir_entries(record.getDirEntries());
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
        auto const parent_id_iter(parent_ids.find(host_id));
        if (parent_id_iter == parent_ids.end()) {
            record.write(&xml_writer);
            ++missing_parent_id_count;
            continue;
        }

        record.insertField("LOK", "  ""\x1F""0852""\x1F""aDE-21");
        record.write(&xml_writer);
    }

    if (verbose) {
        std::cerr << "Read " << count << " records.\n";
        std::cerr << "Added ID's to " << ids_added << " article record(s).\n";
        std::cerr << missing_host_record_ctrl_num_count << " articles had missing host record control number(s).\n";
        std::cerr << "For " << missing_parent_id_count << " articles no host ISBN nor ISSN was found.\n";
    }
}


int main(int argc, char **argv) {
    ::progname = argv[0];

    if ((argc != 3 and argc != 4) or (argc == 4 and std::strcmp(argv[1], "-v") != 0 and std::strcmp(argv[1], "--verbose") != 0))
        Usage();
    const bool verbose(argc == 4);

    const std::string marc_input_filename(argv[argc == 3 ? 1 : 2]);
    std::unique_ptr<File> marc_input(FileUtil::OpenInputFileOrDie(marc_input_filename));

    const std::string marc_output_filename(argv[argc == 3 ? 2 : 3]);
    if (unlikely(marc_input_filename == marc_output_filename))
        Error("Master input file name equals output file name!");
    std::string output_mode("w");
    File marc_output(marc_output_filename, output_mode);
    if (not marc_output)
        Error("can't open \"" + marc_output_filename + "\" for writing!");

    try {
        std::unordered_set<std::string> parent_ids;
        CollectParentIDs(verbose, marc_input.get(), &parent_ids);

        marc_input->rewind();
        AddMissingSigilsToArticleEntries(verbose, marc_input.get(), &marc_output, parent_ids);
    } catch (const std::exception &x) {
        Error("caught exception: " + std::string(x.what()));
    }
}

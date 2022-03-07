/** \file    add_publication_year_to_serials.cc
 *  \brief   Add reasonable publication year to serials provided by an external list
 *  \author  Johannes Riedl
 */

/*
    Copyright (C) 2016-2018, Library of the University of Tübingen

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

/*
    Background: Serials (i.e. "Schriftenreihen") do not in general provide a reasonable
    Sorting date, since the 008 is not properly filled. To circumvent this, we derive
    the sorting date from the the subordinate works and provide it as an (external) list.
    Based on this list, we insert the publication year to reasonable fields here
*/

#include <iostream>
#include <map>
#include <vector>
#include <cstdlib>
#include "Compiler.h"
#include "FileUtil.h"
#include "MARC.h"
#include "StringUtil.h"
#include "util.h"


namespace {


typedef std::map<std::string, std::string> SortList;


[[noreturn]] void Usage() {
    std::cerr << "Usage: " << ::progname << " sort_year_list title_data_marc21_input title_date_marc21_output\n";
    std::exit(EXIT_FAILURE);
}


void SetupPublicationYearMap(File * const sort_year_list, SortList * const sort_year_map) {
    while (not sort_year_list->eof()) {
        std::string line(sort_year_list->getline());
        std::vector<std::string> ppns_and_sort_year;
        if (unlikely(StringUtil::SplitThenTrim(line, ":", " ", &ppns_and_sort_year) != 2)) {
            LOG_WARNING("Invalid line: " + line);
            continue;
        }
        const std::string ppn(ppns_and_sort_year[0]);
        const std::string sort_year(ppns_and_sort_year[1]);
        sort_year_map->insert(std::make_pair(ppn, sort_year));
    }
}


static unsigned modified_count(0);


void ProcessRecord(MARC::Record * const record, const SortList &sort_year_map) {
    SortList::const_iterator iter(sort_year_map.find(record->getControlNumber()));
    if (iter == sort_year_map.cend())
        return;

    std::string sort_year(iter->second);

    // We insert in 190j
    // Case 1: If there is no 190 tag yet, insert subfield j and we are done
    if (not record->hasTag("190")) {
        record->insertField("190", { { 'j', sort_year } });
        ++modified_count;
        return;
    }

    // Case 2: There is a 190 tag
    MARC::Record::Range range(record->getTagRange("190"));
    for (auto &field : range) {
        MARC::Subfields subfields(field.getSubfields());
        if (subfields.hasSubfield('j'))
            LOG_ERROR("We already have a 190j subfield for PPN " + record->getControlNumber());

        // If there is no 190j subfield yet, we insert at the last field occurence
        if (field == range.back()) {
            subfields.appendSubfield('j', sort_year);
            ++modified_count;
        }
    }
}


void AddPublicationYearField(MARC::Reader * const marc_reader, MARC::Writer * const marc_writer, const SortList &sort_year_map) {
    unsigned record_count(0);
    while (MARC::Record record = marc_reader->read()) {
        ProcessRecord(&record, sort_year_map);
        marc_writer->write(record);
        ++record_count;
    }

    std::cout << "Modified " << modified_count << " of " << record_count << " record(s).\n";
}


} // unnamed namespace


int main(int argc, char **argv) {
    ::progname = argv[0];

    if (argc != 4)
        Usage();

    const std::string sort_year_list_filename(argv[1]);
    const std::string marc_input_filename(argv[2]);
    const std::string marc_output_filename(argv[3]);

    if (unlikely(marc_input_filename == marc_output_filename))
        LOG_ERROR("Marc input filename equals marc output filename");

    if (unlikely(marc_input_filename == sort_year_list_filename || marc_output_filename == sort_year_list_filename))
        LOG_ERROR("Either marc input filename or marc output filename equals the sort list filename");

    try {
        std::unique_ptr<MARC::Reader> marc_reader(MARC::Reader::Factory(marc_input_filename, MARC::FileType::BINARY));
        std::unique_ptr<File> sort_year_list(FileUtil::OpenInputFileOrDie(sort_year_list_filename));
        std::unique_ptr<MARC::Writer> marc_writer(MARC::Writer::Factory(marc_output_filename, MARC::FileType::BINARY));
        SortList sort_year_map;
        SetupPublicationYearMap(sort_year_list.get(), &sort_year_map);
        AddPublicationYearField(marc_reader.get(), marc_writer.get(), sort_year_map);
    } catch (const std::exception &x) {
        LOG_ERROR("caught exception: " + std::string(x.what()));
    }
}

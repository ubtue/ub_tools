/** \file    add_publication_year_to_serials.cc
 *  \brief   Add reasonable publication year to serials provided by an external list
 *  \author  Johannes Riedl
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
#include "MarcReader.h"
#include "MarcRecord.h"
#include "MarcWriter.h"
#include "StringUtil.h"
#include "Subfields.h"
#include "util.h"


static unsigned record_count(0);
static unsigned modified_count(0);


typedef std::map<std::string, std::string> SortList;

  
void Usage() {
    std::cerr << "Usage: " << ::progname << " sort_year_list title_data_marc_input title_date_marc_output\n";
    std::exit(EXIT_FAILURE);
}


void SetupPublicationYearMap(File * const sort_year_list, SortList * const sort_year_map) {
    while (not sort_year_list->eof()) {
       std::string line(sort_year_list->getline());
       std::vector<std::string> ppns_and_sort_year;
       if (unlikely(StringUtil::SplitThenTrim(line, ":", " ",  &ppns_and_sort_year) != 2)) {
           Warning("Invalid line: " + line);
           continue;
       }       
       const std::string ppn(ppns_and_sort_year[0]);
       const std::string sort_year(ppns_and_sort_year[1]);
       sort_year_map->insert(std::make_pair(ppn, sort_year));     
    }
}


void ProcessRecord(MarcRecord * const record, const SortList &sort_year_map) {
    SortList::const_iterator iter(sort_year_map.find(record->getControlNumber()));
    if (iter == sort_year_map.cend())
       return;

    std::string sort_year(iter->second);

    // We insert in 936j
    const std::string _936Tag("936");
    const char subfield_code('j');
    std::vector<size_t> _936Indices;
    record->getFieldIndices(_936Tag, &_936Indices);

    // Case 1: If there is no 936 tag yet, insert subfield j and we are done
    if (_936Indices.empty()) {
        record->insertSubfield(_936Tag, 'j', sort_year);
        ++modified_count;
        return;
    }

    // Case 2: There is a 936 tag
    for(auto &_936Index : _936Indices) {
        Subfields _936Subfields = record->getSubfields(_936Tag);
        if (_936Subfields.hasSubfield(subfield_code))
            Error("We already have a 936j subfield for PPN " + record->getControlNumber());

        // If there is no 936j subfield yet, we insert at the last field occurence
        if (_936Index == _936Indices.back()) {
            _936Subfields.addSubfield('j', sort_year);
            record->updateField(_936Index, _936Subfields);
            ++modified_count;
        }
    }
}


void AddPublicationYearField(MarcReader * const marc_reader, MarcWriter * const marc_writer,
                             const SortList &sort_year_map) {
    while (MarcRecord record = marc_reader->read()) {
        ProcessRecord(&record, sort_year_map);
        marc_writer->write(record);
        ++record_count;
    }

    std::cerr << "Modified " << modified_count << " of " << record_count << " record(s).\n";
}


int main(int argc, char **argv) {
    ::progname = argv[0];

    if (argc != 4)
        Usage();

    const std::string sort_year_list_filename(argv[1]);
    const std::string marc_input_filename(argv[2]);
    const std::string marc_output_filename(argv[3]);

    if (unlikely(marc_input_filename == marc_output_filename))
        Error("Marc input filename equals marc output filename");

    if (unlikely(marc_input_filename == sort_year_list_filename || marc_output_filename == sort_year_list_filename))
        Error("Either marc input filename or marc output filename equals the sort list filename");

    try {
        std::unique_ptr<MarcReader> marc_reader(MarcReader::Factory(marc_input_filename, MarcReader::BINARY));
        std::unique_ptr<File> sort_year_list(FileUtil::OpenInputFileOrDie(sort_year_list_filename));
        std::unique_ptr<MarcWriter> marc_writer(MarcWriter::Factory(marc_output_filename, MarcWriter::BINARY));
        SortList sort_year_map;
        SetupPublicationYearMap(sort_year_list.get(), &sort_year_map);
        AddPublicationYearField(marc_reader.get(), marc_writer.get(), sort_year_map);
    } catch (const std::exception &x) {
        Error("caught exception: " + std::string(x.what()));
    }
}

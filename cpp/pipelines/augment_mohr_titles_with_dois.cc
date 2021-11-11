/** \file    augment_mohr_titles_with_dois
 *  \brief   Augment title data of printed works with dois if no
 *           electronic version exists in IxTheo data
 *  \author  Johannes Riedl
 */

/*
    Copyright (C) 2021 Library of the University of Tübingen

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


#include <iostream>
#include <unordered_map>
#include <vector>
#include "MARC.h"
#include "StringUtil.h"
#include "util.h"

namespace {

[[noreturn]] void Usage() {
    ::Usage("marc_input mohr_book_data marc_output");
}

void CreatePPNToISBNMappings(MARC::Reader * const marc_reader,
                          std::unordered_multimap<std::string, std::string> * const print_ppn_to_isbn_map,
                          std::unordered_multimap<std::string, std::string> * const electronic_ppn_to_isbn_map)
{
    while (MARC::Record record = marc_reader->read()) {

        if (record.isMonograph()) {
            const auto &isbns(record.getISBNs());
            if (record.isElectronicResource()) {
                for (const auto &isbn : isbns)
                     electronic_ppn_to_isbn_map->emplace(record.getControlNumber(), isbn);
            } else {
                for (const auto &isbn : isbns)
                     print_ppn_to_isbn_map->emplace(record.getControlNumber(), isbn);
            }
       }
    }
}

void CreateMohrISBNToDOIMapping(MARC::Reader * const marc_reader,
                               std::unordered_map<std::string, std::string> * const mohr_isbn_to_doi_map)
{
   while (MARC::Record record = marc_reader->read()) {
          const auto dois(record.getDOIs());
          if (dois.size() != 1)
              LOG_ERROR("No unique DOI for \"" + record.getControlNumber() + '"');

          // Get "native" ISBNs
          for (const auto &native_isbn : record.getISBNs())
               mohr_isbn_to_doi_map->emplace(native_isbn, *(dois.begin()));

          // Get "alternative" print ISBN
          for (const auto &alternative_isbn : record.getSubfieldValues("776", 'z'))
               mohr_isbn_to_doi_map->emplace(alternative_isbn, *(dois.begin()));
   }
}


void ProcessRecords(MARC::Reader * const marc_reader,
                    MARC::Writer * const marc_writer,
                    const std::unordered_multimap<std::string, std::string> &print_ppn_to_isbn_map,
                    const std::unordered_multimap<std::string, std::string> &electronic_ppn_to_isbn_map,
                    const std::unordered_map<std::string, std::string> &mohr_isbn_to_doi_map)
{
    unsigned count(0), new_electronic_dois(0), new_print_dois(0);
    while (MARC::Record record = marc_reader->read()) {
        ++count;
        const std::string ppn(record.getControlNumber());
        if (electronic_ppn_to_isbn_map.contains(ppn)) {
            const auto electronic_isbns(electronic_ppn_to_isbn_map.equal_range(ppn));
            for (auto ppn_and_isbn(electronic_isbns.first); ppn_and_isbn != electronic_isbns.second; ++ppn_and_isbn) {
                 const auto isbn(ppn_and_isbn->second);
                 if (mohr_isbn_to_doi_map.find(isbn) != mohr_isbn_to_doi_map.end()) {
                     const auto &record_dois(record.getDOIs());
                     const auto doi_for_isbn(mohr_isbn_to_doi_map.find(isbn)->second);
                     // Insert doi if not present
                     if (std::find(record_dois.begin(), record_dois.end(), doi_for_isbn) == record_dois.end()) {
                         LOG_INFO("Inserting previously not existing DOI \"" + doi_for_isbn + "\" for electronic record " +
                                  "\"" + record.getControlNumber());
                         record.insertField("024", { { 'a', doi_for_isbn }, { '2', "doi" } });
                         ++new_electronic_dois;
                     }
                     break;
                 }
             }
        } else if (print_ppn_to_isbn_map.contains(ppn)) {
            const auto print_isbns(print_ppn_to_isbn_map.equal_range(ppn));
            for (auto ppn_and_isbn(print_isbns.first); ppn_and_isbn != print_isbns.second; ++ppn_and_isbn) {
                 const auto isbn(ppn_and_isbn->second);
                 if (mohr_isbn_to_doi_map.find(isbn) != mohr_isbn_to_doi_map.end()) {
                     const auto &record_dois(record.getDOIs());
                     const auto doi_for_isbn(mohr_isbn_to_doi_map.find(isbn)->second);
                     // Insert doi if not present
                     if (std::find(record_dois.begin(), record_dois.end(), doi_for_isbn) == record_dois.end()) {
                         LOG_INFO("Inserting previously not existing DOI \"" + doi_for_isbn + "\" for print record " +
                                  "\"" + record.getControlNumber());
                         record.insertField("024", { { 'a', doi_for_isbn }, { '2', "doi" } });
                         ++new_print_dois;
                     }
                     break;
                 }
            }
        }
        marc_writer->write(record);
    }
    LOG_INFO("Inserted " + std::to_string(new_electronic_dois) + " new electronic DOIs and " +
             std::to_string(new_print_dois) + " print DOIs of " + std::to_string(count) + " records altogether");
}


} // unnamed namespace


int Main(int argc, char **argv) {
    if (argc != 4)
        Usage();

    const std::string marc_input_filename(argv[1]);
    const std::string mohr_book_data(argv[2]);
    const std::string marc_output_filename(argv[3]);
    if (unlikely(marc_input_filename == marc_output_filename))
        LOG_ERROR("Title data input file name equals output file name!");
    if (unlikely(mohr_book_data == marc_output_filename))
        LOG_ERROR("Mohr marc data input file name equals output file name!");

    std::unique_ptr<MARC::Reader> marc_reader(MARC::Reader::Factory(marc_input_filename));
    std::unique_ptr<MARC::Reader> mohr_book_reader(MARC::Reader::Factory(mohr_book_data));
    std::unique_ptr<MARC::Writer> marc_writer(MARC::Writer::Factory(marc_output_filename));

    std::unordered_multimap<std::string, std::string> print_ppn_to_isbn_map;
    std::unordered_multimap<std::string, std::string> electronic_ppn_to_isbn_map;
    std::unordered_map<std::string, std::string> mohr_isbn_to_doi_map;
    LOG_INFO("Create ISBN to DOI map from Mohr data");
    CreateMohrISBNToDOIMapping(mohr_book_reader.get(), &mohr_isbn_to_doi_map);
    LOG_INFO("We extracted " + std::to_string(mohr_isbn_to_doi_map.size())  + " ISBN to DOI mappings");
    LOG_INFO("Create PPN to ISBN Mappings");
    CreatePPNToISBNMappings(marc_reader.get(), &print_ppn_to_isbn_map, &electronic_ppn_to_isbn_map);
    LOG_INFO("We extracted " + std::to_string(print_ppn_to_isbn_map.size()) + " print PPN to ISBN mappings");
    LOG_INFO("We extracted " + std::to_string(electronic_ppn_to_isbn_map.size()) + " electronic PPN to ISBN mappings");
    LOG_INFO("Augmenting records");
    marc_reader->rewind();
    ProcessRecords(marc_reader.get(), marc_writer.get(),
                   print_ppn_to_isbn_map, electronic_ppn_to_isbn_map, mohr_isbn_to_doi_map);

    return EXIT_SUCCESS;
}


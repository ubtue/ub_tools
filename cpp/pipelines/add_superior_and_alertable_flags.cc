/** \file    add_superior_and_alertable_flags.cc
 *  \author  Oliver Obenland
 *  \author  Dr. Johannes Ruscheinski
 *
 *  A tool for marking superior records that have associated inferior records in our data sets.
 */

/*
    Copyright (C) 2016-2019, Library of the University of Tübingen

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
#include <unordered_set>
#include <cstdlib>
#include <cstring>
#include "Compiler.h"
#include "MARC.h"
#include "RegexMatcher.h"
#include "StringUtil.h"
#include "util.h"
#include "Zeder.h"


namespace {


[[noreturn]] void Usage() {
    std::cerr << "Usage: " << ::progname << " zeder_flavour marc_input marc_output\n";
    std::exit(EXIT_FAILURE);
}


void LoadSuperiorPPNs(MARC::Reader * const marc_reader, std::unordered_set<std::string> * const superior_ppns) {
    const std::vector<std::string> TAGS{ "800", "810", "830", "773" };
    while (const MARC::Record record = marc_reader->read()) {
        for (const auto &tag : TAGS) {
            for (const auto &field : record.getTagRange(tag)) {
                const MARC::Subfields subfields(field.getSubfields());
                const std::string subfield_w_contents(subfields.getFirstSubfieldWithCode('w'));
                if (StringUtil::StartsWith(subfield_w_contents, "(DE-627)"))
                    superior_ppns->emplace(subfield_w_contents.substr(__builtin_strlen("(DE-627)")));
            }
        }
    }

    LOG_INFO("Found " + std::to_string(superior_ppns->size()) + " superior PPNs.");
}


bool SeriesHasNotBeenCompleted(const MARC::Record &record) {
    const auto _008_field(record.getFirstField("008"));
    if (unlikely(_008_field == record.end()))
        return false;

    return _008_field->getContents().substr(11, 4) == "9999";
}


void ProcessRecord(MARC::Writer * const marc_writer, const std::unordered_set<std::string> &superior_ppns,
                   MARC::Record * const record, unsigned * const modified_count, const std::set<std::string> &ppns_in_kat)
{
    // Don't add the flag twice:
    if (record->getFirstField("SPR") != record->end()) {
        marc_writer->write(*record);
        return;
    }

    MARC::Subfields superior_subfields;

    // Set the we are a "superior" record, if appropriate:
    const auto iter(superior_ppns.find(record->getControlNumber()));
    if (iter != superior_ppns.end())
        superior_subfields.addSubfield('a', "1"); // Could be anything but we can't have an empty field.

    // Set the, you-can-subscribe-to-this flag, if appropriate:
    if (record->isSerial() and SeriesHasNotBeenCompleted(*record) and ppns_in_kat.find(record->getControlNumber()) != ppns_in_kat.end())
        superior_subfields.addSubfield('b', "1");

    if (not superior_subfields.empty()) {
        record->insertField("SPR", superior_subfields);
        ++*modified_count;
    }

    marc_writer->write(*record);
}


void AddSuperiorFlag(MARC::Reader * const marc_reader, MARC::Writer * const marc_writer,
                     const std::unordered_set<std::string> &superior_ppns, const std::string &flavour)
{
    unsigned modified_count(0);
    
    Zeder::SimpleZeder zeder(Zeder::IXTHEO, { "eppn", "pppn", "kat" });
    std::set<std::string> ppns_in_kat;
    for (const auto &journal : zeder) {
        if (not journal.hasAttribute("kat"))
            continue;
        std::string kat = journal.lookup("kat");
        if (not StringUtil::EndsWith(kat, flavour, true))
            continue;
        ppns_in_kat.emplace(journal.lookup("pppn"));
        ppns_in_kat.emplace(journal.lookup("eppn"));
    }

    while (MARC::Record record = marc_reader->read())
        ProcessRecord(marc_writer, superior_ppns, &record, &modified_count, ppns_in_kat);

    LOG_INFO("Modified " + std::to_string(modified_count) + " record(s).");
}


} // unnamed namespace


int main(int argc, char **argv) {
    ::progname = argv[0];

    if (argc != 4)
        Usage();

    std::string flavour;
    if (__builtin_strcmp(argv[1], "ixtheo") == 0)
        flavour = "IxTheo";
    else if (__builtin_strcmp(argv[1], "krimdok") == 0)
        flavour = "KrimDok"; //KrimDok is not used in Zeder, if used in the future, check naming
    else
        LOG_ERROR("zeder_flavour must be one of (ixtheo,krimdok)!");


    const std::unique_ptr<MARC::Reader> marc_reader(MARC::Reader::Factory(argv[2]));
    const std::unique_ptr<MARC::Writer> marc_writer(MARC::Writer::Factory(argv[3]));

    try {
        std::unordered_set<std::string> superior_ppns;
        LoadSuperiorPPNs(marc_reader.get(), &superior_ppns);
        marc_reader->rewind();
        AddSuperiorFlag(marc_reader.get(), marc_writer.get(), superior_ppns, flavour);
    } catch (const std::exception &x) {
        LOG_ERROR("caught exception: " + std::string(x.what()));
    }
}

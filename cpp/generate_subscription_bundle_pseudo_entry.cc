/** \file    generate_subscription_bundle_pseudo_entry.cc
 *  \brief   Generate a record that represents a bundle of alerts
 *  \author  Johannes Riedl
 */

/*
    Copyright (C) 2018 Library of the University of Tübingen

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
#include "IniFile.h"
#include "MARC.h"
#include "UBTools.h"


namespace {


[[noreturn]] void Usage() {
 std::cerr << "Usage: " << ::progname << "output_filename\n"
           << "Generate a dummy entry for subscriptions from the congfiguration given in journal_alert_bundles.conf\n";
 std::exit(EXIT_FAILURE);
}


MARC::Record GenerateRecord(const std::string &record_id, const std::string &bundle_name) {
    MARC::Record record("00000nac a2200000 u 4500");
    record.insertField("001", record_id);
    record.insertField("005", "20181206120000.0:");
    record.insertField("008", "181206s2018");
    record.insertField("245", MARC::Subfields( { { 'a', bundle_name }, { 'b', "Testentry" }, { 'h', "Subscription Bundle"} }));
    record.insertField("SPR", MARC::Subfields( { { 'a', "1" /* is superior work */ }, 
                                                 { 'b', "1" /* series has not been completed */ } }));
    record.insertField("935", MARC::Subfields( { { 'c', "subskriptionspaket" } }));
    return record;
}


void ProcessBundle(MARC::Writer * const marc_writer, const std::string &bundle_name) {
    marc_writer->write(GenerateRecord("bundle_" + bundle_name, bundle_name));
}

} //unnamed namespace


int Main(int argc, char **argv) {
    if (argc < 2)
        Usage();

    const std::string marc_output_filename(argv[1]);
    std::unique_ptr<MARC::Writer> marc_writer(MARC::Writer::Factory(marc_output_filename));
    const IniFile bundles_config(UBTools::GetTuelibPath() + "journal_alert_bundles.conf");
    for (const auto &bundle_name : bundles_config.getSections()) {
         if (not bundle_name.empty())
             ProcessBundle(marc_writer.get(), bundle_name);
    }
    return EXIT_SUCCESS;
}

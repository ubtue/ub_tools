/** \file    add_subscription_bundle_entries.cc
 *  \brief   Generates MARC title records that represent a journal bundle for alerting and
 *           inserts link tags into the individual journal records referencing the corresponding bundle records.
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
#include "StringUtil.h"
#include "TimeUtil.h"
#include "UBTools.h"


namespace {


using BundleToPPNPair = std::pair<std::string, std::set<std::string>>;
using BundleToPPNsMap = std::map<std::string, std::set<std::string>>;


MARC::Record GenerateBundleRecord(const std::string &record_id, const std::string &bundle_name, const std::vector<std::string> &instances,
                                  const std::string &description, const std::string &media_type)
{
    const std::string today(TimeUtil::GetCurrentDateAndTime("%y%m%d"));
    // exclude from Ixtheo e.g. because it's a pure Relbib list
    const bool exclude_ixtheo(std::find(instances.begin(), instances.end(), "ixtheo") == instances.end());
    const bool include_relbib(std::find(instances.begin(), instances.end(), "relbib") != instances.end());
    const bool include_bibstudies(std::find(instances.begin(), instances.end(), "bibstudies") != instances.end());
    const bool include_churchlaw(std::find(instances.begin(), instances.end(), "churchlaw") != instances.end());
    MARC::Record record("00000nac a2200000 u 4500");
    record.insertField("001", record_id);
    record.insertField("005", "20" + today + "12000000.0:");
    record.insertField("008", today + 's' + TimeUtil::GetCurrentYear());
    record.insertField("245", { { 'a', bundle_name }, { 'h', "Subscription Bundle" } } );
    record.insertField("SPR", { { 'a', "1" /* is superior work */ },
                                { 'b', "1" /* series has not been completed */ } });
    record.insertField("935", 'c', "subskriptionspaket" );

    if (not description.empty())
        record.insertField("500", 'a', description);

    if (exclude_ixtheo)
        record.addSubfield("935", 'x', "1");
    if (include_relbib)
        record.insertField("REL", { { 'a', "1" } });
    if (include_bibstudies)
        record.insertField("BIB", { { 'a', "1" } });
    if (include_churchlaw)
        record.insertField("CAN", { { 'a', "1" } });
    
    std::vector<MARC::Subfield> elc_subfields;
    if (media_type == "online_and_print" or media_type == "online")
        elc_subfields.push_back({ 'a', "1" });
    if (media_type == "online_and_print" or media_type == "print")
        elc_subfields.push_back({ 'b', "1" });
    record.insertField("ELC", elc_subfields);
    
    return record;
}


void ExtractBundlePPNs(const std::string bundle_name, const IniFile &bundles_config, BundleToPPNsMap * const bundle_to_ppns_map) {
    // Generate PPN vector
    const std::string bundle_ppns_string(bundles_config.getString(bundle_name, "ppns", ""));
    if (bundle_ppns_string.empty())
        return;
    std::vector<std::string> bundle_ppns;
    StringUtil::SplitThenTrim(bundle_ppns_string, "," , " \t", &bundle_ppns);
    // Generate the bundle mapping
    bundle_to_ppns_map->insert(BundleToPPNPair(bundle_name, std::set<std::string>(bundle_ppns.begin(), bundle_ppns.end())));
}


void GenerateBundleEntry(MARC::Writer * const marc_writer, const std::string &bundle_name, const IniFile &bundles_config) {
    const std::string instances_string(bundles_config.getString(bundle_name, "instances", ""));
    std::vector<std::string> instances;
    const std::string description(bundles_config.getString(bundle_name, "description", ""));
    const std::string media_type(bundles_config.getString(bundle_name, "media_type", ""));
    if (not instances_string.empty())
        StringUtil::SplitThenTrim(instances_string, ",", " \t", &instances);
    marc_writer->write(GenerateBundleRecord(bundle_name, bundles_config.getString(bundle_name, "display_name"), instances, description, media_type));
}


void ProcessRecords(MARC::Reader * const marc_reader, MARC::Writer * const marc_writer,
                    const BundleToPPNsMap &bundle_to_ppns_map)
    {
    while (MARC::Record record = marc_reader->read()) {
        MARC::Subfields bundle_subfields;
        for (const auto &bundle_to_ppns : bundle_to_ppns_map) {
            if (bundle_to_ppns.second.find(record.getControlNumber()) != bundle_to_ppns.second.end())
               bundle_subfields.addSubfield('a', bundle_to_ppns.first);
        }
        if (not bundle_subfields.empty())
            record.insertField("BSP" /* Bundle Superior */, bundle_subfields);
        marc_writer->write(record);
    }
}

} //unnamed namespace


int Main(int argc, char **argv) {
    if (argc != 3)
        ::Usage("marc_input marc_output\n"
                "Generate a dummy entry for subscriptions from the configuration given in journal_alert_bundles.conf\n");

    const std::string marc_input_filename(argv[1]);
    const std::string marc_output_filename(argv[2]);
    std::unique_ptr<MARC::Reader> marc_reader(MARC::Reader::Factory(marc_input_filename));
    std::unique_ptr<MARC::Writer> marc_writer(MARC::Writer::Factory(marc_output_filename));
    BundleToPPNsMap bundle_to_ppns_map;

    // Insert the pseudo entries at the beginning and generate the PPN map
    const IniFile bundles_config(UBTools::GetTuelibPath() + "journal_alert_bundles.conf");
    for (const auto &bundle_name : bundles_config.getSections()) {
         if (not bundle_name.empty()) {
             GenerateBundleEntry(marc_writer.get(), bundle_name, bundles_config);
             ExtractBundlePPNs(bundle_name, bundles_config, &bundle_to_ppns_map);
         }
    }
    // Tag the title data
    ProcessRecords(marc_reader.get(), marc_writer.get(), bundle_to_ppns_map);
    return EXIT_SUCCESS;
}

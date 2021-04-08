/** \file   generate_subscription_packets.cc
 *  \brief  Imports data from Zeder and writes a subscription packets defintion file.
 *  \author Dr. Johannes Ruscheinski (johannes.ruscheinski@uni-tuebingen.de)
 *  \copyright 2020-2021 Universitätsbibliothek Tübingen.  All rights reserved.
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

#include <map>
#include <unordered_map>
#include <set>
#include "Compiler.h"
#include "FileUtil.h"
#include "IniFile.h"
#include "StringUtil.h"
#include "util.h"
#include "Zeder.h"


namespace {


[[noreturn]] void Usage() {
    ::Usage("packet_definition_config_file packet_subscriptions_output\n"
            "\tFor the documentation of the input config file, please see data/generate_subscription_packets.README.");
}


// Return true if an entry of "class_list" equals one of the vertical-bar-separated values of "expected_values_str."
bool FoundExpectedClassValue(const std::string &expected_values_str, const std::string &class_list_str) {
    std::vector<std::string> expected_values;
    StringUtil::Split(expected_values_str, '|', &expected_values);

    std::vector<std::string> class_list;
    StringUtil::SplitThenTrimWhite(class_list_str, ',', &class_list);

    for (const auto &class_str : class_list) {
        if (std::find_if(expected_values.cbegin(), expected_values.cend(),
                         [&class_str](auto &expected_value) { return ::strcasecmp(class_str.c_str(), expected_value.c_str()) == 0; }) != expected_values.cend())
            return true;
    }
    return false;
}


bool IncludeJournal(const Zeder::Entry &journal, const IniFile::Section &filter_section) {
    for (const auto &entry : filter_section) {
        if (entry.name_.empty() or entry.name_ == "description")
            continue;

        const std::string &zeder_column_name(entry.name_);
        const auto column_value(StringUtil::TrimWhite(journal.lookup(zeder_column_name == "except_class" ? "class" : zeder_column_name)));
        if (column_value.empty())
        {
            LOG_INFO("\tcolumn " + zeder_column_name + " was empty!");
            return false;

        }

        const bool found_it(FoundExpectedClassValue(entry.value_, column_value));
        if (zeder_column_name == "except_class") {
            if (found_it)
                return false;
        } else if (not found_it)
            return false;
    }

    return true;
}


// Please note that Zeder PPN entries are separated by spaces and, unlike what the column names "print_ppn" and
// "online_ppn" imply may in rare cases contain space-separated lists of PPN's.
void ProcessPPNs(const std::string &ppns, std::set<std::string> * const bundle_ppns) {
    std::vector<std::string> individual_ppns;
    StringUtil::Split(ppns, ' ', &individual_ppns);
    bundle_ppns->insert(individual_ppns.cbegin(), individual_ppns.cend());
}


std::string EscapeDoubleQuotes(const std::string &s) {
    std::string escaped_s;
    escaped_s.reserve(s.size());

    for (const char ch : s) {
        if (ch == '"' or ch == '\\')
            escaped_s += '\\';
        escaped_s += ch;
    }

    return escaped_s;
}


void GenerateBundleDefinition(const Zeder::SimpleZeder &zeder, const std::string &bundle_instances,
                              const IniFile::Section &section, File * const output_file)
{
    unsigned included_journal_count(0);
    std::set<std::string> bundle_ppns; // We use a std::set because it is automatically being sorted for us.
    for (const auto &journal : zeder) {
        if (journal.empty() or not IncludeJournal(journal, section))
            continue;

        ++included_journal_count;
        const auto print_ppns(journal.lookup("pppn"));
        const auto online_ppns(journal.lookup("eppn"));

        if (print_ppns.empty() and online_ppns.empty()) {
            --included_journal_count;
            LOG_WARNING("Zeder entry #" + std::to_string(journal.getId()) + " is missing print and online PPN's!");
            continue;
        }

        // Prefer online journals to print journals:
        if (not online_ppns.empty())
            ProcessPPNs(online_ppns, &bundle_ppns);
        else
            ProcessPPNs(print_ppns, &bundle_ppns);
    }

    if (bundle_ppns.empty())
        LOG_WARNING("No bundle generated for \"" + section.getSectionName() + "\" because there were no matching entries in Zeder!");
    else {
        (*output_file) << '[' << section.getSectionName() << "]\n";
        (*output_file) << "display_name = \"" << EscapeDoubleQuotes(section.getSectionName()) << "\"\n";
        const auto description(section.find("description"));
        if (description != section.end())
            (*output_file) << "description  = \"" << EscapeDoubleQuotes(description->value_) << "\"\n";
        (*output_file) << "instances    = \"" << bundle_instances << "\"\n";
        (*output_file) << "ppns         = " << StringUtil::Join(bundle_ppns, ',') << '\n';
        (*output_file) << '\n';
    }

    LOG_INFO("included " + std::to_string(included_journal_count) + " journal(s) with " + std::to_string(bundle_ppns.size())
             + " PPN's in the bundle for \"" + section.getSectionName() + "\".");
}


} // unnamed namespace


int Main(int argc, char *argv[]) {
    if (argc != 3)
        Usage();

    const IniFile packet_definitions_ini_file(argv[1]);
    const auto zeder_instance(packet_definitions_ini_file.getString("", "zeder_instance"));
    if (zeder_instance != "ixtheo" and zeder_instance != "relbib")
        LOG_ERROR("zeder_instance in \"" + packet_definitions_ini_file.getFilename()
                  + "\" must be either \"ixtheo\" or \"relbib\"!");

    const Zeder::SimpleZeder zeder(zeder_instance == "ixtheo" ? Zeder::IXTHEO : Zeder::KRIMDOK);
    if (not zeder)
        LOG_ERROR("can't connect to the Zeder MySQL server!");
    if (unlikely(zeder.empty()))
        LOG_ERROR("found no Zeder entries matching any of our requested columns!"
                  " (This *should* not happen as we included the column ID!)");

    const auto bundle_instances(packet_definitions_ini_file.getString("", "bundle_instances"));

    const auto bundle_definitions_output_file(FileUtil::OpenOutputFileOrDie(argv[2]));
    for (const auto &section : packet_definitions_ini_file) {
        if (section.getSectionName().empty())
            continue; // Skip the global section.
        GenerateBundleDefinition(zeder, bundle_instances, section, bundle_definitions_output_file.get());
    }

    return EXIT_SUCCESS;
}

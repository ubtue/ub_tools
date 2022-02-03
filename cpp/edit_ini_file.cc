/** \brief Utility for copying and modifying of our IniFile-type config files.
 *  \author Dr. Johannes Ruscheinski (johannes.ruscheinski@uni-tuebingen.de)
 *
 *  \copyright 2018 Universitätsbibliothek Tübingen.  All rights reserved.
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

#include <iostream>
#include <stdexcept>
#include <vector>
#include <cstdio>
#include <cstdlib>
#include "IniFile.h"
#include "StringUtil.h"
#include "util.h"


namespace {


[[noreturn]] void Usage() {
    std::cerr << "Usage: " << ::progname << " input output edit1 [edit2 ... editN]\n"
              << "       Where possible edit instructions are\n"
              << "       --delete-section=section_name\n"
              << "       --delete-entry=section_name:entry_name\n"
              << "       --insert-entry=section_name:entry_name:value\n"
              << "       --append-section=section_name\n"
              << "       --replace-value=section_name:entry_name:value\n"
              << "       To include colons in values, you can backslash-escape them.\n"
              << "\n\n";
    std::exit(EXIT_FAILURE);
}


void DeleteSection(IniFile * const ini_file, const std::string &section_name) {
    if (not ini_file->deleteSection(section_name))
        LOG_ERROR("can't delete non-existent section \"" + section_name + "\"!");
}


size_t SplitOnColon(const std::string &s, std::vector<std::string> * const parts) {
    parts->clear();

    bool escaped(false);
    std::string current_part;
    for (auto ch : s) {
        if (escaped) {
            escaped = false;
            current_part += ch;
        } else if (ch == '\\')
            escaped = true;
        else if (ch == ':') {
            parts->emplace_back(current_part);
            current_part.clear();
        } else
            current_part += ch;
    }
    parts->emplace_back(current_part);

    return parts->size();
}


void DeleteEntry(IniFile * const ini_file, const std::string &section_name_and_entry_name) {
    std::vector<std::string> parts;
    if (SplitOnColon(section_name_and_entry_name, &parts) != 2)
        LOG_ERROR("can't split \"" + section_name_and_entry_name + "\" into a section and entry name!");
    if (not ini_file->deleteEntry(parts[0], parts[1]))
        LOG_ERROR("can't delete non-existent entry \"" + parts[1] + "\" in section \"" + parts[0] + "\"!");
}


void InsertEntry(IniFile * const ini_file, const std::string &section_name_and_entry_name) {
    std::vector<std::string> parts;
    if (SplitOnColon(section_name_and_entry_name, &parts) != 3)
        LOG_ERROR("can't split \"" + section_name_and_entry_name + "\" into a section name, entry name and value!");

    if (ini_file->variableIsDefined(parts[0], parts[1]))
        LOG_ERROR("can't insert existing entry \"" + section_name_and_entry_name + "\"!");

    auto section(ini_file->getSection(parts[0]));
    if (section == ini_file->end()) {
        ini_file->appendSection(parts[0]);
        section = ini_file->getSection(parts[0]);
    }

    section->insert(parts[1], parts[2]);
}


void AppendSection(IniFile * const ini_file, const std::string &section_name) {
    if (not ini_file->appendSection(section_name))
        LOG_ERROR("can't create existent section \"" + section_name + "\"!");
}


void ReplaceValue(IniFile * const ini_file, const std::string &section_name_and_entry_name) {
    std::vector<std::string> parts;
    if (SplitOnColon(section_name_and_entry_name, &parts) != 3)
        LOG_ERROR("can't split \"" + section_name_and_entry_name + "\" into a section name, entry name and value!");

    if (not ini_file->variableIsDefined(parts[0], parts[1]))
        LOG_ERROR("can't replace a non-existing entry \"" + section_name_and_entry_name + "\"!");

    auto section(ini_file->getSection(parts[0]));
    section->replace(parts[1], parts[2]);
}


void PerformEdit(IniFile * const ini_file, const std::string &edit_instruction) {
    if (StringUtil::StartsWith(edit_instruction, "--delete-section="))
        DeleteSection(ini_file, edit_instruction.substr(__builtin_strlen("--delete-section=")));
    else if (StringUtil::StartsWith(edit_instruction, "--delete-entry="))
        DeleteEntry(ini_file, edit_instruction.substr(__builtin_strlen("--delete-entry=")));
    else if (StringUtil::StartsWith(edit_instruction, "--insert-entry="))
        InsertEntry(ini_file, edit_instruction.substr(__builtin_strlen("--insert-entry=")));
    else if (StringUtil::StartsWith(edit_instruction, "--append-section="))
        AppendSection(ini_file, edit_instruction.substr(__builtin_strlen("--append-section=")));
    else if (StringUtil::StartsWith(edit_instruction, "--replace-value="))
        ReplaceValue(ini_file, edit_instruction.substr(__builtin_strlen("--replace-value=")));
    else
        LOG_ERROR("unknown edit instruction: \"" + edit_instruction + "\"!");
}


} // unnamed namespace


int Main(int argc, char *argv[]) {
    ::progname = argv[0];

    if (argc < 4)
        Usage();

    IniFile ini_file(argv[1]);

    for (int arg_no(3); arg_no < argc; ++arg_no)
        PerformEdit(&ini_file, argv[arg_no]);

    ini_file.write(argv[2]);

    return EXIT_SUCCESS;
}

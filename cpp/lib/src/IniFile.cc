/** \file    IniFile.cc
 *  \brief   Implementation of class IniFile.
 *  \author  Dr. Johannes Ruscheinski
 *  \author  Artur Kedzierski
 *  \author  Dr. Gordon W. Paynter
 */

/*
 *  Copyright 2002-2008 Project iVia.
 *  Copyright 2002-2008 The Regents of The University of California.
 *  Copyright 2015,2018 Universitätsbibliothek Tübingen
 *
 *  This file is part of the libiViaCore package.
 *
 *  The libiViaCore package is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public License as
 *  published by the Free Software Foundation; either version 2 of the License,
 *  or (at your option) any later version.
 *
 *  libiViaCore is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public License
 *  along with libiViaCore; if not, write to the Free Software Foundation, Inc.,
 *  59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "IniFile.h"
#include <cctype>
#include <cstdio>
#include <cerrno>
#include <cstring>
#include <unistd.h>
#include <fstream>
#include <stdexcept>
#include "Compiler.h"
#include "FileUtil.h"
#include "StringUtil.h"
#include "util.h"


void IniFile::Section::insert(const std::string &variable_name, const std::string &value,
                              const DupeInsertionBehaviour dupe_insertion_behaviour)
{
    if (dupe_insertion_behaviour == ABORT_ON_DUPLICATE_NAME
        and unlikely(name_to_value_map_.find(variable_name) != name_to_value_map_.end()))
        LOG_ERROR("attemting to insert a duplicate variable name: \"" + variable_name + "\" in section \"" + section_name_ + "\"!");

    name_to_value_map_[variable_name] = value;
}


bool IniFile::Section::lookup(const std::string &variable_name, std::string * const s) const {
    const auto name_and_value(name_to_value_map_.find(variable_name));
    if (name_and_value != name_to_value_map_.end()) {
        *s = name_and_value->second;
        return true;
    }

    s->clear();
    return false;
}


long IniFile::Section::getInteger(const std::string &variable_name) const {
    const auto name_and_value(name_to_value_map_.find(variable_name));
    if (name_and_value == name_to_value_map_.end())
        LOG_ERROR("can't find \"" + variable_name + "\" in section \"" + section_name_ + "\"!");

    long number;
    if (not StringUtil::ToNumber(name_and_value->second, &number))
        LOG_ERROR("invalid long entry \"" + variable_name + "\" in section \"" + section_name_ + "\"!");

    return number;
}


double IniFile::Section::getDouble(const std::string &variable_name) const {
    const auto name_and_value(name_to_value_map_.find(variable_name));
    if (name_and_value == name_to_value_map_.end())
        LOG_ERROR("can't find \"" + variable_name + "\" in section \"" + section_name_ + "\"!");

    double number;
    if (not StringUtil::ToDouble(name_and_value->second, &number))
        LOG_ERROR("invalid double entry \"" + variable_name + "\" in section \"" + section_name_ + "\"!");

    return number;
}


double IniFile::Section::getDouble(const std::string &variable_name, const double &default_value) const {
    const auto name_and_value(name_to_value_map_.find(variable_name));
    if (name_and_value == name_to_value_map_.end())
        return default_value;

    double number;
    if (not StringUtil::ToDouble(name_and_value->second, &number))
        LOG_ERROR("invalid double entry \"" + variable_name + "\" in section \"" + section_name_ + "\"!");

    return number;
}


std::string IniFile::Section::getString(const std::string &variable_name) const {
    const auto name_and_value(name_to_value_map_.find(variable_name));
    if (name_and_value == name_to_value_map_.end())
        LOG_ERROR("can't find \"" + variable_name + "\" in section \"" + section_name_ + "\"!");

    return name_and_value->second;
}


std::string IniFile::Section::getString(const std::string &variable_name, const std::string &default_value) const {
    const auto name_and_value(name_to_value_map_.find(variable_name));
    if (name_and_value == name_to_value_map_.end())
        return default_value;

    return name_and_value->second;
}


char IniFile::Section::getChar(const std::string &variable_name) const {
    const auto name_and_value(name_to_value_map_.find(variable_name));
    if (name_and_value == name_to_value_map_.end())
        LOG_ERROR("can't find \"" + variable_name + "\" in section \"" + section_name_ + "\"!");

    if (name_and_value->second.length() != 1)
        throw std::runtime_error("invalid character variable value \"" + variable_name + "\" in section \""
                                 + section_name_ + "\" (must be exactly one character in length)!");

    return name_and_value->second[0];
}


char IniFile::Section::getChar(const std::string &variable_name, const char default_value) const {
    const auto name_and_value(name_to_value_map_.find(variable_name));
    if (name_and_value == name_to_value_map_.end())
        return default_value;

    if (name_and_value->second.length() != 1)
        throw std::runtime_error("invalid character variable value \"" + variable_name + "\" in section \""
                                 + section_name_ + "\" (must be exactly one character in length)!");

    return name_and_value->second[0];
}


unsigned IniFile::Section::getUnsigned(const std::string &variable_name) const {
    const auto name_and_value(name_to_value_map_.find(variable_name));
    if (name_and_value == name_to_value_map_.end())
        LOG_ERROR("can't find \"" + variable_name + "\" in section \"" + section_name_ + "\"!");

    unsigned number;
    if (not StringUtil::ToUnsigned(name_and_value->second, &number))
        LOG_ERROR("invalid unsigned entry \"" + variable_name + "\" in section \"" + section_name_ + "\"!");

    return number;
}


unsigned IniFile::Section::getUnsigned(const std::string &variable_name, const unsigned &default_value) const {
    const auto name_and_value(name_to_value_map_.find(variable_name));
    if (name_and_value == name_to_value_map_.end())
        return default_value;

    unsigned number;
    if (not StringUtil::ToUnsigned(name_and_value->second, &number))
        LOG_ERROR("invalid unsigned entry \"" + variable_name + "\" in section \"" + section_name_ + "\"!");

    return number;
}


uint64_t IniFile::Section::getUint64T(const std::string &variable_name) const {
    const auto name_and_value(name_to_value_map_.find(variable_name));
    if (name_and_value == name_to_value_map_.end())
        LOG_ERROR("can't find \"" + variable_name + "\" in section \"" + section_name_ + "\"!");

    uint64_t number;
    if (not StringUtil::ToUInt64T(name_and_value->second, &number))
        LOG_ERROR("invalid uint64_t entry \"" + variable_name + "\" in section \"" + section_name_ + "\"!");

    return number;
}


uint64_t IniFile::Section::getUint64T(const std::string &variable_name, const uint64_t &default_value) const {
    const auto name_and_value(name_to_value_map_.find(variable_name));
    if (name_and_value == name_to_value_map_.end())
        return default_value;

    uint64_t number;
    if (not StringUtil::ToUInt64T(name_and_value->second, &number))
        LOG_ERROR("invalid uint64_t entry \"" + variable_name + "\" in section \"" + section_name_ + "\"!");

    return number;
}


bool IniFile::Section::getBool(const std::string &variable_name) const {
    const auto name_and_value(name_to_value_map_.find(variable_name));
    if (name_and_value == name_to_value_map_.end())
        LOG_ERROR("can't find \"" + variable_name + "\" in section \"" + section_name_ + "\"!");

    bool retval;
    if (not StringUtil::ToBool(name_and_value->second, &retval))
        LOG_ERROR("invalid boolean value in section \"" + section_name_ + "\", entry \"" + variable_name + "\" (bad value is \""
              + name_and_value->second + "\")!");

    return retval;
}


bool IniFile::Section::getBool(const std::string &variable_name, const bool default_value) const {
    const auto name_and_value(name_to_value_map_.find(variable_name));
    if (name_and_value == name_to_value_map_.end())
        return default_value;

    bool retval;
    if (not StringUtil::ToBool(name_and_value->second, &retval))
        LOG_ERROR("invalid boolean value in section \"" + section_name_ + "\", entry \"" + variable_name + "\" (bad value is \""
              + name_and_value->second + "\")!");

    return retval;
}


int IniFile::Section::getEnum(const std::string &variable_name, const std::map<std::string, int> &string_to_value_map) const {
    const auto name_and_value(name_to_value_map_.find(variable_name));
    if (name_and_value == name_to_value_map_.end())
        LOG_ERROR("can't find \"" + variable_name + "\" in section \"" + section_name_ + "\"!");

    const auto name_and_int_value(string_to_value_map.find(name_and_value->second));
    if (name_and_int_value == string_to_value_map.end())
        LOG_ERROR("in section \"" + section_name_ + "\": invalid enum value for entry \"" + variable_name + "\"!");

    return name_and_int_value->second;
}


int IniFile::Section::getEnum(const std::string &variable_name,
                              const std::map<std::string, int> &string_to_value_map, const int default_value) const
{
    const auto name_and_value(name_to_value_map_.find(variable_name));
    if (name_and_value == name_to_value_map_.end())
        return default_value;

    const auto name_and_int_value(string_to_value_map.find(name_and_value->second));
    if (name_and_int_value == string_to_value_map.end())
        LOG_ERROR("in section \"" + section_name_ + "\": invalid enum value for entry \"" + variable_name + "\"!");

    return name_and_int_value->second;
}


namespace {


std::string BackQuoteUnescape(const std::string &s) {
    std::string result;
    result.reserve(s.length());

    for (std::string::const_iterator ch(s.begin()); ch != s.end(); ++ch) {
        if (unlikely(*ch == '\\')) {
            ++ch;
            if (ch == s.end())
                break;

            switch (*ch) {
            case 'a':
                result += '\a';
                break;
            case 'b':
                result += '\b';
                break;
            case 'f':
                result += '\f';
                break;
            case 'n':
                result += '\n';
                break;
            case 'r':
                result += '\r';
                break;
            case 't':
                result += '\t';
                break;
            case 'v':
                result += '\v';
                break;
            case '\\':
                result += '\\';
                break;
            case '\'':
                result += '\'';
                break;
            case '"':
                result += '"';
                break;
            case '?':
                result += '?';
                break;
            default:
                result += *ch;
            }
        }
        else
            result += *ch;
    }

    return result;
}


std::string StripComment(std::string * const s) {
    size_t hash_mark_pos(0);
    while ((hash_mark_pos = s->find('#', hash_mark_pos)) != std::string::npos) {
        if (hash_mark_pos == 0)
            return (*s = "");
        else if ((*s)[hash_mark_pos - 1] == '\\')
            hash_mark_pos = s->find('#', hash_mark_pos + 1);
        else
            return (*s = s->substr(0, hash_mark_pos));
    }

    return *s;
}


} // unnamed namespace


IniFile::IniFile()
    : ini_file_name_(FileUtil::MakeAbsolutePath(DefaultIniFileName())), ignore_failed_includes_(false)
{
    processFile(ini_file_name_);
}


IniFile::IniFile(const std::string &ini_file_name, const bool ignore_failed_includes, const bool create_empty)
    : ini_file_name_(FileUtil::MakeAbsolutePath(ini_file_name.empty() ? DefaultIniFileName() : ini_file_name)),
      ignore_failed_includes_(ignore_failed_includes)
{
    // Allow creation of empty IniFile
    if (create_empty)
        return;

    processFile(ini_file_name_);
}


std::string IniFile::DefaultIniFileName() {
    return ETC_DIR + std::string("/") + progname + std::string(".conf");
}


void IniFile::processSectionHeader(const std::string &line) {
    if (line[line.length()-1] != ']')
        throw std::runtime_error("in IniFile::processSectionHeader: "
                                 "garbled section header on line " +
                                 std::to_string(getCurrentLineNo()) + " in file \""
                                 + getCurrentFile() + "\"!");

    current_section_name_ = line.substr(1, line.length() - 2);
    StringUtil::Trim(" \t", &current_section_name_);
    if (current_section_name_.empty())
        throw std::runtime_error("In IniFile::processSectionHeader: "
                                 "empty section name on line " +
                                 std::to_string(getCurrentLineNo()) + " in file \""
                                 + getCurrentFile() + "\"!");

    // if the current section
    const auto section(std::find(sections_.cbegin(), sections_.cend(), current_section_name_));
    if (section != sections_.end())
        throw std::runtime_error("in IniFile::processSectionHeader: duplicate section \""
                                 + current_section_name_  + "\" on line "
                                 + std::to_string(getCurrentLineNo()) + " in file \""
                                 + getCurrentFile() + "\"!");
    sections_.emplace_back(current_section_name_);
}


void IniFile::processInclude(const std::string &line) {
    const size_t equal_sign(line.find('='));
    if (unlikely(equal_sign != std::string::npos))
        throw std::runtime_error("in IniFile::processInclude: unexpected '=' on line "
                                 + std::to_string(getCurrentLineNo()) + " in file \""
                                 + getCurrentFile() + "\"!");

    std::string include_filename(line.substr(8));
    StringUtil::Trim(" \t", &include_filename);
    if (include_filename[0] == '"') {
        if (include_filename.length() < 3 or include_filename[include_filename.length() - 1] != '"')
            throw std::runtime_error("in IniFile::processInclude: garbled include file name on line "
                                     + std::to_string(getCurrentLineNo())
                                     + " in file \"" + getCurrentFile() + "\"!");
        include_filename = include_filename.substr(1, include_filename.length() - 2);
    }

    processFile(FileUtil::MakeAbsolutePath(getCurrentFile(), include_filename));
}


namespace {


// IsValidVariableName -- only allow names that start with a letter followed by letters, digits,
// hyphens and underscores.
//
bool IsValidVariableName(const std::string &possible_variable_name) {
    if (unlikely(possible_variable_name.empty()))
        return false;

    std::string::const_iterator ch(possible_variable_name.begin());

    // Variable names must start with a letter...
    if (not StringUtil::IsAsciiLetter(*ch))
        return false;

    // ...followed by letters, digits, hyphens, underscores, periods, slashes or colons:
    for (++ch; ch != possible_variable_name.end(); ++ch) {
        if (not StringUtil::IsAlphanumeric(*ch) and *ch != '-' and *ch != '_' and *ch != '.' and *ch != '/' and *ch != ':')
            return false;
    }

    return true;
}


} // unnamed namespace


void IniFile::processSectionEntry(const std::string &line) {
    const size_t equal_sign = line.find('=');
    if (equal_sign == std::string::npos) { // Not a normal "variable = value" type line.
        std::string trimmed_line(line);
        StringUtil::Trim(&trimmed_line);
        if (unlikely(not IsValidVariableName(trimmed_line)))
            throw std::runtime_error("in IniFile::processSectionEntry: invalid variable name \""
                                     + trimmed_line + "\" on line "
                                     + std::to_string(getCurrentLineNo()) + " in file \""
                                     + getCurrentFile() + "\"!");

        sections_.back().insert(trimmed_line, "true");
    } else {
        std::string variable_name(line.substr(0, equal_sign));
        StringUtil::Trim(" \t", &variable_name);
        if (variable_name.empty())
            throw std::runtime_error("in IniFile::processSectionEntry: missing variable name on line "
                                     + std::to_string(getCurrentLineNo())
                                     + " in file \"" + getCurrentFile() + "\"!");

        if (not IsValidVariableName(variable_name))
            throw std::runtime_error("in IniFile::processSectionEntry: invalid variable name \""
                                     + variable_name + "\" on line "
                                     + std::to_string(getCurrentLineNo()) + " in file \""
                                     + getCurrentFile() + "\"!");

        std::string value = line.substr(equal_sign + 1);
        StringUtil::Trim(" \t", &value);
        if (value.empty())
            throw std::runtime_error("in IniFile::processSectionEntry: missing variable value on line "
                                     + std::to_string(getCurrentLineNo()) + " in file \""
                                     + getCurrentFile() + "\"!");

        if (value[0] == '"') { // double-quoted string
            if (value.length() == 1 or value[value.length()-1] != '"')
                throw std::runtime_error("in IniFile::processSectionEntry: improperly quoted value on line "
                                         + std::to_string(getCurrentLineNo()) + " in file \""
                                         + getCurrentFile() + "!");

            value = value.substr(1, value.length()-2);
            value = BackQuoteUnescape(value);
        }

        sections_.back().insert(variable_name, value);
    }
}


void IniFile::processFile(const std::string &external_filename) {
    std::string dirname, basename;
    FileUtil::DirnameAndBasename(external_filename, &dirname, &basename);

    char hostname[HOST_NAME_MAX + 1];
    if (::gethostname(hostname, sizeof hostname) != 0)
        LOG_ERROR("gethostname(2) failed!");

    std::string filename;
    if (not dirname.empty() and FileUtil::Exists(dirname + "/" + std::string(hostname) + "/" + basename))
        filename = dirname + "/" + std::string(hostname) + "/" + basename;
    else
        filename = external_filename;

    // Open the file:
    if (unlikely(not FileUtil::Exists(filename))) {
        if (not ignore_failed_includes_ or filename == getFilename())
            throw std::runtime_error("in IniFile::processFile: file \"" + filename + "\" does not exist!");
    }
    std::ifstream ini_file(filename.c_str());
    if (ini_file.fail())
        throw std::runtime_error("in IniFile::processFile: can't open \"" + filename + "\"! ("
                                 + std::string(::strerror(errno)) + ")");

    include_file_infos_.push(IncludeFileInfo(filename));

    // Read the file:
    while (not ini_file.eof()) {
        std::string line;

        // read lines until newline character is not preceeded by a '\'
        bool continued_line;
        do {
            std::string buf;
            std::getline(ini_file, buf);
            ++getCurrentLineNo();
            line += buf;
            continued_line = line[line.length() - 1] == '\\';
            if (continued_line)
                line = line.substr(0, line.length() - 1);
        } while (continued_line);

        StripComment(&line);
        StringUtil::Trim(" \t", &line);

        // skip blank lines:
        if (line.length() == 0)
            continue;

        if (line[0] == '[') // should be a section header!
            processSectionHeader(line);
        else if (line.length() > 7 and line.substr(0, 7) == "include" and (line[7] == ' ' or line[7] == '\t'))
            processInclude(line);
        else // should be a new setting!
            processSectionEntry(line);
    }

    include_file_infos_.pop();
}


void IniFile::assign(const IniFile &rhs, const bool clear) {
    // Avoid self-assignment:
    if (unlikely(this == &rhs))
        return;

    ignore_failed_includes_ = rhs.ignore_failed_includes_;
    if (clear) {
        sections_      = rhs.sections_;
        ini_file_name_ = rhs.ini_file_name_;
    } else {
        for (auto &rhs_section : rhs.sections_) {
            const auto section_name_and_contents(std::find(sections_.begin(), sections_.end(), rhs_section.getSectionName()));
            if (section_name_and_contents == sections_.end())
                sections_.emplace_back(rhs_section);
            else { // The section already exists, therefore we have to be careful and override individual
                // entries selectively!

                // 1. Remember the section name so that we can later recreate it:
                const std::string &section_name(rhs_section.getSectionName());

                // 2. Create a map from names to values for the current entries of the section:
                std::unordered_map<std::string, std::string> section_entries;
                for (auto &name_and_value : *section_name_and_contents)
                    section_entries.insert(name_and_value);

                // 3. Remove the section from the map of sections to entries:
                sections_.erase(section_name_and_contents);

                // 4. Override and add in name/value rhs_sections from the rhs section:
                for (auto &rhs_name_and_value : rhs_section) {
                    // If the entry already exists, first erase it before inserting the new value:
                    const auto old_entry(section_entries.find(rhs_name_and_value.first));
                    if (old_entry != section_entries.end())
                        section_entries.erase(old_entry);
                    section_entries.insert(rhs_name_and_value);
                }

                // 5. Restore the section, now with the overridden and new entries:
                Section new_section(section_name);
                for (auto &entry : section_entries)
                    new_section.insert(entry.first, entry.second);
                sections_.emplace_back(new_section);
            }
        }

        ini_file_name_ = rhs.ini_file_name_;
    }
}


bool IniFile::lookup(const std::string &section_name, const std::string &variable_name, std::string * const s) const {
    const auto section(std::find(sections_.cbegin(), sections_.cend(), section_name));
    if (section == sections_.cend())
        return false;

    return section->lookup(variable_name, s);
}


unsigned IniFile::getUnsigned(const std::string &section_name, const std::string &variable_name) const {
    const auto section(std::find(sections_.cbegin(), sections_.cend(), section_name));
    if (section == sections_.cend())
        LOG_ERROR("no such section: \"" + section_name + "\"!");

    return section->getUnsigned(variable_name);
}


unsigned IniFile::getUnsigned(const std::string &section_name, const std::string &variable_name,
                              const unsigned &default_value) const
{
    return variableIsDefined(section_name, variable_name) ? getUnsigned(section_name, variable_name) : default_value;
}


uint64_t IniFile::getUint64T(const std::string &section_name, const std::string &variable_name) const {
    const auto section(std::find(sections_.cbegin(), sections_.cend(), section_name));
    if (section == sections_.cend())
        LOG_ERROR("no such section: \"" + section_name + "\"!");

    return section->getUint64T(variable_name);
}


uint64_t IniFile::getUint64T(const std::string &section_name, const std::string &variable_name, const uint64_t &default_value) const {
    return variableIsDefined(section_name, variable_name) ? getUnsigned(section_name, variable_name) : default_value;
}



long IniFile::getInteger(const std::string &section_name, const std::string &variable_name) const {
    const auto section(std::find(sections_.cbegin(), sections_.cend(), section_name));
    if (section == sections_.cend())
        LOG_ERROR("no such section: \"" + section_name + "\"!");

    return section->getInteger(variable_name);
}


double IniFile::getDouble(const std::string &section_name, const std::string &variable_name) const {
    const auto section(std::find(sections_.cbegin(), sections_.cend(), section_name));
    if (section == sections_.end())
        LOG_ERROR("no such section: \"" + section_name + "\"!");

    return section->getDouble(variable_name);
}


double IniFile::getDouble(const std::string &section_name, const std::string &variable_name, const double &default_value) const {
    const auto section(std::find(sections_.cbegin(), sections_.cend(), section_name));
    if (section == sections_.cend())
        return default_value;

    return section->getDouble(variable_name);
}


std::string IniFile::getString(const std::string &section_name, const std::string &variable_name) const {
    const auto section(std::find(sections_.cbegin(), sections_.cend(), section_name));
    if (section == sections_.cend())
        LOG_ERROR("no such section: \"" + section_name + "\"!");

    return section->getString(variable_name);
}


std::string IniFile::getString(const std::string &section_name, const std::string &variable_name, const std::string &default_value) const {
    const auto section(std::find(sections_.cbegin(), sections_.cend(), section_name));
    if (section == sections_.cend())
        return default_value;

    return section->getString(variable_name, default_value);
}


char IniFile::getChar(const std::string &section_name, const std::string &variable_name) const {
    const auto section(std::find(sections_.cbegin(), sections_.cend(), section_name));
    if (section == sections_.cend())
        LOG_ERROR("no such section: \"" + section_name + "\"!");

    return section->getChar(variable_name);
}


char IniFile::getChar(const std::string &section_name, const std::string &variable_name, const char default_value) const {
    const auto section(std::find(sections_.cbegin(), sections_.cend(), section_name));
    if (section == sections_.cend())
        return default_value;

    return section->getChar(variable_name);
}


bool IniFile::getBool(const std::string &section_name, const std::string &variable_name) const {
    const auto section(std::find(sections_.cbegin(), sections_.cend(), section_name));
    if (section == sections_.cend())
        LOG_ERROR("no such section: \"" + section_name + "\"!");

    return section->getBool(variable_name);
}


bool IniFile::getBool(const std::string &section_name, const std::string &variable_name, const bool default_value) const {
    const auto section(std::find(sections_.cbegin(), sections_.cend(), section_name));
    if (section == sections_.cend())
         return default_value;

    return section->getBool(variable_name);
}


int IniFile::getEnum(const std::string &section_name, const std::string &variable_name,
                     const std::map<std::string, int> &string_to_value_map) const
{
    const auto section(std::find(sections_.cbegin(), sections_.cend(), section_name));
    if (section == sections_.cend())
        LOG_ERROR("no such section: \"" + section_name + "\"!");

    return section->getEnum(variable_name, string_to_value_map);
}


int IniFile::getEnum(const std::string &section_name, const std::string &variable_name,
                     const std::map<std::string, int> &string_to_value_map, const int default_value) const
{
    const auto section(std::find(sections_.cbegin(), sections_.cend(), section_name));
    if (section == sections_.cend())
        return default_value;

    return section->getEnum(variable_name, string_to_value_map, default_value);
}


std::vector<std::string> IniFile::getSections() const {
    std::vector<std::string> result;
    for (const auto &section : sections_)
        result.emplace_back(section.getSectionName());

    return result;
}


std::vector<std::string> IniFile::getSectionEntryNames(const std::string &section_name) const {
    const auto section(std::find(sections_.cbegin(), sections_.cend(), section_name));
    if (section != sections_.cend()) {
        std::vector<std::string> entry_names;
        for (const auto &entry : *section)
            entry_names.emplace_back(entry.first);

        return entry_names;
    } else
        return std::vector<std::string>(); // Empty vector!
}


namespace {

    class StartsWith: public std::unary_function<std::string, bool> {
        std::string start_text_;
    public:
        StartsWith(const std::string &start_text = "") : start_text_(start_text) {}
        bool operator()(const std::string &test) { return test.find(start_text_) != 0; }
    };
}


std::vector<std::string> IniFile::getSectionEntryNamesThatStartWith(const std::string &section_name,
                                                                    const std::string &starting_with) const
{
    std::vector<std::string> work_list(getSectionEntryNames(section_name));
    work_list.erase(std::remove_if(work_list.begin(), work_list.end(), StartsWith(starting_with)), work_list.end());
    return work_list;
}


std::vector<std::string> IniFile::getSectionEntryValuesHavingNamesStartingWith(const std::string &section_name,
                                                                               const std::string &starting_with) const
{
    std::vector<std::string> entry_values;
    const auto section(std::find(sections_.cbegin(), sections_.cend(), section_name));
    if (section == sections_.cend())
        return entry_values;

    for (const auto &entry : *section) {
        if (entry.first.find(starting_with) == 0)
            entry_values.emplace_back(entry.second);
    }

    return entry_values;
}


const IniFile::Section &IniFile::getSection(const std::string &section_name) const {
    const auto section(std::find(sections_.cbegin(), sections_.cend(), section_name));
    if (section != sections_.cend())
        return *section;
    else {
        static Section empty_section("");
        return empty_section;
    }
}


bool IniFile::sectionIsDefined(const std::string &section_name) const {
    return std::find(sections_.cbegin(), sections_.cend(), section_name) != sections_.end();
}


bool IniFile::variableIsDefined(const std::string &section_name, const std::string &variable_name) const {
    std::string temp;
    return lookup(section_name, variable_name, &temp);
}

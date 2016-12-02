/** \file    IniFile.cc
 *  \brief   Implementation of class IniFile.
 *  \author  Dr. Johannes Ruscheinski
 *  \author  Artur Kedzierski
 *  \author  Dr. Gordon W. Paynter
 */

/*
 *  Copyright 2002-2008 Project iVia.
 *  Copyright 2002-2008 The Regents of The University of California.
 *  Copyright 2015 Universitätsbibliothek Tübingen
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
#if defined(__GNUC__) && __GNUC__ < 3
#undef isblank
extern "C" int isblank(const int ch);
#endif
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
    std::map<std::string, SectionContents>::const_iterator section =
        sections_.find(current_section_name_);
    if (section != sections_.end())
        throw std::runtime_error("in IniFile::processSectionHeader: duplicate section \""
                                 + current_section_name_  + "\" on line "
                                 + std::to_string(getCurrentLineNo()) + " in file \""
                                 + getCurrentFile() + "\"!");
    sections_[current_section_name_] = SectionContents();
}


void IniFile::processInclude(const std::string &line) {
    const size_t equal_sign = line.find('=');
    if (unlikely(equal_sign != std::string::npos))
        throw std::runtime_error("in IniFile::processInclude: unexpected '=' on line "
                                 + std::to_string(getCurrentLineNo()) + " in file \""
                                 + getCurrentFile() + "\"!");

    std::string include_filename(line.substr(8));
    StringUtil::Trim(" \t", &include_filename);
    if (include_filename[0] == '"') {
        if (include_filename.length() < 3 or include_filename[include_filename.length() - 1] != '"')
            throw std::runtime_error("in IniFile::processSectionEntry: garbled include file name on line "
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
            if (not StringUtil::IsAlphanumeric(*ch) and *ch != '-' and *ch != '_' and *ch != '.' and *ch != '/'
                and *ch != ':')
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

        SectionContents &section_contents = sections_[current_section_name_];
        section_contents.push_back(std::make_pair(trimmed_line, "true"));
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

        SectionContents &section_contents = sections_[current_section_name_];
        section_contents.push_back(std::make_pair(variable_name, value));
    }
}


void IniFile::processFile(const std::string &filename) {
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
        for (std::map<std::string, SectionContents>::const_iterator pair(rhs.sections_.begin());
             pair != rhs.sections_.end(); ++pair)
        {
            std::map<std::string, SectionContents>::iterator section_name_and_contents(sections_.find(pair->first));
            if (section_name_and_contents == sections_.end())
                sections_.insert(*pair);
            else { // The section already exists, therefore we have to be careful and override individual
                // entries selectively!

                // 1. Remember the section name so that we can later recreate it:
                const std::string section_name(pair->first);

                // 2. Create a map from names to values for the current entries of the section:
                std::map<std::string, std::string> section_entries;
                for (SectionContents::const_iterator name_and_value(
                                                                    section_name_and_contents->second.begin());
                     name_and_value != section_name_and_contents->second.end(); ++name_and_value)
                    section_entries.insert(*name_and_value);

                // 3. Remove the section from the map of sections to entries:
                sections_.erase(section_name_and_contents->first);

                // 4. Override and add in name/value pairs from the rhs section:
                for (SectionContents::const_iterator rhs_name_and_value(pair->second.begin());
                     rhs_name_and_value != pair->second.end(); ++rhs_name_and_value)
                {
                    // If the entry already exists, first erase it before inserting the new value:
                    std::map<std::string, std::string>::iterator old_entry(
                        section_entries.find(rhs_name_and_value->first));
                    if (old_entry != section_entries.end())
                        section_entries.erase(old_entry);
                    section_entries.insert(*rhs_name_and_value);
                }

                // 5. Restore the section, now with the overridden and new entries:
                SectionContents new_section_contents;
                for (std::map<std::string, std::string>::const_iterator entry(section_entries.begin());
                     entry != section_entries.end(); ++entry)
                    new_section_contents.push_back(*entry);
                sections_.insert(std::make_pair(section_name, new_section_contents));
            }
        }

        ini_file_name_ = rhs.ini_file_name_;
    }
}


bool IniFile::lookup(const std::string &section_name, const std::string &variable_name,
                     std::string * const s) const
{
    s->clear();

    std::map<std::string, SectionContents>::const_iterator section_contents
        = sections_.find(section_name);
    if (section_contents == sections_.end())
        return false;

    for (SectionContents::const_iterator entry(section_contents->second.begin());
         entry != section_contents->second.end(); ++entry)
    {
        if (entry->first == variable_name) {
            *s = entry->second;
            return true;
        }
    }

    return false;
}


unsigned IniFile::getUnsigned(const std::string &section_name, const std::string &variable_name) const
{
    std::string variable_value;
    if (not lookup(section_name, variable_name, &variable_value))
        throw std::runtime_error("can't find \"" + variable_name + "\" in section \"" + section_name
                                 + "\" in \"" + ini_file_name_ + "\"!");

    errno = 0;
    char *endp;
    const unsigned number(static_cast<unsigned>(std::strtoul(variable_value.c_str(), &endp, 0)));
    if (errno != 0 or *endp != '\0')
        throw std::runtime_error("invalid unsigned number in \"" + variable_name + "\" in section \""
                                 + section_name + "\" in \"" + ini_file_name_ + "\"!");
    return number;
}


unsigned IniFile::getUnsigned(const std::string &section_name, const std::string &variable_name,
                              const unsigned &default_value) const
{
    return variableIsDefined(section_name, variable_name) ? getUnsigned(section_name, variable_name) : default_value;
}


uint64_t IniFile::getUint64T(const std::string &section_name, const std::string &variable_name) const
{
    std::string variable_value;
    if (not lookup(section_name, variable_name, &variable_value))
        throw std::runtime_error("can't find \"" + variable_name + "\" in section \""
                                 + section_name + "\" in \"" + ini_file_name_ + "\"!");

    errno = 0;
    char *endp;
    const uint64_t number(std::strtoull(variable_value.c_str(), &endp, 0));
    if (errno != 0 or *endp != '\0')
        throw std::runtime_error("invalid uint64_t number in \"" + variable_name
                                 + "\" in section \"" + section_name + "\" in \"" + ini_file_name_ + "\"!");
    return number;
}


uint64_t IniFile::getUint64T(const std::string &section_name, const std::string &variable_name,
                             const uint64_t &default_value) const
{
    return variableIsDefined(section_name, variable_name) ? getUnsigned(section_name, variable_name) : default_value;
}



long IniFile::getInteger(const std::string &section_name, const std::string &variable_name) const {
    std::string variable_value;
    if (not lookup(section_name, variable_name, &variable_value))
        throw std::runtime_error("can't find \"" + variable_name + "\" in section \""
                                 + section_name + "\" in \"" + ini_file_name_ + "\"!");

    errno = 0;
    char *endp;
    long number = std::strtol(variable_value.c_str(), &endp, 10);
    if (errno != 0 or *endp != '\0')
        throw std::runtime_error("invalid integer in \"" + variable_name + "\" in section \""
                                 + section_name + "\" in \"" + ini_file_name_ + "\"!");

    return number;
}


double IniFile::getDouble(const std::string &section_name, const std::string &variable_name) const {
    std::string variable_value;
    if (not lookup(section_name, variable_name, &variable_value))
        throw std::runtime_error("can't find \"" + variable_name + "\" in section \"" + section_name
                                 + "\" in \"" + ini_file_name_ + "\"!");

    errno = 0;
    char *endp;
    double number = std::strtod(variable_value.c_str(), &endp);
    if (errno != 0 or *endp != '\0')
        throw std::runtime_error("invalid double-precision floating point number in \"" + variable_name
                                 + "\" in section \"" + section_name + "\" in \"" + ini_file_name_ + "\"!");

    return number;
}


double IniFile::getDouble(const std::string &section_name, const std::string &variable_name,
                          const double &default_value) const
{
    std::string variable_value;
    if (not lookup(section_name, variable_name, &variable_value))
        return default_value;

    errno = 0;
    char *endp;
    double number = std::strtod(variable_value.c_str(), &endp);
    if (errno != 0 or *endp != '\0')
        throw std::runtime_error("invalid double-precision floating point number in \"" + variable_name
                                 + "\" in section \"" + section_name + "\" in \"" + ini_file_name_ + "\"!");

    return number;
}


std::string IniFile::getString(const std::string &section_name, const std::string &variable_name) const {
    std::string variable_value;
    if (not lookup(section_name, variable_name, &variable_value))
        throw std::runtime_error("can't find \"" + variable_name + "\" in section \"" + section_name
                                 + "\" in \"" + ini_file_name_ + "\"!");

    return variable_value;
}


std::string IniFile::getString(const std::string &section_name, const std::string &variable_name,
                               const std::string &default_value) const
{
    std::string variable_value;
    return lookup(section_name, variable_name, &variable_value) ? variable_value : default_value;
}


char IniFile::getChar(const std::string &section_name, const std::string &variable_name) const {
    std::string variable_value;
    if (not lookup(section_name, variable_name, &variable_value))
        throw std::runtime_error("can't find \"" + variable_name + "\" in section \"" + section_name
                                 + "\" in \"" + ini_file_name_ + "\"!");

    if (variable_value.length() != 1)
        throw std::runtime_error("invalid character variable value \"" + variable_name + "\" in section \""
                                 + section_name + "\"" " in \"" + ini_file_name_
                                 + "\" (must be exactly one character in length)!");

    return variable_value[0];
}


char IniFile::getChar(const std::string &section_name, const std::string &variable_name,
                      const char default_value) const
{
    std::string variable_value;
    if (lookup(section_name, variable_name, &variable_value)) {
        if (variable_value.length() != 1)
            throw std::runtime_error("invalid character variable value \"" + variable_name + "\" in section \""
                                     + section_name + "\"" " in \"" + ini_file_name_
                                     + "\" (must be exactly one character in length)!");

        return variable_value[0];
    }
    else
        return default_value;
}


bool IniFile::getBool(const std::string &section_name, const std::string &variable_name) const
{
    std::string variable_value;
    if (not lookup(section_name, variable_name, &variable_value))
        throw std::runtime_error("can't find \"" + variable_name + "\" in section \"" + section_name
                                 + "\" in \"" + ini_file_name_ + "\"!");

    bool retval;
    if (not StringUtil::ToBool(variable_value, &retval))
        throw std::runtime_error("in IniFile::getBool: invalid value in section \"" + section_name
                                 + "\", entry \"" + variable_name + "\" (bad value is \"" + variable_value + "\")!");

    return retval;
}


bool IniFile::getBool(const std::string &section_name, const std::string &variable_name, const bool default_value) const
{
    std::string variable_value;
    if (not lookup(section_name, variable_name, &variable_value))
        return default_value;

    bool retval;
    if (not StringUtil::ToBool(variable_value, &retval))
        throw std::runtime_error("in IniFile::getBool: invalid value in section \"" + section_name
                                 + "\", entry \"" + variable_name + "\" (bad value is \"" + variable_value + "\")!");

    return retval;
}


namespace {


    class NameValuePairMatch { // Helper class for std::find() in IniFile::getEnum().
        std::string name_;
    public:
        explicit NameValuePairMatch(const std::string &name)
            : name_(name) { }
        bool operator()(const std::pair<std::string, int> &name_and_value) { return name_and_value.first == name_; }
    };


} // unnamed namespace


int IniFile::getEnum(const std::string &section_name, const std::string &variable_name,
                     const std::list< std::pair<std::string, int> > &string_to_value_map) const
{
    std::string variable_value;
    if (not lookup(section_name, variable_name, &variable_value))
        throw std::runtime_error("can't find \"" + variable_name + "\" in section \"" + section_name
                                 + "\" in \"" + ini_file_name_ + "\"!");

    const std::list< std::pair<std::string, int> >::const_iterator pair(
        std::find_if(string_to_value_map.begin(), string_to_value_map.end(),
                     NameValuePairMatch(variable_value)));
    if (unlikely(pair == string_to_value_map.end()))
        throw std::runtime_error("invalid value \"" + variable_value + "\" for \"" + variable_name
                                 + "\" in section \"" + section_name + "\""" in \"" + ini_file_name_ + "\"!");

    return pair->second;
}


int IniFile::getEnum(const std::string &section_name, const std::string &variable_name,
                     const std::list< std::pair<std::string, int> > &string_to_value_map,
                     const int default_value) const
{
    std::string variable_value;
    if (not lookup(section_name, variable_name, &variable_value))
        return default_value;

    const std::list< std::pair<std::string, int> >::const_iterator pair(
        std::find_if(string_to_value_map.begin(), string_to_value_map.end(),
                     NameValuePairMatch(variable_value)));
    if (pair == string_to_value_map.end())
        return default_value;

    return pair->second;
}


std::list<std::string> IniFile::getSections() const {
    std::list<std::string> result;
    for (std::map<std::string, SectionContents>::const_iterator section(sections_.begin());
         section != sections_.end(); ++section)
        result.push_back(section->first);

    return result;
}


std::list<std::string> IniFile::getSectionEntryNames(const std::string &section_name) const {
    std::map<std::string, SectionContents>::const_iterator section = sections_.find(section_name);
    if (section != sections_.end()) {
        std::list<std::string> entry_names;
        for (SectionContents::const_iterator entry(section->second.begin()); entry != section->second.end();
             ++entry)
            entry_names.push_back(entry->first);

        return entry_names;
    }
    else
        return std::list<std::string>(); // Empty list!
}


namespace {

    class StartsWith: public std::unary_function<std::string, bool> {
        std::string start_text_;
    public:
        StartsWith(const std::string &start_text = "") : start_text_(start_text) {}
        bool operator()(const std::string &test) { return test.find(start_text_) != 0; }
    };
}


std::list<std::string> IniFile::getSectionEntryNamesThatStartWith(const std::string &section_name,
                                                                  const std::string &starting_with) const
{
    std::list<std::string> work_list(getSectionEntryNames(section_name));
    work_list.erase(std::remove_if(work_list.begin(), work_list.end(), StartsWith(starting_with)), work_list.end());
    return work_list;
}


std::list<std::string> IniFile::getSectionEntryValuesHavingNamesStartingWith(const std::string &section_name,
                                                                             const std::string &starting_with) const
{
    std::list<std::string> entry_values;

    std::map<std::string, SectionContents>::const_iterator section = sections_.find(section_name);
    if (section == sections_.end())
        return entry_values;

    for (SectionContents::const_iterator entry(section->second.begin()); entry != section->second.end(); ++entry) {
        if (entry->first.find(starting_with) == 0)
            entry_values.push_back(entry->second);
    }

    return entry_values;
}


IniFile::SectionContents IniFile::getSection(const std::string &section_name) const {
    std::map<std::string, SectionContents>::const_iterator section = sections_.find(section_name);
    if (section != sections_.end())
        return section->second;
    else
        return SectionContents();
}


bool IniFile::sectionIsDefined(const std::string &section_name) const {
    std::map<std::string, SectionContents>::const_iterator section = sections_.find(section_name);
    return section != sections_.end();
}


bool IniFile::variableIsDefined(const std::string &section_name, const std::string &variable_name) const {
    std::string temp;
    return lookup(section_name, variable_name, &temp);
}

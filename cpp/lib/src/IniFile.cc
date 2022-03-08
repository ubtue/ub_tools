/** \file    IniFile.cc
 *  \brief   Implementation of class IniFile.
 *  \author  Dr. Johannes Ruscheinski
 *  \author  Artur Kedzierski
 *  \author  Dr. Gordon W. Paynter
 */

/*
 *  Copyright 2002-2008 Project iVia.
 *  Copyright 2002-2008 The Regents of The University of California.
 *  Copyright 2015-2021 Universitätsbibliothek Tübingen
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
#include <fstream>
#include <stdexcept>
#include <unordered_map>
#include <cctype>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <unistd.h>
#include "Compiler.h"
#include "FileUtil.h"
#include "StringUtil.h"
#include "TextUtil.h"
#include "UBTools.h"
#include "util.h"


void IniFile::Section::insert(const std::string &variable_name, const std::string &value, const std::string &comment,
                              const DupeInsertionBehaviour dupe_insertion_behaviour) {
    // Handle comment-only lines first:
    if (variable_name.empty() and value.empty()) {
        entries_.emplace_back("", "", comment);
        return;
    }

    const bool variable_is_defined(find(variable_name) != end());
    if (dupe_insertion_behaviour == ABORT_ON_DUPLICATE_NAME and unlikely(variable_is_defined))
        LOG_ERROR("attempting to insert a duplicate variable name: \"" + variable_name + "\" in section \"" + section_name_ + "\"!");

    replace(variable_name, value, comment);
}


void IniFile::Section::replace(const std::string &variable_name, const std::string &value, const std::string &comment) {
    const auto existing_entry(find(variable_name));
    if (existing_entry == entries_.end())
        entries_.emplace_back(variable_name, value, comment);
    else {
        existing_entry->name_ = variable_name;
        existing_entry->value_ = value;
        existing_entry->comment_ = comment;
    }
}


bool IniFile::Section::lookup(const std::string &variable_name, std::string * const s) const {
    const auto existing_entry(find(variable_name));
    if (existing_entry == entries_.end()) {
        s->clear();
        return false;
    }

    *s = existing_entry->value_;
    return true;
}


long IniFile::Section::getInteger(const std::string &variable_name) const {
    const auto existing_entry(find(variable_name));
    if (unlikely(existing_entry == end()))
        LOG_ERROR("can't find \"" + variable_name + "\" in section \"" + section_name_ + "\"!");

    long number;
    if (not StringUtil::ToNumber(existing_entry->value_, &number))
        LOG_ERROR("invalid long entry \"" + variable_name + "\" in section \"" + section_name_ + "\"!");

    return number;
}


double IniFile::Section::getDouble(const std::string &variable_name) const {
    const auto existing_entry(find(variable_name));
    if (unlikely(existing_entry == end()))
        LOG_ERROR("can't find \"" + variable_name + "\" in section \"" + section_name_ + "\"!");

    double number;
    if (not StringUtil::ToDouble(existing_entry->value_, &number))
        LOG_ERROR("invalid double entry \"" + variable_name + "\" in section \"" + section_name_ + "\"!");

    return number;
}


double IniFile::Section::getDouble(const std::string &variable_name, const double &default_value) const {
    const auto existing_entry(find(variable_name));
    if (unlikely(existing_entry == end()))
        return default_value;

    double number;
    if (not StringUtil::ToDouble(existing_entry->value_, &number))
        LOG_ERROR("invalid double entry \"" + variable_name + "\" in section \"" + section_name_ + "\"!");

    return number;
}


std::string IniFile::Section::getString(const std::string &variable_name) const {
    const auto existing_entry(find(variable_name));
    if (unlikely(existing_entry == end()))
        LOG_ERROR("can't find \"" + variable_name + "\" in section \"" + section_name_ + "\"!");

    return existing_entry->value_;
}


std::string IniFile::Section::getString(const std::string &variable_name, const std::string &default_value) const {
    const auto existing_entry(find(variable_name));
    if (unlikely(existing_entry == end()))
        return default_value;

    return existing_entry->value_;
}


char IniFile::Section::getChar(const std::string &variable_name) const {
    const auto existing_entry(find(variable_name));
    if (unlikely(existing_entry == end()))
        LOG_ERROR("can't find \"" + variable_name + "\" in section \"" + section_name_ + "\"!");

    if (existing_entry->value_.length() != 1)
        throw std::runtime_error("invalid character variable value \"" + variable_name + "\" in section \"" + section_name_
                                 + "\" (must be exactly one character in length)!");

    return existing_entry->value_[0];
}


char IniFile::Section::getChar(const std::string &variable_name, const char default_value) const {
    const auto existing_entry(find(variable_name));
    if (unlikely(existing_entry == end()))
        return default_value;

    if (existing_entry->value_.length() != 1)
        throw std::runtime_error("invalid character variable value \"" + variable_name + "\" in section \"" + section_name_
                                 + "\" (must be exactly one character in length)!");

    return existing_entry->value_[0];
}


unsigned IniFile::Section::getUnsigned(const std::string &variable_name) const {
    const auto existing_entry(find(variable_name));
    if (unlikely(existing_entry == end()))
        LOG_ERROR("can't find \"" + variable_name + "\" in section \"" + section_name_ + "\"!");

    unsigned number;
    if (not StringUtil::ToUnsigned(existing_entry->value_, &number))
        LOG_ERROR("invalid unsigned entry \"" + variable_name + "\" in section \"" + section_name_ + "\"!");

    return number;
}


unsigned IniFile::Section::getUnsigned(const std::string &variable_name, const unsigned &default_value) const {
    const auto existing_entry(find(variable_name));
    if (unlikely(existing_entry == end()))
        return default_value;

    unsigned number;
    if (not StringUtil::ToUnsigned(existing_entry->value_, &number))
        LOG_ERROR("invalid unsigned entry \"" + variable_name + "\" in section \"" + section_name_ + "\"!");

    return number;
}


uint64_t IniFile::Section::getUint64T(const std::string &variable_name) const {
    const auto existing_entry(find(variable_name));
    if (unlikely(existing_entry == end()))
        LOG_ERROR("can't find \"" + variable_name + "\" in section \"" + section_name_ + "\"!");

    uint64_t number;
    if (not StringUtil::ToUInt64T(existing_entry->value_, &number))
        LOG_ERROR("invalid uint64_t entry \"" + variable_name + "\" in section \"" + section_name_ + "\"!");

    return number;
}


uint64_t IniFile::Section::getUint64T(const std::string &variable_name, const uint64_t &default_value) const {
    const auto existing_entry(find(variable_name));
    if (unlikely(existing_entry == end()))
        return default_value;

    uint64_t number;
    if (not StringUtil::ToUInt64T(existing_entry->value_, &number))
        LOG_ERROR("invalid uint64_t entry \"" + variable_name + "\" in section \"" + section_name_ + "\"!");

    return number;
}


bool IniFile::Section::getBool(const std::string &variable_name) const {
    const auto existing_entry(find(variable_name));
    if (unlikely(existing_entry == end()))
        LOG_ERROR("can't find \"" + variable_name + "\" in section \"" + section_name_ + "\"!");

    bool retval;
    if (not StringUtil::ToBool(existing_entry->value_, &retval))
        LOG_ERROR("invalid boolean value in section \"" + section_name_ + "\", entry \"" + variable_name + "\" (bad value is \""
                  + existing_entry->value_ + "\")!");

    return retval;
}


bool IniFile::Section::getBool(const std::string &variable_name, const bool default_value) const {
    const auto existing_entry(find(variable_name));
    if (unlikely(existing_entry == end()))
        return default_value;

    bool retval;
    if (not StringUtil::ToBool(existing_entry->value_, &retval))
        LOG_ERROR("invalid boolean value in section \"" + section_name_ + "\", entry \"" + variable_name + "\" (bad value is \""
                  + existing_entry->value_ + "\")!");

    return retval;
}


int IniFile::Section::getEnum(const std::string &variable_name, const std::map<std::string, int> &string_to_value_map) const {
    const auto existing_entry(find(variable_name));
    if (unlikely(existing_entry == end()))
        LOG_ERROR("can't find \"" + variable_name + "\" in section \"" + section_name_ + "\"!");

    const auto name_and_int_value(string_to_value_map.find(existing_entry->value_));
    if (name_and_int_value == string_to_value_map.end())
        LOG_ERROR("in section \"" + section_name_ + "\": invalid enum value for entry \"" + variable_name + "\"!");

    return name_and_int_value->second;
}


int IniFile::Section::getEnum(const std::string &variable_name, const std::map<std::string, int> &string_to_value_map,
                              const int default_value) const {
    const auto existing_entry(find(variable_name));
    if (unlikely(existing_entry == end()))
        return default_value;

    const auto name_and_int_value(string_to_value_map.find(existing_entry->value_));
    if (name_and_int_value == string_to_value_map.end())
        LOG_ERROR("in section \"" + section_name_ + "\": invalid enum value for entry \"" + variable_name + "\"!");

    return name_and_int_value->second;
}

std::vector<std::string> IniFile::Section::getEntryNames() const {
    std::vector<std::string> entry_names;

    for (const auto &entry : entries_) {
        if (not entry.name_.empty())
            entry_names.emplace_back(entry.name_);
    }

    return entry_names;
}


bool IniFile::Section::deleteEntry(const std::string &entry_name) {
    auto entry(find(entry_name));
    if (unlikely(entry == end()))
        return false;

    entries_.erase(entry);
    return true;
}


const std::string &IniFile::Section::getComment(const std::string &entry_name) const {
    auto entry(find(entry_name));
    if (unlikely(entry == end()))
        LOG_ERROR("can't find \"" + entry_name + "\" in section \"" + section_name_ + "\"!");

    return entry->comment_;
}


static inline bool ContainsSpacesOrDoubleQuotes(const std::string &value) {
    if (value.find(' ') != std::string::npos)
        return true;
    return value.find('"') != std::string::npos;
}


static inline bool StartsWithEqualSign(const std::string &value) {
    return (not value.empty()) and (value[0] == '=');
}


void IniFile::Section::write(File * const output, const bool pretty_print, const bool compact) const {
    if (unlikely(not section_name_.empty() and not output->writeln("[" + section_name_ + "]")))
        LOG_ERROR("failed to write section header to \"" + output->getPath() + "\"!");

    // If we want pretty output we align all the equal signs in a section:
    size_t max_name_length(0);
    if (pretty_print) {
        for (const auto &entry : entries_) {
            if (entry.name_.length() > max_name_length)
                max_name_length = entry.name_.length();
        }
    }

    bool last_line_empty(false);
    for (size_t i(0); i < entries_.size(); ++i) {
        const auto &entry(entries_.at(i));
        bool last_line(i == entries_.size() - 1);

        // when compacting, strip all empty lines except for the final one
        if (entry.empty()) {
            if (last_line)
                last_line_empty = true;
            else if (compact)
                continue;
        }

        if (entry.name_.empty()) {
            if (unlikely(not output->writeln(entry.comment_)))
                LOG_ERROR("failed to write a comment to \"" + output->getPath() + "\"!");
        } else {
            std::string line(entry.name_);
            if (max_name_length > 0)
                line += std::string(max_name_length - entry.name_.length(), ' ');
            line += " = ";
            const bool need_quotes(entry.value_.empty() or ContainsSpacesOrDoubleQuotes(entry.value_) or StartsWithEqualSign(entry.value_));
            if (need_quotes)
                line += '"';
            if (need_quotes)
                line += TextUtil::CStyleEscape(entry.value_);
            else
                line += entry.value_;
            if (need_quotes)
                line += '"';
            line += entry.comment_;
            if (unlikely(not output->writeln(line)))
                LOG_ERROR("failed to write a name/value pair to \"" + output->getPath() + "\"!");
        }
    }

    if (not last_line_empty and unlikely(not output->writeln("")))
        LOG_ERROR("failed to write section final empty line to \"" + output->getPath() + "\"!");
}


IniFile::IniFile(): ini_file_name_(FileUtil::MakeAbsolutePath(DefaultIniFileName())), ignore_failed_includes_(false) {
    processFile(ini_file_name_);
}


IniFile::IniFile(const std::string &ini_file_name, const bool ignore_failed_includes, const bool create_empty)
    : ini_file_name_(FileUtil::MakeAbsolutePath(ini_file_name.empty() ? DefaultIniFileName() : ini_file_name)),
      ignore_failed_includes_(ignore_failed_includes) {
    // Allow creation of empty IniFile
    if (create_empty)
        return;

    processFile(ini_file_name_);
}


std::string IniFile::DefaultIniFileName() {
    return UBTools::GetTuelibPath() + FileUtil::GetBasename(::program_invocation_name) + std::string(".conf");
}


void IniFile::processSectionHeader(const std::string &line) {
    if (line[line.length() - 1] != ']')
        throw std::runtime_error(
            "in IniFile::processSectionHeader: "
            "garbled section header on line "
            + std::to_string(getCurrentLineNo()) + " in file \"" + getCurrentFile() + "\"!");

    current_section_name_ = line.substr(1, line.length() - 2);
    StringUtil::Trim(" \t", &current_section_name_);
    if (current_section_name_.empty())
        throw std::runtime_error(
            "In IniFile::processSectionHeader: "
            "empty section name on line "
            + std::to_string(getCurrentLineNo()) + " in file \"" + getCurrentFile() + "\"!");

    // if the current section
    const auto section(std::find(sections_.cbegin(), sections_.cend(), current_section_name_));
    if (section != sections_.end())
        throw std::runtime_error("in IniFile::processSectionHeader: duplicate section \"" + current_section_name_ + "\" on line "
                                 + std::to_string(getCurrentLineNo()) + " in file \"" + getCurrentFile() + "\"!");
    sections_.emplace_back(current_section_name_);
}


void IniFile::processInclude(const std::string &line) {
    const size_t equal_sign(line.find('='));
    if (unlikely(equal_sign != std::string::npos))
        throw std::runtime_error("in IniFile::processInclude: unexpected '=' on line " + std::to_string(getCurrentLineNo()) + " in file \""
                                 + getCurrentFile() + "\"!");

    std::string include_filename(line.substr(8));
    StringUtil::Trim(" \t", &include_filename);
    if (include_filename[0] == '"') {
        if (include_filename.length() < 3 or include_filename[include_filename.length() - 1] != '"')
            throw std::runtime_error("in IniFile::processInclude: garbled include file name on line " + std::to_string(getCurrentLineNo())
                                     + " in file \"" + getCurrentFile() + "\"!");
        include_filename = include_filename.substr(1, include_filename.length() - 2);
    }

    processFile(FileUtil::MakeAbsolutePath(getCurrentFile(), include_filename));
}


void IniFile::processInherit(const std::string &line, Section * const current_section) {
    if (not StringUtil::StartsWith(line, "@inherit "))
        throw std::runtime_error("in IniFile::processInherit: malformed @inherit statement on line " + std::to_string(getCurrentLineNo())
                                 + " in file \"" + getCurrentFile() + "\"!");

    auto quoted_section_name(StringUtil::TrimWhite(line.substr(__builtin_strlen("@inherit "))));
    if (quoted_section_name.length() < 3 or quoted_section_name.front() != '"' or quoted_section_name.back() != '"') {
        throw std::runtime_error("in IniFile::processInherit: malformed @inherit statement on line " + std::to_string(getCurrentLineNo())
                                 + " in file \"" + getCurrentFile() + "\"! (2)");
    }

    try {
        const auto section_name(StringUtil::CStyleUnescape(quoted_section_name.substr(1, quoted_section_name.length() - 2)));
        const auto section(std::find_if(sections_.begin(), sections_.end(),
                                        [&section_name](const Section &section) { return section.section_name_ == section_name; }));
        if (unlikely(section == sections_.end())) {
            throw std::runtime_error("in IniFile::processInherit: unknown section name \"" + section_name
                                     + "\" in @inherit statement on line " + std::to_string(getCurrentLineNo()) + " in file \""
                                     + getCurrentFile() + "\"!");
        }
        for (const auto &entry : *section)
            current_section->insert(entry.name_, entry.value_, entry.comment_);
    } catch (...) {
        throw std::runtime_error("in IniFile::processInherit: malformed @inherit statement on line " + std::to_string(getCurrentLineNo())
                                 + " in file \"" + getCurrentFile() + "\"! (3)");
    }
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


void IniFile::processSectionEntry(const std::string &line, const std::string &comment) {
    const size_t equal_sign = line.find('=');
    if (equal_sign == std::string::npos) { // Not a normal "variable = value" type line.
        std::string trimmed_line(line);
        StringUtil::Trim(&trimmed_line);
        if (unlikely(not IsValidVariableName(trimmed_line)))
            throw std::runtime_error("in IniFile::processSectionEntry: invalid variable name \"" + trimmed_line + "\" on line "
                                     + std::to_string(getCurrentLineNo()) + " in file \"" + getCurrentFile() + "\"!");

        sections_.back().insert(trimmed_line, "true");
    } else {
        std::string variable_name(line.substr(0, equal_sign));
        StringUtil::Trim(" \t", &variable_name);
        if (variable_name.empty())
            throw std::runtime_error("in IniFile::processSectionEntry: missing variable name on line " + std::to_string(getCurrentLineNo())
                                     + " in file \"" + getCurrentFile() + "\"!");

        if (not IsValidVariableName(variable_name))
            throw std::runtime_error("in IniFile::processSectionEntry: invalid variable name \"" + variable_name + "\" on line "
                                     + std::to_string(getCurrentLineNo()) + " in file \"" + getCurrentFile() + "\"!");

        std::string value = line.substr(equal_sign + 1);
        StringUtil::Trim(" \t", &value);
        if (value.empty())
            throw std::runtime_error("in IniFile::processSectionEntry: missing variable value on line " + std::to_string(getCurrentLineNo())
                                     + " in file \"" + getCurrentFile() + "\"!");

        if (value[0] == '"') { // double-quoted string
            if (value.length() == 1 or value[value.length() - 1] != '"')
                throw std::runtime_error("in IniFile::processSectionEntry: improperly quoted value on line "
                                         + std::to_string(getCurrentLineNo()) + " in file \"" + getCurrentFile() + "!");

            value = value.substr(1, value.length() - 2);
            try {
                TextUtil::CStyleUnescape(&value);
            } catch (const std::runtime_error &x) {
                throw std::runtime_error("in IniFile::processSectionEntry: bad escape on line " + std::to_string(getCurrentLineNo())
                                         + " in file \"" + getCurrentFile() + "! (" + std::string(x.what()) + ")");
            }
        }

        sections_.back().insert(variable_name, value, comment);
    }
}


static std::string StripComment(std::string * const line, std::string * const comment) {
    comment->clear();

    size_t comment_start_pos(0);
    bool inside_string_literal(false);
    for (auto character(line->begin()); character != line->end(); ++character) {
        if (*character == '\"')
            inside_string_literal = inside_string_literal == false;
        else if (*character == '#') {
            if (character != line->begin() and *(character - 1) == '\\')
                continue; // skip escaped hash characters
            else if (inside_string_literal)
                continue;
            else {
                comment_start_pos = std::distance(line->begin(), character);
                while (comment_start_pos > 0 and (*line)[comment_start_pos - 1] == ' ')
                    --comment_start_pos;
                *comment = line->substr(comment_start_pos);
                line->resize(comment_start_pos);
                return *line;
            }
        }
    }

    return *line;
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
        throw std::runtime_error("in IniFile::processFile: can't open \"" + filename + "\"! (" + std::string(::strerror(errno)) + ")");

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
            line += StringUtil::Trim(buf, " \t");
            if (line.empty())
                continue;

            continued_line = line[line.length() - 1] == '\\';
            if (continued_line)
                line = StringUtil::Trim(line.substr(0, line.length() - 1), " \t");
        } while (continued_line);

        std::string comment;
        StripComment(&line, &comment);
        StringUtil::Trim(" \t", &line);
        if (line.empty()) {
            if (sections_.empty())
                sections_.emplace_back(Section(""));
            sections_.back().insert("", "", comment);
            continue;
        }

        // skip blank lines:
        if (line.length() == 0)
            continue;

        if (line[0] == '[') // should be a section header!
            processSectionHeader(line);
        else if (line.length() > 7 and line.substr(0, 7) == "include" and (line[7] == ' ' or line[7] == '\t'))
            processInclude(line);
        else if (StringUtil::StartsWith(line, "@inherit")) {
            if (unlikely(sections_.empty()))
                throw std::runtime_error("in IniFile::processFile: file \"" + filename + " @inherit in global section!");
            processInherit(line, &(sections_.back()));
        } else { // should be a new setting!
            if (sections_.empty())
                sections_.emplace_back(Section(""));
            processSectionEntry(line, comment);
        }
    }

    include_file_infos_.pop();
}


void IniFile::assign(const IniFile &rhs, const bool clear) {
    // Avoid self-assignment:
    if (unlikely(this == &rhs))
        return;

    ignore_failed_includes_ = rhs.ignore_failed_includes_;
    if (clear) {
        sections_ = rhs.sections_;
        ini_file_name_ = rhs.ini_file_name_;
    } else {
        for (auto &rhs_section : rhs.sections_) {
            const auto lhs_section(std::find(sections_.begin(), sections_.end(), rhs_section.getSectionName()));
            if (lhs_section == sections_.end())
                sections_.emplace_back(rhs_section);
            else { // The section already exists, therefore we have to be careful and override individual entries selectively!

                // Create a map from names to values for the current entries of the section...
                std::unordered_map<std::string, Entry *> lhs_names_and_entries;
                for (auto entry(lhs_section->begin()); entry != lhs_section->end(); ++entry)
                    lhs_names_and_entries.emplace(entry->name_, &*entry);

                // ... and override and add in name/value rhs_sections from the rhs section:
                for (auto &rhs_entry : rhs_section) {
                    const auto lhs_name_and_entry(lhs_names_and_entries.find(rhs_entry.name_));
                    if (lhs_name_and_entry == lhs_names_and_entries.end())
                        lhs_section->entries_.emplace_back(rhs_entry);
                    else // Overwrite the existing entry.
                        *lhs_name_and_entry->second = rhs_entry;
                }
            }
        }

        ini_file_name_ = rhs.ini_file_name_;
    }
}


bool IniFile::lookup(const std::string &section_name, const std::string &variable_name, std::string * const s) const {
    const auto section(std::find(sections_.cbegin(), sections_.cend(), section_name));
    if (section == sections_.cend())
        return false;

    const auto entry(section->find(variable_name));
    if (entry == section->end())
        return false;

    *s = entry->value_;
    return true;
}


unsigned IniFile::getUnsigned(const std::string &section_name, const std::string &variable_name) const {
    const auto section(std::find(sections_.cbegin(), sections_.cend(), section_name));
    if (section == sections_.cend())
        LOG_ERROR("no such section: \"" + section_name + "\"! (variable: \"" + variable_name + "\")");

    return section->getUnsigned(variable_name);
}


unsigned IniFile::getUnsigned(const std::string &section_name, const std::string &variable_name, const unsigned &default_value) const {
    return variableIsDefined(section_name, variable_name) ? getUnsigned(section_name, variable_name) : default_value;
}


uint64_t IniFile::getUint64T(const std::string &section_name, const std::string &variable_name) const {
    const auto section(std::find(sections_.cbegin(), sections_.cend(), section_name));
    if (section == sections_.cend())
        LOG_ERROR("no such section: \"" + section_name + "\"! (variable: \"" + variable_name + "\")");

    return section->getUint64T(variable_name);
}


uint64_t IniFile::getUint64T(const std::string &section_name, const std::string &variable_name, const uint64_t &default_value) const {
    return variableIsDefined(section_name, variable_name) ? getUnsigned(section_name, variable_name) : default_value;
}


long IniFile::getInteger(const std::string &section_name, const std::string &variable_name) const {
    const auto section(std::find(sections_.cbegin(), sections_.cend(), section_name));
    if (section == sections_.cend())
        LOG_ERROR("no such section: \"" + section_name + "\"! (variable: \"" + variable_name + "\")");

    return section->getInteger(variable_name);
}


long IniFile::getInteger(const std::string &section_name, const std::string &variable_name, const long default_value) const {
    const auto section(std::find(sections_.cbegin(), sections_.cend(), section_name));
    if (section == sections_.cend() or not section->hasEntry(variable_name))
        return default_value;

    return section->getInteger(variable_name);
}


double IniFile::getDouble(const std::string &section_name, const std::string &variable_name) const {
    const auto section(std::find(sections_.cbegin(), sections_.cend(), section_name));
    if (section == sections_.end())
        LOG_ERROR("no such section: \"" + section_name + "\"! (variable: \"" + variable_name + "\")");

    return section->getDouble(variable_name);
}


double IniFile::getDouble(const std::string &section_name, const std::string &variable_name, const double &default_value) const {
    const auto section(std::find(sections_.cbegin(), sections_.cend(), section_name));
    if (section == sections_.cend() or not section->hasEntry(variable_name))
        return default_value;

    return section->getDouble(variable_name);
}


std::string IniFile::getString(const std::string &section_name, const std::string &variable_name) const {
    const auto section(std::find(sections_.cbegin(), sections_.cend(), section_name));
    if (section == sections_.cend())
        LOG_ERROR("no such section: \"" + section_name + "\"! (variable: \"" + variable_name + "\")");

    return section->getString(variable_name);
}


std::string IniFile::getString(const std::string &section_name, const std::string &variable_name, const std::string &default_value) const {
    const auto section(std::find(sections_.cbegin(), sections_.cend(), section_name));
    if (section == sections_.cend() or not section->hasEntry(variable_name))
        return default_value;

    return section->getString(variable_name, default_value);
}


char IniFile::getChar(const std::string &section_name, const std::string &variable_name) const {
    const auto section(std::find(sections_.cbegin(), sections_.cend(), section_name));
    if (section == sections_.cend())
        LOG_ERROR("no such section: \"" + section_name + "\"! (variable: \"" + variable_name + "\")");

    return section->getChar(variable_name);
}


char IniFile::getChar(const std::string &section_name, const std::string &variable_name, const char default_value) const {
    const auto section(std::find(sections_.cbegin(), sections_.cend(), section_name));
    if (section == sections_.cend() or not section->hasEntry(variable_name))
        return default_value;

    return section->getChar(variable_name);
}


bool IniFile::getBool(const std::string &section_name, const std::string &variable_name) const {
    const auto section(std::find(sections_.cbegin(), sections_.cend(), section_name));
    if (section == sections_.cend())
        LOG_ERROR("no such section: \"" + section_name + "\"! (variable: \"" + variable_name + "\")");

    return section->getBool(variable_name);
}


bool IniFile::getBool(const std::string &section_name, const std::string &variable_name, const bool default_value) const {
    const auto section(std::find(sections_.cbegin(), sections_.cend(), section_name));
    if (section == sections_.cend() or not section->hasEntry(variable_name))
        return default_value;

    return section->getBool(variable_name);
}


int IniFile::getEnum(const std::string &section_name, const std::string &variable_name,
                     const std::map<std::string, int> &string_to_value_map) const {
    const auto section(std::find(sections_.cbegin(), sections_.cend(), section_name));
    if (section == sections_.cend())
        LOG_ERROR("no such section: \"" + section_name + "\"! (variable: \"" + variable_name + "\")");

    return section->getEnum(variable_name, string_to_value_map);
}


int IniFile::getEnum(const std::string &section_name, const std::string &variable_name,
                     const std::map<std::string, int> &string_to_value_map, const int default_value) const {
    const auto section(std::find(sections_.cbegin(), sections_.cend(), section_name));
    if (section == sections_.cend() or not section->hasEntry(variable_name))
        return default_value;

    return section->getEnum(variable_name, string_to_value_map, default_value);
}


std::vector<std::string> IniFile::getSections() const {
    std::vector<std::string> result;
    for (const auto &section : sections_)
        result.emplace_back(section.getSectionName());

    return result;
}


bool IniFile::deleteSection(const std::string &section_name) {
    auto section(std::find(sections_.begin(), sections_.end(), section_name));
    if (section == sections_.end())
        return false;

    sections_.erase(section);
    return true;
}


bool IniFile::deleteEntry(const std::string &section_name, const std::string &entry_name) {
    auto section(std::find(sections_.begin(), sections_.end(), section_name));
    if (section == sections_.end())
        return false;

    return section->deleteEntry(entry_name);
}


std::vector<std::string> IniFile::getSectionEntryNames(const std::string &section_name) const {
    const auto section(std::find(sections_.cbegin(), sections_.cend(), section_name));
    if (section != sections_.cend()) {
        std::vector<std::string> entry_names;
        for (const auto &entry : *section)
            entry_names.emplace_back(entry.name_);

        return entry_names;
    } else
        return std::vector<std::string>(); // Empty vector!
}


namespace {

class StartsWith : public std::unary_function<std::string, bool> {
    std::string start_text_;

public:
    StartsWith(const std::string &start_text = ""): start_text_(start_text) { }
    bool operator()(const std::string &test) { return test.find(start_text_) != 0; }
};
} // namespace


std::vector<std::string> IniFile::getSectionEntryNamesThatStartWith(const std::string &section_name,
                                                                    const std::string &starting_with) const {
    std::vector<std::string> work_list(getSectionEntryNames(section_name));
    work_list.erase(std::remove_if(work_list.begin(), work_list.end(), StartsWith(starting_with)), work_list.end());
    return work_list;
}


std::vector<std::string> IniFile::getSectionEntryValuesHavingNamesStartingWith(const std::string &section_name,
                                                                               const std::string &starting_with) const {
    std::vector<std::string> entry_values;
    const auto section(std::find(sections_.cbegin(), sections_.cend(), section_name));
    if (section == sections_.cend())
        return entry_values;

    for (const auto &entry : *section) {
        if (entry.name_.find(starting_with) == 0)
            entry_values.emplace_back(entry.value_);
    }

    return entry_values;
}


bool IniFile::sectionIsDefined(const std::string &section_name) const {
    return std::find(sections_.cbegin(), sections_.cend(), section_name) != sections_.cend();
}


bool IniFile::appendSection(const std::string &section_name) {
    const auto section(std::find(sections_.cbegin(), sections_.cend(), section_name));
    if (section != sections_.cend())
        return false;

    sections_.emplace_back(section_name);
    return true;
}


bool IniFile::variableIsDefined(const std::string &section_name, const std::string &variable_name) const {
    std::string temp;
    return lookup(section_name, variable_name, &temp);
}


void IniFile::write(const std::string &path, const bool pretty_print, const bool compact) const {
    std::unique_ptr<File> output(FileUtil::OpenOutputFileOrDie(path));

    for (const auto &section : sections_)
        section.write(output.get(), pretty_print, compact);
}

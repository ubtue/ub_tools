/** \file    IniFile.h
 *  \brief   Declarations for an initialisation file parsing class.
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
#pragma once


#include <algorithm>
#include <map>
#include <stack>
#include <string>
#include <unordered_map>
#include <vector>
#include <cassert>


// Forward declaration:
class File;


/** \class  IniFile
 *  \brief  Read a configuration file in our .ini format.
 *
 *  This class allows access to the contents of an ini file.  It is initialised with the name of the file, and the
 *  settings stored in the file can then be accessed through the Lookup and get* methods.  String constants can use
 *  C-style character backslash escapes like \\n.  If you want to embed a hash mark in a string you must preceede it with
 *  a single backslash.  In order to extend a string constant over multiple lines, put backslashes just before the line
 *  ends on all but the last line.
 *  Entries in one section can be inherited by later sections by using a '@inhert "section_name"' directive.  The name of
 *  the section whose values will be inherited must be a double-quoted string.
 */
class IniFile {
public:
    struct Entry {
        std::string name_, value_, comment_;

    public:
        Entry(const std::string &name, const std::string &value, const std::string &comment)
            : name_(name), value_(value), comment_(comment) { }
        inline bool empty() const { return name_.empty() and value_.empty() and comment_.empty(); }
    };

public:
    class Section {
        friend class IniFile;
        std::string section_name_;
        std::vector<Entry> entries_;

    public:
        enum DupeInsertionBehaviour { OVERWRITE_EXISTING_VALUE, ABORT_ON_DUPLICATE_NAME };
        typedef std::vector<Entry>::const_iterator const_iterator;
        typedef std::vector<Entry>::iterator iterator;

    public:
        explicit Section(const std::string &section_name): section_name_(section_name) { }
        Section() = default;
        Section(const Section &other) = default;

        inline bool operator==(const std::string &section_name) const { return section_name == section_name_; }

        inline const std::string &getSectionName() const { return section_name_; }
        inline void setSectionName(const std::string &section_name) { section_name_ = section_name; }

        inline const_iterator begin() const { return entries_.cbegin(); }
        inline const_iterator end() const { return entries_.cend(); }

        inline iterator begin() { return entries_.begin(); }
        inline iterator end() { return entries_.end(); }

        void insert(const std::string &variable_name, const std::string &value, const std::string &comment = "",
                    const DupeInsertionBehaviour dupe_insertion_behaviour = ABORT_ON_DUPLICATE_NAME);

        void replace(const std::string &variable_name, const std::string &value, const std::string &comment = "");

        bool lookup(const std::string &variable_name, std::string * const s) const;

        /** \brief   Retrieves an integer value from a configuration file.
         *  \param   variable_name  The name of the section entry to read.
         *  \return  The value of the string in the specified section.
         *  \throws  A std::runtime_error if the variable is not found or the value cannot be converted to an integer.
         */
        long getInteger(const std::string &variable_name) const;

        /** \brief   Retrieves a floating point value from a configuration file.
         *  \param   variable_name  The name of the section entry to read.
         *  \return  The value of the string in the specified section.
         *  \throws  A std::runtime_error if the variable is not found or the value cannot be converted to a double.
         */
        double getDouble(const std::string &variable_name) const;

        /** \brief   Retrieves a floating point value from a configuration file.
         *  \param   variable_name  The name of the section entry to read.
         *  \param   default_value  A default to return if the variable is not defined.
         *  \return  The value of the string in the specified section.
         *  \throws  A std::runtime_error if the value was found but cannot be converted to a double.
         */
        double getDouble(const std::string &variable_name, const double &default_value) const;

        /** \brief   Retrieves a string value from a configuration file.
         *  \param   variable_name  The name of the section entry to read.
         *  \return  The value of the string in the specified section.
         *  \throws  A std::runtime_error if the variable is not found.
         *  \note    If the variable is not defined in the section, the program throws an exception.
         */
        std::string getString(const std::string &variable_name) const;

        /** \brief   Retrieves a string value from a configuration file.
         *  \param   variable_name  The name of the section entry to read.
         *  \param   default_value  A default to return if the variable is not defined.
         *  \return  The value of the specified variable in the specified section, or "default_value" if it is not
         *           defined.
         */
        std::string getString(const std::string &variable_name, const std::string &default_value) const;

        /** \brief   Retrieves a single character value from a configuration file.
         *  \param   variable_name  The name of the section entry to read.
         *  \return  The value of the character in the specified section.
         *  \note    If the variable is not defined in the section, the program throws an exception.
         */
        char getChar(const std::string &variable_name) const;

        /** \brief   Retrieves a single character value from a configuration file.
         *  \param   variable_name  The name of the section entry to read.
         *  \param   default_value  A default to return if the variable is not defined.
         *  \return  The value of the specified variable in the specified section, or "default_value" if it is not
         *           defined.
         */
        char getChar(const std::string &variable_name, const char default_value) const;

        /** \brief   Retrieves an unsigned value from a configuration file.
         *  \param   variable_name  The name of the section entry to read.
         *  \return  The value of the specified variable in the specified section.
         *  \note    If the variable is not defined in the section, an exception is thrown.
         */
        unsigned getUnsigned(const std::string &variable_name) const;

        /** \brief   Retrieves an unsigned value from a configuration file.
         *  \param   variable_name  The name of the section entry to read.
         *  \param   default_value  A default to return if the variable is not defined.
         *  \return  The value of the specified variable in the specified section, or "default_value" if it is not
         *           defined.
         */
        unsigned getUnsigned(const std::string &variable_name, const unsigned &default_value) const;

        /** \brief   Retrieves a uint64_t value from a configuration file.
         *  \param   variable_name  The name of the section entry to read.
         *  \return  The value of the specified variable in the specified section.
         *  \note    If the variable is not defined in the section, an exception is thrown.
         */
        uint64_t getUint64T(const std::string &variable_name) const;

        /** \brief   Retrieves a uint64_t value from a configuration file.
         *  \param   variable_name  The name of the section entry to read.
         *  \param   default_value  A default to return if the variable is not defined.
         *  \return  The value of the specified variable in the specified section, or "default_value" if it is not
         *           defined.
         */
        uint64_t getUint64T(const std::string &variable_name, const uint64_t &default_value) const;

        /** \brief   Retrieves a boolean value from a configuration file.
         *  \param   variable_name  The name of the section entry to read.
         *  \return  True if the retrieved value was "true", "yes" or "on" and false if the retrieved value was "false",
         *           "no" or "off".
         *  \note    If the variable is not defined in the section, the program throws an exception.  The expected
         *           values for the variable are case insensitive and can be any of "true", "yes", "on", "false", "no" or
         *           "off", and any other value results in an exception being thrown.
         */
        bool getBool(const std::string &variable_name) const;

        /** \brief   Retrieves a boolean value from a configuration file.
         *  \param   variable_name  The name of the section entry to read.
         *  \param   default_value  A default to return if the variable is not defined.
         *  \return  True if the retrieved value was "true", "yes" or "on" and false if the retrieved value was "false",
         *           "no" or "off", or the default if it was not defined.
         *  \note    If the variable is not defined in the section, the program returns the default value.  The expected
         *           values for the variable are case insensitive and can be any of "true", "yes", "on" "false", "no" or
         *           "off".  Any unknown value results in an exception being thrown.
         */
        bool getBool(const std::string &variable_name, const bool default_value) const;

        /** \brief   Retrieves an enum value from a configuration file.
         *  \param   variable_name        The name of the section entry to read.
         *  \param   string_to_value_map  A mapping of allowable string constants in the config file, to integer values.
         *  \return  The integer corresponding to the numeric value of the string constant found in the IniFile and
         *           specified via "string_to_value_map" or the default value if no entry has been found.
         *  \note    The expected values for the variable are case sensitive.  The caller will have to use a static_cast
         *           to convert the int-encoded enum to a variable of the approriate enumumerated type.  An unknown or
         *           missing value results in an exception being thrown.
         */
        int getEnum(const std::string &variable_name, const std::map<std::string, int> &string_to_value_map) const;

        /** \brief   Retrieves an enum value from a configuration file.
         *  \param   variable_name        The name of the section entry to read.
         *  \param   string_to_value_map  A mapping of allowable string constants in the config file,
         *                                to integer values.
         *  \param   default_value        A default to return if the variable is not defined.
         *  \return  The integer corresponding to the numeric value of the string constant found in the IniFile and
         *           specified via "string_to_value_map" or the default value if no entry has been found.
         *  \note    If the variable is not defined in the section, the program returns the default value.
         *           The expected values for the variable are case sensitive.  The caller will have to use a
         *           static_cast to convert the int-encoded enum to a variable of the approriate enumumerated
         *           type.  Any unknown value results in an exception being thrown.
         */
        int getEnum(const std::string &variable_name, const std::map<std::string, int> &string_to_value_map, const int default_value) const;

        std::vector<std::string> getEntryNames() const;

        inline size_t size() const { return entries_.size(); }

        // \return An iterator referencing the found entry or end() if no matching enmtry was found.
        inline const_iterator find(const std::string &variable_name) const {
            return std::find_if(entries_.cbegin(), entries_.cend(),
                                [&variable_name](const Entry &entry) { return entry.name_ == variable_name; });
        }

        // \return An iterator referencing the found entry or end() if no matching enmtry was found.
        inline iterator find(const std::string &variable_name) {
            return std::find_if(entries_.begin(), entries_.end(),
                                [&variable_name](const Entry &entry) { return entry.name_ == variable_name; });
        }

        inline bool hasEntry(const std::string &variable_name) const { return find(variable_name) != end(); }

        bool deleteEntry(const std::string &entry_name);
        const std::string &getComment(const std::string &entry_name) const;

    private:
        void write(File * const output, const bool pretty_print, const bool compact) const;
    };

public:
    typedef std::vector<Section> Sections;
    typedef Sections::iterator iterator;
    typedef Sections::const_iterator const_iterator;

protected:
    Sections sections_;
    std::string ini_file_name_;
    std::string current_section_name_;

    struct IncludeFileInfo {
        std::string filename_;
        unsigned current_lineno_;

    public:
        explicit IncludeFileInfo(const std::string &filename): filename_(filename), current_lineno_(0) { }
    };
    std::stack<IncludeFileInfo> include_file_infos_;

    bool ignore_failed_includes_;

public:
    /** \brief  Construct an IniFile based on the named file.
     *  \param  ini_file_name           The name of the .ini file. (We first look for the basename of this in a subdirectory whose
     *                                  name is the hostname of the system we're on.)
     *  \param  ignore_failed_includes  If "true", don't throw an exception if an "include" directive can't be honoured.
     *  \param  create_empty            Creates an empty INI file if none is found.
     */
    explicit IniFile(const std::string &ini_file_name, const bool ignore_failed_includes = false, const bool create_empty = false);

    /**  \brief  Construct an IniFile based on the program's configuration file.
     *
     *   This constructor creates an instance based on a program-specific configuration file.  The file is in the
     *   /usr/local/var/lib/tuelib/ directory, and is named X.conf (where "X" is the program name, as stored in "program_invocation_name"
     *   in "util.h".
     */
    IniFile();

    inline const_iterator begin() const { return sections_.begin(); }
    inline const_iterator end() const { return sections_.end(); }

    /** \brief  Assigns one IniFile to another.
     *  \param  rhs    The IniFile to copy into "this" IniFile.
     *  \param  clear  If "true", first erase all settings of the "this" InfFile.
     *  \note   The purpose of setting the "clear" parameter to "false" is to allow for loading an IniFile as a
     *          default and then override selected values by later loading another IniFile and assigning it to
     *          the IniFile that was loaded earlier while setting "clear" to "false".
     */
    void assign(const IniFile &rhs, const bool clear = true);

    inline void reload() {
        IniFile new_ini_file(ini_file_name_);
        assign(new_ini_file);
    }

    const IniFile &operator=(const IniFile &rhs) {
        assign(rhs);
        return *this;
    }

    /** \brief   Get the name of the file used to construct the object.
     *  \return  The file name.
     */
    std::string getFilename() const { return ini_file_name_; }

    bool lookup(const std::string &section_name, const std::string &variable_name, std::string * const s) const;

    /** \brief   Retrieves an integer value from a configuration file.
     *  \param   section_name   The name of the section to search.
     *  \param   variable_name  The name of the section entry to read.
     *  \return  The value of the string in the specified section.
     *  \throws  A std::runtime_error if the variable is not found or the value cannot be converted to an integer.
     */
    long getInteger(const std::string &section_name, const std::string &variable_name) const;

    /** \brief   Retrieves an integer value from a configuration file.
     *  \param   section_name   The name of the section to search.
     *  \param   variable_name  The name of the section entry to read.
     *  \param   default_value  A default to return if the variable is not defined.
     *  \return  The value of the string in the specified section.
     *  \throws  A std::runtime_error if the variable is not found or the value cannot be converted to an integer.
     */
    long getInteger(const std::string &section_name, const std::string &variable_name, const long default_value) const;

    /** \brief   Retrieves a floating point value from a configuration file.
     *  \param   section_name   The name of the section to search.
     *  \param   variable_name  The name of the section entry to read.
     *  \return  The value of the string in the specified section.
     *  \throws  A std::runtime_error if the variable is not found or the value cannot be converted to a double.
     */
    double getDouble(const std::string &section_name, const std::string &variable_name) const;

    /** \brief   Retrieves a floating point value from a configuration file.
     *  \param   section_name   The name of the section to search.
     *  \param   variable_name  The name of the section entry to read.
     *  \param   default_value  A default to return if the variable is not defined.
     *  \return  The value of the string in the specified section.
     *  \throws  A std::runtime_error if the value was found but cannot be converted to a double.
     */
    double getDouble(const std::string &section_name, const std::string &variable_name, const double &default_value) const;

    /** \brief   Retrieves a string value from a configuration file.
     *  \param   section_name   The name of the section to search.
     *  \param   variable_name  The name of the section entry to read.
     *  \return  The value of the string in the specified section.
     *  \throws  A std::runtime_error if the variable is not found.
     *  \note    If the variable is not defined in the section, the program throws an exception.
     */
    std::string getString(const std::string &section_name, const std::string &variable_name) const;

    /** \brief   Retrieves a string value from a configuration file.
     *  \param   section_name   The name of the section to search.
     *  \param   variable_name  The name of the section entry to read.
     *  \param   default_value  A default to return if the variable is not defined.
     *  \return  The value of the specified variable in the specified section, or "default_value" if it is not
     *           defined.
     */
    std::string getString(const std::string &section_name, const std::string &variable_name, const std::string &default_value) const;

    /** \brief   Retrieves a single character value from a configuration file.
     *  \param   section_name   The name of the section to search.
     *  \param   variable_name  The name of the section entry to read.
     *  \return  The value of the character in the specified section.
     *  \note    If the variable is not defined in the section, the program throws an exception.
     */
    char getChar(const std::string &section_name, const std::string &variable_name) const;

    /** \brief   Retrieves a single character value from a configuration file.
     *  \param   section_name   The name of the section to search.
     *  \param   variable_name  The name of the section entry to read.
     *  \param   default_value  A default to return if the variable is not defined.
     *  \return  The value of the specified variable in the specified section, or "default_value" if it is not
     *           defined.
     */
    char getChar(const std::string &section_name, const std::string &variable_name, const char default_value) const;

    /** \brief   Retrieves an unsigned value from a configuration file.
     *  \param   section_name   The name of the section to search.
     *  \param   variable_name  The name of the section entry to read.
     *  \return  The value of the specified variable in the specified section.
     *  \note    If the variable is not defined in the section, an exception is thrown.
     */
    unsigned getUnsigned(const std::string &section_name, const std::string &variable_name) const;

    /** \brief   Retrieves an unsigned value from a configuration file.
     *  \param   section_name   The name of the section to search.
     *  \param   variable_name  The name of the section entry to read.
     *  \param   default_value  A default to return if the variable is not defined.
     *  \return  The value of the specified variable in the specified section, or "default_value" if it is not
     *           defined.
     */
    unsigned getUnsigned(const std::string &section_name, const std::string &variable_name, const unsigned &default_value) const;

    /** \brief   Retrieves a uint64_t value from a configuration file.
     *  \param   section_name   The name of the section to search.
     *  \param   variable_name  The name of the section entry to read.
     *  \return  The value of the specified variable in the specified section.
     *  \note    If the variable is not defined in the section, an exception is thrown.
     */
    uint64_t getUint64T(const std::string &section_name, const std::string &variable_name) const;

    /** \brief   Retrieves a uint64_t value from a configuration file.
     *  \param   section_name   The name of the section to search.
     *  \param   variable_name  The name of the section entry to read.
     *  \param   default_value  A default to return if the variable is not defined.
     *  \return  The value of the specified variable in the specified section, or "default_value" if it is not
     *           defined.
     */
    uint64_t getUint64T(const std::string &section_name, const std::string &variable_name, const uint64_t &default_value) const;

    /** \brief   Retrieves a boolean value from a configuration file.
     *  \param   section_name   The name of the configuration file section to search for the entry.
     *  \param   variable_name  The name of the section entry to read.
     *  \return  True if the retrieved value was "true", "yes" or "on" and false if the retrieved value was "false",
     *           "no" or "off".
     *  \note    If the variable is not defined in the section, the program throws an exception.  The expected
     *           values for the variable are case insensitive and can be any of "true", "yes", "on", "false", "no" or
     *           "off", and any other value results in an exception being thrown.
     */
    bool getBool(const std::string &section_name, const std::string &variable_name) const;

    /** \brief   Retrieves a boolean value from a configuration file.
     *  \param   section_name   The name of the configuration file section to search for the entry.
     *  \param   variable_name  The name of the section entry to read.
     *  \param   default_value  A default to return if the variable is not defined.
     *  \return  True if the retrieved value was "true", "yes" or "on" and false if the retrieved value was "false",
     *           "no" or "off", or the default if it was not defined.
     *  \note    If the variable is not defined in the section, the program returns the default value.  The expected
     *           values for the variable are case insensitive and can be any of "true", "yes", "on" "false", "no" or
     *           "off".  Any unknown value results in an exception being thrown.
     */
    bool getBool(const std::string &section_name, const std::string &variable_name, const bool default_value) const;


    /** \brief   Retrieves an enum value from a configuration file.
     *  \param   section_name         The name of the configuration file section to search for the entry.
     *  \param   variable_name        The name of the section entry to read.
     *  \param   string_to_value_map  A mapping of allowable string constants in the config file, to integer values.
     *  \return  The integer corresponding to the numeric value of the string constant found in the IniFile and
     *           specified via "string_to_value_map" or the default value if no entry has been found.
     *  \note    The expected values for the variable are case sensitive.  The caller will have to use a static_cast
     *           to convert the int-encoded enum to a variable of the approriate enumumerated type.  An unknown or
     *           missing value results in an exception being thrown.
     */
    int getEnum(const std::string &section_name, const std::string &variable_name,
                const std::map<std::string, int> &string_to_value_map) const;


    /** \brief   Retrieves an enum value from a configuration file.
     *  \param   section_name         The name of the configuration file section to search for the entry.
     *  \param   variable_name        The name of the section entry to read.
     *  \param   string_to_value_map  A mapping of allowable string constants in the config file,
     *                                to integer values.
     *  \param   default_value        A default to return if the variable is not defined.
     *  \return  The integer corresponding to the numeric value of the string constant found in the IniFile and
     *           specified via "string_to_value_map" or the default value if no entry has been found.
     *  \note    If the variable is not defined in the section, the program returns the default value.
     *           The expected values for the variable are case sensitive.  The caller will have to use a
     *           static_cast to convert the int-encoded enum to a variable of the approriate enumumerated
     *           type.  Any unknown value results in an exception being thrown.
     */
    int getEnum(const std::string &section_name, const std::string &variable_name, const std::map<std::string, int> &string_to_value_map,
                const int default_value) const;

    std::vector<std::string> getSections() const;
    std::vector<std::string> getSectionEntryNames(const std::string &section_name) const;

    /** \brief  Returns entry values from a section that start with a particular string, case insensitive
     *  \param            section_name     The section name to check
     *  \param            starting_with    The string which determines which items will be returned.
     *  \return           the std::string values of all the items that matched.
     *  \note             This is used to workaround the limitation that InFiles cannot have two
     *                    elements in a section with the same name. So, if you want to simulate this,
     *                    you must call them something like entry1, entry2, entry_last and then use
     *                    this function like this: getSectionEntryValuesHavingNamesStartingWith("init", "entry");
     */
    std::vector<std::string> getSectionEntryValuesHavingNamesStartingWith(const std::string &section_name,
                                                                          const std::string &starting_with) const;

    /** \brief  Returns entry names from a section that start with a particular string, case insensitive
     *  \param            section_name     The section name to check
     *  \param            starting_with    The string which determines which items will be returned.
     *  \return           the std::string values of all the items that matched.
     *  \note             This is used to workaround the limiation that InFiles cannot have two
     *                    elements in a section with the same name. So, if you want to simulate this,
     *                    you must call them something like entry1, entry2, entry_last and
     *                    then use this function like this: getSectionEntryNamesThatStartWith("init", "entry");
     */
    std::vector<std::string> getSectionEntryNamesThatStartWith(const std::string &section_name, const std::string &starting_with) const;

    /** \brief  Generates a map from a sections' entries to it's values.
     *  \param  section_name  The name of the section whose map will be returned.
     *  \return An iterator referencing the found section or end() if it doesn't exist.
     */
    inline const_iterator getSection(const std::string &section_name) const {
        return std::find(sections_.cbegin(), sections_.cend(), section_name);
    }

    /** \brief  Generates a map from a sections' entries to it's values.
     *  \param  section_name  The name of the section whose map will be returned.
     *  \return An iterator referencing the foloung section or end() if it doesn't exist.
     */
    inline iterator getSection(const std::string &section_name) { return std::find(sections_.begin(), sections_.end(), section_name); }

    bool deleteSection(const std::string &section_name);

    bool deleteEntry(const std::string &section_name, const std::string &entry_name);

    bool sectionIsDefined(const std::string &section_name) const;

    /** \brief Creates "section_name" if it doesn't already exist.
     *  \return True if a nedw section was created and false if the section already existed.
     */
    bool appendSection(const std::string &section_name);

    bool variableIsDefined(const std::string &section_name, const std::string &variable_name) const;

    // \param pretty_print  If true we vertically align equal signs w/in each section.
    // \param compact       If true we remove all empty lines except for section-final ones
    void write(const std::string &path, const bool pretty_print = true, const bool compact = false) const;

    /** \brief  Generate an ini file name based upon the program name, i.e, /usr/local/var/lib/tuelib/programname.conf */
    static std::string DefaultIniFileName();

private:
    inline unsigned &getCurrentLineNo() {
        assert(not include_file_infos_.empty());
        return include_file_infos_.top().current_lineno_;
    }

    inline const std::string &getCurrentFile() const {
        assert(not include_file_infos_.empty());
        return include_file_infos_.top().filename_;
    }

    void processSectionHeader(const std::string &line);
    void processInclude(const std::string &line);
    void processInherit(const std::string &line, Section * const current_section);
    void processSectionEntry(const std::string &line, const std::string &comment);
    void processFile(const std::string &filename);
};


inline bool operator==(const std::string &section_name, const IniFile::Section &section) {
    return section_name == section.getSectionName();
}

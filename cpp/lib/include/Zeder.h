/** \brief Interaction with the Zeder collaboration tool
 *  \author Madeesh Kannan
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
#pragma once

#include <functional>
#include <memory>
#include <unordered_map>
#include "IniFile.h"
#include "StringUtil.h"
#include "TimeUtil.h"
#include "util.h"


namespace Zeder {


class CsvReader;
class IniReader;


static constexpr auto MODIFIED_TIMESTAMP_FORMAT_STRING = "%Y-%m-%d %H:%M:%S";


// A basic entry in a Zeder spreadsheet
class Entry {
protected:
    using AttributeMap = std::unordered_map<std::string, std::string>;
    unsigned id_;
    tm last_modified_timestamp_;
    AttributeMap attributes_;     // column name => content
public:
    using iterator = AttributeMap::iterator;
    using const_iterator = AttributeMap::const_iterator;

    Entry(const unsigned id = 0): id_(id), last_modified_timestamp_{}, attributes_() {};

    unsigned getId() const { return id_; }
    void setId(unsigned id) { id_ = id; }
    const tm &getLastModifiedTimestamp() const { return last_modified_timestamp_; }
    void setModifiedTimestamp(const tm &timestamp);
    const std::string &getAttribute(const std::string &name) const;
    void setAttribute(const std::string &name, const std::string &value, bool overwrite = false);
    bool hasAttribute(const std::string &name) const { return attributes_.find(name) != attributes_.end(); }

    const_iterator begin() const { return attributes_.begin(); }
    const_iterator end() const { return attributes_.end(); }
    size_t size() const { return attributes_.size(); }
};


// A collection of related entries
class EntryCollection {
    tm last_modified_timestamp_;    // when the config, as a whole, was modified
    std::vector<Entry> entries_;
public:
    using iterator = std::vector<Entry>::iterator;
    using const_iterator = std::vector<Entry>::const_iterator;

    EntryCollection(): last_modified_timestamp_{}, entries_() {}

    const struct tm &getModifiedTimestamp() const { return last_modified_timestamp_; }
    void setModifiedTimestamp(const struct tm &new_timestamp);

    /* Sorts entries by their Zeder ID */
    void sortEntries();

    /* Adds an entry to the config if it's not already present */
    void addEntry(const Entry &new_entry, const bool sort_after_add = false);

    /* Attempts to merge the changes specified in the diff into the config.
       The ID field of the diff specifies the entry to merge into. If the
       entry doesn't exist and 'add_if_absent' is true, a new entry is created for the ID.
       'modified_attributes' contains pairs of old and new values for each modified attribute.

       Returns true if the entry was modified, or if a new one was created.
    */
    bool mergeEntry(const Entry &diff, std::unordered_map<std::string, std::pair<std::string, std::string>> * const modified_attributes = nullptr,
                    const bool skip_timestamp_check = false, const bool add_if_absent = true);

    iterator find(const unsigned id);
    const_iterator find(const unsigned id) const;
    const_iterator begin() const { return entries_.begin(); }
    const_iterator end() const { return entries_.end(); }
    size_t size() const { return entries_.size(); }
};


class ExportedDataReader {
    enum FileType { CSV, JSON, INI };

    static FileType GetFileTypeFromPath(const std::string &path);
public:
    class InputParams {
        friend class ExportedDataReader;
        friend class CsvReader;
        friend class IniReader;
    protected:
        const std::string file_path_;

        /* Callback to modify and/or validate entries after they are parsed.
           If the callback returns true, the entry is added to the collection. If not, it's discarded.
        */
        std::function<bool(Entry * const)> postprocessor_;
    public:
        InputParams(const std::string &file_path, const std::function<bool(Entry * const)> &postprocessor): file_path_(file_path), postprocessor_(postprocessor) {}
        virtual ~InputParams() = default;
    };
protected:
    enum MandatoryField { Z, MTIME };

    const std::map<MandatoryField, std::string> MANDATORY_FIELD_TO_STRING_MAP {
        { Z,        "Z"      },
        { MTIME,    "Mtime"  }
    };

    std::unique_ptr<InputParams> input_params_;

    ExportedDataReader(std::unique_ptr<InputParams> params): input_params_(std::move(params)) {}
public:
    virtual ~ExportedDataReader() = default;

    virtual void parse(EntryCollection * const collection) = 0;

    static std::unique_ptr<ExportedDataReader> Factory(std::unique_ptr<InputParams> &&params);
};


class CsvReader : public ExportedDataReader {
    friend class ExportedDataReader;

    DSVReader reader_;

    CsvReader(std::unique_ptr<InputParams> &&params): ExportedDataReader(std::forward<std::unique_ptr<InputParams>>(params)), reader_(params->file_path_, ',') {}
public:
    virtual ~CsvReader() override = default;

    virtual void parse(EntryCollection * const collection) override;
};


class IniReader : public ExportedDataReader {
    friend class ExportedDataReader;

    IniFile config_;

    IniReader(std::unique_ptr<InputParams> &&params): ExportedDataReader(std::forward<std::unique_ptr<ExportedDataReader::InputParams>>(params)), config_(params->file_path_) {}
public:
    class InputParams : public ExportedDataReader::InputParams {
        friend class IniReader;
    protected:
        std::string
        std::vector<std::string> valid_section_names_;
        std::unordered_map<std::string, std::string> key_to_column_map_;
    public:
        InputParams(const std::string &file_path, const std::function<bool(Entry * const)> &postprocessor,
                    const std::vector<std::string> &valid_section_names, std::unordered_map<std::string, std::string> &key_to_column_map):
                    ExportedDataReader::InputParams(file_path, postprocessor), valid_section_names_(valid_section_names), key_to_column_map_(key_to_column_map) {}
        virtual ~InputParams() = default;
    };
public:
    virtual ~IniReader() override = default;

    virtual void parse(EntryCollection * const collection) override;
};


}

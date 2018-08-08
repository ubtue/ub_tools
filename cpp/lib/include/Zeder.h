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
#include <unordered_set>
#include "IniFile.h"
#include "StringUtil.h"
#include "TimeUtil.h"
#include "util.h"


namespace Zeder {


enum Flavour { IXTHEO, KRIMDOK };


} // unnamed namespace


namespace std {
    template <>
    struct hash<Zeder::Flavour> {
        size_t operator()(const Zeder::Flavour &flavour) const {
            // hash method here.
            return hash<int>()(flavour);
        }
    };
} // namespace std


namespace Zeder {


const std::unordered_map<Flavour, std::string> FLAVOUR_TO_STRING_MAP{
    { IXTHEO,   "IxTheo"  },
    { KRIMDOK,  "KrimDok" }
};


static constexpr auto MODIFIED_TIMESTAMP_FORMAT_STRING = "%Y-%m-%d %H:%M:%S";


// A basic entry in a Zeder spreadsheet
class Entry {
    using AttributeMap = std::unordered_map<std::string, std::string>;

    unsigned id_;
    tm last_modified_timestamp_;
    AttributeMap attributes_;     // column name => content
public:
    using iterator = AttributeMap::iterator;
    using const_iterator = AttributeMap::const_iterator;
public:
    explicit Entry(const unsigned id = 0): id_(id), last_modified_timestamp_{}, attributes_() {}

    unsigned getId() const { return id_; }
    void setId(unsigned id) { id_ = id; }
    const tm &getLastModifiedTimestamp() const { return last_modified_timestamp_; }
    void setModifiedTimestamp(const tm &timestamp) { std::memcpy(&last_modified_timestamp_, &timestamp, sizeof(timestamp)); }
    const std::string &getAttribute(const std::string &name) const;
    void setAttribute(const std::string &name, const std::string &value, bool overwrite = false);
    bool hasAttribute(const std::string &name) const { return attributes_.find(name) != attributes_.end(); }
    void removeAttribute(const std::string &name);
    unsigned keepAttributes(const std::vector<std::string> &names_to_keep);
    void prettyPrint(std::string * const print_buffer) const;

    const_iterator begin() const { return attributes_.begin(); }
    const_iterator end() const { return attributes_.end(); }
    size_t size() const { return attributes_.size(); }

    struct DiffResult {
        // True if the modified revision's timestamp is newer than the source revision's
        bool timestamp_is_newer_;
        // ID of the corresponding entry
        unsigned id_;
        // Last modified timestamp of the modified/newer revision
        tm last_modified_timestamp_;
        // Attribute => (old value, new value)
        // If the attribute was not present in the source revision, the old value is an empty string
        std::unordered_map<std::string, std::pair<std::string, std::string>> modified_attributes_;

        void prettyPrint(std::string * const print_buffer) const;
    };

    // Compares the LHS (old revision) with the RHS (new revision) and returns the differences
    static DiffResult Diff(const Entry &lhs, const Entry &rhs, const bool skip_timestamp_check = false);
    // Merges the delta into an entry, overwritting any previous values
    static void Merge(const DiffResult &delta, Entry * const merge_into);
};


// A collection of related entries
class EntryCollection {
    std::vector<Entry> entries_;
public:
    using iterator = std::vector<Entry>::iterator;
    using const_iterator = std::vector<Entry>::const_iterator;

    /* Sorts entries by their Zeder ID */
    void sortEntries();

    /* Adds an entry to the config if it's not already present */
    void addEntry(const Entry &new_entry, const bool sort_after_add = false);

    iterator find(const unsigned id);
    const_iterator find(const unsigned id) const;
    const_iterator begin() const { return entries_.begin(); }
    const_iterator end() const { return entries_.end(); }
    size_t size() const { return entries_.size(); }
    void clear() { entries_.clear(); }
};


inline void EntryCollection::sortEntries() {
    std::sort(entries_.begin(), entries_.end(),
              [](const Entry &a, const Entry &b) { return a.getId() < b.getId(); });
}


inline EntryCollection::iterator EntryCollection::find(const unsigned id) {
    return std::find_if(entries_.begin(), entries_.end(),
                        [id] (const Entry &entry) { return entry.getId() == id; });
}


inline EntryCollection::const_iterator EntryCollection::find(const unsigned id) const {
    return std::find_if(entries_.begin(), entries_.end(),
                        [id] (const Entry &entry) { return entry.getId() == id; });
}


enum FileType { CSV, JSON, INI };


FileType GetFileTypeFromPath(const std::string &path, bool check_if_file_exists = true);


class Importer {
public:
    class Params {
        friend class Importer;
        friend class CsvReader;
        friend class IniReader;
    protected:
        const std::string file_path_;

        // Callback to modify and/or validate entries after they are parsed.
        // If the callback returns true, the entry is added to the collection. If not, it's discarded.
        std::function<bool(Entry * const)> postprocessor_;
    public:
        Params(const std::string &file_path, const std::function<bool(Entry * const)> &postprocessor): file_path_(file_path), postprocessor_(postprocessor) {}
        virtual ~Params() = default;
    };
protected:
    enum MandatoryField { Z, MTIME };

    const std::map<MandatoryField, std::string> MANDATORY_FIELD_TO_STRING_MAP {
        { Z,        "Z"      },
        { MTIME,    "Mtime"  }
    };

    std::unique_ptr<Params> input_params_;
protected:
    explicit Importer(std::unique_ptr<Params> params): input_params_(std::move(params)) {}
public:
    virtual ~Importer() = default;
public:
    virtual void parse(EntryCollection * const collection) = 0;

    static std::unique_ptr<Importer> Factory(std::unique_ptr<Params> params);
};


class CsvReader : public Importer {
    friend class Importer;

    DSVReader reader_;
private:
    explicit CsvReader(std::unique_ptr<Params> params): Importer(std::move(params)), reader_(input_params_->file_path_, ',') {}
public:
    virtual ~CsvReader() override = default;
public:
    virtual void parse(EntryCollection * const collection) override;
};


class IniReader : public Importer {
    friend class Importer;

    IniFile config_;
private:
    explicit IniReader(std::unique_ptr<Params> params): Importer(std::move(params)), config_(input_params_->file_path_) {}
public:
    class Params : public Importer::Params {
        friend class IniReader;
    protected:
        std::vector<std::string> valid_section_names_;
        std::string section_name_attribute_;
        std::string zeder_id_key_;
        std::string zeder_last_modified_timestamp_key_;
        std::unordered_map<std::string, std::string> key_to_attribute_map_;
    public:
        Params(const std::string &file_path, const std::function<bool(Entry * const)> &postprocessor,
                    const std::vector<std::string> &valid_section_names, const std::string &section_name_attribute,
                    const std::string &zeder_id_key, const std::string &zeder_last_modified_timestamp_key,
                    const std::unordered_map<std::string, std::string> &key_to_attribute_map):
                    Importer::Params(file_path, postprocessor),
                    valid_section_names_(valid_section_names), section_name_attribute_(section_name_attribute),
                    zeder_id_key_(zeder_id_key), zeder_last_modified_timestamp_key_(zeder_last_modified_timestamp_key),
                    key_to_attribute_map_(key_to_attribute_map) {}
        virtual ~Params() = default;
    };
public:
    virtual ~IniReader() override = default;
public:
    virtual void parse(EntryCollection * const collection) override;
};


class Exporter {
public:
    class Params {
        friend class Exporter;
        friend class IniWriter;
    protected:
        const std::string file_path_;
    public:
        Params(const std::string &file_path): file_path_(file_path) {}
        virtual ~Params() = default;
    };
protected:
    std::unique_ptr<Params> input_params_;
protected:
    explicit Exporter(std::unique_ptr<Params> params): input_params_(std::move(params)) {}
public:
    virtual ~Exporter() = default;
public:
    virtual void write(const EntryCollection &collection) = 0;

    static std::unique_ptr<Exporter> Factory(std::unique_ptr<Params> params);
};


class IniWriter : public Exporter {
    friend class Exporter;

    std::unique_ptr<IniFile> config_;
private:
    explicit IniWriter(std::unique_ptr<Params> params);
public:
    class Params : public Exporter::Params {
        friend class IniWriter;
    protected:
        // Ordered list. The ID and the timestamp always go first.
        std::vector<std::string> attributes_to_export_;
        std::string section_name_attribute_;
        std::string zeder_id_key_;
        std::string zeder_last_modified_timestamp_key_;
        std::unordered_map<std::string, std::string> attribute_to_key_map_;

        // Callback to append extra data. Invoked after all the (exportable) attributes have been exported.
        std::function<void(IniFile::Section * const, const Entry &)> extra_keys_appender_;
    public:
        Params(const std::string &file_path, const std::vector<std::string> &attributes_to_export,
               const std::string &section_name_attribute, const std::string &zeder_id_key, const std::string &zeder_last_modified_timestamp_key,
               const std::unordered_map<std::string, std::string> &attribute_name_to_key_map,
               const std::function<void(IniFile::Section * const, const Entry &)> &extra_keys_appender):
               Exporter::Params(file_path), attributes_to_export_(attributes_to_export), section_name_attribute_(section_name_attribute),
               zeder_id_key_(zeder_id_key), zeder_last_modified_timestamp_key_(zeder_last_modified_timestamp_key),
               attribute_to_key_map_(attribute_name_to_key_map), extra_keys_appender_(extra_keys_appender) {}
        virtual ~Params() = default;
    };
public:
    virtual ~IniWriter() override = default;
public:
    virtual void write(const EntryCollection &collection) override;
};


} // namespace Zeder

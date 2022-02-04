/** \brief API to interact with the Zeder collaboration tool
 *  \author Madeeswaran Kannan (madeeswaran.kannan@uni-tuebingen.de)
 *
 *  \copyright 2018-2021 Universitätsbibliothek Tübingen.  All rights reserved.
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
#include "File.h"
#include "IniFile.h"
#include "JSON.h"
#include "RegexMatcher.h"
#include "TimeUtil.h"
#include "util.h"


namespace Zeder {


enum Flavour { IXTHEO, KRIMDOK };


} // namespace Zeder


namespace std {


template <>
struct hash<Zeder::Flavour> {
    size_t operator()(const Zeder::Flavour &flavour) const {
        // hash method here.
        return hash<int>()(flavour);
    }
};


} // end namespace std


namespace Zeder {


const std::unordered_map<Flavour, std::string> FLAVOUR_TO_STRING_MAP{ { IXTHEO, "IxTheo" }, { KRIMDOK, "KrimDok" } };


Flavour GetFlavourByString(const std::string &flavour);


static constexpr auto MODIFIED_TIMESTAMP_FORMAT_STRING = "%Y-%m-%d %H:%M:%S";

// Characters that need to be stripped from attribute( value)s imported from Zeder before they are (de)serialised
const std::string ATTRIBUTE_INVALID_CHARS = "#\"'";


// A basic entry in a Zeder spreadsheet. Each column maps to an attribute.
class Entry {
    using AttributeMap = std::unordered_map<std::string, std::string>;

    unsigned id_;
    tm last_modified_timestamp_;
    AttributeMap attributes_; // column name => content
public:
    using iterator = AttributeMap::iterator;
    using const_iterator = AttributeMap::const_iterator;

public:
    explicit Entry(const unsigned id = 0): id_(id), last_modified_timestamp_{}, attributes_() { }

    unsigned getId() const { return id_; }
    void setId(unsigned id) { id_ = id; }
    const tm &getLastModifiedTimestamp() const { return last_modified_timestamp_; }
    void setModifiedTimestamp(const tm &timestamp) { std::memcpy(&last_modified_timestamp_, &timestamp, sizeof(timestamp)); }
    const std::string &getAttribute(const std::string &name) const;
    const std::string &getAttribute(const std::string &name, const std::string &default_value) const;

    // Like getAttribute but returns an empty string if the attribute (= value for the short_column_name) is missing.
    inline std::string lookup(const std::string &short_column_name) const {
        const auto attribute_value(getAttribute(short_column_name, ""));
        return (attribute_value == "NV") ? "" : attribute_value;
    }

    // Invalid characters (as found in 'ATTRIBUTE_INVALID_CHARS') in 'value' will be replaced with '_'.
    void setAttribute(const std::string &name, const std::string &value, bool overwrite = false);
    bool hasAttribute(const std::string &name) const { return attributes_.find(name) != attributes_.end(); }
    void removeAttribute(const std::string &name);
    unsigned keepAttributes(const std::vector<std::string> &names_to_keep);
    void prettyPrint(std::string * const print_buffer) const;
    std::string prettyPrint() const;

    iterator begin() { return attributes_.begin(); }
    iterator end() { return attributes_.end(); }
    const_iterator begin() const { return attributes_.begin(); }
    const_iterator end() const { return attributes_.end(); }
    size_t size() const { return attributes_.size(); }
    bool empty() const { return attributes_.empty(); }

    struct DiffResult {
        // True if the modified revision's timestamp is newer than the source revision's
        bool timestamp_is_newer_;

        // Difference in days between the modified revision and the source revision
        double timestamp_time_difference_;

        // ID of the corresponding entry
        unsigned id_;

        // Last modified timestamp of the modified/newer revision
        tm last_modified_timestamp_;

        // Attribute => (old value, new value)
        // If the attribute was not present in the source revision, the old value is an empty string
        std::unordered_map<std::string, std::pair<std::string, std::string>> modified_attributes_;

    public:
        void prettyPrint(std::string * const print_buffer) const;
        std::string prettyPrint() const;
    };

    // Compares the LHS (old revision) with the RHS (new revision) and returns the differences
    static DiffResult Diff(const Entry &lhs, const Entry &rhs, const bool skip_timestamp_check = false);
    // Merges the delta into an entry, overwritting any previous values
    static void Merge(const DiffResult &delta, Entry * const merge_into);
};


// A collection of related entries (from the same Zeder Instance)
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
    iterator begin() { return entries_.begin(); }
    iterator end() { return entries_.end(); }
    const_iterator find(const unsigned id) const;
    const_iterator begin() const { return entries_.begin(); }
    const_iterator end() const { return entries_.end(); }
    size_t size() const { return entries_.size(); }
    void clear() { entries_.clear(); }
    iterator erase(iterator entry) { return entries_.erase(entry); }
    bool empty() const { return entries_.empty(); }
};


inline void EntryCollection::sortEntries() {
    std::sort(entries_.begin(), entries_.end(), [](const Entry &a, const Entry &b) { return a.getId() < b.getId(); });
}


inline EntryCollection::iterator EntryCollection::find(const unsigned id) {
    return std::find_if(entries_.begin(), entries_.end(), [id](const Entry &entry) { return entry.getId() == id; });
}


inline EntryCollection::const_iterator EntryCollection::find(const unsigned id) const {
    return std::find_if(entries_.begin(), entries_.end(), [id](const Entry &entry) { return entry.getId() == id; });
}


enum FileType { CSV, JSON, INI };


FileType GetFileTypeFromPath(const std::string &path, bool check_if_file_exists = true);


// Abstract base class for importing Zeder data from different sources.
class Importer {
public:
    enum MandatoryField { Z, MTIME };

    static const std::map<MandatoryField, std::string> MANDATORY_FIELD_TO_STRING_MAP;

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
        Params(const std::string &file_path, const std::function<bool(Entry * const)> &postprocessor)
            : file_path_(file_path), postprocessor_(postprocessor) { }
        virtual ~Params() = default;
    };

protected:
    std::unique_ptr<Params> input_params_;

protected:
    explicit Importer(std::unique_ptr<Params> params): input_params_(std::move(params)) { }

public:
    virtual ~Importer() = default;

public:
    virtual void parse(EntryCollection * const collection) = 0;

    static std::unique_ptr<Importer> Factory(std::unique_ptr<Params> params);
};


// Reader for CSV files exported through the Zeder interface.
class CsvReader : public Importer {
    friend class Importer;

    DSVReader reader_;

private:
    explicit CsvReader(std::unique_ptr<Params> params): Importer(std::move(params)), reader_(input_params_->file_path_, ',') { }

public:
    virtual ~CsvReader() override = default;

public:
    virtual void parse(EntryCollection * const collection) override;
};


// Reader for Zotero Harvester compatible INI/config files.
class IniReader : public Importer {
    friend class Importer;

    IniFile config_;

private:
    explicit IniReader(std::unique_ptr<Params> params): Importer(std::move(params)), config_(input_params_->file_path_) { }

public:
    class Params : public Importer::Params {
        friend class IniReader;

    protected:
        // Sections to process.
        std::vector<std::string> valid_section_names_;
        //
        std::string section_name_attribute_;
        std::string zeder_id_key_;
        std::string zeder_last_modified_timestamp_key_;
        std::unordered_map<std::string, std::string> key_to_attribute_map_;

    public:
        Params(const std::string &file_path, const std::function<bool(Entry * const)> &postprocessor,
               const std::vector<std::string> &valid_section_names, const std::string &section_name_attribute,
               const std::string &zeder_id_key, const std::string &zeder_last_modified_timestamp_key,
               const std::unordered_map<std::string, std::string> &key_to_attribute_map)
            : Importer::Params(file_path, postprocessor), valid_section_names_(valid_section_names),
              section_name_attribute_(section_name_attribute), zeder_id_key_(zeder_id_key),
              zeder_last_modified_timestamp_key_(zeder_last_modified_timestamp_key), key_to_attribute_map_(key_to_attribute_map) { }
        virtual ~Params() = default;
    };

public:
    virtual ~IniReader() override = default;

public:
    virtual void parse(EntryCollection * const collection) override;
};


// Abstract base class for exporting/serializing Zeder::Entry instances.
class Exporter {
public:
    class Params {
        friend class Exporter;
        friend class IniWriter;
        friend class CsvWriter;

    protected:
        const std::string file_path_;

    public:
        Params(const std::string &file_path): file_path_(file_path) { }
        virtual ~Params() = default;
    };

protected:
    std::unique_ptr<Params> input_params_;

protected:
    explicit Exporter(std::unique_ptr<Params> params): input_params_(std::move(params)) { }

public:
    virtual ~Exporter() = default;

public:
    virtual void write(const EntryCollection &collection) = 0;

    static std::unique_ptr<Exporter> Factory(std::unique_ptr<Params> params);
};


// Writer for Zotero Harvester compatible INI/config files.
class IniWriter : public Exporter {
    friend class Exporter;

    std::unique_ptr<IniFile> config_;

    void writeEntry(IniFile::Section * const section, const std::string &name, const std::string &value) const;

private:
    explicit IniWriter(std::unique_ptr<Params> params);

public:
    class Params : public Exporter::Params {
        friend class IniWriter;

    protected:
        // Ordered list of attributes to export. The ID and the timestamp always go first.
        std::vector<std::string> attributes_to_export_;

        // The attribute that should be saved as the section name.
        std::string section_name_attribute_;

        // The INI key for the Zeder ID.
        std::string zeder_id_key_;

        // The INI key for the Zeder last modified timestamp.
        std::string zeder_last_modified_timestamp_key_;

        // Map between attribute names and their corresponding INI keys.
        std::unordered_map<std::string, std::string> attribute_to_key_map_;

        // Callback to append extra data. Invoked after all the (exportable) attributes have been exported.
        std::function<void(IniFile::Section * const, const Entry &)> extra_keys_appender_;

    public:
        Params(const std::string &file_path, const std::vector<std::string> &attributes_to_export,
               const std::string &section_name_attribute, const std::string &zeder_id_key,
               const std::string &zeder_last_modified_timestamp_key,
               const std::unordered_map<std::string, std::string> &attribute_name_to_key_map,
               const std::function<void(IniFile::Section * const, const Entry &)> &extra_keys_appender)
            : Exporter::Params(file_path), attributes_to_export_(attributes_to_export), section_name_attribute_(section_name_attribute),
              zeder_id_key_(zeder_id_key), zeder_last_modified_timestamp_key_(zeder_last_modified_timestamp_key),
              attribute_to_key_map_(attribute_name_to_key_map), extra_keys_appender_(extra_keys_appender) { }
        virtual ~Params() = default;
    };

public:
    virtual ~IniWriter() override = default;

public:
    virtual void write(const EntryCollection &collection) override;
};


// Writer for CSV files.
class CsvWriter : public Exporter {
    friend class Exporter;

    File output_file_;

private:
    explicit CsvWriter(std::unique_ptr<Params> params)
        : Exporter(std::move(params)), output_file_(this->input_params_->file_path_, "w", File::ThrowOnOpenBehaviour::THROW_ON_ERROR){};

public:
    class Params : public Exporter::Params {
        friend class CsvWriter;

    protected:
        // Ordered list of attributes to export. The ID and the timestamp always go first and last respectively.
        // If empty, all attributes are exported in an indeterminate order.
        std::vector<std::string> attributes_to_export_;

        // Column name for the Zeder ID.
        std::string zeder_id_column_;

        // Column name for the Zeder last modified timestamp.
        std::string zeder_last_modified_timestamp_column_;

    public:
        Params(const std::string &file_path, const std::vector<std::string> &attributes_to_export,
               const std::string &zeder_id_column = Importer::MANDATORY_FIELD_TO_STRING_MAP.at(Importer::MandatoryField::Z),
               const std::string &zeder_last_modified_timestamp_column =
                   Importer::MANDATORY_FIELD_TO_STRING_MAP.at(Importer::MandatoryField::MTIME))
            : Exporter::Params(file_path), attributes_to_export_(attributes_to_export), zeder_id_column_(zeder_id_column),
              zeder_last_modified_timestamp_column_(zeder_last_modified_timestamp_column) { }
        virtual ~Params() = default;
    };

public:
    virtual ~CsvWriter() override = default;

public:
    virtual void write(const EntryCollection &collection) override;
};


// Abstract base class for querying and downloading entries from a Zeder instance.
class EndpointDownloader {
public:
    enum Type { FULL_DUMP };

    class Params {
        friend class EndpointDownloader;

    protected:
        const std::string endpoint_url_;

    public:
        explicit Params(const std::string &endpoint_url): endpoint_url_(endpoint_url) { }
        virtual ~Params() = default;
    };

protected:
    std::unique_ptr<Params> downloader_params_;

protected:
    explicit EndpointDownloader(std::unique_ptr<Params> params): downloader_params_(std::move(params)) { }

public:
    virtual ~EndpointDownloader() = default;

public:
    virtual bool download(EntryCollection * const collection, const bool disable_cache_mechanism = false) = 0;

    static std::unique_ptr<EndpointDownloader> Factory(Type downloader_type, std::unique_ptr<Params> params);
};


// Downloads the entire database of a Zeder instance as a JSON file.
class FullDumpDownloader : public EndpointDownloader {
    friend class EndpointDownloader;

private:
    explicit FullDumpDownloader(std::unique_ptr<Params> params): EndpointDownloader(std::move(params)){};

public:
    class Params : public EndpointDownloader::Params {
        friend class FullDumpDownloader;

    protected:
        // Zeder IDs of entries to download. If empty, all entries are downloaded.
        std::unordered_set<unsigned> entries_to_download_;
        // Names of columns to download. If empty, all columns are downloaded.
        std::unordered_set<std::string> columns_to_download_;

        // Filters applied to each row. Column name => Filter reg-ex.
        std::unordered_map<std::string, std::unique_ptr<RegexMatcher>> filter_regexps_;

    public:
        Params(const std::string &endpoint_path, const std::unordered_set<unsigned> &entries_to_download,
               const std::unordered_set<std::string> &columns_to_download,
               const std::unordered_map<std::string, std::string> &filter_regexps);
        virtual ~Params() = default;
    };

private:
    struct ColumnMetadata {
        std::string column_type_;
        std::unordered_map<int64_t, std::string> ordinal_to_value_map_;
    };

    bool downloadData(const std::string &endpoint_url, std::shared_ptr<JSON::ObjectNode> * const json_data);
    void parseColumnMetadata(const std::shared_ptr<JSON::ObjectNode> &json_data,
                             std::unordered_map<std::string, ColumnMetadata> * const column_to_metadata_map);
    void parseRows(const Params &params, const std::shared_ptr<JSON::ObjectNode> &json_data,
                   const std::unordered_map<std::string, ColumnMetadata> &column_to_metadata_map, EntryCollection * const collection);

public:
    virtual ~FullDumpDownloader() = default;

public:
    virtual bool download(EntryCollection * const collection, const bool disable_cache_mechanism = false) override;
};


std::string GetFullDumpEndpointPath(Flavour zeder_flavour);

Flavour ParseFlavour(const std::string &flavour, const bool case_sensitive = false);


class SimpleZeder {
    typedef EntryCollection::const_iterator const_iterator;

private:
    bool failed_to_connect_to_database_server_;
    EntryCollection entries_;

public:
    // \param "column_filter" If not empty, only the specified short column names will be accessible via the
    //        lookup member function of class Journal.  This is a performance and memory optimisation only.
    explicit SimpleZeder(const Flavour flavour, const std::unordered_set<std::string> &column_filter = {},
                         const std::unordered_map<std::string, std::string> &filter_regexps = {});

    inline operator bool() const { return not failed_to_connect_to_database_server_; }
    inline size_t size() const { return entries_.size(); }
    inline size_t empty() const { return entries_.empty(); }
    inline const_iterator begin() const { return entries_.begin(); }
    inline const_iterator end() const { return entries_.end(); }
};


// \brief Upload information about new journal articles
// \param path         path of the JSON file to upload
// \param data_source  source label, e.g. the script name, or just "test".
void UploadArticleList(const std::string &json_path, const std::string &data_source);


} // end namespace Zeder

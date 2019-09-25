/** \file   JournalConfig.h
 *  \brief  Central repository for all journal-related config data
 *  \author Madeeswaran Kannan (madeeswaran.kannan@uni-tuebingen.de)
 *
 *  \copyright 2018, 2019 Universitätsbibliothek Tübingen.  All rights reserved.
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU Affero General Public License as
 *  published by the Free Software Foundation, either version 3 of the
 *  License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Affero %General Public License for more details.
 *
 *  You should have received a copy of the GNU Affero General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include <functional>
#include <map>
#include <unordered_map>
#include <unordered_set>
#include <type_traits>
#include "IniFile.h"
#include "StringUtil.h"
#include "util.h"


/*
 * The following provide a centeralized API to read and write journal related data stored in
 * config files that store journal-specific data. The structure of such config files is as follows:
 *      [journal name]
 *      <bundle_name>_<key> = <value>
 * Bundles are collections of related key-value config entries. Journals can have multiple bundles.
 * Keys inside a bundle are required to be unique, but different bundles can have keys with the same name.
 */
namespace JournalConfig {


// Represents an (unsigned) integer ID for entries
using EntryId = unsigned;


// A basic triple of an ID, a key and a value. Each Entry is associated with a specific bundle.
struct Entry {
    EntryId id_;
    std::string key_;
    std::string value_;
};


// Represents a collection of related entries. The template type parameter "Traits" must be a type that implements the following:
//      * A Key-ID-Resolver map that maps key names to their corresponding ID.
//      * A prefix string that specifies the name of the bundle.
template<typename Traits>
class EntryBundle {
    using KeyIdResolutionMap = std::unordered_map<std::string, typename Traits::Entries>;

    std::vector<Entry> entries_;

    static bool ResolveKeyId(const KeyIdResolutionMap &key_id_resolver_map, const std::string &key, typename Traits::Entries * const entry_id)
    {
        const auto match(key_id_resolver_map.find(key));
        if (match == key_id_resolver_map.end())
            return false;
        *entry_id = match->second;
        return true;
    }
public:
    EntryBundle() = default;
    explicit EntryBundle(const IniFile::Section &config_section) { load(config_section); }
public:
    // Loads key-value pairs into the bundle. The key name must include the bundle name as its prefix.
    size_t load(const std::vector<std::pair<std::string, std::string>> &entries, const bool clear_entries = true) {
        if (clear_entries)
            clear();

        const std::string bundle_prefix(Traits::prefix);
        for (const auto &entry : entries) {
            const auto &key(entry.first), &value(entry.second);

            if (StringUtil::StartsWith(key, bundle_prefix)) {
                const auto trimmed_key(key.substr(bundle_prefix.length() + 1));
                typename Traits::Entries entry_id;

                if (not ResolveKeyId(Traits::key_id_resolver_map, trimmed_key, &entry_id))
                    LOG_ERROR("Couldn't resolve key '" + trimmed_key + "' for prefix '" + bundle_prefix + "'");

                entries_.push_back({ static_cast<JournalConfig::EntryId>(entry_id), trimmed_key, value });
            }
        }

        std::sort(entries_.begin(), entries_.end(), [](const Entry &a, const Entry &b) -> bool { return a.id_ < b.id_; });
        return entries_.size();
    }

    // Loads entries directly from a section in a config file.
    size_t load(const IniFile::Section &section, const bool clear_entries = true) {
        if (clear_entries)
            clear();

        const std::string bundle_prefix(Traits::prefix);
        for (const auto &entry : section) {
            const auto &key(entry.name_), &value(entry.value_);

            if (StringUtil::StartsWith(key, bundle_prefix)) {
                const auto trimmed_key(key.substr(bundle_prefix.length() + 1));
                typename Traits::Entries entry_id;

                if (ResolveKeyId(Traits::key_id_resolver_map, trimmed_key, &entry_id))
                    entries_.push_back({ static_cast<JournalConfig::EntryId>(entry_id), trimmed_key, value });
            }
        }

        std::sort(entries_.begin(), entries_.end(), [](const Entry &a, const Entry &b) -> bool { return a.id_ < b.id_; });
        return entries_.size();
    };

    // Saves the entries as an ordered-list of key-value pairs. The key names include the bundle name as their prefixes.
    void save(std::vector<std::pair<std::string, std::string>> * const entries) const {
        const std::string bundle_prefix(Traits::prefix);

        for (const auto &entry: entries_)
            entries->emplace_back(bundle_prefix + "_" + entry.key_, entry.value_);
    }

    // Saves the entries directly to a section in a config file.
    void save(IniFile::Section * const section, const IniFile::Section::DupeInsertionBehaviour insertion_behaviour) const {
        const std::string bundle_prefix(Traits::prefix);

        for (const auto &entry: entries_)
            section->insert(bundle_prefix + "_" + entry.key_, entry.value_, "", insertion_behaviour);
    }
    void clear() { entries_.clear(); }
    size_t size() const { return entries_.size(); }

    // Returns the value of the entry specified by the given entry ID.
    const std::string &value(const EntryId &entry_id) const {
        for (const auto &entry: entries_) {
            if (entry.id_ == entry_id)
                return entry.value_;
        }

        LOG_ERROR("Couldn't find entry with key " + Key(static_cast<const typename Traits::Entries>(entry_id)) +
                  " (id = '" + std::to_string(entry_id) + "')");
    }

    std::string value(const EntryId &entry_id, const std::string &default_value) const {
        for (const auto &entry: entries_) {
            if (entry.id_ == entry_id)
                return entry.value_;
        }

        return default_value;
    }

    // Returns the fully-qualified key name for a specific entry. This includes the bundle name as its prefix.
    static std::string Key(const typename Traits::Entries &entry_id) {
        const auto match(std::find_if(Traits::key_id_resolver_map.begin(),
                         Traits::key_id_resolver_map.end(),
                         [entry_id](const std::pair<std::string, JournalConfig::EntryId> &entry) {
                            return entry.second == entry_id;
                         }));

        if (match == Traits::key_id_resolver_map.end())
            throw std::runtime_error("Couldn't resolve entry id " + std::to_string(entry_id) + " for bundle prefix '" + Traits::prefix + "'");

        return Traits::prefix + "_" + match->first;
    }

    // Returns the entry ID of the given key. The key may optionally include the bundle name as its prefix.
    static typename Traits::Entries EntryId(const std::string &key) {
        std::string trimmed_key(key);
        if (StringUtil::StartsWith(key + "_", Traits::prefix))
            trimmed_key = trimmed_key.substr(Traits::prefix.length() + 1);

        auto match(Traits::key_id_resolver_map.find(trimmed_key));
        if (match == Traits::key_id_resolver_map.end())
            throw std::runtime_error("Couldn't resolve key '" + key + "' for bundle prefix '" + Traits::prefix + "'");

        return match->second;
    }
};


enum class BundleType { PRINT, ONLINE, ZEDER, ZOTERO };


struct Print {
    enum Entries : EntryId { PPN, ISSN };

    static const std::unordered_map<std::string, Print::Entries> key_id_resolver_map;
    static const std::string prefix;
};


using PrintBundle = EntryBundle<Print>;


struct Online {
    enum Entries : EntryId { PPN, ISSN };

    static const std::unordered_map<std::string, Online::Entries> key_id_resolver_map;
    static const std::string prefix;
};


using OnlineBundle = EntryBundle<Online>;

struct Zeder {
    enum Entries : EntryId { ID, MODIFIED_TIME };

    static const std::unordered_map<std::string, Zeder::Entries> key_id_resolver_map;
    static const std::string prefix;
};


using ZederBundle = EntryBundle<Zeder>;

struct Zotero {
    enum Entries : EntryId { TYPE, GROUP, URL, STRPTIME_FORMAT, EXTRACTION_REGEX, REVIEW_REGEX, MAX_CRAWL_DEPTH, DELIVERY_MODE,
                             EXPECTED_LANGUAGES, CRAWL_URL_REGEX, UPDATE_WINDOW };

    static const std::unordered_map<std::string, Zotero::Entries> key_id_resolver_map;
    static const std::string prefix;
};


using ZoteroBundle = EntryBundle<Zotero>;


// Helper class to parse a config file into bundle collections.
class Reader {
public:
    struct Bundles {
        PrintBundle bundle_print_;
        OnlineBundle bundle_online_;
        ZederBundle bundle_zeder_;
        ZoteroBundle bundle_zotero_;
    public:
        Bundles() = default;
    };

    std::unordered_map<std::string, Bundles> sections_to_bundles_map_;

    void loadFromIni(const IniFile &config);
    const Bundles &find(const std::string &section) const;
public:
    Reader(const IniFile &config) { loadFromIni(config); }
public:
    inline const PrintBundle &print(const std::string &section) const {
        return find(section).bundle_print_;
    }
    inline const OnlineBundle &online(const std::string &section) const {
        return find(section).bundle_online_;
    }
    inline const ZederBundle &zeder(const std::string &section) const {
        return find(section).bundle_zeder_;
    }
    inline const ZoteroBundle &zotero(const std::string &section) const {
        return find(section).bundle_zotero_;
    }
};


} // namespace JournalConfig

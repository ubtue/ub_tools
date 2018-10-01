/** \file   JournalConfig.h
 *  \brief  Central repository for all journal-related config data
 *  \author Madeeswaran Kannan (madeeswaran.kannan@uni-tuebingen.de)
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

namespace JournalConfig {


using EntryId = unsigned;


struct Entry {
    EntryId id;
    std::string key;       // without a prefix
    std::string value;
};


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
    size_t load(const std::vector<std::pair<std::string, std::string>> &entries, bool clear_entries = true) {
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

        std::sort(entries_.begin(), entries_.end(), [](const Entry &a, const Entry &b) -> bool { return a.id < b.id; });
        return entries_.size();
    }
    size_t load(const IniFile::Section &section, bool clear_entries = true) {
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

        std::sort(entries_.begin(), entries_.end(), [](const Entry &a, const Entry &b) -> bool { return a.id < b.id; });
        return entries_.size();
    };

    void save(std::vector<std::pair<std::string, std::string>> * const entries) const {
        const std::string bundle_prefix(Traits::prefix);

        for (const auto &entry: entries_)
            entries->emplace_back(bundle_prefix + "_" + entry.key, entry.value);
    }
    void clear() { entries_.clear(); }
    size_t size() const { return entries_.size(); }
    const std::string &value(const EntryId &entry_id) const {
        for (const auto &entry: entries_) {
            if (entry.id == entry_id)
                return entry.value;
        }

        LOG_ERROR("Couldn't find entry with id " + std::to_string(entry_id));
    }
    std::string value(const EntryId &entry_id, const std::string &default_value) const {
        for (const auto &entry: entries_) {
            if (entry.id == entry_id)
                return entry.value;
        }

        return default_value;
    }

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
    enum Entries : EntryId { ID, MODIFIED_TIME, COMMENT };

    static const std::unordered_map<std::string, Zeder::Entries> key_id_resolver_map;
    static const std::string prefix;
};


using ZederBundle = EntryBundle<Zeder>;

struct Zotero {
    enum Entries : EntryId { TYPE, GROUP, URL, STRPTIME_FORMAT, EXTRACTION_REGEX, MAX_CRAWL_DEPTH, DELIVERY_MODE };

    static const std::unordered_map<std::string, Zotero::Entries> key_id_resolver_map;
    static const std::string prefix;
};


using ZoteroBundle = EntryBundle<Zotero>;


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
    const PrintBundle &print(const std::string &section) const {
        return find(section).bundle_print_;
    }
    const OnlineBundle &online(const std::string &section) const {
        return find(section).bundle_online_;
    }
    const ZederBundle &zeder(const std::string &section) const {
        return find(section).bundle_zeder_;
    }
    const ZoteroBundle &zotero(const std::string &section) const {
        return find(section).bundle_zotero_;
    }
};


} // namespace JournalConfig

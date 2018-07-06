/** \file   zeder_tools.cc
 *  \brief  Collection of tools to marshal configuration files between Zeder and zts_harvester
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
 *  GNU Affero General Public License for more details.
 *
 *  You should have received a copy of the GNU Affero General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#include <algorithm>
#include <iostream>
#include <map>
#include <unordered_map>
#include <unordered_set>
#include <cinttypes>
#include <csignal>
#include <cstring>
#include <unistd.h>
#include "IniFile.h"
#include "StringUtil.h"
#include "TimeUtil.h"
#include "util.h"
#include "Zotero.h"


namespace {


using namespace Zotero;


[[noreturn]] void Usage() {
    std::cerr << "Usage: " << ::progname
              << " [--verbosity=min_verbosity] --mode=tool_mode flavour first_path second_path \n"
              << "Modes:\n"
              << "\t" << "generate:" << "\t" << "Converts the .csv file exported from Zeder into a zeder_tools generated .conf "                                                  "file. The first path points to the .csv file and the second to the output .conf "                                                "file.\n"
              << "\t" << "merge:"   << "\t\t"  << "Compares the last modified time stamps of entries in a pair of zeder_tools generated "                                        << ".conf files and merges any changes. The first path points to the source/updated .conf file and "
                                               << " file and the second to the destination/old .conf into which the changes are merged.\n\n"
              << "Flavour: Either 'ixtheo' or 'krimdok'.\n\n";
    std::exit(EXIT_FAILURE);
}


struct ZederEntry {
    static constexpr auto MODIFIED_TIMESTAMP_FORMAT_STRING = "%Y-%m-%d %H:%M:%S";

    using Id = unsigned;

    Id id_;
    std::string parent_ppn_;
    std::string parent_issn_print_;
    std::string parent_issn_online_;
    std::string title_;
    std::string comment_;
    std::string primary_url_;
    std::string auxiliary_url_;
    bool has_rss_feed_;
    bool has_multiple_downloads_;
    struct tm last_modified_timestamp_;

    ZederEntry() : id_(0), has_rss_feed_(false), has_multiple_downloads_(false), last_modified_timestamp_{} {}

    // ZederEntry& operator=(const ZederEntry &rhs) {
    //     id_ = rhs.id_;
    //     parent_ppn_ = rhs.parent_ppn_;
    //     parent_issn_print_ = rhs.parent_issn_print_;
    //     parent_issn_online_ = rhs.parent_issn_online_;
    //     title_ = rhs.title_;
    //     comment_ = rhs.comment_;
    //     primary_url_ = rhs.primary_url_;
    //     auxiliary_url_ = rhs.auxiliary_url_;
    //     has_rss_feed_ = rhs.has_rss_feed_;
    //     has_multiple_downloads_ = rhs.has_multiple_downloads_;
    //     std::memcpy(&this->last_modified_timestamp_, &rhs.last_modified_timestamp_, sizeof(ZederEntry));

    //     return *this;
    // }
};


using ZederConfigDiff = ZederEntry;     // stores modified/new values of a given entry


enum Flavour { IXTHEO, KRIMDOK };

class ZederConfigData {
    Flavour source_;
    struct tm last_modified_timestamp_; // when the config, as a whole, was modified
    std::vector<ZederEntry> entries_;
public:
    using iterator = std::vector<ZederEntry>::iterator;
    using const_iterator = std::vector<ZederEntry>::const_iterator;

    ZederConfigData(Flavour source) : source_(source), last_modified_timestamp_{}, entries_() {}

    Flavour getSource() const { return source_; }

    const struct tm& getModifiedTimestamp() const { return last_modified_timestamp_; }
    void setModifiedTimestamp(const struct tm &timestamp);

    void sortEntries();
    void addEntry(const ZederEntry &new_entry, bool sort_after_add = false);
    void mergeEntry(const ZederConfigDiff &diff, bool add_new_entries = true);

    iterator find(const ZederEntry::Id id);
    const_iterator find(const ZederEntry::Id id) const;
    const_iterator begin() const { return entries_.begin(); }
    const_iterator end() const { return entries_.end(); }

    size_t size() const { return entries_.size(); }
};


inline void ZederConfigData::setModifiedTimestamp(const struct tm &timestamp) {
    std::memcpy(&last_modified_timestamp_, &timestamp, sizeof(timestamp));
}


inline void ZederConfigData::sortEntries() {
    std::sort(entries_.begin(), entries_.end(), [](const ZederEntry &a, const ZederEntry &b) {
        return a.id_ < b.id_;
    });
}


void ZederConfigData::addEntry(const ZederEntry &new_entry, bool sort_after_add) {
    const iterator match(find(new_entry.id_));
    if (unlikely(match != end()))
        LOG_ERROR("Duplicate ID " + std::to_string(new_entry.id_) + "! Existing title: '" + match->title_ + "'");
    else
        entries_.emplace_back(new_entry);

    if (sort_after_add)
        sortEntries();
}


void ZederConfigData::mergeEntry(const ZederConfigDiff &diff, bool add_new_entries) {
    const iterator into(find(diff.id_));
    if (into != end()) {
        if (add_new_entries) {
            addEntry(diff);
            LOG_INFO("New entry " + std::to_string(diff.id_) + " merged into config data");
        } else
            LOG_INFO("New entry " + std::to_string(diff.id_) + " not merged into config data");
    } else {
        const auto time_difference(TimeUtil::DiffStructTm(diff.last_modified_timestamp_, into->last_modified_timestamp_));
        if (time_difference <= 0) {
            LOG_ERROR("The existing entry " + std::to_string(diff.id_) + " is newer than the diff by "
                      + std::to_string(time_difference) + " seconds");
        }

        if (not diff.parent_ppn_.empty()) {
            LOG_INFO("Updating parent PPN: '" + into->parent_ppn_ + "' => '" + diff.parent_ppn_ + "'");
            into->parent_ppn_ = diff.parent_ppn_;
        }

        if (not diff.parent_issn_print_.empty()) {
            LOG_INFO("Updating parent ISSN (print): '" + into->parent_issn_print_ + "' => '" + diff.parent_issn_print_ + "'");
            into->parent_issn_print_ = diff.parent_issn_print_;
        }

        if (not diff.parent_issn_online_.empty()) {
            LOG_INFO("Updating parent ISSN (online): '" + into->parent_issn_online_ + "' => '" + diff.parent_issn_online_ + "'");
            into->parent_issn_online_ = diff.parent_issn_online_;
        }

        if (not diff.primary_url_.empty()) {
            LOG_INFO("Updating primary URL: '" + into->primary_url_ + "' => '" + diff.primary_url_ + "'");
            into->primary_url_ = diff.primary_url_;
        }
    }
}


inline ZederConfigData::iterator ZederConfigData::find(const ZederEntry::Id id) {
    return std::find_if(entries_.begin(), entries_.end(), [id] (const ZederEntry &entry) {
        return entry.id_ == id;
    });
}

inline ZederConfigData::const_iterator ZederConfigData::find(const ZederEntry::Id id) const {
    return std::find_if(entries_.begin(), entries_.end(), [id] (const ZederEntry &entry) {
        return entry.id_ == id;
    });
}


enum Mode { GENERATE, MERGE };

enum ZederColumn { Z, PPPN, EPPN, ISSN, ESSN, TIT, KAT, PRODF, LRT, P_ZOT1, P_ZOT2, B_ZOT, URL1, URL2, MTIME };
enum ZederSpecificConfigKey { ID, MODIFIED_TIME };


const std::map<ZederColumn, std::string> ZEDER_COLUMN_TO_STRING_MAP {
    { Z, "Z"},
    { PPPN, "pppn" },
    { EPPN, "eppn" },
    { ISSN, "issn" },
    { ESSN, "essn" },
    { TIT, "tit" },
    { KAT, "kat" },
    { PRODF, "prodf" },
    { LRT, "lrt" },
    { P_ZOT1, "p_zot1" },
    { P_ZOT2, "p_zot2" },
    { B_ZOT, "b_zot" },
    { URL1, "url1" },
    { URL2, "url2" },
    { MTIME, "Mtime" }
};
const std::map<ZederSpecificConfigKey, std::string> ZEDER_CONFIG_KEY_TO_STRING_MAP {
    { ID, "zeder_id"},
    { MODIFIED_TIME, "zeder_modified_time" }
};


void ParseZederCsv(const std::string &csv_path, ZederConfigData * const zeder_config, bool break_on_error = false) {
    DSVReader reader(csv_path, ',');
    std::vector<std::string> splits;
    size_t line(0);

    while (reader.readLine(&splits)) {
        ++line;
        if (splits.size() != ZEDER_COLUMN_TO_STRING_MAP.size())
            LOG_ERROR("Invalid CSV format in '" + csv_path + "'");
        else if (line == 1) {
            for (size_t i(0); i < splits.size(); ++i) {
                if (ZEDER_COLUMN_TO_STRING_MAP.at(static_cast<ZederColumn>(i)) != splits[i])
                    LOG_ERROR("Invalid data column '" + splits[i] + "' at index " + std::to_string(i));
            }
            continue;
        }

        ZederEntry new_entry;

        for (size_t i(0); i < splits.size(); ++i) {
            auto element(splits[i]);
            const auto column(static_cast<ZederColumn>(i));
            try {
                switch (column) {
                case Z:
                    if (not StringUtil::ToUnsigned(element, &new_entry.id_))
                        throw "Couldn't convert to unsigned";

                    break;
                case PPPN:
                case EPPN: {
                    static constexpr size_t MAX_PPN_LENGTH = 9;

                    if (element.length() > MAX_PPN_LENGTH)
                        throw "Invalid PPN length " + std::to_string(element.length());

                    if (not element.empty())
                        element = StringUtil::PadLeading(element, MAX_PPN_LENGTH, '0');
                    StringUtil::Trim(&element);
                    if (column == PPPN or new_entry.parent_ppn_.empty())
                        new_entry.parent_ppn_ = element;
                    break;
                }
                case ISSN:
                case ESSN: {
                    constexpr size_t ISSN_LENGTH = 9;

                    StringUtil::Trim(&element);
                    if (element.empty() or element == "NV")
                        break;
                    else if (element.length() != ISSN_LENGTH)
                        throw "Invalid ISSN length " + std::to_string(element.length());

                    if (column == ISSN)
                        new_entry.parent_issn_print_ = element;
                    else
                        new_entry.parent_issn_online_ = element;
                    break;
                }
                case TIT:
                    StringUtil::Trim(&element);
                    new_entry.title_ = element;
                    break;
                case KAT:
                    // nothing to do here for the moment
                    break;
                case PRODF:
                    if (zeder_config->getSource() == IXTHEO and element != "zot")
                        throw "Non-zotero entry";
                    break;
                case LRT:
                    if (zeder_config->getSource() == IXTHEO and element.find("RSS.zotero") != std::string::npos)
                        new_entry.has_rss_feed_ = true;
                    break;
                case P_ZOT1:
                    if (zeder_config->getSource() == IXTHEO and element == "z-button2")
                        new_entry.has_multiple_downloads_ = true;
                    else if (zeder_config->getSource() == KRIMDOK)
                        new_entry.has_multiple_downloads_ = true;
                    break;
                case P_ZOT2:
                    new_entry.primary_url_ = element;
                    break;
                case B_ZOT:
                    new_entry.comment_ = element;
                    break;
                case URL1:
                    if (new_entry.primary_url_.empty())
                        new_entry.primary_url_ = element;
                    else
                        new_entry.auxiliary_url_ = element;
                    break;
                case URL2:
                    if (new_entry.auxiliary_url_.empty())
                        new_entry.auxiliary_url_ = element;
                    else
                        LOG_INFO("Discarding URL2 '" + element + "' for entry " + std::to_string(new_entry.id_));
                    break;
                case MTIME:
                    new_entry.last_modified_timestamp_ = TimeUtil::StringToStructTm(element.c_str(),
                                                                                    ZederEntry::MODIFIED_TIMESTAMP_FORMAT_STRING);
                    break;
                default:
                    LOG_ERROR("Unknown data column '" + std::to_string(column) + "'");
                }
            } catch (const std::string &ex) {
                const auto error_msg("Invalid element '" + element + "' for column '" +
                                             ZEDER_COLUMN_TO_STRING_MAP.at(column) + "' at line " + std::to_string(line) +
                                             ": " + ex);
                if (break_on_error)
                    throw std::runtime_error(error_msg);
                else
                    LOG_WARNING(error_msg);
            }
        }

        if (new_entry.primary_url_.empty())
            LOG_ERROR("No URL for entry " + std::to_string(new_entry.id_) + "!");

        zeder_config->addEntry(new_entry);
    }

    zeder_config->sortEntries();
}


void ParseZederIni(const IniFile &ini, ZederConfigData * const zeder_config) {
    if (ini.getSections().empty())
        return;

    zeder_config->setModifiedTimestamp(TimeUtil::StringToStructTm(ini.getString("", ZEDER_CONFIG_KEY_TO_STRING_MAP.at(ZederSpecificConfigKey::ID)),
                                                                        ZederEntry::MODIFIED_TIMESTAMP_FORMAT_STRING));

    std::map<std::string, int> type_string_to_value_map;
    for (const auto &type : Zotero::HARVESTER_TYPE_TO_STRING_MAP)
        type_string_to_value_map[type.second] = type.first;

    for (const auto &section : ini) {
        ZederEntry new_entry;

        new_entry.id_ = section.getUnsigned(ZEDER_CONFIG_KEY_TO_STRING_MAP.at(ZederSpecificConfigKey::ID));
        new_entry.last_modified_timestamp_ = TimeUtil::StringToStructTm(section.getString(ZEDER_CONFIG_KEY_TO_STRING_MAP.at(ZederSpecificConfigKey::ID)),
                                                                        ZederEntry::MODIFIED_TIMESTAMP_FORMAT_STRING);
        new_entry.title_ = section.getSectionName();
        new_entry.parent_issn_print_ = section.getString(HARVESTER_CONFIG_ENTRY_TO_STRING_MAP.at(HarvesterConfigEntry::PARENT_ISSN_PRINT));
        new_entry.parent_issn_online_ = section.getString(HARVESTER_CONFIG_ENTRY_TO_STRING_MAP.at(HarvesterConfigEntry::PARENT_ISSN_ONLINE));
        new_entry.parent_ppn_ = section.getString(HARVESTER_CONFIG_ENTRY_TO_STRING_MAP.at(HarvesterConfigEntry::PARENT_PPN));

        const auto type(static_cast<HarvesterType>(section.getEnum(HARVESTER_CONFIG_ENTRY_TO_STRING_MAP.at(HarvesterConfigEntry::TYPE),
                                                                                            type_string_to_value_map)));
        switch (type) {
        case RSS:
            new_entry.has_rss_feed_= true;
            new_entry.primary_url_ = section.getString(HARVESTER_CONFIG_ENTRY_TO_STRING_MAP.at(HarvesterConfigEntry::FEED));
            break;
        case CRAWL:
            new_entry.has_multiple_downloads_ = true;
            new_entry.primary_url_ = section.getString(HARVESTER_CONFIG_ENTRY_TO_STRING_MAP.at(HarvesterConfigEntry::BASE_URL));
            break;
        case DIRECT:
            new_entry.primary_url_ = section.getString(HARVESTER_CONFIG_ENTRY_TO_STRING_MAP.at(HarvesterConfigEntry::URL));
            break;
        }

        zeder_config->addEntry(new_entry);
    }

    zeder_config->sortEntries();
}


void WriteZederIni(IniFile * const ini, const ZederConfigData &zeder_config) {
    ini->appendSection("");

    char time_buffer[0x50]{};
    strftime(time_buffer, sizeof(time_buffer), ZederEntry::MODIFIED_TIMESTAMP_FORMAT_STRING, &zeder_config.getModifiedTimestamp());
    ini->getSection("")->insert(ZEDER_CONFIG_KEY_TO_STRING_MAP.at(ZederSpecificConfigKey::MODIFIED_TIME), time_buffer);

    // we assume that the entries are sorted at this point
    for (const auto &entry : zeder_config) {
        ini->appendSection(entry.title_);
        auto current_section(ini->getSection(entry.title_));

        current_section->insert(ZEDER_CONFIG_KEY_TO_STRING_MAP.at(ZederSpecificConfigKey::ID), std::to_string(entry.id_));
        strftime(time_buffer, sizeof(time_buffer), ZederEntry::MODIFIED_TIMESTAMP_FORMAT_STRING, &entry.last_modified_timestamp_);
        current_section->insert(ZEDER_CONFIG_KEY_TO_STRING_MAP.at(ZederSpecificConfigKey::MODIFIED_TIME), time_buffer);

        HarvesterType type;
        if (entry.has_rss_feed_)
            type = HarvesterType::RSS;
        else if (entry.has_multiple_downloads_)
            type = HarvesterType::CRAWL;
        else
            type = HarvesterType::DIRECT;
        current_section->insert(HARVESTER_CONFIG_ENTRY_TO_STRING_MAP.at(HarvesterConfigEntry::TYPE), HARVESTER_TYPE_TO_STRING_MAP.at(type));

        switch (zeder_config.getSource()) {
        case IXTHEO:
            current_section->insert(HARVESTER_CONFIG_ENTRY_TO_STRING_MAP.at(HarvesterConfigEntry::GROUP), "IxTheo");
            break;
        case KRIMDOK:
            current_section->insert(HARVESTER_CONFIG_ENTRY_TO_STRING_MAP.at(HarvesterConfigEntry::GROUP), "KrimDok");
            break;
        }

        if (not entry.parent_ppn_.empty())
            current_section->insert(HARVESTER_CONFIG_ENTRY_TO_STRING_MAP.at(HarvesterConfigEntry::PARENT_PPN), entry.parent_ppn_);

        if (not entry.parent_issn_print_.empty())
            current_section->insert(HARVESTER_CONFIG_ENTRY_TO_STRING_MAP.at(HarvesterConfigEntry::PARENT_ISSN_PRINT), entry.parent_issn_print_);

        if (not entry.parent_issn_online_.empty())
            current_section->insert(HARVESTER_CONFIG_ENTRY_TO_STRING_MAP.at(HarvesterConfigEntry::PARENT_ISSN_ONLINE), entry.parent_issn_online_);

        switch (type) {
            case HarvesterType::RSS:
                current_section->insert(HARVESTER_CONFIG_ENTRY_TO_STRING_MAP.at(HarvesterConfigEntry::FEED), entry.primary_url_);
                break;
            case HarvesterType::CRAWL:
                current_section->insert(HARVESTER_CONFIG_ENTRY_TO_STRING_MAP.at(HarvesterConfigEntry::BASE_URL), entry.primary_url_);
                break;
            case HarvesterType::DIRECT:
                current_section->insert(HARVESTER_CONFIG_ENTRY_TO_STRING_MAP.at(HarvesterConfigEntry::URL), entry.primary_url_);
                break;
        }
    }
}


bool DiffZederEntries(const ZederConfigData &old_config,
                      const ZederConfigData &new_config,
                      std::map<ZederEntry::Id, ZederConfigDiff> * const diffs,
                      bool skip_global_timestamp_check = false)
{
    if (not skip_global_timestamp_check) {
        if (TimeUtil::DiffStructTm(new_config.getModifiedTimestamp(), old_config.getModifiedTimestamp()) <= 0)
            return false;
    }

    for (auto &new_entry : new_config) {
        const auto old_entry(old_config.find(new_entry.id_));
        ZederEntry diff;
        if (old_entry != old_config.end()) {
            if (TimeUtil::DiffStructTm(new_entry.last_modified_timestamp_, old_entry->last_modified_timestamp_) <= 0)
                continue;

            if (old_entry->title_ != new_entry.title_) {
                LOG_ERROR("Entry " + std::to_string(old_entry->id_) + "'s title changed unexpectedly! '"
                          + old_entry->title_ + "' => '" + new_entry.title_ + "'");
            }

            if (old_entry->parent_ppn_ != new_entry.parent_ppn_)
                diff.parent_ppn_ = new_entry.parent_ppn_;
            if (old_entry->parent_issn_print_ != new_entry.parent_issn_print_)
                diff.parent_issn_print_ = new_entry.parent_issn_print_;
            if (old_entry->parent_issn_online_ != new_entry.parent_issn_online_)
                diff.parent_issn_online_ = new_entry.parent_issn_online_;
            if (old_entry->primary_url_ != new_entry.primary_url_)
                diff.primary_url_ = new_entry.primary_url_;

            diffs->insert(std::make_pair(new_entry.id_, diff));
        } else
            diffs->insert(std::make_pair(new_entry.id_, new_entry));
    }

    return not diffs->empty();
}


} // unnamed namespace


int Main(int argc, char *argv[]) {
    ::progname = argv[0];

    if (argc < 4)
        Usage();

    Mode current_mode;
    if (StringUtil::StartsWith(argv[1], "--mode=")) {
        const auto mode_string(argv[1] + __builtin_strlen("--mode="));
        if (std::strcmp(mode_string, "generate") == 0)
            current_mode = Mode::GENERATE;
        else if (std::strcmp(mode_string, "merge") == 0)
            current_mode = Mode::MERGE;
        else
            Usage();

        --argc, ++argv;
    } else
        Usage();

    const std::string flavour(argv[1]), input_path(argv[2]), output_path(argv[3]);
    Flavour source(IXTHEO);
    if (flavour == "krimdok")
        source = KRIMDOK;

    switch (current_mode) {
    case Mode::GENERATE: {
        ZederConfigData parsed_config(source);
        IniFile ini(output_path, true, true);

        // is the Zeder last modified timestamp in UTC? let's hope so...
        struct tm *current_time(nullptr);
        time_t now(time(0));
        current_time = ::gmtime(&now);


        ParseZederCsv(input_path, &parsed_config);
        parsed_config.setModifiedTimestamp(*current_time);
        WriteZederIni(&ini, parsed_config);
        ini.write(output_path);

        LOG_INFO("Created " + std::to_string(parsed_config.size()) + " entries");

        break;
    }
    case Mode::MERGE: {
        ZederConfigData old_data(source), new_data(source);
        const IniFile old_ini(input_path);
        IniFile new_ini(output_path, true, true);

        ParseZederIni(old_ini, &old_data);
        ParseZederIni(new_ini, &new_data);

        std::map<ZederEntry::Id, ZederConfigDiff> diffs;
        if (DiffZederEntries(old_data, new_data, &diffs)) {
            for (const auto &entry : diffs)
                new_data.mergeEntry(entry.second);

            new_data.sortEntries();
            WriteZederIni(&new_ini, new_data);
            new_ini.write(output_path);

            LOG_INFO("Modified entries: " + std::to_string(diffs.size()));
        }

        break;
    }
    }


    return EXIT_SUCCESS;
}

/** \file   zeder_importer.cc
 *  \brief  Imports data from Zeder and merges it into zts_harvester config files
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
#include "MiscUtil.h"
#include "StringUtil.h"
#include "TimeUtil.h"
#include "util.h"
#include "Zotero.h"


namespace {


[[noreturn]] void Usage() {
    std::cerr << "Usage: " << ::progname
              << " [--min-log-level=min_verbosity] --mode=tool_mode [--skip-timestamp-check] flavour first_path second_path\n"
              << "Modes:\n"
              << "\t" << "generate:" << "\t"    << "Converts the .csv file exported from Zeder into a zeder_tools generated .conf "                                                  "file. The first path points to the .csv file and the second to the output .conf "                                                "file.\n"
              << "\t" << "diff:"     << "\t\t"  << "Compares the last modified time stamps of entries in a pair of zeder_tools generated "                                           ".conf files. The first path points to the source/updated .conf file and "
                                                   " file and the second to the destination/old .conf.\n"
              << "\t" << "merge:"    << "\t\t"  << "Same as above but additionally merges any changes into the destination/old .conf.\n\n"
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
public:
    ZederEntry(): id_(0), has_rss_feed_(false), has_multiple_downloads_(false), last_modified_timestamp_{} {}
    ZederEntry &operator=(const ZederEntry &rhs);
    void setModifiedTimestamp(const struct tm &timestamp);
};


ZederEntry &ZederEntry::operator=(const ZederEntry &rhs) {
    if (this != &rhs) {
        id_ = rhs.id_;
        parent_ppn_ = rhs.parent_ppn_;
        parent_issn_print_ = rhs.parent_issn_print_;
        parent_issn_online_ = rhs.parent_issn_online_;
        title_ = rhs.title_;
        comment_ = rhs.comment_;
        primary_url_ = rhs.primary_url_;
        auxiliary_url_ = rhs.auxiliary_url_;
        has_rss_feed_ = rhs.has_rss_feed_;
        has_multiple_downloads_ = rhs.has_multiple_downloads_;
        std::memcpy(&this->last_modified_timestamp_, &rhs.last_modified_timestamp_, sizeof(struct tm));
    }
    return *this;
}


inline void ZederEntry::setModifiedTimestamp(const struct tm &timestamp) {
    std::memcpy(&last_modified_timestamp_, &timestamp, sizeof(timestamp));
}


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
    const struct tm &getModifiedTimestamp() const { return last_modified_timestamp_; }
    void setModifiedTimestamp(const struct tm &new_timestamp) {
        std::memcpy(&last_modified_timestamp_, &new_timestamp, sizeof(new_timestamp));
    }

    /* Sorts entries by their Zeder ID */
    void sortEntries();

    /* Adds an entry to the config if it's not already present */
    void addEntry(const ZederEntry &new_entry, const bool sort_after_add = false);

    /* Attempts to merge the changes specified in the diff into the config.
       The ID field of the diff specifies the entry to merge into. If the
       entry doesn't exist and 'add_if_absent' is true, a new entry is created for the ID.

       Returns true if an exisiting entry was modified or a new entry was added.
    */
    bool mergeEntry(const ZederEntry &diff, const bool skip_timestamp_check = false, const bool add_if_absent = true);

    iterator find(const ZederEntry::Id id);
    const_iterator find(const ZederEntry::Id id) const;
    const_iterator begin() const { return entries_.begin(); }
    const_iterator end() const { return entries_.end(); }
    size_t size() const { return entries_.size(); }
};


inline void ZederConfigData::sortEntries() {
    std::sort(entries_.begin(), entries_.end(),
              [](const ZederEntry &a, const ZederEntry &b) { return a.id_ < b.id_; });
}


void ZederConfigData::addEntry(const ZederEntry &new_entry, const bool sort_after_add) {
    const iterator match(find(new_entry.id_));
    if (unlikely(match != end()))
        LOG_ERROR("Duplicate ID " + std::to_string(new_entry.id_) + "! Existing title: '" + match->title_ + "'");
    else
        entries_.emplace_back(new_entry);

    if (sort_after_add)
        sortEntries();
}


bool ZederConfigData::mergeEntry(const ZederEntry &diff, const bool skip_timestamp_check, const bool add_if_absent) {
    const iterator into(find(diff.id_));
    bool modified(false);

    if (into == end()) {
        if (add_if_absent) {
            addEntry(diff);
            modified = true;
            LOG_INFO("New entry " + std::to_string(diff.id_) + " merged into config data");
        } else
            LOG_INFO("New entry " + std::to_string(diff.id_) + " not merged into config data");
    } else {
        if (not skip_timestamp_check) {
            const auto time_difference(TimeUtil::DiffStructTm(diff.last_modified_timestamp_, into->last_modified_timestamp_));
            if (time_difference <= 0) {
                LOG_ERROR("The existing entry " + std::to_string(diff.id_) + " is newer than the diff by "
                        + std::to_string(time_difference) + " seconds");
            }
        }

        into->setModifiedTimestamp(diff.last_modified_timestamp_);
        if (not diff.parent_ppn_.empty()) {
            LOG_INFO("Updating parent PPN: '" + into->parent_ppn_ + "' => '" + diff.parent_ppn_ + "'");
            into->parent_ppn_ = diff.parent_ppn_;
            modified = true;
        }

        if (not diff.parent_issn_print_.empty()) {
            LOG_INFO("Updating parent ISSN (print): '" + into->parent_issn_print_ + "' => '" + diff.parent_issn_print_ + "'");
            into->parent_issn_print_ = diff.parent_issn_print_;
            modified = true;
        }

        if (not diff.parent_issn_online_.empty()) {
            LOG_INFO("Updating parent ISSN (online): '" + into->parent_issn_online_ + "' => '" + diff.parent_issn_online_ + "'");
            into->parent_issn_online_ = diff.parent_issn_online_;
            modified = true;
        }

        if (not diff.primary_url_.empty()) {
            LOG_INFO("Updating primary URL: '" + into->primary_url_ + "' => '" + diff.primary_url_ + "'");
            into->primary_url_ = diff.primary_url_;
            modified = true;
        }

        if (not diff.auxiliary_url_.empty()) {
            LOG_INFO("Updating auxiliary URL: '" + into->auxiliary_url_ + "' => '" + diff.auxiliary_url_ + "'");
            into->auxiliary_url_ = diff.auxiliary_url_;
            modified = true;
        }

        if (not diff.comment_.empty()) {
            LOG_INFO("Updating comment: '" + into->comment_ + "' => '" + diff.comment_ + "'");
            into->comment_ = diff.comment_;
            modified = true;
        }
    }

    return modified;
}


inline ZederConfigData::iterator ZederConfigData::find(const ZederEntry::Id id) {
    return std::find_if(entries_.begin(), entries_.end(),
                        [id] (const ZederEntry &entry) { return entry.id_ == id; });
}

inline ZederConfigData::const_iterator ZederConfigData::find(const ZederEntry::Id id) const {
    return std::find_if(entries_.begin(), entries_.end(),
                        [id] (const ZederEntry &entry) { return entry.id_ == id; });
}


enum Mode { GENERATE, DIFF, MERGE };

enum ZederColumn { Z, PPPN, EPPN, ISSN, ESSN, TIT, KAT, PRODF, LRT, P_ZOT1, P_ZOT2, B_ZOT, URL1, URL2, MTIME };
enum ZederSpecificConfigKey { ID, MODIFIED_TIME, COMMENT };


const std::map<ZederColumn, std::string> ZEDER_COLUMN_TO_STRING_MAP {
    { Z,        "Z"      },
    { PPPN,     "pppn"   },
    { EPPN,     "eppn"   },
    { ISSN,     "issn"   },
    { ESSN,     "essn"   },
    { TIT,      "tit"    },
    { KAT,      "kat"    },
    { PRODF,    "prodf"  },
    { LRT,      "lrt"    },
    { P_ZOT1,   "p_zot1" },
    { P_ZOT2,   "p_zot2" },
    { B_ZOT,    "b_zot"  },
    { URL1,     "url1"   },
    { URL2,     "url2"   },
    { MTIME,    "Mtime"  }
};
const std::map<ZederSpecificConfigKey, std::string> ZEDER_CONFIG_KEY_TO_STRING_MAP {
    { ID,            "zeder_id"            },
    { MODIFIED_TIME, "zeder_modified_time" },
    { COMMENT,       "zeder_comment"       }
};


void GetCurrentTimeGMT(struct tm * const tm) {
    struct tm *current_time(nullptr);
    time_t now(time(0));
    current_time = ::gmtime(&now);
    std::memcpy(tm, current_time, sizeof(struct tm));
}


void ParseZederCsv(const std::string &csv_path, ZederConfigData * const zeder_config, const bool break_on_error = false) {
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
                    if (element.empty() or element == "NV")
                        break;
                    else if (not MiscUtil::IsValidPPN(element))
                        throw "Invalid PPN";

                    if (column == PPPN or new_entry.parent_ppn_.empty())
                        new_entry.parent_ppn_ = element;
                    break;
                }
                case ISSN:
                case ESSN: {
                    StringUtil::Trim(&element);
                    if (element.empty() or element == "NV")
                        break;
                    else if (not MiscUtil::IsPossibleISSN(element))
                        throw "Invalid ISSN";

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
                    if (element.find("RSS.zotero") != std::string::npos)
                        new_entry.has_rss_feed_ = true;
                    break;
                case P_ZOT1:
                    if (element.empty())
                        break;

                    if (zeder_config->getSource() == IXTHEO and element == "z-button2")
                        new_entry.has_multiple_downloads_ = true;
                    break;
                case P_ZOT2:
                    new_entry.primary_url_ = element;
                    break;
                case B_ZOT:
                    new_entry.comment_ = element;
                    break;
                case URL1:
                    if (element.empty())
                        break;

                    if (new_entry.primary_url_.empty())
                        new_entry.primary_url_ = element;
                    else
                        new_entry.auxiliary_url_ = element;
                    break;
                case URL2:
                    if (element.empty())
                        break;

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
            } catch (const char *ex) {
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
            LOG_WARNING("No URL for entry " + std::to_string(new_entry.id_) + "! Skipping...");
        else
            zeder_config->addEntry(new_entry);
    }

    zeder_config->sortEntries();
}


void ParseZederIni(const IniFile &ini, ZederConfigData * const zeder_config) {
    if (ini.getSections().empty())
        return;

    zeder_config->setModifiedTimestamp(TimeUtil::StringToStructTm(ini.getString("", ZEDER_CONFIG_KEY_TO_STRING_MAP.at(ZederSpecificConfigKey::MODIFIED_TIME)),
                                                                        ZederEntry::MODIFIED_TIMESTAMP_FORMAT_STRING));

    std::map<std::string, int> type_string_to_value_map;
    std::vector<std::string> groups;
    for (const auto &type : Zotero::HARVESTER_TYPE_TO_STRING_MAP)
        type_string_to_value_map[type.second] = type.first;

    StringUtil::Split(ini.getString("", Zotero::HARVESTER_CONFIG_ENTRY_TO_STRING_MAP.at(Zotero::HarvesterConfigEntry::GROUP) + "s", ""),
                     ',', &groups);

    for (const auto &section : ini) {
        const auto section_name(section.getSectionName());
        if (section_name.empty())
            continue;
        else if (std::find(groups.begin(), groups.end(), section_name) != groups.end())
            continue;   // skip the sections pertain to groups
        else if (section.getString(ZEDER_CONFIG_KEY_TO_STRING_MAP.at(ZederSpecificConfigKey::ID), "").empty()) {
            LOG_WARNING("Entry '" + section_name + "' has no Zeder ID. Skipping...");
            continue;
        }

        ZederEntry new_entry;

        new_entry.id_ = section.getUnsigned(ZEDER_CONFIG_KEY_TO_STRING_MAP.at(ZederSpecificConfigKey::ID));
        new_entry.last_modified_timestamp_ = TimeUtil::StringToStructTm(section.getString(ZEDER_CONFIG_KEY_TO_STRING_MAP.at(ZederSpecificConfigKey::MODIFIED_TIME)),
                                                                        ZederEntry::MODIFIED_TIMESTAMP_FORMAT_STRING);
        new_entry.comment_ = section.getString(section.getString(ZEDER_CONFIG_KEY_TO_STRING_MAP.at(ZederSpecificConfigKey::COMMENT)));
        new_entry.title_ = section_name;
        new_entry.parent_issn_print_ = section.getString(Zotero::HARVESTER_CONFIG_ENTRY_TO_STRING_MAP.at(Zotero::HarvesterConfigEntry::PARENT_ISSN_PRINT), "");
        new_entry.parent_issn_online_ = section.getString(Zotero::HARVESTER_CONFIG_ENTRY_TO_STRING_MAP.at(Zotero::HarvesterConfigEntry::PARENT_ISSN_ONLINE), "");
        new_entry.parent_ppn_ = section.getString(Zotero::HARVESTER_CONFIG_ENTRY_TO_STRING_MAP.at(Zotero::HarvesterConfigEntry::PARENT_PPN), "");

        const auto type(static_cast<Zotero::HarvesterType>(section.getEnum(Zotero::HARVESTER_CONFIG_ENTRY_TO_STRING_MAP.at(Zotero::HarvesterConfigEntry::TYPE),
                                                                                            type_string_to_value_map)));
        switch (type) {
        case Zotero::HarvesterType::RSS:
            new_entry.has_rss_feed_= true;
            new_entry.primary_url_ = section.getString(Zotero::HARVESTER_CONFIG_ENTRY_TO_STRING_MAP.at(Zotero::HarvesterConfigEntry::FEED));
            break;
        case Zotero::HarvesterType::CRAWL:
            new_entry.has_multiple_downloads_ = true;
            new_entry.primary_url_ = section.getString(Zotero::HARVESTER_CONFIG_ENTRY_TO_STRING_MAP.at(Zotero::HarvesterConfigEntry::BASE_URL));
            break;
        case Zotero::HarvesterType::DIRECT:
            new_entry.primary_url_ = section.getString(Zotero::HARVESTER_CONFIG_ENTRY_TO_STRING_MAP.at(Zotero::HarvesterConfigEntry::URL));
            break;
        }

        zeder_config->addEntry(new_entry);
    }

    zeder_config->sortEntries();
}


void WriteZederIni(IniFile * const ini, const ZederConfigData &zeder_config) {
    ini->appendSection("");

    char time_buffer[0x50]{};
    std::strftime(time_buffer, sizeof(time_buffer), ZederEntry::MODIFIED_TIMESTAMP_FORMAT_STRING, &zeder_config.getModifiedTimestamp());
    ini->getSection("")->insert(ZEDER_CONFIG_KEY_TO_STRING_MAP.at(ZederSpecificConfigKey::MODIFIED_TIME), time_buffer, "",
                                IniFile::Section::DupeInsertionBehaviour::OVERWRITE_EXISTING_VALUE);

    // we assume that the entries are sorted at this point
    for (const auto &entry : zeder_config) {
        ini->appendSection(entry.title_);
        auto current_section(ini->getSection(entry.title_));

        current_section->insert(ZEDER_CONFIG_KEY_TO_STRING_MAP.at(ZederSpecificConfigKey::ID), std::to_string(entry.id_), "",
                                IniFile::Section::DupeInsertionBehaviour::OVERWRITE_EXISTING_VALUE);
        std::strftime(time_buffer, sizeof(time_buffer), ZederEntry::MODIFIED_TIMESTAMP_FORMAT_STRING, &entry.last_modified_timestamp_);
        current_section->insert(ZEDER_CONFIG_KEY_TO_STRING_MAP.at(ZederSpecificConfigKey::MODIFIED_TIME), time_buffer, "",
                                IniFile::Section::DupeInsertionBehaviour::OVERWRITE_EXISTING_VALUE);
        if (not entry.comment_.empty()) {
            current_section->insert(ZEDER_CONFIG_KEY_TO_STRING_MAP.at(ZederSpecificConfigKey::COMMENT), entry.comment_, "",
                                IniFile::Section::DupeInsertionBehaviour::OVERWRITE_EXISTING_VALUE);
        }

        Zotero::HarvesterType type;
        if (entry.has_rss_feed_)
            type = Zotero::HarvesterType::RSS;
        else if (entry.has_multiple_downloads_)
            type = Zotero::HarvesterType::CRAWL;
        else
            type = Zotero::HarvesterType::DIRECT;
        current_section->insert(Zotero::HARVESTER_CONFIG_ENTRY_TO_STRING_MAP.at(Zotero::HarvesterConfigEntry::TYPE), Zotero::HARVESTER_TYPE_TO_STRING_MAP.at(type), "",
                                IniFile::Section::DupeInsertionBehaviour::OVERWRITE_EXISTING_VALUE);

        switch (zeder_config.getSource()) {
        case IXTHEO:
            current_section->insert(Zotero::HARVESTER_CONFIG_ENTRY_TO_STRING_MAP.at(Zotero::HarvesterConfigEntry::GROUP), "IxTheo", "",
                                IniFile::Section::DupeInsertionBehaviour::OVERWRITE_EXISTING_VALUE);
            break;
        case KRIMDOK:
            current_section->insert(Zotero::HARVESTER_CONFIG_ENTRY_TO_STRING_MAP.at(Zotero::HarvesterConfigEntry::GROUP), "KrimDok", "",
                                IniFile::Section::DupeInsertionBehaviour::OVERWRITE_EXISTING_VALUE);
            break;
        }

        if (not entry.parent_ppn_.empty()) {
            current_section->insert(Zotero::HARVESTER_CONFIG_ENTRY_TO_STRING_MAP.at(Zotero::HarvesterConfigEntry::PARENT_PPN),
                                    entry.parent_ppn_, "", IniFile::Section::DupeInsertionBehaviour::OVERWRITE_EXISTING_VALUE);
        }

        if (not entry.parent_issn_print_.empty()) {
            current_section->insert(Zotero::HARVESTER_CONFIG_ENTRY_TO_STRING_MAP.at(Zotero::HarvesterConfigEntry::PARENT_ISSN_PRINT),
                                    entry.parent_issn_print_, "", IniFile::Section::DupeInsertionBehaviour::OVERWRITE_EXISTING_VALUE);
        }

        if (not entry.parent_issn_online_.empty()) {
            current_section->insert(Zotero::HARVESTER_CONFIG_ENTRY_TO_STRING_MAP.at(Zotero::HarvesterConfigEntry::PARENT_ISSN_ONLINE),
                                    entry.parent_issn_online_, "", IniFile::Section::DupeInsertionBehaviour::OVERWRITE_EXISTING_VALUE);
        }

        switch (type) {
        case Zotero::HarvesterType::RSS:
            current_section->insert(Zotero::HARVESTER_CONFIG_ENTRY_TO_STRING_MAP.at(Zotero::HarvesterConfigEntry::FEED), entry.primary_url_, "",
                            IniFile::Section::DupeInsertionBehaviour::OVERWRITE_EXISTING_VALUE);
            break;
        case Zotero::HarvesterType::CRAWL: {
            current_section->insert(Zotero::HARVESTER_CONFIG_ENTRY_TO_STRING_MAP.at(Zotero::HarvesterConfigEntry::BASE_URL), entry.primary_url_, "",
                            IniFile::Section::DupeInsertionBehaviour::OVERWRITE_EXISTING_VALUE);

            // insert other required keys if not present
            std::string temp_buffer;
            if (not current_section->lookup(Zotero::HARVESTER_CONFIG_ENTRY_TO_STRING_MAP.at(Zotero::HarvesterConfigEntry::MAX_CRAWL_DEPTH), &temp_buffer))
                current_section->insert(Zotero::HARVESTER_CONFIG_ENTRY_TO_STRING_MAP.at(Zotero::HarvesterConfigEntry::MAX_CRAWL_DEPTH), "1", "");

            if (not current_section->lookup(Zotero::HARVESTER_CONFIG_ENTRY_TO_STRING_MAP.at(Zotero::HarvesterConfigEntry::EXTRACTION_REGEX), &temp_buffer))
                current_section->insert(Zotero::HARVESTER_CONFIG_ENTRY_TO_STRING_MAP.at(Zotero::HarvesterConfigEntry::EXTRACTION_REGEX), "", "");

            break;
        }
        case Zotero::HarvesterType::DIRECT:
            current_section->insert(Zotero::HARVESTER_CONFIG_ENTRY_TO_STRING_MAP.at(Zotero::HarvesterConfigEntry::URL), entry.primary_url_, "",
                            IniFile::Section::DupeInsertionBehaviour::OVERWRITE_EXISTING_VALUE);
            break;
        }

        current_section->insert(Zotero::HARVESTER_CONFIG_ENTRY_TO_STRING_MAP.at(Zotero::HarvesterConfigEntry::STRPTIME_FORMAT), "", "",
                            IniFile::Section::DupeInsertionBehaviour::OVERWRITE_EXISTING_VALUE);
    }
}


bool DiffZederEntries(const ZederConfigData &old_config, const ZederConfigData &new_config,
                      std::map<ZederEntry::Id, ZederEntry> * const diffs,
                      const struct tm &current_time,
                      const bool skip_timestamp_check = false)
{
    if (not skip_timestamp_check and TimeUtil::DiffStructTm(new_config.getModifiedTimestamp(),
        old_config.getModifiedTimestamp()) <= 0)
    {
        return false;
    }

    for (const auto &new_entry : new_config) {
        const auto old_entry(old_config.find(new_entry.id_));
        if (old_entry != old_config.end()) {
            if (not skip_timestamp_check and TimeUtil::DiffStructTm(new_entry.last_modified_timestamp_,
                old_entry->last_modified_timestamp_) <= 0)
            {
                continue;
            }

            // copy immutable fields from the older entry
            ZederEntry diff = *old_entry;
            diff.setModifiedTimestamp(skip_timestamp_check ? current_time : new_entry.last_modified_timestamp_);
            diff.parent_ppn_ = diff.parent_issn_print_ = diff.parent_issn_online_ = diff.primary_url_ = "";

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
    if (argc < 5)
        Usage();

    Mode current_mode;
    if (StringUtil::StartsWith(argv[1], "--mode=")) {
        const auto mode_string(argv[1] + __builtin_strlen("--mode="));
        if (std::strcmp(mode_string, "generate") == 0)
            current_mode = Mode::GENERATE;
        else if (std::strcmp(mode_string, "diff") == 0)
            current_mode = Mode::DIFF;
        else if (std::strcmp(mode_string, "merge") == 0)
            current_mode = Mode::MERGE;
        else
            Usage();

        --argc, ++argv;
    } else
        Usage();

    bool skip_timestamp_check(false);
    if (std::strcmp(argv[1], "--skip-timestamp-check") == 0) {
        skip_timestamp_check = true;
        --argc, ++argv;
    }

    if (argc != 4)
        Usage();

    const std::string flavour(argv[1]), first_path(argv[2]), second_path(argv[3]);
    Flavour source(IXTHEO);
    if (flavour == "krimdok")
        source = KRIMDOK;

    // is the Zeder last modified timestamp in UTC? let's hope so...
    struct tm current_time{};
    GetCurrentTimeGMT(&current_time);

    switch (current_mode) {
    case Mode::GENERATE: {
        ZederConfigData parsed_config(source);
        IniFile ini(second_path, true, true);

        ParseZederCsv(first_path, &parsed_config);
        parsed_config.setModifiedTimestamp(current_time);
        WriteZederIni(&ini, parsed_config);
        ini.write(second_path);

        LOG_INFO("Created " + std::to_string(parsed_config.size()) + " entries");

        break;
    }
    case Mode::DIFF:
    case Mode::MERGE: {
        ZederConfigData old_data(source), new_data(source);
        IniFile updated_ini(first_path), old_ini(second_path);

        ParseZederIni(old_ini, &old_data);
        ParseZederIni(updated_ini, &new_data);

        std::map<ZederEntry::Id, ZederEntry> diffs;
        if (DiffZederEntries(old_data, new_data, &diffs, current_time, skip_timestamp_check)) {
            for (const auto &entry : diffs) {
                LOG_INFO("Differing entry " + std::to_string(entry.first) + "...");
                old_data.mergeEntry(entry.second, skip_timestamp_check);
            }

            if (current_mode == Mode::MERGE) {
                old_data.sortEntries();
                old_data.setModifiedTimestamp(current_time);
                WriteZederIni(&old_ini, old_data);
                old_ini.write(second_path);
            }

            LOG_INFO("Modified entries: " + std::to_string(diffs.size()));
        }

        break;
    }
    }

    return EXIT_SUCCESS;
}

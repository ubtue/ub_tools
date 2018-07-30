/** \file   zeder_to_zotero_importer.cc
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
#include "Zeder.h"
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


enum Flavour { IXTHEO, KRIMDOK };

const std::unordered_map<Flavour, std::string> FLAVOUR_TO_STRING_MAP{
    { IXTHEO,   "IxTheo"    },
    { KRIMDOK,  "KrimDok"   }
};

enum Mode { GENERATE, DIFF, MERGE };

// Fields exported to the zts_harvester config file.
enum ExportField {
    TITLE,
    ZEDER_ID, ZEDER_MODIFIED_TIMESTAMP, ZEDER_COMMENT,
    TYPE, GROUP,
    PARENT_PPN, PARENT_ISSN_PRINT, PARENT_ISSN_ONLINE,
    ENTRY_POINT_URL, STRPTIME_FORMAT,
    EXTRACTION_REGEX, MAX_CRAWL_DEPTH
};


class ExportFieldNameResolver {
    std::unordered_map<ExportField, std::string> attribute_names_;
    std::unordered_map<ExportField, std::string> ini_entry_names_;
    std::unordered_map<std::string, std::string> ini_entry_name_to_attribute_name_map_;
public:
    ExportFieldNameResolver();

    const std::string &getAttributeName(ExportField field) const { return attribute_names_.at(field); }
    std::vector<std::string> getAllValidAttributeNames() const;
    const std::string &getIniEntryName(ExportField field) const;
    const std::string &getIniEntryName(ExportField field, Zotero::HarvesterType harvester_type) const;
    const std::unordered_map<std::string, std::string> getIniEntryNameToAttributeNameMap() const { return ini_entry_name_to_attribute_name_map_; }  // only includes the entries that are relevant, i.e., those that are stored as attributes in the Zeder Entry class
};


ExportFieldNameResolver::ExportFieldNameResolver(): attribute_names_{
    { TITLE,                    "zts_title"                 },
    { ZEDER_ID,                 "" /* unused */             },      // stored directly in the Entry class
    { ZEDER_MODIFIED_TIMESTAMP, "" /* unused */             },      // same as above
    { ZEDER_COMMENT,            "zts_zeder_comment"         },
    { TYPE,                     "zts_type"                  },
    { GROUP,                    "zts_group"                 },
    { PARENT_PPN,               "zts_parent_ppn"            },
    { PARENT_ISSN_PRINT,        "zts_parent_issn_print"     },
    { PARENT_ISSN_ONLINE,       "zts_parent_issn_online"    },
    { ENTRY_POINT_URL,          "zts_entry_point_url"       },
    { STRPTIME_FORMAT,          "" /* unused */             },
    { EXTRACTION_REGEX,         "" /* unused */             },
    { MAX_CRAWL_DEPTH,          "" /* unused */             },
}, ini_entry_names_{
    { TITLE,                    "" /* exported as the section name */   },
    { ZEDER_ID,                 "zeder_id"                              },
    { ZEDER_MODIFIED_TIMESTAMP, "zeder_modified_time"                   },
    { ZEDER_COMMENT,            "zeder_comment"                         },
    { TYPE,                     Zotero::HARVESTER_CONFIG_ENTRY_TO_STRING_MAP.at(Zotero::HarvesterConfigEntry::TYPE)                 },
    { GROUP,                    Zotero::HARVESTER_CONFIG_ENTRY_TO_STRING_MAP.at(Zotero::HarvesterConfigEntry::GROUP)                },
    { PARENT_PPN,               Zotero::HARVESTER_CONFIG_ENTRY_TO_STRING_MAP.at(Zotero::HarvesterConfigEntry::PARENT_PPN)           },
    { PARENT_ISSN_PRINT,        Zotero::HARVESTER_CONFIG_ENTRY_TO_STRING_MAP.at(Zotero::HarvesterConfigEntry::PARENT_ISSN_PRINT)    },
    { PARENT_ISSN_ONLINE,       Zotero::HARVESTER_CONFIG_ENTRY_TO_STRING_MAP.at(Zotero::HarvesterConfigEntry::PARENT_ISSN_ONLINE)   },
    { ENTRY_POINT_URL,          "" /* has multiple entries, resolved directly in the member function */                             },
    { STRPTIME_FORMAT,          "strptime_format"                       },
    { EXTRACTION_REGEX,         "extraction_regex"                      },
    { MAX_CRAWL_DEPTH,          "max_crawl_depth"                       },
}, ini_entry_name_to_attribute_name_map_{
    { Zotero::HARVESTER_CONFIG_ENTRY_TO_STRING_MAP.at(Zotero::HarvesterConfigEntry::URL),      attribute_names_.at(ENTRY_POINT_URL) },
    { Zotero::HARVESTER_CONFIG_ENTRY_TO_STRING_MAP.at(Zotero::HarvesterConfigEntry::BASE_URL), attribute_names_.at(ENTRY_POINT_URL) },
    { Zotero::HARVESTER_CONFIG_ENTRY_TO_STRING_MAP.at(Zotero::HarvesterConfigEntry::FEED),     attribute_names_.at(ENTRY_POINT_URL) },
}
{
    for (const auto &field_name_pair : ini_entry_names_) {
        switch (field_name_pair.first) {
        case TITLE:
        case ZEDER_ID:
        case ZEDER_MODIFIED_TIMESTAMP:
        case ENTRY_POINT_URL:
        case STRPTIME_FORMAT:
        case EXTRACTION_REGEX:
        case MAX_CRAWL_DEPTH:
            break;
        default:
            ini_entry_name_to_attribute_name_map_[field_name_pair.second] = attribute_names_[field_name_pair.first];
        }
    }
}


std::vector<std::string> ExportFieldNameResolver::getAllValidAttributeNames() const {
    std::vector<std::string> valid_attribute_names;
    for (const auto &field_name_pair : attribute_names_)
        valid_attribute_names.push_back(field_name_pair.second);
    return valid_attribute_names;
}


const std::string &ExportFieldNameResolver::getIniEntryName(ExportField field) const {
    if (field == ENTRY_POINT_URL)
        LOG_ERROR("Cannot resolve INI name for field '" + std::to_string(field) + "' without flavour data");
    else
        return ini_entry_names_.at(field);
}


const std::string &ExportFieldNameResolver::getIniEntryName(ExportField field, Zotero::HarvesterType harvester_type) const {
    if (field == ENTRY_POINT_URL) {
        switch (harvester_type) {
        case Zotero::HarvesterType::DIRECT:
            return Zotero::HARVESTER_CONFIG_ENTRY_TO_STRING_MAP.at(Zotero::HarvesterConfigEntry::URL);
        case Zotero::HarvesterType::RSS:
            return Zotero::HARVESTER_CONFIG_ENTRY_TO_STRING_MAP.at(Zotero::HarvesterConfigEntry::FEED);
        case Zotero::HarvesterType::CRAWL:
            return Zotero::HARVESTER_CONFIG_ENTRY_TO_STRING_MAP.at(Zotero::HarvesterConfigEntry::BASE_URL);
        }
    } else
        return ini_entry_names_.at(field);
}


struct ImporterParams {
    std::vector<std::string> url_field_priority_;   // highest to lowest

    static void Load(Flavour flavour, const IniFile &config, ImporterParams * const params);
};


void ImporterParams::Load(Flavour flavour, const IniFile &config, ImporterParams * const params) {
    const auto section(config.getSection(FLAVOUR_TO_STRING_MAP.at(flavour)));

    const auto url_field_priority(section->getString("url_field_priority"));
    if (StringUtil::Split(url_field_priority, ",", &params->url_field_priority_) == 0)
        LOG_ERROR("Invalid URL field priority for flavour " + std::to_string(flavour) + " in '" + config.getFilename() + "'");
}


bool PostProcessZederImportedEntry(Flavour flavour, const ImporterParams &params, const ExportFieldNameResolver &name_resolver,
                                   Zeder::Entry * const entry, bool ignore_invalid_ppn_issn = true)
{
    bool valid(true);

    const auto &pppn(entry->getAttribute("pppn")), &eppn(entry->getAttribute("eppn"));
    if (pppn.empty() or pppn == "NV" or not MiscUtil::IsValidPPN(pppn)) {
        if (eppn.empty() or eppn == "NV" or not MiscUtil::IsValidPPN(eppn)) {
            LOG_WARNING("Entry " + std::to_string(entry->getId()) + " | No valid PPPN found");
            valid = ignore_invalid_ppn_issn ? valid : false;
        } else
            entry->setAttribute(name_resolver.getAttributeName(PARENT_PPN), eppn);
    } else
        entry->setAttribute(name_resolver.getAttributeName(PARENT_PPN), pppn);

    const auto &issn_print(entry->getAttribute("issn")), &issn_online(entry->getAttribute("essn"));
    if (issn_print.empty() or issn_print == "NV")
        ;// skip
    else if (not MiscUtil::IsPossibleISSN(issn_print)) {
        LOG_WARNING("Entry " + std::to_string(entry->getId()) + " | Invalid print ISSN '" + issn_print + "'");
        valid = ignore_invalid_ppn_issn ? valid : false;
    } else
        entry->setAttribute(name_resolver.getAttributeName(PARENT_ISSN_PRINT), issn_print);

    if (issn_online.empty() or issn_online == "NV")
        ;// skip
    else if (not MiscUtil::IsPossibleISSN(issn_online)) {
        LOG_WARNING("Entry " + std::to_string(entry->getId()) + " | Invalid online ISSN '" + issn_online + "'");
        valid = ignore_invalid_ppn_issn ? valid : false;
    } else
        entry->setAttribute(name_resolver.getAttributeName(PARENT_ISSN_ONLINE), issn_online);

    auto title(entry->getAttribute("tit"));
    StringUtil::Trim(&title);
    entry->setAttribute(name_resolver.getAttributeName(TITLE), title);

    Zotero::HarvesterType harvester_type(Zotero::HarvesterType::CRAWL);
    if (flavour == IXTHEO and entry->getAttribute("prodf") != "zot") {
        LOG_WARNING("Entry " + std::to_string(entry->getId()) + " | Not a Zotero entry");
        valid = false;
    }

    if (entry->hasAttribute("rss") or entry->getAttribute("lrt").find("RSS.zotero") != std::string::npos)
        harvester_type = Zotero::HarvesterType::RSS;

    entry->setAttribute(name_resolver.getAttributeName(TYPE), Zotero::HARVESTER_TYPE_TO_STRING_MAP.at(harvester_type));
    entry->setAttribute(name_resolver.getAttributeName(GROUP), FLAVOUR_TO_STRING_MAP.at(flavour));
    entry->setAttribute(name_resolver.getAttributeName(ZEDER_COMMENT), entry->getAttribute("b_zot"));

    // resolve URL based on the importer's config
    std::string resolved_url;
    for (const auto &url_field : params.url_field_priority_) {
        if (entry->hasAttribute(url_field)) {
            const auto &imported_url(entry->getAttribute(url_field));
            if (not resolved_url.empty()) {
                LOG_INFO("Entry " + std::to_string(entry->getId()) + " | Discarding URL '" +
                         imported_url + "' in field '" + url_field + "'");
            } else if (not imported_url.empty())
                resolved_url = imported_url;
        }
    }

    if (resolved_url.empty()) {
        LOG_WARNING("Entry " + std::to_string(entry->getId()) + " | Couldn't resolve harvest URL");
        valid = false;
    } else
        entry->setAttribute(name_resolver.getAttributeName(ENTRY_POINT_URL), resolved_url);

    // remove the original attributes
    entry->keepAttributes(name_resolver.getAllValidAttributeNames());

    return valid;
}


void ParseZederIni(const IniFile &ini, const ExportFieldNameResolver &name_resolver,
                   Zeder::EntryCollection * const zeder_config) {
    if (ini.getSections().empty())
        return;

    // select the sections that are Zeder-compatible, i.e., that were exported by this tool
    std::vector<std::string> valid_section_names, groups;
    StringUtil::Split(ini.getString("", Zotero::HARVESTER_CONFIG_ENTRY_TO_STRING_MAP.at(Zotero::HarvesterConfigEntry::GROUP) + "s", ""),
                     ',', &groups);

    for (const auto &section : ini) {
        const auto section_name(section.getSectionName());
        if (section_name.empty())
            continue;
        else if (std::find(groups.begin(), groups.end(), section_name) != groups.end())
            continue;   // skip the sections pertain to groups
        else if (section.getString(name_resolver.getIniEntryName(ZEDER_ID), "").empty()) {
            LOG_WARNING("Entry '" + section_name + "' has no Zeder ID. Skipping...");
            continue;
        }

        valid_section_names.push_back(section_name);
    }

    // we don't need to peform any validation on the entries read-in from an INI file
    static const std::function<bool(Zeder::Entry * const)> postprocessor([](Zeder::Entry * const) -> bool { return true; });
    std::unique_ptr<Zeder::IniReader::InputParams> parser_params(new Zeder::IniReader::InputParams(ini.getFilename(), postprocessor,
                                                                 valid_section_names, name_resolver.getAttributeName(TITLE), name_resolver.getIniEntryNameToAttributeNameMap()));

    auto parser(Zeder::Importer::Factory(std::move(parser_params)));
    parser->parse(zeder_config);
}


void WriteZederIni(IniFile * const ini, const Zeder::EntryCollection &zeder_config) {
    char time_buffer[0x50]{};

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

/*
### chec kif the type has changed
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
*/

void GetCurrentTimeGMT(struct tm * const tm) {
    struct tm *current_time(nullptr);
    time_t now(time(0));
    current_time = ::gmtime(&now);
    std::memcpy(tm, current_time, sizeof(struct tm));
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

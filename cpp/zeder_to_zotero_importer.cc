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
              << " [--min-log-level=min_verbosity] --mode=tool_mode [--skip-timestamp-check] flavour config_file first_path second_path"
                 " [entry_ids]\n"
              << "Modes:\n"
              << "\t" << "generate:" << "\t"    << "Converts the .csv file exported from Zeder into a zeder_to_zotero_importer generated .conf"                                      "file. The first path points to the .csv file and the second to the output .conf file.\n"
              << "\t" << "diff:"     << "\t\t"  << "Compares the last modified time stamps of entries in a pair of zeder_to_zotero_importer"                                         "generated .conf files. The first path points to the source/updated .conf file and "
                                                   " file and the second to the destination/old .conf.\n"
              << "\t" << "merge:"    << "\t\t"  << "Same as above but additionally merges any changes into the destination/old .conf.\n\n"
              << "Flavour: Either 'ixtheo' or 'krimdok'.\n"
              << "Entry IDs: Comma-seperated list of entries IDs to process. All other entries will be ignored.\n\n";
    std::exit(EXIT_FAILURE);
}


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
    std::unordered_map<ExportField, std::string> ini_keys_;
public:
    ExportFieldNameResolver();

    const std::string &getAttributeName(ExportField field) const { return attribute_names_.at(field); }
    std::vector<std::string> getAllValidAttributeNames() const;
    const std::string &getIniKey(ExportField field) const;
    const std::string &getIniKey(ExportField field, Zotero::HarvesterType harvester_type) const;
    std::pair<std::string, std::string> getIniKeyAttributeNamePair(ExportField field) const;
    std::pair<std::string, std::string> getAttributeNameIniKeyPair(ExportField field) const;
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
}, ini_keys_{
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
    { STRPTIME_FORMAT,          Zotero::HARVESTER_CONFIG_ENTRY_TO_STRING_MAP.at(Zotero::HarvesterConfigEntry::STRPTIME_FORMAT)      },
    { EXTRACTION_REGEX,         Zotero::HARVESTER_CONFIG_ENTRY_TO_STRING_MAP.at(Zotero::HarvesterConfigEntry::EXTRACTION_REGEX)     },
    { MAX_CRAWL_DEPTH,          Zotero::HARVESTER_CONFIG_ENTRY_TO_STRING_MAP.at(Zotero::HarvesterConfigEntry::MAX_CRAWL_DEPTH)      },
} {}


std::vector<std::string> ExportFieldNameResolver::getAllValidAttributeNames() const {
    std::vector<std::string> valid_attribute_names;
    for (const auto &field_name_pair : attribute_names_)
        valid_attribute_names.emplace_back(field_name_pair.second);
    return valid_attribute_names;
}


const std::string &ExportFieldNameResolver::getIniKey(ExportField field) const {
    if (field == ENTRY_POINT_URL)
        LOG_ERROR("Cannot resolve INI name for field '" + std::to_string(field) + "' without harvester type data");
    else
        return ini_keys_.at(field);
}


const std::string &ExportFieldNameResolver::getIniKey(ExportField field, Zotero::HarvesterType harvester_type) const {
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
        return ini_keys_.at(field);
}


std::pair<std::string, std::string> ExportFieldNameResolver::getIniKeyAttributeNamePair(ExportField field) const {
    if (field == ENTRY_POINT_URL)
        LOG_ERROR("Cannot resolve INI name for field '" + std::to_string(field) + "' without harvester type data");
    else
        return std::make_pair(getIniKey(field), getAttributeName(field));
}


std::pair<std::string, std::string> ExportFieldNameResolver::getAttributeNameIniKeyPair(ExportField field) const {
    auto ini_key_attribute_name_pair(getIniKeyAttributeNamePair(field));
    std::swap(ini_key_attribute_name_pair.first, ini_key_attribute_name_pair.second);
    return ini_key_attribute_name_pair;
}


struct ImporterParams {
    Zeder::Flavour flavour_;
    std::vector<std::string> url_field_priority_;   // highest to lowest
    std::unordered_set<unsigned> entries_to_process_;
public:
    ImporterParams(const std::string &config_file_path, const std::string &flavour_string, const std::string &entry_ids_string = "");
};


ImporterParams::ImporterParams(const std::string &config_file_path, const std::string &flavour_string, const std::string &entry_ids_string)
    : url_field_priority_(), entries_to_process_()
{
    if (flavour_string == "krimdok")
        flavour_ = Zeder::KRIMDOK;
    else if (flavour_string == "ixtheo")
        flavour_ = Zeder::IXTHEO;
    else
        LOG_ERROR("Unknown Zeder flavour '" + flavour_string + "'");

    const IniFile config(config_file_path);
    const auto section(config.getSection(Zeder::FLAVOUR_TO_STRING_MAP.at(flavour_)));

    const auto url_field_priority(section->getString("url_field_priority"));
    if (StringUtil::Split(url_field_priority, ",", &url_field_priority_) == 0)
        LOG_ERROR("Invalid URL field priority for flavour " + std::to_string(flavour_) + " in '" + config.getFilename() + "'");

    if (not entry_ids_string.empty()) {
        std::unordered_set<std::string> id_strings;
        StringUtil::Split(entry_ids_string, ',', &id_strings);
        for (const auto &id_string : id_strings) {
            unsigned converted_id(0);
            if (not StringUtil::ToUnsigned(id_string, &converted_id))
                LOG_ERROR("Couldn't convert Zeder ID '" + id_string + "'");

            entries_to_process_.insert(converted_id);
        }
    }

}


bool GetHarvesterTypeFromEntry(const Zeder::Entry &entry, const ExportFieldNameResolver &name_resolver,
                               Zotero::HarvesterType * const harvester_type)
{
    const auto entry_type(entry.getAttribute(name_resolver.getAttributeName(TYPE)));
    if (entry_type == Zotero::HARVESTER_TYPE_TO_STRING_MAP.at(Zotero::HarvesterType::DIRECT))
        *harvester_type = Zotero::HarvesterType::DIRECT;
    else if (entry_type == Zotero::HARVESTER_TYPE_TO_STRING_MAP.at(Zotero::HarvesterType::CRAWL))
        *harvester_type = Zotero::HarvesterType::CRAWL;
    else if (entry_type == Zotero::HARVESTER_TYPE_TO_STRING_MAP.at(Zotero::HarvesterType::RSS))
        *harvester_type = Zotero::HarvesterType::RSS;
    else
       return false;

    return true;
}


bool PostProcessZederImportedEntry(const ImporterParams &params, const ExportFieldNameResolver &name_resolver,
                                   Zeder::Entry * const entry, bool ignore_invalid_ppn_issn = true)
{
    if (not params.entries_to_process_.empty() and
        params.entries_to_process_.find(entry->getId()) == params.entries_to_process_.end())
    {
        LOG_DEBUG("Entry " + std::to_string(entry->getId()) + " ignored");
        return false;
    }

    bool valid(true);

    const auto &pppn(entry->getAttribute("pppn")), &eppn(entry->getAttribute("eppn"));
    if (eppn.empty() or eppn == "NV" or not MiscUtil::IsValidPPN(eppn)) {
        if (pppn.empty() or pppn == "NV" or not MiscUtil::IsValidPPN(pppn)) {
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
    if (params.flavour_ == Zeder::IXTHEO and entry->getAttribute("prodf") != "zot") {
        LOG_WARNING("Entry " + std::to_string(entry->getId()) + " | Not a Zotero entry");
        valid = false;
    }

    if (entry->hasAttribute("rss") or entry->getAttribute("lrt").find("RSS.zotero") != std::string::npos)
        harvester_type = Zotero::HarvesterType::RSS;

    entry->setAttribute(name_resolver.getAttributeName(TYPE), Zotero::HARVESTER_TYPE_TO_STRING_MAP.at(harvester_type));
    entry->setAttribute(name_resolver.getAttributeName(GROUP), Zeder::FLAVOUR_TO_STRING_MAP.at(params.flavour_));
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

bool PostProcessIniImportedEntry(const ImporterParams &params, const ExportFieldNameResolver &name_resolver, Zeder::Entry * const entry) {
    if (not params.entries_to_process_.empty() and
        params.entries_to_process_.find(entry->getId()) == params.entries_to_process_.end())
    {
        LOG_DEBUG("Entry " + std::to_string(entry->getId()) + " ignored");
        return false;
    }

    Zotero::HarvesterType harvester_type;
    if (not GetHarvesterTypeFromEntry(*entry, name_resolver, &harvester_type))
         LOG_WARNING("Entry " + std::to_string(entry->getId()) + " | Invalid harvester type");

    return true;
}


unsigned DiffZederEntries(const Zeder::EntryCollection &old_entries, const Zeder::EntryCollection &new_entries,
                          const ExportFieldNameResolver &name_resolver,
                          std::vector<Zeder::Entry::DiffResult> * const diff_results, std::unordered_set<unsigned> * const new_entry_ids,
                          const bool skip_timestamp_check = false)
{
    static const std::vector<std::string> immutable_fields{
        name_resolver.getAttributeName(TYPE),
        name_resolver.getAttributeName(TITLE)
    };

    for (const auto &new_entry : new_entries) {
        const auto old_entry(old_entries.find(new_entry.getId()));
        if (old_entry == old_entries.end()) {
            // it's a new entry altogether
            diff_results->push_back({ true, new_entry.getId(), new_entry.getLastModifiedTimestamp(), {} });
            for (const auto &key_value : new_entry)
                diff_results->back().modified_attributes_[key_value.first] = std::make_pair("", key_value.second);

            new_entry_ids->insert(new_entry.getId());
            continue;
        }

        auto diff(Zeder::Entry::Diff(*old_entry, new_entry, skip_timestamp_check));
        bool unexpected_modifications(false);
        for (const auto &immutable_field : immutable_fields) {
            const auto modified_immutable_field(diff.modified_attributes_.find(immutable_field));

            if (modified_immutable_field != diff.modified_attributes_.end()) {
                LOG_WARNING("Entry " + std::to_string(diff.id_) + " | Field '" + immutable_field +
                            "' was unexpectedly modified from '" + modified_immutable_field->second.first + "' to '" +
                            modified_immutable_field->second.second + "'");
                unexpected_modifications = true;

                std::string debug_print_buffer;
                diff.prettyPrint(&debug_print_buffer);
                LOG_DEBUG(debug_print_buffer);
            }
        }

        if (not unexpected_modifications and not diff.modified_attributes_.empty())
            diff_results->push_back(diff);
    }

    return diff_results->size();
}


void MergeZederEntries(Zeder::EntryCollection * const merge_into,
                       const std::vector<Zeder::Entry::DiffResult> &diff_results)
{
    for (const auto &diff : diff_results) {
        if (not diff.is_timestamp_newer) {
            LOG_DEBUG("Skiping diff for entry " + std::to_string(diff.id_));
            continue;
        }

        auto entry(merge_into->find(diff.id_));
        if (entry == merge_into->end()) {
            // add a new entry
            Zeder::Entry new_entry(diff.id_);

            new_entry.setModifiedTimestamp(diff.last_modified_timestamp_);
            for (const auto &modified_attribute : diff.modified_attributes_)
                new_entry.setAttribute(modified_attribute.first, modified_attribute.second.second);
        } else
            Zeder::Entry::Merge(diff, &*entry);
    }

    merge_into->sortEntries();
}


void ParseZederCsv(const std::string &file_path, const ExportFieldNameResolver &name_resolver,
                   const ImporterParams &importer_params, Zeder::EntryCollection * const zeder_config)
{
    auto postprocessor([importer_params, name_resolver](Zeder::Entry * const entry) -> bool {
        return PostProcessZederImportedEntry(importer_params, name_resolver, entry);
    });
    std::unique_ptr<Zeder::Importer::Params> parser_params(new Zeder::Importer::Params(file_path, postprocessor));
    auto parser(Zeder::Importer::Factory(std::move(parser_params)));
    parser->parse(zeder_config);
}


void ParseZederIni(const std::string &file_path, const ExportFieldNameResolver &name_resolver,
                   const ImporterParams &params, Zeder::EntryCollection * const zeder_config)
{
    static const std::unordered_map<std::string, std::string> ini_key_to_attribute_map{
        name_resolver.getIniKeyAttributeNamePair(ZEDER_COMMENT),
        name_resolver.getIniKeyAttributeNamePair(TYPE),
        name_resolver.getIniKeyAttributeNamePair(GROUP),
        name_resolver.getIniKeyAttributeNamePair(PARENT_PPN),
        name_resolver.getIniKeyAttributeNamePair(PARENT_ISSN_PRINT),
        name_resolver.getIniKeyAttributeNamePair(PARENT_ISSN_ONLINE),
        name_resolver.getIniKeyAttributeNamePair(PARENT_ISSN_ONLINE),

        { name_resolver.getIniKey(ENTRY_POINT_URL, Zotero::HarvesterType::DIRECT), name_resolver.getAttributeName(ENTRY_POINT_URL) },
        { name_resolver.getIniKey(ENTRY_POINT_URL, Zotero::HarvesterType::CRAWL), name_resolver.getAttributeName(ENTRY_POINT_URL) },
        { name_resolver.getIniKey(ENTRY_POINT_URL, Zotero::HarvesterType::RSS), name_resolver.getAttributeName(ENTRY_POINT_URL) },
    };

    IniFile ini(file_path);
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
        else if (section.getString(name_resolver.getIniKey(ZEDER_ID), "").empty()) {
            LOG_DEBUG("Entry '" + section_name + "' has no Zeder ID. Skipping...");
            continue;
        }

        valid_section_names.emplace_back(section_name);
    }

    auto postprocessor([params, name_resolver](Zeder::Entry * const entry) -> bool {
        return PostProcessIniImportedEntry(params, name_resolver, entry);
    });
    std::unique_ptr<Zeder::IniReader::Params> parser_params(new Zeder::IniReader::Params(ini.getFilename(), postprocessor, valid_section_names,
                                                                                         name_resolver.getAttributeName(TITLE),
                                                                                         name_resolver.getIniKey(ZEDER_ID),
                                                                                         name_resolver.getIniKey(ZEDER_MODIFIED_TIMESTAMP),
                                                                                         ini_key_to_attribute_map));

    auto parser(Zeder::Importer::Factory(std::move(parser_params)));
    parser->parse(zeder_config);
}


void WriteZederIni(const std::string &file_path, const ExportFieldNameResolver &name_resolver,
                   const Zeder::EntryCollection &zeder_config)
{
    static const std::vector<std::string> attributes_to_export{
        name_resolver.getAttributeName(ZEDER_COMMENT),
        name_resolver.getAttributeName(TYPE),
        name_resolver.getAttributeName(GROUP),
        name_resolver.getAttributeName(PARENT_PPN),
        name_resolver.getAttributeName(PARENT_ISSN_PRINT),
        name_resolver.getAttributeName(PARENT_ISSN_ONLINE),
    };

    static const std::unordered_map<std::string, std::string> attribute_to_ini_key_map{
        name_resolver.getAttributeNameIniKeyPair(ZEDER_COMMENT),
        name_resolver.getAttributeNameIniKeyPair(TYPE),
        name_resolver.getAttributeNameIniKeyPair(GROUP),
        name_resolver.getAttributeNameIniKeyPair(PARENT_PPN),
        name_resolver.getAttributeNameIniKeyPair(PARENT_ISSN_PRINT),
        name_resolver.getAttributeNameIniKeyPair(PARENT_ISSN_ONLINE),
    };

    auto extra_keys_appender([name_resolver](IniFile::Section * const section, const Zeder::Entry &entry) {
        Zotero::HarvesterType harvester_type;
        if (not GetHarvesterTypeFromEntry(entry, name_resolver, &harvester_type))
            LOG_ERROR("Entry " + std::to_string(entry.getId()) + " | Invalid harvester type");

        // we need to manually resolve the key names for the entry point URL as they are dependent on the harvester type
        section->insert(name_resolver.getIniKey(ENTRY_POINT_URL, harvester_type), entry.getAttribute(name_resolver.getAttributeName(ENTRY_POINT_URL)),
                        "", IniFile::Section::DupeInsertionBehaviour::OVERWRITE_EXISTING_VALUE);

        if (harvester_type == Zotero::HarvesterType::CRAWL) {
            std::string temp_buffer;
            if (not section->lookup(name_resolver.getIniKey(MAX_CRAWL_DEPTH), &temp_buffer)) {
                section->insert(name_resolver.getIniKey(MAX_CRAWL_DEPTH), "1", "",
                        IniFile::Section::DupeInsertionBehaviour::OVERWRITE_EXISTING_VALUE);
            }

            if (not section->lookup(name_resolver.getIniKey(EXTRACTION_REGEX), &temp_buffer)) {
                section->insert(name_resolver.getIniKey(EXTRACTION_REGEX), "", "",
                        IniFile::Section::DupeInsertionBehaviour::OVERWRITE_EXISTING_VALUE);
            }
        }

        section->insert(name_resolver.getIniKey(STRPTIME_FORMAT), "", "",
                        IniFile::Section::DupeInsertionBehaviour::OVERWRITE_EXISTING_VALUE);
    });
    std::unique_ptr<Zeder::IniWriter::Params> writer_params(new Zeder::IniWriter::Params(file_path, attributes_to_export,
                                                                                         name_resolver.getAttributeName(TITLE),
                                                                                         name_resolver.getIniKey(ZEDER_ID),
                                                                                         name_resolver.getIniKey(ZEDER_MODIFIED_TIMESTAMP),
                                                                                         attribute_to_ini_key_map, extra_keys_appender));
    auto writer(Zeder::Exporter::Factory(std::move(writer_params)));
    writer->write(zeder_config);
}


void PrintZederDiffs(const std::vector<Zeder::Entry::DiffResult> &diff_results, const std::unordered_set<unsigned> &new_entry_ids) {
    LOG_INFO("\nDifferences ========================>");

    for (const auto &diff : diff_results) {
        std::string attribute_print_buffer;
        bool is_new_entry(new_entry_ids.find(diff.id_) != new_entry_ids.end());
        diff.prettyPrint(&attribute_print_buffer);

        if (is_new_entry)
            LOG_INFO("[NEW] " + attribute_print_buffer);
        else
            LOG_INFO("[MOD] " + attribute_print_buffer);
    }

    LOG_INFO("\n");
}


} // unnamed namespace


int Main(int argc, char *argv[]) {
    if (argc < 6)
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

    if (argc != 5 and argc != 6)
        Usage();

    ImporterParams importer_params(argv[2], argv[1], argc == 6 ? argv[5] : "");
    ExportFieldNameResolver name_resolver;

    switch (current_mode) {
    case Mode::GENERATE: {
        const std::string zeder_export_path(argv[3]), output_ini_path(argv[4]);
        Zeder::EntryCollection parsed_config;

        ParseZederCsv(zeder_export_path, name_resolver, importer_params, &parsed_config);
        WriteZederIni(output_ini_path, name_resolver, parsed_config);

        LOG_INFO("Created " + std::to_string(parsed_config.size()) + " entries");

        break;
    }
    case Mode::DIFF:
    case Mode::MERGE: {
        const std::string new_ini_path(argv[3]), old_ini_path(argv[4]);
        Zeder::EntryCollection old_data, new_data;

        ParseZederIni(old_ini_path, name_resolver, importer_params, &old_data);
        ParseZederIni(new_ini_path, name_resolver, importer_params, &new_data);

        std::vector<Zeder::Entry::DiffResult> diff_results;
        std::unordered_set<unsigned> new_entry_ids;
        const auto num_modified_entries(DiffZederEntries(old_data, new_data, name_resolver,
                                                         &diff_results, &new_entry_ids, skip_timestamp_check));

        if (num_modified_entries > 0) {
            PrintZederDiffs(diff_results, new_entry_ids);

            if (current_mode == Mode::MERGE) {
                MergeZederEntries(&old_data, diff_results);
                WriteZederIni(old_ini_path, name_resolver, old_data);
            }

            LOG_INFO("Modified entries: " + std::to_string(diff_results.size() - new_entry_ids.size()));
            LOG_INFO("New entries: " + std::to_string(new_entry_ids.size()));
        } else
            LOG_INFO("No modified/new entries.");

        break;
    }
    }

    return EXIT_SUCCESS;
}

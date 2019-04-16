/** \brief  Tools to convert data downloaded from Zeder into ZTS harvester file formats
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
#include "JournalConfig.h"
#include "MiscUtil.h"
#include "StringUtil.h"
#include "util.h"
#include "Zeder.h"
#include "Zotero.h"


namespace {


[[noreturn]] void Usage() {
    ::Usage(" --mode=tool_mode [--skip-timestamp-check] flavour config_file first_path second_path [entry_ids]\n"
            "Modes:\n"
            "     generate - Converts the .csv file exported from Zeder into a zts_harvester-compatible .conf file.\n"                          "                The first path points to the .csv file and the second to the output .conf file.\n"
            "         diff - Compares the values of entries in a pair of zts_harvester-compatible .conf files.\n"
            "                The first path points to the source/updated .conf file and the second to the destination/old .conf.\n"
            "        merge - Same as above but additionally merges any changes into the destination/old .conf.\n\n"
            " --skip-timestamp-check\t\tIgnore the Zeder modified timestamp when diff'ing entries.\n"
            "   flavour\t\tEither 'ixtheo' or 'krimdok'.\n"
            "   entry_ids\t\tComma-separated list of entries IDs to process. All other entries will be ignored.\n");
    std::exit(EXIT_FAILURE);
}


enum Mode { GENERATE, DIFF, MERGE };


// An enumeration of the fields exported to a zts_harvester compatible config file.
// This is the primary key used to refer to the corresponding fields throughout this tool.

// Adding a new field involves adding a new entry to this enumeration and updating the ExportFieldNameResolver
// class with its string identifiers.
enum ExportField {
    TITLE,
    ZEDER_ID, ZEDER_MODIFIED_TIMESTAMP, ZEDER_UPDATE_WINDOW,
    TYPE, GROUP,
    PARENT_PPN_PRINT, PARENT_PPN_ONLINE, PARENT_ISSN_PRINT, PARENT_ISSN_ONLINE,
    ENTRY_POINT_URL, EXPECTED_LANGUAGES,
    // The following fields are only used when exporting entries of type 'CRAWL'.
    EXTRACTION_REGEX, MAX_CRAWL_DEPTH
};


} // unnamed namespace
namespace std {
    template <>
    struct hash<::ExportField> {
        size_t operator()(const ::ExportField &flavour) const {
            // hash method here.
            return hash<int>()(flavour);
        }
    };
} // namespace std


namespace {


// Used to convert export field enumerations to their respective string identifiers.
// Each export field enumeration has two string identifiers, one of which is use used as an attribute
// in Zeder::Entry and the other as the INI key in the generated zts_harvester compatible config files
class ExportFieldNameResolver {
    std::unordered_map<ExportField, std::string> attribute_names_;
    std::unordered_map<ExportField, std::string> ini_keys_;
public:
    ExportFieldNameResolver();

    const std::string &getAttributeName(ExportField field) const { return attribute_names_.at(field); }
    std::vector<std::string> getAllValidAttributeNames() const;
    const std::string &getIniKey(ExportField field) const;
    std::pair<std::string, std::string> getIniKeyAttributeNamePair(ExportField field) const;
    std::pair<std::string, std::string> getAttributeNameIniKeyPair(ExportField field) const;
};


// Unused attributes correspond to fields that are not stored as attributes in Zeder::Entry
// INI key identifiers should be fetched using the bundle API in JournalConfig.h/.cc
ExportFieldNameResolver::ExportFieldNameResolver(): attribute_names_{
    { TITLE,                    "zts_title"                 },
    { ZEDER_ID,                 "" /* unused */             },      // stored directly in the Entry class
    { ZEDER_MODIFIED_TIMESTAMP, "" /* unused */             },      // same as above
    { ZEDER_UPDATE_WINDOW,      "zts_update_window"         },
    { TYPE,                     "zts_type"                  },
    { GROUP,                    "zts_group"                 },
    { PARENT_PPN_PRINT,         "zts_parent_ppn_print"      },
    { PARENT_PPN_ONLINE,        "zts_parent_ppn_online"     },
    { PARENT_ISSN_PRINT,        "zts_parent_issn_print"     },
    { PARENT_ISSN_ONLINE,       "zts_parent_issn_online"    },
    { ENTRY_POINT_URL,          "zts_entry_point_url"       },
    { EXPECTED_LANGUAGES,       "zts_expected_languages"    },
    { EXTRACTION_REGEX,         "" /* unused */             },
    { MAX_CRAWL_DEPTH,          "" /* unused */             },
}, ini_keys_{
    { TITLE,                    "" /* exported as the section name */   },
    { ZEDER_ID,                 JournalConfig::ZederBundle::Key(JournalConfig::Zeder::ID)                   },
    { ZEDER_MODIFIED_TIMESTAMP, JournalConfig::ZederBundle::Key(JournalConfig::Zeder::MODIFIED_TIME)        },
    { ZEDER_UPDATE_WINDOW,      JournalConfig::ZederBundle::Key(JournalConfig::Zeder::UPDATE_WINDOW)        },
    { TYPE,                     JournalConfig::ZoteroBundle::Key(JournalConfig::Zotero::TYPE)               },
    { GROUP,                    JournalConfig::ZoteroBundle::Key(JournalConfig::Zotero::GROUP)              },
    { PARENT_PPN_PRINT,         JournalConfig::PrintBundle::Key(JournalConfig::Print::PPN)                  },
    { PARENT_PPN_ONLINE,        JournalConfig::OnlineBundle::Key(JournalConfig::Online::PPN)                },
    { PARENT_ISSN_PRINT,        JournalConfig::PrintBundle::Key(JournalConfig::Print::ISSN)                 },
    { PARENT_ISSN_ONLINE,       JournalConfig::OnlineBundle::Key(JournalConfig::Online::ISSN)               },
    { ENTRY_POINT_URL,          JournalConfig::ZoteroBundle::Key(JournalConfig::Zotero::URL)                },
    { EXPECTED_LANGUAGES,       JournalConfig::ZoteroBundle::Key(JournalConfig::Zotero::EXPECTED_LANGUAGES) },
    { EXTRACTION_REGEX,         JournalConfig::ZoteroBundle::Key(JournalConfig::Zotero::EXTRACTION_REGEX)   },
    { MAX_CRAWL_DEPTH,          JournalConfig::ZoteroBundle::Key(JournalConfig::Zotero::MAX_CRAWL_DEPTH)    },
} {}


std::vector<std::string> ExportFieldNameResolver::getAllValidAttributeNames() const {
    std::vector<std::string> valid_attribute_names;
    for (const auto &field_name_pair : attribute_names_)
        valid_attribute_names.emplace_back(field_name_pair.second);
    return valid_attribute_names;
}


const std::string &ExportFieldNameResolver::getIniKey(ExportField field) const {
    return ini_keys_.at(field);
}


std::pair<std::string, std::string> ExportFieldNameResolver::getIniKeyAttributeNamePair(ExportField field) const {
    return std::make_pair(getIniKey(field), getAttributeName(field));
}


std::pair<std::string, std::string> ExportFieldNameResolver::getAttributeNameIniKeyPair(ExportField field) const {
    auto ini_key_attribute_name_pair(getIniKeyAttributeNamePair(field));
    std::swap(ini_key_attribute_name_pair.first, ini_key_attribute_name_pair.second);
    return ini_key_attribute_name_pair;
}


struct ConversionParams {
    Zeder::Flavour flavour_;
    bool ignore_invalid_ppn_issn_;
    std::vector<std::string> url_field_priority_;   // highest to lowest
    std::unordered_set<unsigned> entries_to_process_;
public:
    ConversionParams(const std::string &config_file_path, const std::string &flavour_string, const std::string &entry_ids_string);
};


ConversionParams::ConversionParams(const std::string &config_file_path, const std::string &flavour_string, const std::string &entry_ids_string)
    : url_field_priority_(), entries_to_process_()
{
    flavour_ = Zeder::ParseFlavour(flavour_string);

    const IniFile config(config_file_path);
    ignore_invalid_ppn_issn_ = config.getBool("", "ignore_invalid_ppn_issn");
    const auto section(config.getSection(Zeder::FLAVOUR_TO_STRING_MAP.at(flavour_)));

    const auto url_field_priority(section->getString("url_field_priority"));
    if (StringUtil::Split(url_field_priority, ',', &url_field_priority_, /* suppress_empty_components = */true) == 0)
        LOG_ERROR("Invalid URL field priority for flavour " + std::to_string(flavour_) + " in '" + config.getFilename() + "'");

    if (not entry_ids_string.empty()) {
        std::unordered_set<std::string> id_strings;
        StringUtil::Split(entry_ids_string, ',', &id_strings, /* suppress_empty_components = */true);
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


// Calculate an admissible range in days for a frequency given per year
// Right now we simply ignore entries that cannot be suitably converted to float
std::string CalculateUpdateWindowFromFrequency(const std::string &frequency) {
    float frequency_as_float;
    if (not StringUtil::ToFloat(frequency, &frequency_as_float))
        return "";
    float admissible_range = (365 / frequency_as_float) * 1.5;
    return std::to_string(static_cast<int>(std::round(admissible_range)));
}


// Validates and normalises the Zeder::Entry generated from a Zeder CSV file.
// The significance of the imported attributes can be found in the Zeder manual.
bool PostProcessCsvImportedEntry(const ConversionParams &params, const ExportFieldNameResolver &name_resolver,
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
    if (MiscUtil::IsValidPPN(eppn))
        entry->setAttribute(name_resolver.getAttributeName(PARENT_PPN_ONLINE), eppn);
    if (MiscUtil::IsValidPPN(pppn))
        entry->setAttribute(name_resolver.getAttributeName(PARENT_PPN_PRINT), pppn);
    if (not MiscUtil::IsValidPPN(pppn) and not MiscUtil::IsValidPPN(eppn)) {
        LOG_WARNING("Entry " + std::to_string(entry->getId()) + " | No valid PPN found");
        valid = ignore_invalid_ppn_issn ? valid : false;
    }

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

    if (entry->hasAttribute(name_resolver.getAttributeName(PARENT_ISSN_ONLINE)) xor
        entry->hasAttribute(name_resolver.getAttributeName(PARENT_PPN_ONLINE))) {
        LOG_WARNING("Entry " + std::to_string(entry->getId()) + " | Invalid online ISSN/PPN pair");
        valid = ignore_invalid_ppn_issn ? valid : false;
    } else if (entry->hasAttribute(name_resolver.getAttributeName(PARENT_ISSN_PRINT)) xor
               entry->hasAttribute(name_resolver.getAttributeName(PARENT_PPN_PRINT))) {
        LOG_WARNING("Entry " + std::to_string(entry->getId()) + " | Invalid print ISSN/PPN pair");
        valid = ignore_invalid_ppn_issn ? valid : false;
    }

    auto title(entry->getAttribute("tit"));
    StringUtil::Trim(&title);
    entry->setAttribute(name_resolver.getAttributeName(TITLE), title);

    Zotero::HarvesterType harvester_type(Zotero::HarvesterType::CRAWL);
    if (params.flavour_ == Zeder::IXTHEO and entry->getAttribute("prodf") != "zot") {
        LOG_WARNING("Entry " + std::to_string(entry->getId()) + " | Not a Zotero entry");
        valid = false;
    }

    if ((entry->hasAttribute("rss") and not entry->getAttribute("rss").empty()) or
        entry->getAttribute("lrt").find("RSS.zotero") != std::string::npos)
    {
        harvester_type = Zotero::HarvesterType::RSS;
    }

    entry->setAttribute(name_resolver.getAttributeName(TYPE), Zotero::HARVESTER_TYPE_TO_STRING_MAP.at(harvester_type));
    entry->setAttribute(name_resolver.getAttributeName(GROUP), Zeder::FLAVOUR_TO_STRING_MAP.at(params.flavour_));

    // resolve URL based on the importer's config
    std::string resolved_url;
    for (const auto &url_field : params.url_field_priority_) {
        if (entry->hasAttribute(url_field)) {
            const auto &imported_url(entry->getAttribute(url_field));
            if (not resolved_url.empty() and not imported_url.empty()) {
                LOG_INFO("Entry " + std::to_string(entry->getId()) + " | Discarding '" + url_field + "' URL '" +
                         imported_url + "'");
            } else if (not imported_url.empty())
                resolved_url = imported_url;
        }
    }

    if (resolved_url.empty()) {
        LOG_WARNING("Entry " + std::to_string(entry->getId()) + " | Couldn't resolve harvest URL");
        valid = false;
    } else
        entry->setAttribute(name_resolver.getAttributeName(ENTRY_POINT_URL), resolved_url);

    // Extract the frequency and calculate the update window in days
    std::string journal_frequency(entry->getAttribute("freq")); // <- issues per year, can be a fraction
    if (not journal_frequency.empty()) {
        const std::string update_window(CalculateUpdateWindowFromFrequency(journal_frequency));
        if (not update_window.empty())
            entry->setAttribute(name_resolver.getAttributeName(ZEDER_UPDATE_WINDOW), update_window);
        else
            LOG_WARNING("Entry " + std::to_string(entry->getId()) + " | Unable to derive a proper update window from \""
                                                                       + journal_frequency + "\"");
    }

    if (entry->hasAttribute("spr")) {
        auto expected_languages(entry->getAttribute("spr"));
        StringUtil::Trim(&expected_languages);
        entry->setAttribute(name_resolver.getAttributeName(EXPECTED_LANGUAGES), expected_languages);
    }

    // remove the original attributes
    entry->keepAttributes(name_resolver.getAllValidAttributeNames());

    std::string pretty_print;
    entry->prettyPrint(&pretty_print);
    LOG_DEBUG(pretty_print);

    return valid;
}


// Validates a Zeder::Entry generated from a zts_harvester compatible config file.
bool PostProcessIniImportedEntry(const ConversionParams &params, const ExportFieldNameResolver &name_resolver, Zeder::Entry * const entry) {
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
                          const ExportFieldNameResolver &name_resolver, std::vector<Zeder::Entry::DiffResult> * const diff_results,
                          std::unordered_set<unsigned> * const new_entry_ids, const bool skip_timestamp_check = false)
{
    static const std::vector<std::string> immutable_fields{
        name_resolver.getAttributeName(TYPE),
        name_resolver.getAttributeName(TITLE),
        name_resolver.getAttributeName(GROUP)
    };

    for (const auto &new_entry : new_entries) {
        const auto old_entry(old_entries.find(new_entry.getId()));
        if (old_entry == old_entries.end()) {
            // it's a new entry altogether
            Zeder::Entry::DiffResult new_diff;
            new_diff.id_ = new_entry.getId();
            new_diff.timestamp_is_newer_ = true;
            new_diff.last_modified_timestamp_ = new_entry.getLastModifiedTimestamp();
            new_diff.timestamp_time_difference_ = 0;

            for (const auto &key_value : new_entry)
                new_diff.modified_attributes_[key_value.first] = std::make_pair(std::string(""), key_value.second);

            diff_results->push_back(new_diff);
            new_entry_ids->insert(new_entry.getId());
            continue;
        }

        auto diff(Zeder::Entry::Diff(*old_entry, new_entry, skip_timestamp_check));
        bool unexpected_modifications(false);
        for (const auto &immutable_field : immutable_fields) {
            const auto modified_immutable_field(diff.modified_attributes_.find(immutable_field));

            if (modified_immutable_field != diff.modified_attributes_.end()) {
                LOG_WARNING("Entry " + std::to_string(diff.id_) + " | Field '" + immutable_field +
                            "' was unexpectedly modified");
                unexpected_modifications = true;
            }
        }

        if (unexpected_modifications) {
            std::string debug_print_buffer;
            diff.prettyPrint(&debug_print_buffer);
            LOG_WARNING(debug_print_buffer);
        }

        if (not unexpected_modifications and not diff.modified_attributes_.empty())
            diff_results->push_back(diff);
    }

    return diff_results->size();
}


void MergeZederEntries(Zeder::EntryCollection * const merge_into, const std::vector<Zeder::Entry::DiffResult> &diff_results) {
    for (const auto &diff : diff_results) {
        if (not diff.timestamp_is_newer_) {
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

            merge_into->addEntry(new_entry);
        } else
            Zeder::Entry::Merge(diff, &*entry);
    }

    merge_into->sortEntries();
}


void ParseZederCsv(const std::string &file_path, const ExportFieldNameResolver &name_resolver,
                   const ConversionParams &conversion_params, Zeder::EntryCollection * const zeder_config)
{
    auto postprocessor([conversion_params, name_resolver](Zeder::Entry * const entry) -> bool {
        return PostProcessCsvImportedEntry(conversion_params, name_resolver, entry, conversion_params.ignore_invalid_ppn_issn_);
    });
    std::unique_ptr<Zeder::Importer::Params> parser_params(new Zeder::Importer::Params(file_path, postprocessor));
    auto parser(Zeder::Importer::Factory(std::move(parser_params)));
    parser->parse(zeder_config);
}


void ParseZederIni(const std::string &file_path, const ExportFieldNameResolver &name_resolver,
                   const ConversionParams &params, Zeder::EntryCollection * const zeder_config)
{
    static const std::unordered_map<std::string, std::string> ini_key_to_attribute_map{
        name_resolver.getIniKeyAttributeNamePair(ZEDER_UPDATE_WINDOW),
        name_resolver.getIniKeyAttributeNamePair(TYPE),
        name_resolver.getIniKeyAttributeNamePair(GROUP),
        name_resolver.getIniKeyAttributeNamePair(PARENT_PPN_PRINT),
        name_resolver.getIniKeyAttributeNamePair(PARENT_PPN_ONLINE),
        name_resolver.getIniKeyAttributeNamePair(PARENT_ISSN_PRINT),
        name_resolver.getIniKeyAttributeNamePair(PARENT_ISSN_ONLINE),
        name_resolver.getIniKeyAttributeNamePair(ENTRY_POINT_URL),
        name_resolver.getIniKeyAttributeNamePair(EXPECTED_LANGUAGES),
    };

    IniFile ini(file_path);
    if (ini.getSections().empty())
        return;

    // select the sections that are Zeder-compatible, i.e., that were exported by this tool
    std::vector<std::string> groups, valid_section_names;

    StringUtil::Split(ini.getString("", "groups", ""), ',', &groups, /* suppress_empty_components = */true);
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

        // only read in sections that are pertinent to the importer's invocation flavour
        const auto group(section.getString(name_resolver.getIniKey(GROUP)));
        if (group == Zeder::FLAVOUR_TO_STRING_MAP.at(params.flavour_))
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


// Writes out the contents of a Zeder::EntryCollection to an INI file. If the path already exists,
// the entries in the INI file will be overwritten by the corresponding entry in Zeder::EntryCollection.
// All other existing entries will be preserved.
void WriteZederIni(const std::string &file_path, const ExportFieldNameResolver &name_resolver,
                   const Zeder::EntryCollection &zeder_config, const bool create_file_anew = false)
{
    static const std::vector<std::string> attributes_to_export{
        name_resolver.getAttributeName(ZEDER_UPDATE_WINDOW),
        name_resolver.getAttributeName(PARENT_PPN_PRINT),
        name_resolver.getAttributeName(PARENT_ISSN_PRINT),
        name_resolver.getAttributeName(PARENT_PPN_ONLINE),
        name_resolver.getAttributeName(PARENT_ISSN_ONLINE),
        name_resolver.getAttributeName(TYPE),
        name_resolver.getAttributeName(GROUP),
        name_resolver.getAttributeName(ENTRY_POINT_URL),
        name_resolver.getAttributeName(EXPECTED_LANGUAGES),
    };

    static const std::unordered_map<std::string, std::string> attribute_to_ini_key_map{
        name_resolver.getAttributeNameIniKeyPair(TYPE),
        name_resolver.getAttributeNameIniKeyPair(GROUP),
        name_resolver.getAttributeNameIniKeyPair(PARENT_PPN_PRINT),
        name_resolver.getAttributeNameIniKeyPair(PARENT_PPN_ONLINE),
        name_resolver.getAttributeNameIniKeyPair(PARENT_ISSN_PRINT),
        name_resolver.getAttributeNameIniKeyPair(PARENT_ISSN_ONLINE),
        name_resolver.getAttributeNameIniKeyPair(ENTRY_POINT_URL),
        name_resolver.getAttributeNameIniKeyPair(ZEDER_UPDATE_WINDOW),
        name_resolver.getAttributeNameIniKeyPair(EXPECTED_LANGUAGES),
    };

    // remove existing output config file, if any
    if (create_file_anew)
        ::unlink(file_path.c_str());

    auto extra_keys_appender([name_resolver](IniFile::Section * const section, const Zeder::Entry &entry) {
        Zotero::HarvesterType harvester_type;
        if (not GetHarvesterTypeFromEntry(entry, name_resolver, &harvester_type))
            LOG_ERROR("Entry " + std::to_string(entry.getId()) + " | Invalid harvester type");

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
    LOG_INFO("\nDifferences:");

    std::string modified_entry_ids_string, new_entry_ids_string;
    for (const auto &diff : diff_results) {
        std::string attribute_print_buffer;
        bool is_new_entry(new_entry_ids.find(diff.id_) != new_entry_ids.end());
        diff.prettyPrint(&attribute_print_buffer);

        if (is_new_entry) {
            new_entry_ids_string += " " + std::to_string(diff.id_) + ",";
            LOG_INFO("[NEW] " + attribute_print_buffer);
        }
        else {
            modified_entry_ids_string += " " + std::to_string(diff.id_) + ",";
            LOG_INFO("[MOD] " + attribute_print_buffer);
        }
    }

    LOG_INFO("\n\n");

    if (not modified_entry_ids_string.empty()) {
        modified_entry_ids_string.erase(modified_entry_ids_string.length() - 1);
        LOG_INFO("Modified entries: " + modified_entry_ids_string);
    }

    if (not new_entry_ids_string.empty()) {
        new_entry_ids_string.erase(new_entry_ids_string.length() - 1);
        LOG_INFO("New entries: " + new_entry_ids_string);
    }

    LOG_INFO("\n\n");
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

    const auto flavour(argv[1]);
    const auto config_path(argv[2]);
    const auto first_path(argv[3]);
    const auto second_path(argv[4]);
    const auto entries_to_process(argc == 6 ? argv[5] : "");

    ConversionParams conversion_params(config_path, flavour, entries_to_process);
    ExportFieldNameResolver name_resolver;

    switch (current_mode) {
    case Mode::GENERATE: {
        const std::string zeder_export_path(first_path), output_ini_path(second_path);
        Zeder::EntryCollection parsed_config;

        ParseZederCsv(zeder_export_path, name_resolver, conversion_params, &parsed_config);
        WriteZederIni(output_ini_path, name_resolver, parsed_config, /*create_file_anew =*/ true);

        LOG_INFO("Created " + std::to_string(parsed_config.size()) + " entries");

        break;
    }
    case Mode::DIFF:
    case Mode::MERGE: {
        const std::string new_ini_path(first_path), old_ini_path(second_path);

        Zeder::EntryCollection old_data, new_data;
        ParseZederIni(old_ini_path, name_resolver, conversion_params, &old_data);
        ParseZederIni(new_ini_path, name_resolver, conversion_params, &new_data);

        std::vector<Zeder::Entry::DiffResult> diff_results;
        std::unordered_set<unsigned> new_entry_ids;
        const auto num_modified_entries(DiffZederEntries(old_data, new_data, name_resolver, &diff_results,
                                                         &new_entry_ids, skip_timestamp_check));

        if (num_modified_entries > 0) {
            PrintZederDiffs(diff_results, new_entry_ids);

            if (current_mode == Mode::MERGE) {
                MergeZederEntries(&old_data, diff_results);
                WriteZederIni(old_ini_path, name_resolver, old_data);
            }

            LOG_INFO("Number of modified entries: " + std::to_string(diff_results.size() - new_entry_ids.size()));
            LOG_INFO("Number of new entries: " + std::to_string(new_entry_ids.size()));
        } else
            LOG_INFO("No modified/new entries.");

        break;
    }
    }

    return EXIT_SUCCESS;
}

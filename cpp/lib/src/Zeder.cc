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
#include "Zeder.h"
#include <cmath>
#include "Downloader.h"
#include "FileUtil.h"
#include "StringUtil.h"
#include "UBTools.h"


namespace Zeder {


Flavour GetFlavourByString(const std::string &flavour) {
    for (const auto &flavour_and_string : FLAVOUR_TO_STRING_MAP) {
        if (::strcasecmp(flavour_and_string.second.c_str(), flavour.c_str()) == 0)
            return flavour_and_string.first;
    }

    LOG_ERROR("Unknown Zeder Flavour: " + flavour);
}


const std::string &Entry::getAttribute(const std::string &name) const {
    const auto match(attributes_.find(name));
    if (match == attributes_.end())
        LOG_ERROR("Couldn't find attribute '" + name + "'! in entry " + std::to_string(id_));
    else
        return match->second;
}


const std::string &Entry::getAttribute(const std::string &name, const std::string &default_value) const {
    const auto match(attributes_.find(name));
    if (match == attributes_.end())
        return default_value;
    else
        return match->second;
}


void Entry::setAttribute(const std::string &name, const std::string &value, bool overwrite) {
    if (attributes_.find(name) == attributes_.end() or overwrite)
        attributes_[name] = StringUtil::Map(value, Zeder::ATTRIBUTE_INVALID_CHARS, std::string(Zeder::ATTRIBUTE_INVALID_CHARS.length(), '_'));
    else
        LOG_ERROR("Attribute '" + name + "' already exists in entry " + std::to_string(id_));
}


void Entry::removeAttribute(const std::string &name) {
    const auto match(attributes_.find(name));
    if (match == attributes_.end())
        LOG_ERROR("Couldn't find attribute '" + name + "'! in entry " + std::to_string(id_));
    else
        attributes_.erase(match);
}


unsigned Entry::keepAttributes(const std::vector<std::string> &names_to_keep) {
    std::vector<std::string> names_to_remove;
    for (const auto &key_value : attributes_) {
        if (std::find(names_to_keep.begin(), names_to_keep.end(), key_value.first) == names_to_keep.end())
            names_to_remove.emplace_back(key_value.first);
    }

    for (const auto &name : names_to_remove)
        attributes_.erase(attributes_.find(name));
    return names_to_remove.size();
}


void Entry::prettyPrint(std::string * const print_buffer) const {
    *print_buffer = "Entry " + std::to_string(id_) + ":\n";
    for (const auto &attribute : attributes_)
        *print_buffer += "\t{" + attribute.first + "} -> '" + attribute.second + "'\n";
}


std::string Entry::prettyPrint() const {
    std::string buffer;
    prettyPrint(&buffer);
    return buffer;
}


void Entry::DiffResult::prettyPrint(std::string * const print_buffer) const {
    *print_buffer = "Diff " + std::to_string(id_) + ":\n";

    char time_string_buffer[19 + 1]{};  // YYYY-MM-DD hh:mm:ss
    std::strftime(time_string_buffer, sizeof(time_string_buffer), MODIFIED_TIMESTAMP_FORMAT_STRING, &last_modified_timestamp_);
    *print_buffer += "\tTimestamp: " + std::string(time_string_buffer) + " (" + (timestamp_is_newer_ ? "newer than the current revision by " :
                     "older than the current revision by ") + std::to_string(std::fabs(timestamp_time_difference_)) + " days)\n\n";

    for (const auto &attribute : modified_attributes_) {
        if (not attribute.second.first.empty())
            *print_buffer += "\t{" + attribute.first + "} -> '" + attribute.second.first + "' => '" + attribute.second.second + "'\n";
        else
            *print_buffer += "\t{" + attribute.first + "} -> '" + attribute.second.second + "'\n";
    }
}


std::string Entry::DiffResult::prettyPrint() const {
    std::string buffer;
    prettyPrint(&buffer);
    return buffer;
}


Entry::DiffResult Entry::Diff(const Entry &lhs, const Entry &rhs, const bool skip_timestamp_check) {
    if (lhs.getId() != rhs.getId())
        LOG_ERROR("Can only diff revisions of the same entry! LHS = " + std::to_string(lhs.getId()) + ", RHS = "
                  + std::to_string(rhs.getId()));

    DiffResult delta;
    delta.id_ = rhs.getId();
    delta.last_modified_timestamp_ = rhs.getLastModifiedTimestamp();
    delta.timestamp_time_difference_ = TimeUtil::DiffStructTm(rhs.getLastModifiedTimestamp(), lhs.getLastModifiedTimestamp()) / 3600 / 24;
    delta.timestamp_is_newer_ = delta.timestamp_time_difference_ >= 0;

    if (not skip_timestamp_check) {
        if (not delta.timestamp_is_newer_) {
            const auto time_difference(std::fabs(delta.timestamp_time_difference_));
            LOG_WARNING("The existing entry " + std::to_string(rhs.getId()) + " is newer than the diff by "
                        + std::to_string(time_difference) + " days (was the modified timestamp changed manually?)");
        }
    } else {
        delta.timestamp_is_newer_ = true;
        delta.timestamp_time_difference_ = 0;
        delta.last_modified_timestamp_ = lhs.getLastModifiedTimestamp();
    }

    for (const auto &key_value : rhs) {
        const auto &attribute_name(key_value.first), &attribute_value(key_value.second);

        if (lhs.hasAttribute(attribute_name)) {
            const auto value(lhs.getAttribute(attribute_name));
            if (value != attribute_value) {
                // copy the old value before updating it (since it's a reference)
                delta.modified_attributes_[attribute_name] = std::make_pair(value, attribute_value);
            }
        } else
            delta.modified_attributes_[attribute_name] = std::make_pair("", attribute_value);
    }

    return delta;
}


void Entry::Merge(const DiffResult &delta, Entry * const merge_into) {
    if (delta.id_ != merge_into->getId()) {
        LOG_ERROR("ID mismatch between diff and entry. Diff  = " + std::to_string(delta.id_) +
                  ", Entry = " + std::to_string(merge_into->getId()));
    }

    if (not delta.timestamp_is_newer_) {
        const auto time_difference(TimeUtil::DiffStructTm(delta.last_modified_timestamp_, merge_into->getLastModifiedTimestamp()));
        LOG_WARNING("Diff of entry " + std::to_string(delta.id_) + " is not newer than the source revision. " +
                    "Timestamp difference: " + std::to_string(std::abs(time_difference)) + " days");
    }

    merge_into->setModifiedTimestamp(delta.last_modified_timestamp_);
    for (const auto &key_value : delta.modified_attributes_)
        merge_into->setAttribute(key_value.first, key_value.second.second, true);
}


void EntryCollection::addEntry(const Entry &new_entry, const bool sort_after_add) {
    const iterator match(find(new_entry.getId()));
    if (unlikely(match != end()))
        LOG_ERROR("Duplicate ID " + std::to_string(new_entry.getId()) + "!");
    else
        entries_.emplace_back(new_entry);

    if (sort_after_add)
        sortEntries();
}


FileType GetFileTypeFromPath(const std::string &path, bool check_if_file_exists) {
    if (check_if_file_exists and not FileUtil::Exists(path))
        LOG_ERROR("file '" + path + "' not found");

    if (StringUtil::EndsWith(path, ".csv", /* ignore_case = */true))
        return FileType::CSV;
    if (StringUtil::EndsWith(path, ".json", /* ignore_case = */true))
        return FileType::JSON;
    if (StringUtil::EndsWith(path, ".conf", /* ignore_case = */true) or StringUtil::EndsWith(path, ".ini", /* ignore_case = */true) )
        return FileType::INI;

    LOG_ERROR("can't guess the file type of \"" + path + "\"!");
}


const std::map<Importer::MandatoryField, std::string> Importer::MANDATORY_FIELD_TO_STRING_MAP {
    { Importer::MandatoryField::Z,     "Z"     },
    { Importer::MandatoryField::MTIME, "Mtime" }
};


std::unique_ptr<Importer> Importer::Factory(std::unique_ptr<Params> params) {
    auto file_type(GetFileTypeFromPath(params->file_path_));
    switch (file_type) {
    case FileType::CSV:
        return std::unique_ptr<Importer>(new CsvReader(std::move(params)));
    case FileType::INI:
        return std::unique_ptr<Importer>(new IniReader(std::move(params)));
    default:
        LOG_ERROR("Reader not implemented for file '" + params->file_path_ + "'");
    };
}


void CsvReader::parse(EntryCollection * const collection) {
    std::vector<std::string> columns, splits;
    size_t line(0);

    while (reader_.readLine(&splits)) {
        ++line;
        if (splits.size() < 2)
            LOG_ERROR("Incorrect number of splits on line " + std::to_string(line));
        else if (line == 1) {
            columns.swap(splits);
            if (columns[0] != MANDATORY_FIELD_TO_STRING_MAP.at(Z) or columns[columns.size() - 1] != MANDATORY_FIELD_TO_STRING_MAP.at(MTIME))
                LOG_ERROR("Mandatory fields were not found in the first and last columns!");

            continue;
        }

        if (splits.size() != columns.size()) {
            std::string debug_data("column\tvalue\n\n");
            for (unsigned i(0); i < columns.size(); ++i)
                debug_data += columns.at(i) + "\t" + (i < splits.size() ? splits.at(i) : "<MISSING>") + "\n";

            LOG_ERROR("column length mismatch on line " + std::to_string(line) + "! expected " +
                      std::to_string(columns.size()) + ", found " + std::to_string(splits.size()) +
                      ". debug data:\n" + debug_data);
        }

        // the first and last columns are Z and Mtime respectively
        unsigned id(0);
        if (not StringUtil::ToUnsigned(splits[0], &id)) {
            LOG_WARNING("Couldn't parse Zeder ID on line " + std::to_string(line));
            continue;
        }

        Entry new_entry(id);
        new_entry.setModifiedTimestamp(TimeUtil::StringToStructTm(splits[splits.size() - 1].c_str(), MODIFIED_TIMESTAMP_FORMAT_STRING));

        for (size_t i(1); i < splits.size() - 1; ++i) {
            const auto &attribute_name(columns[i]);
            const auto &attribute_value (splits[i]);
            new_entry.setAttribute(attribute_name, attribute_value);
        }

        if (input_params_->postprocessor_(&new_entry))
            collection->addEntry(new_entry);
    }

    collection->sortEntries();
}


void IniReader::parse(EntryCollection * const collection) {
    const auto params(dynamic_cast<IniReader::Params * const>(input_params_.get()));
    if (not params)
        LOG_ERROR("Invalid input parameters passed to IniReader!");

    for (const auto &section_name : params->valid_section_names_) {
        const auto &section(*config_.getSection(section_name));
        Entry new_entry;

        bool has_zeder_id(false), has_zeder_modified_timestamp(false);
        new_entry.setAttribute(params->section_name_attribute_, section_name);
        for (const auto &entry : section) {
            // skip empty lines
            if (entry.name_.empty())
                continue;

            if (entry.name_ == params->zeder_id_key_) {
                unsigned id(0);
                if (not StringUtil::ToUnsigned(entry.value_, &id))
                    LOG_WARNING("Couldn't parse Zeder ID in section '" + section_name + "'");
                else {
                    new_entry.setId(id);
                    has_zeder_id = true;
                }
            } else if (entry.name_ == params->zeder_last_modified_timestamp_key_) {
                new_entry.setModifiedTimestamp(TimeUtil::StringToStructTm(entry.value_, MODIFIED_TIMESTAMP_FORMAT_STRING));
                has_zeder_modified_timestamp = true;
            } else {
                const auto attribute_name(params->key_to_attribute_map_.find(entry.name_));
                if (attribute_name == params->key_to_attribute_map_.end()) {
                    LOG_DEBUG("Key '" + entry.name_ + "' has no corresponding Zeder attribute. Section: '" +
                              section_name + "', value: '" + entry.value_ + "'");
                } else
                    new_entry.setAttribute(attribute_name->second, entry.value_);
            }
        }

        if (not has_zeder_id or not has_zeder_modified_timestamp) {
            LOG_WARNING("Mandatory fields were not found in section '" + section_name + "' (was it manually added?); ignoring section...");

            std::string debug_print_buffer;
            new_entry.prettyPrint(&debug_print_buffer);
            LOG_DEBUG(debug_print_buffer);
        } else if (input_params_->postprocessor_(&new_entry))
            collection->addEntry(new_entry);
    }

    collection->sortEntries();
}


std::unique_ptr<Exporter> Exporter::Factory(std::unique_ptr<Params> params) {
    auto file_type(GetFileTypeFromPath(params->file_path_, /* check_if_file_exists = */ false));
    switch (file_type) {
    case FileType::INI:
        return std::unique_ptr<Exporter>(new IniWriter(std::move(params)));
    case FileType::CSV:
        return std::unique_ptr<Exporter>(new CsvWriter(std::move(params)));
    default:
        LOG_ERROR("Reader not implemented for file '" + params->file_path_ + "'");
    };
}


IniWriter::IniWriter(std::unique_ptr<Exporter::Params> params): Exporter(std::move(params)) {
    if (FileUtil::Exists(input_params_->file_path_))
        config_.reset(new IniFile(input_params_->file_path_));
    else
        config_.reset(new IniFile(input_params_->file_path_, /* ignore_failed_includes = */ true, /* create_empty = */ true));
}


void IniWriter::writeEntry(IniFile::Section * const section, const std::string &name, const std::string &value) const {
    // merge existing comments
    const auto existing_entry(section->find(name));
    const auto existing_entry_comment(existing_entry != section->end() ? existing_entry->comment_ : "");
    section->insert(name, value, existing_entry_comment, IniFile::Section::DupeInsertionBehaviour::OVERWRITE_EXISTING_VALUE);
}


void IniWriter::write(const EntryCollection &collection) {
    const auto params(dynamic_cast<IniWriter::Params * const>(input_params_.get()));
    char time_buffer[100]{};

    // we assume that the entries are sorted at this point
    for (const auto &entry : collection) {
        config_->appendSection(entry.getAttribute(params->section_name_attribute_));
        auto current_section(config_->getSection(entry.getAttribute(params->section_name_attribute_)));

        writeEntry(&*current_section, params->zeder_id_key_, std::to_string(entry.getId()));

        std::strftime(time_buffer, sizeof(time_buffer), MODIFIED_TIMESTAMP_FORMAT_STRING, &entry.getLastModifiedTimestamp());
        writeEntry(&*current_section, params->zeder_last_modified_timestamp_key_, time_buffer);

        for (const auto &attribute_name : params->attributes_to_export_) {
            if (entry.hasAttribute(attribute_name)) {
                const auto &attribute_value(entry.getAttribute(attribute_name));
                if (not attribute_value.empty())
                    writeEntry(&*current_section, params->attribute_to_key_map_.at(attribute_name), attribute_value);
            } else
                LOG_DEBUG("Attribute '" + attribute_name + "' not found for exporting in entry " + std::to_string(entry.getId()));
        }

        params->extra_keys_appender_(&*current_section, entry);
    }

    config_->write(params->file_path_);
}


void CsvWriter::write(const EntryCollection &collection) {
    const auto params(dynamic_cast<CsvWriter::Params * const>(input_params_.get()));
    char time_buffer[100]{};

    if (params->attributes_to_export_.empty() and not collection.empty()) {
        // add all attributes in the order of iteration
        std::set<std::string> attributes;
        for (const auto &entry : collection)
            for (const auto &attribute_entry : entry)
                attributes.insert(attribute_entry.first);

        for (const auto &attribute : attributes)
            params->attributes_to_export_.emplace_back(attribute);
    }

    std::string header;
    header += TextUtil::CSVEscape(params->zeder_id_column_) + ",";
    for (const auto &column : params->attributes_to_export_)
        header += TextUtil::CSVEscape(column) + ",";
    header += TextUtil::CSVEscape(params->zeder_last_modified_timestamp_column_) + "\n";

    output_file_.write(header);

    for (const auto &entry : collection) {
        std::string row;
        row += TextUtil::CSVEscape(std::to_string(entry.getId())) + ",";

        for (const auto &attribute : params->attributes_to_export_) {
            if (entry.hasAttribute(attribute))
                row += TextUtil::CSVEscape(entry.getAttribute(attribute));
            else {
                LOG_DEBUG("Attribute '" + attribute + "' not found for exporting in entry " + std::to_string(entry.getId()));
                row += TextUtil::CSVEscape("");
            }

            row += ",";
        }

        std::strftime(time_buffer, sizeof(time_buffer), MODIFIED_TIMESTAMP_FORMAT_STRING, &entry.getLastModifiedTimestamp());
        row += TextUtil::CSVEscape(time_buffer) + "\n";

        output_file_.write(row);
    }

    output_file_.close();
}


std::unique_ptr<EndpointDownloader> EndpointDownloader::Factory(Type downloader_type, std::unique_ptr<Params> params) {
    switch (downloader_type) {
    case FULL_DUMP:
        return std::unique_ptr<EndpointDownloader>(new FullDumpDownloader(std::move(params)));
     default:
        LOG_ERROR("Endpoint downloader not implemented for type " + std::to_string(downloader_type));
    }
}


FullDumpDownloader::Params::Params(const std::string &endpoint_path, const std::unordered_set<unsigned> &entries_to_download,
                                   const std::unordered_set<std::string> &columns_to_download,
                                   const std::unordered_map<std::string, std::string> &filter_regexps)
    : EndpointDownloader::Params(endpoint_path), entries_to_download_(entries_to_download),
      columns_to_download_(columns_to_download)
{
    for (const auto &filter_pair : filter_regexps) {
        std::unique_ptr<RegexMatcher> matcher(RegexMatcher::RegexMatcherFactoryOrDie(filter_pair.second));
        filter_regexps_.insert(std::make_pair(filter_pair.first, std::move(matcher)));
    }
}


bool FullDumpDownloader::downloadData(const std::string &endpoint_url, std::shared_ptr<JSON::JSONNode> * const json_data) {
    const IniFile zeder_authentification(UBTools::GetTuelibPath() + "zeder_authentification.conf");

    Downloader::Params downloader_params;
    downloader_params.user_agent_ = "ub_tools/zeder_importer";
    downloader_params.authentication_username_ = zeder_authentification.getString("", "username");
    downloader_params.authentication_password_ = zeder_authentification.getString("", "password");

    const TimeLimit time_limit(20000U);
    Downloader downloader(endpoint_url, downloader_params, time_limit);

    if (downloader.anErrorOccurred()) {
        LOG_WARNING("Couldn't download full from endpoint '" + endpoint_url + "'! Error: " + downloader.getLastErrorMessage());
        return false;
    }

    const int response_code_category(downloader.getResponseCode() / 100);
    switch (response_code_category) {
    case 4:
    case 5:
    case 9:
        LOG_WARNING("Couldn't download full from endpoint '" + endpoint_url + "'! Error Code: "
                    + std::to_string(downloader.getResponseCode()));
        return false;
    }

    JSON::Parser json_parser(downloader.getMessageBody());
    if (not json_parser.parse(json_data)) {
        LOG_WARNING("Couldn't parse JSON response from endpoint '" + endpoint_url + "'! Error: "
                    + json_parser.getErrorMessage());
        return false;
    }

    return true;
}


void FullDumpDownloader::parseColumnMetadata(const std::shared_ptr<JSON::JSONNode> &json_data,
                                             std::unordered_map<std::string, ColumnMetadata> * const column_to_metadata_map)
{
    static const std::unordered_set<std::string> valid_column_types{ "text", "multi", "dropdown" };

    const auto root_node(JSON::JSONNode::CastToObjectNodeOrDie("tree_root", json_data));
    for (const auto &metadata : *root_node->getArrayNode("meta")) {
        const auto metadata_wrapper(JSON::JSONNode::CastToObjectNodeOrDie("entry", metadata));
        const auto column_name(metadata_wrapper->getStringValue("Kurz"));
        const auto column_type(metadata_wrapper->getStringValue("Feldtyp"));

        if (valid_column_types.find(column_type) == valid_column_types.end())
            LOG_ERROR("Unknown type '" + column_type + "' for column '" + column_name + "'");

        ColumnMetadata column_metadata;
        column_metadata.column_type_ = column_type;
        for (const auto &option : *metadata_wrapper->getArrayNode("Optionen")) {
            const auto option_wrapper(JSON::JSONNode::CastToObjectNodeOrDie("entry", option));
            column_metadata.ordinal_to_value_map_[option_wrapper->getIntegerValue("id")] = option_wrapper->getOptionalStringValue("wert");
        }

        column_to_metadata_map->insert(std::make_pair(column_name, column_metadata));
    }
}


void FullDumpDownloader::parseRows(const Params &params, const std::shared_ptr<JSON::JSONNode> &json_data,
                                   const std::unordered_map<std::string, ColumnMetadata> &column_to_metadata_map,
                                   EntryCollection * const collection)
{
    // Validate short column names:
    for (const auto &short_column_name : params.columns_to_download_) {
        if (column_to_metadata_map.find(short_column_name) == column_to_metadata_map.cend())
            LOG_ERROR("can't download unknown short column \"" + short_column_name + "\" from Zeder!");
    }

    const auto root_node(JSON::JSONNode::CastToObjectNodeOrDie("tree_root", json_data));
    for (const auto &data : *root_node->getArrayNode("daten")) {
        const auto data_wrapper(JSON::JSONNode::CastToObjectNodeOrDie("entry", data));
        const auto row_id(data_wrapper->getIntegerValue("DT_RowId"));
        const auto mtime(data_wrapper->getStringValue("Mtime"));

        if (not params.entries_to_download_.empty() and params.entries_to_download_.find(row_id) == params.entries_to_download_.end())
            continue;

        Entry new_entry;
        new_entry.setId(row_id);
        new_entry.setModifiedTimestamp(TimeUtil::StringToStructTm(mtime, MODIFIED_TIMESTAMP_FORMAT_STRING));

        bool skip_entry(false);
        size_t filtered_columns(0);

        for (const auto &field : *data_wrapper) {
            std::string column_name(field.first);
            if (not params.columns_to_download_.empty() and params.columns_to_download_.find(column_name) == params.columns_to_download_.end())
                continue;
            else if (column_name == "DT_RowId" || column_name == "Mtime")
                continue;

            const auto column_metadata(column_to_metadata_map.find(column_name));
            if (column_metadata == column_to_metadata_map.end())
                LOG_ERROR("Unknown column '" + column_name + "'");

            std::string resolved_value;
            if (column_metadata->second.column_type_ == "text")
                resolved_value = JSON::JSONNode::CastToStringNodeOrDie(column_name, field.second)->getValue();
            else if (column_metadata->second.column_type_ == "dropdown") {
                resolved_value = JSON::JSONNode::CastToStringNodeOrDie(column_name, field.second)->getValue();
                if (not resolved_value.empty()) {
                    const auto ordinal(StringUtil::ToInt64T(resolved_value));
                    const auto match(column_metadata->second.ordinal_to_value_map_.find(ordinal));
                    if (match == column_metadata->second.ordinal_to_value_map_.end())
                        LOG_ERROR("Unknown value ordinal " + std::to_string(ordinal) + " in column '" + column_name + "'");

                    resolved_value = match->second;
                }
            } else if (column_metadata->second.column_type_ == "multi") {
                const auto selected_values(JSON::JSONNode::CastToArrayNodeOrDie(column_name, field.second));
                std::vector<std::string> resolved_items;

                for (const auto &entry : *selected_values) {
                    if (entry->getType() != JSON::JSONNode::Type::STRING_NODE)
                        continue;

                    const auto value_string(JSON::JSONNode::CastToStringNodeOrDie("entry", entry)->getValue());
                    const auto ordinal(StringUtil::ToInt64T(value_string));
                    const auto match(column_metadata->second.ordinal_to_value_map_.find(ordinal));
                    if (match == column_metadata->second.ordinal_to_value_map_.end())
                        LOG_ERROR("Unknown value ordinal " + std::to_string(ordinal) + " in column '" + column_name + "'");

                    resolved_items.emplace_back(match->second);
                }

                resolved_value = StringUtil::Join(resolved_items, ',');
            }

            resolved_value = TextUtil::CollapseAndTrimWhitespace(resolved_value);
            auto filter_regex(params.filter_regexps_.find(column_name));
            if (filter_regex != params.filter_regexps_.end()) {
                ++filtered_columns;

                if (not filter_regex->second->matched(resolved_value)) {
                    LOG_DEBUG("Skipping row " + std::to_string(row_id) + " on column '" + column_name + "' reg-ex mismatch");
                    skip_entry = true;
                    break;
                }
            }

            if (not resolved_value.empty())
                new_entry.setAttribute(column_name, resolved_value);
        }

        if (filtered_columns != params.filter_regexps_.size()) {
            // at least one filter was not applied, skip the entry
            skip_entry = true;
        }

        if (not skip_entry)
            collection->addEntry(new_entry);
    }

    collection->sortEntries();
}


bool IsCacheUpToDate(const std::string &zeder_cache_path) {
    timespec mtim, now;
    clock_gettime(CLOCK_REALTIME, &now);
    if (not FileUtil::GetLastModificationTimestamp(zeder_cache_path, &mtim))
        return false;
    else {
        double dif = std::difftime(now.tv_sec, mtim.tv_sec);
        if (dif > 60.0 /*min*/ * 60.0 /*sec*/)
            return false;
    }
    return true;
}


bool FullDumpDownloader::download(EntryCollection * const collection, const bool disable_cache_mechanism) {
    const auto params(dynamic_cast<FullDumpDownloader::Params * const>(downloader_params_.get()));
    if (unlikely(params == nullptr))
        LOG_ERROR("dynamic_cast failed!");

    std::unordered_map<std::string, ColumnMetadata> column_to_metadata_map;
    std::shared_ptr<JSON::JSONNode> json_data;
    std::shared_ptr<JSON::JSONNode> json_cached_data;

    bool use_cache = not disable_cache_mechanism;
    bool cache_present = false;
    bool downloaded = false;
    std::string zeder_cache_path = UBTools::GetTuelibPath();
    if (StringUtil::EndsWith(params->endpoint_url_, "ixtheo", true))
        zeder_cache_path += "zeder_ixtheo.json";
    else if (StringUtil::EndsWith(params->endpoint_url_, "krim", true))
        zeder_cache_path += "zeder_krimdok.json";
    else
        use_cache = false;

    if (use_cache) {
        std::string json_document;
        if (not FileUtil::ReadString(zeder_cache_path, &json_document))
            use_cache = false;
        else {
            JSON::Parser json_parser(json_document);
            if (not json_parser.parse(&json_cached_data))
                use_cache = false;
            else {
                cache_present = true;
                if (not IsCacheUpToDate(zeder_cache_path))
                    use_cache = false;
            }
        }            
    }

    if (not use_cache) {
        if (not downloadData(params->endpoint_url_, &json_data)) {
            if (not cache_present) {
                LOG_WARNING("Zeder Download and also Zeder cache failed");
                return false;
            }
            else {
                json_data = std::move(json_cached_data);
                LOG_INFO("Used zeder cache file due to failed zeder download");
            }
        }
        else
            downloaded = true;
    }
    else {
        json_data = std::move(json_cached_data);
        LOG_INFO("Used zeder cache file");
    }

    if (downloaded)
    {
        FileUtil::WriteString(zeder_cache_path, json_data->toString());
    }
    parseColumnMetadata(json_data, &column_to_metadata_map);
    parseRows(*params, json_data, column_to_metadata_map,  collection);
    return true;
}


std::string GetFullDumpEndpointPath(Flavour zeder_flavour) {
    //if the URL changes FullDumpDownloader::download might also be changed
    static const std::string endpoint_base_url("http://www-ub.ub.uni-tuebingen.de/zeder/cgi-bin/zeder.cgi?"
                                               "action=get&Dimension=wert&Bearbeiter=&Instanz=");
    switch (zeder_flavour) {
    case IXTHEO:
        return endpoint_base_url + "ixtheo";
    case KRIMDOK:
        return endpoint_base_url + "krim";
    default:
        LOG_ERROR("we should *never* get here! (zeder_flavour=" + std::to_string(zeder_flavour) + ")");
    }
}


Flavour ParseFlavour(const std::string &flavour, const bool case_sensitive) {
    std::string flavour_str(flavour);
    std::string ixtheo_str(FLAVOUR_TO_STRING_MAP.at(IXTHEO));
    std::string krimdok_str(FLAVOUR_TO_STRING_MAP.at(KRIMDOK));

    if (not case_sensitive) {
        StringUtil::ASCIIToLower(&flavour_str);
        StringUtil::ASCIIToLower(&ixtheo_str);
        StringUtil::ASCIIToLower(&krimdok_str);
    }

    if (flavour_str == ixtheo_str)
        return IXTHEO;
    else if (flavour_str == krimdok_str)
        return KRIMDOK;
    else {
        LOG_ERROR("unknown Zeder flavour '" + flavour + "'");
    }
}


SimpleZeder::SimpleZeder(const Flavour flavour, const std::unordered_set<std::string> &column_filter) {
    const auto endpoint_url(GetFullDumpEndpointPath(flavour));
    const std::unordered_map<std::string, std::string> filter_regexps {}; // intentionally empty
    const std::unordered_set<unsigned> entries_to_download; // empty means all entries
    std::unique_ptr<FullDumpDownloader::Params> downloader_params(
        new FullDumpDownloader::Params(endpoint_url, entries_to_download, column_filter, filter_regexps));

    auto downloader(Zeder::FullDumpDownloader::Factory(FullDumpDownloader::Type::FULL_DUMP, std::move(downloader_params)));
    failed_to_connect_to_database_server_ = not downloader->download(&entries_);
}


} // namespace Zeder

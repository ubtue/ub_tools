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
#include "FileUtil.h"
#include "Zeder.h"


namespace Zeder {


const std::string &Entry::getAttribute(const std::string &name) const {
    const auto match(attributes_.find(name));
    if (match == attributes_.end())
        LOG_ERROR("Couldn't find attribute '" + name + "'! in entry " + std::to_string(id_));
    else
        return match->second;
}


void Entry::setAttribute(const std::string &name, const std::string &value, bool overwrite) {
    if (attributes_.find(name) == attributes_.end() or overwrite)
        attributes_[name] = value;
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
        print_buffer->append("\t{").append(attribute.first).append("} -> '").append(attribute.second).append("'\n");
}


void Entry::DiffResult::prettyPrint(std::string * const print_buffer) const {
    *print_buffer = "Diff " + std::to_string(id_) + ":\n";
    for (const auto &attribute : modified_attributes_) {
        if (not attribute.second.first.empty()) {
            print_buffer->append("\t{").append(attribute.first).append("} -> '").append(attribute.second.first)
                                       .append("' => '").append(attribute.second.second).append("'\n");
        } else
            print_buffer->append("\t{").append(attribute.first).append("} -> '").append(attribute.second.second).append("'\n");
    }
}


Entry::DiffResult Entry::Diff(const Entry &lhs, const Entry &rhs, const bool skip_timestamp_check) {
    if (lhs.getId() != rhs.getId())
        LOG_ERROR("Can only diff revisions of the same entry! LHS = " + std::to_string(lhs.getId()) + ", RHS = "
                  + std::to_string(rhs.getId()));

    DiffResult delta{ false, rhs.getId(), rhs.getLastModifiedTimestamp() };
    if (not skip_timestamp_check) {
        const auto time_difference(TimeUtil::DiffStructTm(rhs.getLastModifiedTimestamp(), lhs.getLastModifiedTimestamp()));
        if (time_difference < 0) {
            LOG_WARNING("The existing entry " + std::to_string(rhs.getId()) + " is newer than the diff by "
                        + std::to_string(time_difference) + " seconds");
        } else
            delta.timestamp_is_newer_ = true;
    } else {
        delta.timestamp_is_newer_ = true;
        delta.last_modified_timestamp_ = TimeUtil::GetCurrentTimeGMT();
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
                    "Timestamp difference: " + std::to_string(time_difference) + " seconds");
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

        new_entry.setAttribute(params->section_name_attribute_, section_name);
        for (const auto &entry : section) {
            // skip empty lines
            if (entry.name_.empty())
                continue;

            if (entry.name_ == params->zeder_id_key_) {
                unsigned id(0);
                if (not StringUtil::ToUnsigned(entry.value_, &id))
                    LOG_WARNING("Couldn't parse Zeder ID in section '" + section_name + "'");
                else
                    new_entry.setId(id);
            } else if (entry.name_ == params->zeder_last_modified_timestamp_key_)
                new_entry.setModifiedTimestamp(TimeUtil::StringToStructTm(entry.value_, MODIFIED_TIMESTAMP_FORMAT_STRING));
            else {
                const auto attribute_name(params->key_to_attribute_map_.find(entry.name_));
                if (attribute_name == params->key_to_attribute_map_.end()) {
                    LOG_DEBUG("Key '" + entry.name_ + "' has no corresponding Zeder attribute. Section: '" +
                              section_name + "', value: '" + entry.value_ + "'");
                } else
                    new_entry.setAttribute(attribute_name->second, entry.value_);
            }
        }

        struct tm last_modified(new_entry.getLastModifiedTimestamp());
        if (new_entry.getId() == 0 or std::mktime(&last_modified) == -1) {
            LOG_WARNING("Mandatory fields were not found in section '" + section_name + "'");

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


void IniWriter::write(const EntryCollection &collection) {
    const auto params(dynamic_cast<IniWriter::Params * const>(input_params_.get()));
    char time_buffer[100]{};

    // we assume that the entries are sorted at this point
    for (const auto &entry : collection) {
        config_->appendSection(entry.getAttribute(params->section_name_attribute_));
        auto current_section(config_->getSection(entry.getAttribute(params->section_name_attribute_)));

        current_section->insert(params->zeder_id_key_, std::to_string(entry.getId()), "", IniFile::Section::DupeInsertionBehaviour::OVERWRITE_EXISTING_VALUE);

        std::strftime(time_buffer, sizeof(time_buffer), MODIFIED_TIMESTAMP_FORMAT_STRING, &entry.getLastModifiedTimestamp());
        current_section->insert(params->zeder_last_modified_timestamp_key_, time_buffer, "", IniFile::Section::DupeInsertionBehaviour::OVERWRITE_EXISTING_VALUE);

        for (const auto &attribute_name : params->attributes_to_export_) {
            if (entry.hasAttribute(attribute_name)) {
                const auto &attribute_value(entry.getAttribute(attribute_name));
                if (not attribute_value.empty()) {
                    current_section->insert(params->attribute_to_key_map_.at(attribute_name), attribute_value,
                                            "", IniFile::Section::DupeInsertionBehaviour::OVERWRITE_EXISTING_VALUE);
                }
            } else
                LOG_DEBUG("Attribute '" + attribute_name + "' not found for exporting in entry " + std::to_string(entry.getId()));
        }

        params->extra_keys_appender_(&*current_section, entry);
    }

    config_->write(params->file_path_);
}


} // namespace Zeder

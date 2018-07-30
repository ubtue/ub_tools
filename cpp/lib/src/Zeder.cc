#include "FileUtil.h"
#include "Zeder.h"


namespace Zeder {


inline void Entry::setModifiedTimestamp(const tm &timestamp) {
    std::memcpy(&last_modified_timestamp_, &timestamp, sizeof(timestamp));
}


const std::string &Entry::getAttribute(const std::string &name) const {
    if (not hasAttribute(name))
        LOG_ERROR("Couldn't find attribute '" + name + "'! in entry " + std::to_string(id_));
    else
        return attributes_.at(name);
}


void Entry::setAttribute(const std::string &name, const std::string &value, bool overwrite) {
    if (attributes_.find(name) == attributes_.end() or overwrite)
        attributes_[name] = value;
    else
        LOG_ERROR("Attribute '" + name + "' already exists in entry " + std::to_string(id_));
}


void Entry::removeAttribute(const std::string &name) {
    if (not hasAttribute(name))
        LOG_ERROR("Couldn't find attribute '" + name + "'! in entry " + std::to_string(id_));
    else
        attributes_.erase(attributes_.find(name));
}


unsigned Entry::keepAttributes(const std::vector<std::string> &names_to_keep) {
    std::vector<std::string> names_to_remove;
    for (const auto &key_value : attributes_) {
        if (std::find(names_to_keep.begin(), names_to_keep.end(), key_value.first) == names_to_keep.end())
            names_to_remove.push_back(key_value.first);
    }

    for (const auto &name : names_to_remove)
        attributes_.erase(attributes_.find(name));
    return names_to_remove.size();
}


Entry::DiffResult Entry::Diff(const Entry &lhs, const Entry &rhs, const bool skip_timestamp_check) {
    if (lhs.getId() != rhs.getId())
        LOG_ERROR("Can only diff revisions of the same entry! LHS = " + std::to_string(lhs.getId()) + ", RHS = " + std::to_string(rhs.getId()));

    DiffResult delta{ false, rhs.getId(), rhs.getLastModifiedTimestamp(), {} };
    if (not skip_timestamp_check) {
        const auto time_difference(TimeUtil::DiffStructTm(rhs.getLastModifiedTimestamp(), lhs.getLastModifiedTimestamp()));
        if (time_difference <= 0) {
            LOG_WARNING("The existing entry " + std::to_string(rhs.getId()) + " is newer than the diff by "
                    + std::to_string(time_difference) + " seconds");
        } else
            delta.is_timestamp_newer = true;
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

    merge_into->setModifiedTimestamp(delta.last_modified_timestamp_);
    for (const auto &key_value : delta.modified_attributes_)
        merge_into->setAttribute(key_value.first, key_value.second.second, true);
}



inline void EntryCollection::sortEntries() {
    std::sort(entries_.begin(), entries_.end(),
              [](const Entry &a, const Entry &b) { return a.getId() < b.getId(); });
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


bool EntryCollection::mergeEntry(const Entry &diff, std::unordered_map<std::string, std::pair<std::string, std::string>> * const modified_attributes,
                                 const bool skip_timestamp_check, const bool add_if_absent)
{
    const iterator into(find(diff.getId()));
    std::unordered_map<std::string, std::pair<std::string, std::string>> modified_attributes_buffer;
    modified_attributes_buffer.reserve(diff.size());

    if (into == end()) {
        if (add_if_absent) {
            LOG_INFO("New entry " + std::to_string(diff.getId()) + " merged into config data");

            addEntry(diff);
            for (const auto &key_value : diff)
                modified_attributes_buffer[key_value.first] = std::make_pair("", key_value.second);
        } else
            LOG_INFO("New entry " + std::to_string(diff.getId()) + " not merged into config data");
    } else {
        if (not skip_timestamp_check) {
            const auto time_difference(TimeUtil::DiffStructTm(diff.getLastModifiedTimestamp(), into->getLastModifiedTimestamp()));
            if (time_difference <= 0) {
                LOG_ERROR("The existing entry " + std::to_string(diff.getId()) + " is newer than the diff by "
                        + std::to_string(time_difference) + " seconds");
            }
        }

        into->setModifiedTimestamp(diff.getLastModifiedTimestamp());
        for (const auto &key_value : diff) {
            const auto &attribute_name(key_value.first), &attribute_value(key_value.second);

            if (into->hasAttribute(attribute_name)) {
                const auto value(into->getAttribute(attribute_name));
                if (value != attribute_value) {
                    // copy the old value before updating it (since it's a reference)
                    modified_attributes_buffer[attribute_name] = std::make_pair(value, attribute_value);
                    into->setAttribute(attribute_name, attribute_value, true);
                }
            } else {
                modified_attributes_buffer[attribute_name] = std::make_pair("", attribute_value);
                into->setAttribute(attribute_name, attribute_value, true);
            }
        }
    }

    bool modified(not modified_attributes_buffer.empty());
    if (modified_attributes)
        modified_attributes->swap(modified_attributes_buffer);
    return modified;
}


inline EntryCollection::iterator EntryCollection::find(const unsigned id) {
    return std::find_if(entries_.begin(), entries_.end(),
                        [id] (const Entry &entry) { return entry.getId() == id; });
}

inline EntryCollection::const_iterator EntryCollection::find(const unsigned id) const {
    return std::find_if(entries_.begin(), entries_.end(),
                        [id] (const Entry &entry) { return entry.getId() == id; });
}


FileType GetFileTypeFromPath(const std::string &path) {
    if (FileUtil::Exists(path)) {
        if (StringUtil::EndsWith(path, ".csv", /* ignore_case = */true))
            return FileType::CSV;
        if (StringUtil::EndsWith(path, ".json", /* ignore_case = */true))
            return FileType::JSON;
        if (StringUtil::EndsWith(path, ".conf", /* ignore_case = */true) or StringUtil::EndsWith(path, ".ini", /* ignore_case = */true) )
            return FileType::INI;
    }

    LOG_ERROR("can't guess the file type of \"" + path + "\"!");
}


std::unique_ptr<Importer> Importer::Factory(std::unique_ptr<Params> &&params) {
    auto file_type(GetFileTypeFromPath(params->file_path_));
    switch (file_type) {
    case FileType::CSV:
        return std::unique_ptr<CsvReader>(new CsvReader(std::forward<std::unique_ptr<Params>>(params)));
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
                LOG_ERROR("Mandatory fields were not found!");

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
    IniReader::Params * const params(dynamic_cast<IniReader::Params * const>(input_params_.get()));
    if (not params)
        LOG_ERROR("Invalid input parameters passed to IniReader!");

    for (const auto &section_name : params->valid_section_names_) {
        const auto &section(*config_.getSection(section_name));
        Entry new_entry;

        new_entry.setAttribute(params->section_name_attribute_, section_name);
        for (const auto &entry : section) {
            const auto attribute_name(params->key_to_attribute_map_.find(entry.name_));
            if (attribute_name == params->key_to_attribute_map_.end())
                LOG_DEBUG("Key '" + entry.name_ + "' has no corresponding Zeder attribute.");
            else {
                if (attribute_name->second == MANDATORY_FIELD_TO_STRING_MAP.at(Z)) {
                    unsigned id(0);
                    if (not StringUtil::ToUnsigned(attribute_name->second, &id))
                        LOG_WARNING("Couldn't parse Zeder ID in section '" + section_name + "'");
                    else
                        new_entry.setId(id);
                } else if (attribute_name->second == MANDATORY_FIELD_TO_STRING_MAP.at(MTIME))
                    new_entry.setModifiedTimestamp(TimeUtil::StringToStructTm(entry.value_, MODIFIED_TIMESTAMP_FORMAT_STRING));
                else
                    new_entry.setAttribute(attribute_name->second, entry.value_);
            }
        }

        struct tm last_modified(new_entry.getLastModifiedTimestamp());
        if (not(new_entry.getId() == 0) or std::mktime(&last_modified) == -1)
            LOG_WARNING("Mandatory fields were not found in section '" + section_name + "'");
        else if (input_params_->postprocessor_(&new_entry))
            collection->addEntry(new_entry);
    }

    collection->sortEntries();
}


std::unique_ptr<Exporter> Exporter::Factory(std::unique_ptr<Params> &&params) {
    auto file_type(GetFileTypeFromPath(params->file_path_));
    switch (file_type) {
    case FileType::INI:
        return std::unique_ptr<IniWriter>(new IniWriter(std::forward<std::unique_ptr<Params>>(params)));
    default:
        LOG_ERROR("Reader not implemented for file '" + params->file_path_ + "'");
    };
}


}

/** \file   BeaconFile.h
 *  \brief  Interface for the BeaconFile class.
 *  \author Dr. Johannes Ruscheinski (johannes.ruscheinski@uni-tuebingen.de)
 *
 *  \copyright 2019 Universitätsbibliothek Tübingen.  All rights reserved.
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
#pragma once


#include <map>
#include <string>
#include <unordered_set>
#include "util.h"


class BeaconFile {
public:
    struct Entry {
        std::string gnd_number_;
        unsigned optional_count_;
        std::string id_or_url_;

    public:
        Entry() = default;
        Entry(const Entry &other) = default;
        Entry(const std::string &gnd_number, const unsigned optional_count, const std::string &id_or_url)
            : gnd_number_(gnd_number), optional_count_(optional_count), id_or_url_(id_or_url) { }

        Entry &operator=(const Entry &rhs) = default;
        inline bool operator==(const Entry &rhs) const { return rhs.gnd_number_ == gnd_number_; }
    };

    class EntryHasher {
    public:
        inline std::size_t operator()(const Entry &entry) const { return std::hash<std::string>()(entry.gnd_number_); }
    };

private:
    std::string filename_;
    std::string url_template_;
    std::unordered_set<Entry, EntryHasher> entries_;
    std::map<std::string, std::string> keys_and_values_;

public:
    typedef std::unordered_set<Entry, EntryHasher>::const_iterator const_iterator;

public:
    explicit BeaconFile(const std::string &filename);

    inline size_t size() const { return entries_.size(); }
    inline const std::string &getFileName() const { return filename_; }
    inline const std::string &getUrlTemplate() const { return url_template_; }
    std::string getURL(const Entry &entry) const;

    inline const_iterator begin() const { return entries_.cbegin(); }
    inline const_iterator end() const { return entries_.cend(); }
    inline const_iterator find(const std::string &gnd_number) const { return entries_.find(Entry(gnd_number, 0, "")); }

    /** \return the value if the metadatum with name "name" exists o/w the empty string */
    inline std::string getMetadatum(const std::string &name) const {
        const auto key_and_value(keys_and_values_.find(name));
        return (key_and_value == keys_and_values_.cend()) ? "" : key_and_value->second;
    }

    /** \return a descriptive name for the Beacon source */
    std::string getName() const;
};

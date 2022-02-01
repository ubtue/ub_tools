/** \file    add_authority_beacon_information.cc
 *  \brief   Adds BEACON information to authority files
 *  \author  Dr. Johannes Ruscheinski
 *
 *  Copyright (C) 2018,2019 Library of the University of Tübingen
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

#include <cstdlib>
#include "BeaconFile.h"
#include "MARC.h"
#include "FileUtil.h"
#include "StringUtil.h"
#include "util.h"
#include <iostream>


namespace {


class TypeFile {
    struct Entry {
        std::string gnd_number_;
        std::vector<std::string> types_;
    public:
        Entry() = default;
        Entry(const Entry &other) = default;
        Entry(const std::string gnd_number, const std::vector<std::string> &types): gnd_number_(gnd_number), types_(types) {}
        inline bool operator<(const Entry &rhs) { return gnd_number_ < rhs.gnd_number_; }
        inline bool operator==(const Entry &rhs) const { return gnd_number_ == rhs.gnd_number_; }
        inline bool operator()(const Entry &rhs) { return gnd_number_ < rhs.gnd_number_; }
    };

    class EntryHasher {
    public:
        inline std::size_t operator()(const Entry &entry) const { return std::hash<std::string>()(entry.gnd_number_); }
    };

    private:
        const std::string filename_;
        std::unordered_set<Entry, EntryHasher> entries_;
    public:
        typedef std::unordered_set<Entry, EntryHasher>::const_iterator const_iterator;
    public:
        explicit TypeFile(const std::string filename) : filename_(filename) {
            unsigned line_no(0);
            const auto input(FileUtil::OpenInputFileOrDie(filename));
            while (not input->eof()) {
                const std::string line(input->getLineAny());
                ++line_no;
                std::vector<std::string> gnd_and_types;
                if (StringUtil::Split(line, std::string(" - "), &gnd_and_types) != 2)
                    LOG_ERROR("Invalid type file " + filename + " in line " + std::to_string(line_no));
                std::vector<std::string> types;
                StringUtil::SplitThenTrimWhite(gnd_and_types[1], ',', &types);
                entries_.emplace(Entry(StringUtil::TrimWhite(gnd_and_types[0]), types));
            }
        }
        inline const_iterator begin() const { return entries_.cbegin(); }
        inline const_iterator end() const { return entries_.cend(); }
        inline const_iterator find(const std::string &gnd_number) const { return entries_.find(Entry(gnd_number, {})); };
};


void ProcessAuthorityRecords(MARC::Reader * const authority_reader, MARC::Writer * const authority_writer,
                             const std::vector<BeaconFile> &beacon_files,
                             const std::map<std::string, TypeFile> &beacon_to_type_files_map)
{
    unsigned gnd_tagged_count(0);
    while (auto record = authority_reader->read()) {
        std::string gnd_number;
        if (MARC::GetGNDCode(record, &gnd_number)) {
            for (const auto &beacon_file : beacon_files) {
                const auto beacon_entry(beacon_file.find(gnd_number));
                if (beacon_entry == beacon_file.end())
                    continue;

                ++gnd_tagged_count;
                std::string beacon_file_filename = beacon_file.getFileName();
                std::string beacon_url = beacon_file.getURL(*beacon_entry);

                // special substiutions due to individual beacon configurations
                StringUtil::ReplaceString("deutsche-biographie.de/pnd", "deutsche-biographie.de/", beacon_url);

                if (beacon_file_filename.find(".lr.") != beacon_file_filename.npos)
                    record.insertField("BEA", { { 'a', beacon_file.getName() }, { 'u', beacon_url }, { '0', "lr" } });
                else
                    record.insertField("BEA", { { 'a', beacon_file.getName() }, { 'u', beacon_url } });
                if (beacon_to_type_files_map.find(beacon_file.getFileName()) != beacon_to_type_files_map.end()) {
                    const TypeFile &type_file(beacon_to_type_files_map.at(beacon_file_filename));
                    const auto &type_entry(type_file.find(gnd_number));
                    if (type_entry != type_file.end()) {
                        for (const auto &type : type_entry->types_)
                            record.addSubfield("BEA", 'v', type);
                    }
                }
            }
        }

        authority_writer->write(record);
    }

    LOG_INFO("tagged " + std::to_string(gnd_tagged_count) + " author records with beacon links.");
}


} // unnamed namespace


int Main(int argc, char **argv) {
    if (argc < 4)
        ::Usage("authority_records augmented_authority_records [beacon_list1 [--type-file type_file1] beacon_list2 [--type-file type_file2] .. beacon_listN [--type-file type-fileN]");

    const std::string authority_records_filename(argv[1]);
    const std::string augmented_authority_records_filename(argv[2]);

    if (unlikely(authority_records_filename == augmented_authority_records_filename))
        LOG_ERROR("Authority data input file name equals authority output file name!");

    auto authority_reader(MARC::Reader::Factory(authority_records_filename));
    auto authority_writer(MARC::Writer::Factory(augmented_authority_records_filename));

    std::vector<BeaconFile> beacon_files;
    std::map<std::string, TypeFile> beacon_to_type_files_map;

    for (int arg_no(3); arg_no < argc; ++arg_no) {
        if (std::strcmp(argv[arg_no], "--type-file") == 0) {
            if (not (arg_no + 1 < argc))
                LOG_ERROR("No typefile given");
            if (arg_no - 1 < 3)
                LOG_ERROR("No beacon file given for type file " + std::string(argv[arg_no + 1]));
            beacon_to_type_files_map.emplace(argv[arg_no - 1], TypeFile(argv[arg_no + 1]));
            ++arg_no;
        } else
            beacon_files.emplace_back(argv[arg_no]);
    }


    ProcessAuthorityRecords(authority_reader.get(), authority_writer.get(), beacon_files, beacon_to_type_files_map);

    return EXIT_SUCCESS;
}

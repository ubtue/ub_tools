/** \file    map_ddc_and_rvk_to_ixtheo_notations.cc
 *  \brief   Map certain DDC and RVK categories to ixTheo notations and add them to field 652a.
 *  \author  Dr. Johannes Ruscheinski
 */

/*
    Copyright (C) 2015-2018, Library of the University of Tübingen

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU Affero General Public License as
    published by the Free Software Foundation, either version 3 of the
    License, or (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Affero General Public License for more details.

    You should have received a copy of the GNU Affero General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <algorithm>
#include <iostream>
#include <iterator>
#include <unordered_map>
#include <cctype>
#include <cstdlib>
#include "MARC.h"
#include "StringUtil.h"
#include "util.h"


namespace {


[[noreturn]] void Usage() {
    std::cerr << "Usage: " << ::progname << " [--verbose] marc_input marc_output ddc_to_ixtheo_notations_map "
              << "rvk_to_ixtheo_notations_map\n";
    std::exit(EXIT_FAILURE);
}


/** \class IxTheoMapper
 *  \brief Maps from a DDC or RVK hierarchy entry to an IxTheo notation.
 */
class IxTheoMapper {
    std::string from_hierarchy_;
    std::string to_ix_theo_notation_;
    std::vector<std::string> exclusions_;
public:
    explicit IxTheoMapper(const std::vector<std::string> &map_file_line);

    /** \brief Returns an IxTheo notation if we can match "hierarchy_classification".  O/w we return the empty string. */
    std::string map(const std::string &hierarchy_classification) const;
};


IxTheoMapper::IxTheoMapper(const std::vector<std::string> &map_file_line) {
    if (map_file_line.size() < 2)
        throw std::runtime_error("in IxTheoMapper::IxTheoMapper: need at least 2 elements in \"map_file_line\"!");
    from_hierarchy_ = map_file_line[0];
    to_ix_theo_notation_ = map_file_line[1];
    std::copy(map_file_line.begin() + 2, map_file_line.end(), std::back_inserter(exclusions_));
}


std::string IxTheoMapper::map(const std::string &hierarchy_classification) const {
    if (not StringUtil::StartsWith(hierarchy_classification, from_hierarchy_))
        return "";

    for (const auto &exclusion : exclusions_) {
        if (StringUtil::StartsWith(hierarchy_classification, exclusion))
            return "";
    }

    return to_ix_theo_notation_;
}


void LoadCSVFile(const std::string &filename, std::vector<IxTheoMapper> * const mappers) {
    DSVReader csv_reader(filename);
    std::vector<std::string> csv_values;
    while (csv_reader.readLine(&csv_values))
        mappers->emplace_back(csv_values);

    std::cerr << "Read " << mappers->size() << " mappings from \"" << filename << "\".\n";
}


void UpdateIxTheoNotations(const std::vector<IxTheoMapper> &mappers, const std::vector<std::string> &orig_values,
                           std::string * const ixtheo_notations_list)
{
    std::vector<std::string> ixtheo_notations_vector;
    StringUtil::Split(*ixtheo_notations_list, ':', &ixtheo_notations_vector);
    std::set<std::string> previously_assigned_notations(std::make_move_iterator(ixtheo_notations_vector.begin()),
                                                        std::make_move_iterator(ixtheo_notations_vector.end()));

    for (const auto &mapper : mappers) {
        for (const auto &orig_value : orig_values) {
            const std::string mapped_value(mapper.map(orig_value));
            if (not mapped_value.empty()
                and previously_assigned_notations.find(mapped_value) == previously_assigned_notations.end())
            {
                if (not ixtheo_notations_list->empty())
                    *ixtheo_notations_list += ':';
                *ixtheo_notations_list += mapped_value;
                previously_assigned_notations.insert(mapped_value);
            }
        }
    }
}


void ProcessRecords(MARC::Reader * const marc_reader, MARC::Writer * const marc_writer,
                    const std::vector<IxTheoMapper> &ddc_to_ixtheo_notation_mappers,
                    const std::vector<IxTheoMapper> &/*rvk_to_ixtheo_notation_mappers*/)
{
    unsigned count(0), records_with_ixtheo_notations(0), records_with_new_notations(0), skipped_group_count(0);
    while (MARC::Record record = marc_reader->read()) {
        ++count;

        std::string ixtheo_notations_list(record.getFirstSubfieldValue("652", 'a'));
        if (not ixtheo_notations_list.empty()) {
            ++records_with_ixtheo_notations;
            marc_writer->write(record);
            continue;
        }

        std::vector<std::string> ddc_values(record.getSubfieldValues("082", 'a'));
        if (ddc_values.empty()) {
            marc_writer->write(record);
            continue;
        }

        // "K" stands for children's literature and "B" stands for fiction, both of which we don't want to
        // import into IxTheo;
        if (std::find(ddc_values.cbegin(), ddc_values.cend(), "K") != ddc_values.cend()
            or std::find(ddc_values.cbegin(), ddc_values.cend(), "B") != ddc_values.cend())
        {
            ++skipped_group_count;
            marc_writer->write(record);
            continue;
        }

        // Many DDC's have superfluous backslashes which are non-standard and need to be removed before further
        // processing can take place:
        for (auto &ddc_value : ddc_values)
            StringUtil::RemoveChars("/", &ddc_value);

        UpdateIxTheoNotations(ddc_to_ixtheo_notation_mappers, ddc_values, &ixtheo_notations_list);
        if (not ixtheo_notations_list.empty())
            LOG_DEBUG(record.getControlNumber() + ": " + StringUtil::Join(ddc_values, ',') + " -> " + ixtheo_notations_list);

/*
        std::vector<std::string> rvk_values;
        int _084_index(record.getFieldIndex("084"));
        if (_084_index != -1) {
            const std::vector<std::string> &fields(record.getFields());
            while (_084_index < static_cast<ssize_t>(dir_entries.size()) and dir_entries[_084_index].getTag() == "084") {
                const Subfields subfields(fields[_084_index]);
                if (subfields.hasSubfieldWithValue('2', "rvk"))
                    rvk_values.emplace_back(subfields.getFirstSubfieldValue('a'));
                ++_084_index;
            }
        }
        UpdateIxTheoNotations(rvk_to_ixtheo_notation_mappers, rvk_values, &ixtheo_notations_list);
*/

        if (not ixtheo_notations_list.empty()) {
            ++records_with_new_notations;
            record.insertField("652", "  ""\x1F""a" + ixtheo_notations_list + "\x1F""bDDCoderRVK");
        }

        marc_writer->write(record);
    }

    std::cout << ::progname << ": Read " << count << " records.\n";
    std::cout << ::progname << ": " << records_with_ixtheo_notations << " records had Ixtheo notations.\n";
    std::cout << ::progname << ": " << records_with_new_notations << " records received new Ixtheo notations.\n";
    std::cout << ::progname << ": " << skipped_group_count
              << " records where skipped because they were in a group that we are not interested in.\n";
}


} // unnamed namespace


int Main(int argc, char **argv) {
    if (argc < 5)
        Usage();

    auto marc_reader(MARC::Reader::Factory(argv[1]));
    auto marc_writer(MARC::Writer::Factory(argv[2]));

    std::vector<IxTheoMapper> ddc_to_ixtheo_notation_mappers;
    LoadCSVFile(argv[3], &ddc_to_ixtheo_notation_mappers);

    std::vector<IxTheoMapper> rvk_to_ixtheo_notation_mappers;
    // The RVK notations were never provided to us by the librarians
    // So, we don't  use it at the moment
    //LoadCSVFile(argv[4], &rvk_to_ixtheo_notation_mappers);

    ProcessRecords(marc_reader.get(), marc_writer.get(), ddc_to_ixtheo_notation_mappers,
                   rvk_to_ixtheo_notation_mappers);
    return EXIT_SUCCESS;
}

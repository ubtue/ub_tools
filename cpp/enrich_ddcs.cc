/** \brief A tool to add DDC metadata to title data using various means.
 *  \author Dr. Johannes Ruscheinski (johannes.ruscheinski@uni-tuebingen.de)
 *
 *  \copyright 2015-2019 Universitätsbibliothek Tübingen.  All rights reserved.
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

#include <iostream>
#include <memory>
#include <set>
#include <unordered_map>
#include <cstdlib>
#include <cstring>
#include "MARC.h"
#include "RegexMatcher.h"
#include "StringUtil.h"
#include "util.h"


namespace {


[[noreturn]] void Usage() {
    std::cerr << "usage: " << ::progname << " input_title_data norm_data output_title_data\n";
    std::exit(EXIT_FAILURE);
}


bool IsPossibleDDC(const std::string &ddc_candidate) {
    static RegexMatcher * const matcher(RegexMatcher::RegexMatcherFactory("^\\d\\d\\d"));
    std::string err_msg;
    if (not matcher->matched(ddc_candidate, &err_msg)) {
        if (err_msg.empty())
            return false;
        LOG_ERROR("unexpected regex error while trying to match \"" + ddc_candidate + "\": " + err_msg);
    }

    return true;
}


void ExtractDDCsFromField(const std::string &tag, const MARC::Record &record, std::set<std::string> * const ddcs) {
    for (const auto &field : record.getTagRange(tag)) {
        const auto subfields(field.getSubfields());
        if (subfields.hasSubfield('z')) // Auxillary table number => not a regular DDC in $a!
            continue;

        for (auto ddc : subfields.extractSubfields('a')) {
            if (IsPossibleDDC(ddc))
                ddcs->insert(ddc);
        }
    }
}


void ExtractDDCsFromAuthorityData(MARC::Reader * const authority_reader,
                                  std::unordered_map<std::string, std::set<std::string>> * const norm_ids_to_ddcs_map) {
    norm_ids_to_ddcs_map->clear();
    LOG_INFO("Starting loading of norm data.");

    unsigned count(0), ddc_record_count(0);
    while (const MARC::Record record = authority_reader->read()) {
        ++count;

        if (not record.hasTag("001"))
            continue;

        std::set<std::string> ddcs;
        ExtractDDCsFromField("083", record, &ddcs);
        ExtractDDCsFromField("089", record, &ddcs);

        if (not ddcs.empty()) {
            ++ddc_record_count;
            norm_ids_to_ddcs_map->insert(std::make_pair(record.getControlNumber(), ddcs));
        }
    }

    LOG_INFO("Read " + std::to_string(count) + " norm data records.");
    LOG_INFO(std::to_string(ddc_record_count) + " records had DDC entries.");
}


void ExtractTopicIDs(const std::string &tags, const MARC::Record &record, const std::set<std::string> &existing_ddcs,
                     std::set<std::string> * const topic_ids) {
    topic_ids->clear();

    std::vector<std::string> individual_tags;
    StringUtil::Split(tags, ':', &individual_tags, /* suppress_empty_components = */ true);

    for (const auto &tag : individual_tags) {
        for (const auto &field : record.getTagRange(tag)) {
            for (const auto &subfield_value : field.getSubfields().extractSubfields('0')) {
                if (not StringUtil::StartsWith(subfield_value, "(DE-627)"))
                    continue;

                const std::string topic_id(subfield_value.substr(8));
                if (existing_ddcs.find(topic_id) == existing_ddcs.end()) // This one is new!
                    topic_ids->insert(topic_id);
            }
        }
    }
}


void AugmentRecordsWithDDCs(MARC::Reader * const title_reader, MARC::Writer * const title_writer,
                            const std::unordered_map<std::string, std::set<std::string>> &norm_ids_to_ddcs_map) {
    LOG_INFO("Starting augmenting of data.");

    unsigned count(0), augmented_count(0), already_had_ddcs(0), never_had_ddcs_and_now_have_ddcs(0);
    while (MARC::Record record = title_reader->read()) {
        ++count;

        // Extract already existing DDCs:
        std::set<std::string> existing_ddcs;
        ExtractDDCsFromField("082", record, &existing_ddcs);
        ExtractDDCsFromField("083", record, &existing_ddcs);
        if (not existing_ddcs.empty())
            ++already_had_ddcs;

        std::set<std::string> topic_ids; // = the IDs of the corresponding norm data records
        ExtractTopicIDs("600:610:611:630:650:653:656:689", record, existing_ddcs, &topic_ids);
        if (topic_ids.empty()) {
            title_writer->write(record);
            continue;
        }

        std::set<std::string> new_ddcs;
        for (const auto &topic_id : topic_ids) {
            const auto iter(norm_ids_to_ddcs_map.find(topic_id));
            if (iter != norm_ids_to_ddcs_map.end())
                new_ddcs.insert(iter->second.begin(), iter->second.end());
        }

        if (not new_ddcs.empty()) {
            ++augmented_count;
            if (existing_ddcs.empty())
                ++never_had_ddcs_and_now_have_ddcs;
            for (const auto &new_ddc : new_ddcs) {
                const std::string new_field("0 ""\x1F""a" + new_ddc + "\x1F""cfrom_topic_norm_data");
                record.insertField("082", new_field);
            }
        }

        title_writer->write(record);
    }

    LOG_INFO("Read " + std::to_string(count) + " title data records.");
    LOG_INFO(std::to_string(already_had_ddcs) + " already had DDCs.");
    LOG_INFO("Augmented " + std::to_string(augmented_count) + " records.");
    LOG_INFO(std::to_string(never_had_ddcs_and_now_have_ddcs) + " now have DDCs but didn't before.");
}


} // unnamed namespace


int Main(int argc, char *argv[]) {
    if (argc < 4)
        Usage();

    const std::string title_input_filename(argv[1]);
    const std::string authority_input_filename(argv[2]);
    const std::string title_output_filename(argv[3]);

    if (unlikely(title_input_filename == title_output_filename))
        LOG_ERROR("Title input file name equals title output file name!");

    if (unlikely(authority_input_filename == title_output_filename))
        LOG_ERROR("Authority data input file name equals title output file name!");

    auto title_reader(MARC::Reader::Factory(title_input_filename));
    auto authority_reader(MARC::Reader::Factory(authority_input_filename));
    auto title_writer(MARC::Writer::Factory(title_output_filename));

    std::unordered_map<std::string, std::set<std::string>> authority_ids_to_ddcs_map;
    ExtractDDCsFromAuthorityData(authority_reader.get(), &authority_ids_to_ddcs_map);
    AugmentRecordsWithDDCs(title_reader.get(), title_writer.get(), authority_ids_to_ddcs_map);

    return EXIT_SUCCESS;
}

/** \brief A tool to add DDC metadata to title data using various means.
 *  \author Dr. Johannes Ruscheinski (johannes.ruscheinski@uni-tuebingen.de)
 *
 *  \copyright 2015-2017 Universitätsbibliothek Tübingen.  All rights reserved.
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
#include "DirectoryEntry.h"
#include "Leader.h"
#include "MarcReader.h"
#include "MarcRecord.h"
#include "MarcWriter.h"
#include "RegexMatcher.h"
#include "Subfields.h"
#include "util.h"


bool IsPossibleDDC(const std::string &ddc_candidate) {
    static const RegexMatcher *matcher(RegexMatcher::RegexMatcherFactory("^\\d\\d\\d"));
    std::string err_msg;
    if (not matcher->matched(ddc_candidate, &err_msg)) {
        if (err_msg.empty())
            return false;
        logger->error("unexpected regex error while trying to match \"" + ddc_candidate + "\": " + err_msg);
    }

    return true;
}


void ExtractDDCsFromField(const std::string &tag, const MarcRecord &record, std::set<std::string> * const ddcs) {
    for (size_t index = record.getFieldIndex(tag); index < record.getNumberOfFields() and record.getTag(index) == tag;
         ++index)
    {
        const Subfields subfields(record.getSubfields(index));
        if (subfields.hasSubfield('z')) // Auxillary table number => not a regular DDC in $a!
            continue;

        const auto subfield_a_begin_end(subfields.getIterators('a'));
        for (auto ddc(subfield_a_begin_end.first); ddc != subfield_a_begin_end.second; ++ddc) {
            if (IsPossibleDDC(ddc->value_))
                ddcs->insert(ddc->value_);
        }
    }
}


void ExtractDDCsFromAuthorityData(const bool verbose, MarcReader * const authority_reader,
                                  std::unordered_map<std::string, std::set<std::string>> * const norm_ids_to_ddcs_map)
{
    norm_ids_to_ddcs_map->clear();
    if (verbose)
        std::cerr << "Starting loading of norm data.\n";

    unsigned count(0), ddc_record_count(0);
    while (const MarcRecord record = authority_reader->read()) {
        ++count;

        const size_t _001_index = record.getFieldIndex("001");
        if (_001_index == MarcRecord::FIELD_NOT_FOUND)
            continue;
        std::set<std::string> ddcs;
        ExtractDDCsFromField("083", record, &ddcs);
        ExtractDDCsFromField("089", record, &ddcs);

        if (not ddcs.empty()) {
            ++ddc_record_count;
            norm_ids_to_ddcs_map->insert(std::make_pair(record.getControlNumber(), ddcs));
        }
    }

    if (verbose) {
        std::cerr << "Read " << count << " norm data records.\n";
        std::cerr << ddc_record_count << " records had DDC entries.\n";
    }
}


void ExtractTopicIDs(const std::string &tags, const MarcRecord &record, const std::set<std::string> &existing_ddcs,
                     std::set<std::string> * const topic_ids)
{
    topic_ids->clear();

    std::vector<std::string> individual_tags;
    StringUtil::Split(tags, ':', &individual_tags);

    for (const auto &tag : individual_tags) {
        for (size_t index(record.getFieldIndex(tag));
             index < record.getNumberOfFields() and record.getTag(index) == tag; ++index)
        {
            const Subfields subfields(record.getSubfields(index));
            const auto begin_end(subfields.getIterators('0'));
            for (auto subfield0(begin_end.first); subfield0 != begin_end.second; ++subfield0) {
                if (not StringUtil::StartsWith(subfield0->value_, "(DE-576)"))
                    continue;

                const std::string topic_id(subfield0->value_.substr(8));
                if (existing_ddcs.find(topic_id) == existing_ddcs.end()) // This one is new!
                    topic_ids->insert(topic_id);
            }
        }
    }
}


void AugmentRecordsWithDDCs(const bool verbose, MarcReader * const title_reader, MarcWriter * const title_writer,
                            const std::unordered_map<std::string, std::set<std::string>> &norm_ids_to_ddcs_map)
{
    if (verbose)
        std::cerr << "Starting augmenting of data.\n";

    unsigned count(0), augmented_count(0), already_had_ddcs(0), never_had_ddcs_and_now_have_ddcs(0);
    while (MarcRecord record = title_reader->read()) {
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

    if (verbose) {
        std::cerr << "Read " << count << " title data records.\n";
        std::cerr << already_had_ddcs << " already had DDCs.\n";
        std::cerr << "Augmented " << augmented_count << " records.\n";
        std::cerr << never_had_ddcs_and_now_have_ddcs << " now have DDCs but didn't before.\n";
    }
}


void Usage() {
    std::cerr << "usage: " << ::progname << " [--verbose] input_title_data norm_data output_title_data\n";
    std::exit(EXIT_FAILURE);
}


int main(int argc, char *argv[]) {
    ::progname = argv[0];

    if (argc != 4 and argc != 5)
        Usage();
    bool verbose(false);
    if (argc == 5) {
        if (std::strcmp(argv[1], "--verbose") == 0)
            verbose = true;
        else
            Usage();
    }

    const std::string title_input_filename(argv[verbose ? 2 : 1]);
    const std::string authority_input_filename(argv[verbose ? 3 : 2]);
    const std::string title_output_filename(argv[verbose ? 4 : 3]);

    if (unlikely(title_input_filename == title_output_filename))
        logger->error("Title input file name equals title output file name!");

    if (unlikely(authority_input_filename == title_output_filename))
        logger->error("Authority data input file name equals title output file name!");

    std::unique_ptr<MarcReader> title_reader(MarcReader::Factory(title_input_filename, MarcReader::BINARY));
    std::unique_ptr<MarcReader> authority_reader(MarcReader::Factory(authority_input_filename, MarcReader::BINARY));
    std::unique_ptr<MarcWriter> title_writer(MarcWriter::Factory(title_output_filename, MarcWriter::BINARY));
    try {
        std::unordered_map<std::string, std::set<std::string>> authority_ids_to_ddcs_map;
        ExtractDDCsFromAuthorityData(verbose, authority_reader.get(), &authority_ids_to_ddcs_map);
        AugmentRecordsWithDDCs(verbose, title_reader.get(), title_writer.get(), authority_ids_to_ddcs_map);
    } catch (const std::exception &x) {
        logger->error("caught exception: " + std::string(x.what()));
    }
}

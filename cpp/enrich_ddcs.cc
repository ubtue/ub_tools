/** \brief A tool to add DDC metadata to title data using various means.
 *  \author Dr. Johannes Ruscheinski (johannes.ruscheinski@uni-tuebingen.de)
 *
 *  \copyright 2015 Universitätsbiblothek Tübingen.  All rights reserved.
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
#include "MarcUtil.h"
#include "RegexMatcher.h"
#include "Subfields.h"
#include "util.h"
#include "XmlWriter.h"


bool IsPossibleDDC(const std::string &ddc_candidate) {
    static const RegexMatcher *matcher(RegexMatcher::RegexMatcherFactory("^\\d\\d\\d"));
    std::string err_msg;
    if (not matcher->matched(ddc_candidate, &err_msg)) {
	if (err_msg.empty())
	    return false;
	Error("unexpected regex error while trying to match \"" + ddc_candidate + "\": " + err_msg);
    }

    return true;
}


void ExtractDDCsFromField(const std::string &tag, const std::vector<DirectoryEntry> &dir_entries,
			  const std::vector<std::string> &fields, std::set<std::string> * const ddcs)
{
    const auto begin_end(DirectoryEntry::FindFields(tag, dir_entries));
    for (auto iter(begin_end.first); iter != begin_end.second; ++iter) {
	const Subfields subfields(fields[iter - dir_entries.begin()]);
	if (subfields.hasSubfield('z')) // Auxillary table number => not a regular DDC in $a!
	    continue;

	const auto subfield_a_begin_end(subfields.getIterators('a'));
	for (auto ddc(subfield_a_begin_end.first); ddc != subfield_a_begin_end.second; ++ddc) {
	    if (IsPossibleDDC(ddc->second))
		ddcs->insert(ddc->second);
	}
    }
}


void ExtractDDCsFromNormdata(const bool verbose, File * const norm_input,
			     std::unordered_map<std::string, std::set<std::string>> * const norm_ids_to_ddcs_map)
{
    norm_ids_to_ddcs_map->clear();
    if (verbose)
        std::cerr << "Starting loading of norm data.\n";

    unsigned count(0), ddc_record_count(0);
    while (const MarcUtil::Record record = MarcUtil::Record::XmlFactory(norm_input)) {
        ++count;

	const std::vector<DirectoryEntry> &dir_entries(record.getDirEntries());
        const auto _001_iter(DirectoryEntry::FindField("001", dir_entries));
        if (_001_iter == dir_entries.end())
            continue;
	const std::vector<std::string> &fields(record.getFields());
        const std::string &control_number(fields[_001_iter - dir_entries.begin()]);

	std::set<std::string> ddcs;
	ExtractDDCsFromField("083", dir_entries, fields, &ddcs);
	ExtractDDCsFromField("089", dir_entries, fields, &ddcs);

	if (not ddcs.empty()) {
	    ++ddc_record_count;
	    norm_ids_to_ddcs_map->insert(std::make_pair(control_number, ddcs));
	}
    }

    if (verbose) {
        std::cerr << "Read " << count << " norm data records.\n";
        std::cerr << ddc_record_count << " records had DDC entries.\n";
    }
}


void ExtractTopicIDs(const std::string &tags, const MarcUtil::Record &record, const std::set<std::string> &existing_ddcs,
		     std::set<std::string> * const topic_ids)
{
    topic_ids->clear();

    std::vector<std::string> individual_tags;
    StringUtil::Split(tags, ':', &individual_tags);

    const std::vector<DirectoryEntry> &dir_entries(record.getDirEntries());
    const std::vector<std::string> &fields(record.getFields());
    for (const auto &tag : individual_tags) {
        const ssize_t first_index(record.getFieldIndex(tag));
        if (first_index == -1)
            continue;

        for (size_t index(first_index); index < dir_entries.size() and dir_entries[index].getTag() == tag; ++index) {
            const Subfields subfields(fields[index]);
            const auto begin_end(subfields.getIterators('0'));
            for (auto subfield0(begin_end.first); subfield0 != begin_end.second; ++subfield0) {
                if (not StringUtil::StartsWith(subfield0->second, "(DE-576)"))
                    continue;

                const std::string topic_id(subfield0->second.substr(8));
		if (existing_ddcs.find(topic_id) == existing_ddcs.end()) // This one is new!
		    topic_ids->insert(topic_id);
            }
        }
    }
}


void AugmentRecordsWithDDCs(const bool verbose, File * const title_input, File * const title_output,
			    const std::unordered_map<std::string, std::set<std::string>> &norm_ids_to_ddcs_map)
{
    if (verbose)
        std::cerr << "Starting augmenting of data.\n";

    XmlWriter xml_writer(title_output);
    xml_writer.openTag("marc:collection",
                       { std::make_pair("xmlns:marc", "http://www.loc.gov/MARC21/slim"),
                         std::make_pair("xmlns:xsi", "http://www.w3.org/2001/XMLSchema-instance"),
                         std::make_pair("xsi:schemaLocation", "http://www.loc.gov/standards/marcxml/schema/MARC21slim.xsd")});
    unsigned count(0), augmented_count(0), already_had_ddcs(0), never_had_ddcs_and_now_have_ddcs(0);
    while (MarcUtil::Record record = MarcUtil::Record::XmlFactory(title_input)) {
	record.setRecordWillBeWrittenAsXml(true);
        ++count;

	// Extract already existing DDCs:
	std::set<std::string> existing_ddcs;
	const std::vector<DirectoryEntry> &dir_entries(record.getDirEntries());
	const std::vector<std::string> &fields(record.getFields());
	ExtractDDCsFromField("082", dir_entries, fields, &existing_ddcs);
	ExtractDDCsFromField("083", dir_entries, fields, &existing_ddcs);
	if (not existing_ddcs.empty())
	    ++already_had_ddcs;
	
	std::set<std::string> topic_ids; // = the IDs of the corresponding norm data records
	ExtractTopicIDs("600:610:611:630:650:653:656:689", record, existing_ddcs, &topic_ids);
	if (topic_ids.empty()) {
	    record.write(&xml_writer);
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

	record.write(&xml_writer);
    }
    xml_writer.closeTag();

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
    File title_input(title_input_filename, "rm");
    if (not title_input)
        Error("can't open \"" + title_input_filename + "\" for reading!");

    const std::string norm_input_filename(argv[verbose ? 3 : 2]);
    File norm_input(norm_input_filename, "rm");
    if (not norm_input)
        Error("can't open \"" + norm_input_filename + "\" for reading!");

    const std::string title_output_filename(argv[verbose ? 4 : 3]);
    File title_output(title_output_filename, "w");
    if (not title_output)
        Error("can't open \"" + title_output_filename + "\" for writing!");

    if (unlikely(title_input_filename == title_output_filename))
        Error("Title input file name equals title output file name!");

    if (unlikely(norm_input_filename == title_output_filename))
        Error("Norm data input file name equals title output file name!");

    try {
	std::unordered_map<std::string, std::set<std::string>> norm_ids_to_ddcs_map;
	ExtractDDCsFromNormdata(verbose, &norm_input, &norm_ids_to_ddcs_map);
	AugmentRecordsWithDDCs(verbose, &title_input, &title_output, norm_ids_to_ddcs_map);
    } catch (const std::exception &x) {
	Error("caught exception: " + std::string(x.what()));
    }
}

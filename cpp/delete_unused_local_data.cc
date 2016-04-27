/** \file    delete_unused_local_data.cc
 *  \author  Oliver Obenland
 *
 *  Local data blocks are embedded marc records inside of a record using LOK-Fields.
 *  Each local data block belongs to an institution and is marked by the institution's sigil.
 *  This tool filters for local data blocks of some institutions of the University of Tübingen
 *  and deletes all other local blocks.
 */

/*
    Copyright (C) 2016, Library of the University of Tübingen

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

#include <iostream>
#include "MarcUtil.h"
#include "RegexMatcher.h"
#include "util.h"
#include "XmlWriter.h"


static ssize_t count(0), before_count(0), after_count(0), no_local_data_records_count(0);


void Usage() {
    std::cerr << "Usage: " << ::progname << "  marc_input marc_output\n";
    std::exit(EXIT_FAILURE);
}


bool IsUnusedLocalBlock(const MarcUtil::Record * const record, const std::pair<size_t, size_t> &block_start_and_end) {
    static RegexMatcher *matcher(nullptr);
    std::string err_msg;
    if (unlikely(matcher == nullptr)) {
        matcher = RegexMatcher::RegexMatcherFactory("^.*aDE-21.*$|^.*aDE-21-24.*$|^.*aDE-21-110.*$|^.*aTü 135.*$", &err_msg);
        if (matcher == nullptr)
            Error(err_msg);
    }

    std::vector<size_t> field_indices;
    record->findFieldsInLocalBlock("852", "??", block_start_and_end, &field_indices);

    const std::vector<std::string> &fields(record->getFields());
    for (const auto field_index : field_indices) {
        const bool matched = matcher->matched(fields[field_index], &err_msg);
        if (not matched and not err_msg.empty())
            Error("Unexpected error while trying to match a field in IsUnusedLocalBlock: " + err_msg);
        if (matched)
            return false;
    }
    return true;
}


void DeleteLocalBlock(MarcUtil::Record * const record, const std::pair<size_t, size_t> &block_start_and_end) {
    for (size_t field_index(block_start_and_end.second - 1); field_index >= block_start_and_end.first; --field_index)
        record->deleteField(field_index);
}


bool ProcessRecord(MarcUtil::Record * const record) {
    std::vector<std::pair<size_t, size_t>> local_block_boundaries;
    ssize_t local_data_count = record->findAllLocalDataBlocks(&local_block_boundaries);
    std::reverse(local_block_boundaries.begin(), local_block_boundaries.end());

    before_count += local_data_count;
    for (const std::pair<size_t, size_t> &block_start_and_end : local_block_boundaries) {
        if (IsUnusedLocalBlock(record, block_start_and_end)) {
            DeleteLocalBlock(record, block_start_and_end);
            --local_data_count;
        }
    }

    after_count += local_data_count;
    return local_data_count != 0;
}


void DeleteUnusedLocalData(File * const input, File * const output) {
    XmlWriter xml_writer(output);
    xml_writer.openTag("marc:collection",
                        { std::make_pair("xmlns:marc", "http://www.loc.gov/MARC21/slim"),
                          std::make_pair("xmlns:xsi", "http://www.w3.org/2001/XMLSchema-instance"),
                          std::make_pair("xsi:schemaLocation", "http://www.loc.gov/standards/marcxml/schema/MARC21slim.xsd")});

    while (MarcUtil::Record record = MarcUtil::Record::XmlFactory(input)) {
        ++count;
        record.setRecordWillBeWrittenAsXml(true);
        ProcessRecord(&record);
        record.write(&xml_writer);
    }

    xml_writer.closeTag();

    std::cerr << ::progname << ": Deleted " << (before_count - after_count) << " of " << before_count << " local data blocks.\n";
    std::cerr << ::progname << ": Deleted " << no_local_data_records_count << " of " << count << "records without local data.\n";
}


int main(int argc, char **argv) {
    ::progname = argv[0];

    if (argc != 3)
        Usage();

    const std::string input_filename(argv[1]);
    File input(input_filename, "r");
    if (not input)
        Error("can't open \"" + input_filename + "\" for reading!");

    const std::string output_filename(argv[2]);
    File output(output_filename, "w");
    if (not output)
        Error("can't open \"" + output_filename + "\" for writing!");

    try {
        DeleteUnusedLocalData(&input, &output);
    } catch (const std::exception &x) {
        Error("caught exception: " + std::string(x.what()));
    }
}

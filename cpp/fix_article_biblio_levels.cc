/** \file    fix_article_biblio_levels.cc
 *  \author  Dr. Johannes Ruscheinski
 *
 *  A tool for patching up the bibliographic level of article records.
 *  Many, possibly all, article records that we get have an 'a' in leader position 7 instead of a 'b'.
 *  If the referenced parent is a serial this tool changes the 'a' to a 'b'.
 */

/*
    Copyright (C) 2015, Library of the University of Tübingen

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
#include <unordered_set>
#include <cstdlib>
#include "DirectoryEntry.h"
#include "Leader.h"
#include "MarcUtil.h"
#include "RegexMatcher.h"
#include "StringUtil.h"
#include "Subfields.h"
#include "util.h"


void Usage() {
    std::cerr << "Usage: " << progname << " [--verbose] marc_input marc_output\n";
    std::exit(EXIT_FAILURE);
}


static std::unordered_set<std::string> serial_control_numbers;


bool RecordSerialControlNumbers(MarcUtil::Record * const record, std::string * const /* err_msg */) {
    const Leader &leader(record->getLeader());
    if (leader[7] == 's') {
	const std::vector<std::string> &fields(record->getFields());
	serial_control_numbers.insert(fields[0]);
    }

    return true;
}


void CollectSerials(const bool verbose, FILE * const input) {
    std::string err_msg;
    if (not MarcUtil::ProcessRecords(input, RecordSerialControlNumbers, &err_msg))
	Error("error while looking for serials: " + err_msg);

    if (verbose)
	std::cout << "Found " << serial_control_numbers.size() << " serial records.\n";
}


static FILE *output_ptr;
static unsigned patch_count;


bool HasSerialParent(const std::string &subfield, const MarcUtil::Record &record) {
    const std::string tag(subfield.substr(0, 3));
    const char subfield_code(subfield[3]);
    const ssize_t field_index(record.getFieldIndex(tag));
    if (field_index == -1)
	return false;

    const std::vector<std::string> &fields(record.getFields());
    const Subfields subfields(fields[field_index]);
    const std::string subfield_contents(subfields.getFirstSubfieldValue(subfield_code));
    if (subfield_contents.empty())
	return false;

    static const RegexMatcher * const matcher(RegexMatcher::RegexMatcherFactory("\\(.+\\)(\\d{8}[\\dX])"));
    if (not matcher->matched(subfield_contents))
	return false;

    const std::string parent_id((*matcher)[1]);
    return serial_control_numbers.find(parent_id) != serial_control_numbers.cend();
}


bool HasAtLeastOneSerialParent(const std::string &subfield_list, const MarcUtil::Record &record) {
    std::vector<std::string> subfields;
    StringUtil::Split(subfield_list, ':', &subfields);
    for (const auto &subfield : subfields) {
	if (HasSerialParent(subfield, record))
	    return true;
    }

    return false;
}


// Changes the bibliographic level of a record from 'a' to 'b' (= serial component part) if the parent is a serial.
// Also writes all records to "output_ptr".
bool PatchUpArticle(MarcUtil::Record * const record, std::string * const /*err_msg*/)
{
    Leader &leader(record->getLeader());
    if (leader[7] != 'a') {
	record->write(output_ptr);
	return true;
    }

    if (not HasAtLeastOneSerialParent("800w:810w:830w:773w", *record)) {
	record->write(output_ptr);
	return true;
    }

    leader[7] = 'b';
    ++patch_count;
    record->write(output_ptr);

    return true;
}


void PatchUpSerialComponentParts(const bool verbose, FILE * const input, FILE * const output) {
    output_ptr = output;

    std::string err_msg;
    if (not MarcUtil::ProcessRecords(input, PatchUpArticle, &err_msg))
	Error("error while patching up article records: " + err_msg);

    if (verbose)
	std::cout << "Fixed the bibliographic level of " << patch_count << " article records.\n";
}


int main(int argc, char **argv) {
    progname = argv[0];

    if (argc != 3 and argc != 4)
        Usage();

    bool verbose;
    if (argc == 3)
	verbose = false;
    else { // argc == 4
	if (std::strcmp(argv[1], "--verbose") != 0)
	    Usage();
	verbose = true;
    }

    const std::string marc_input_filename(argv[argc == 3 ? 1 : 2]);
    FILE *marc_input(std::fopen(marc_input_filename.c_str(), "rbm"));
    if (marc_input == nullptr)
        Error("can't open \"" + marc_input_filename + "\" for reading!");

    const std::string marc_output_filename(argv[argc == 3 ? 2 : 3]);
    FILE *marc_output(std::fopen(marc_output_filename.c_str(), "wb"));
    if (marc_output == nullptr)
        Error("can't open \"" + marc_output_filename + "\" for writing!");

    try {
	CollectSerials(verbose, marc_input);

	std::rewind(marc_input);
	PatchUpSerialComponentParts(verbose, marc_input, marc_output);
    } catch (const std::exception &x) {
	Error("caught exception: " + std::string(x.what()));
    }

    std::fclose(marc_input);
    std::fclose(marc_output);
}

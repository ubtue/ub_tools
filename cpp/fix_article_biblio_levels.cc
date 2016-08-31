/** \file    fix_article_biblio_levels.cc
 *  \author  Dr. Johannes Ruscheinski
 *
 *  A tool for patching up the bibliographic level of article records.
 *  Many, possibly all, article records that we get have an 'a' in leader position 7 instead of a 'b'.
 *  If the referenced parent is no a monograph this tool changes the 'a' to a 'b'.
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
#include <vector>
#include <cstdlib>
#include "DirectoryEntry.h"
#include "Leader.h"
#include "MarcUtil.h"
#include "RegexMatcher.h"
#include "StringUtil.h"
#include "Subfields.h"
#include "util.h"
#include "XmlWriter.h"


void Usage() {
    std::cerr << "Usage: " << progname << " [--verbose] marc_input1 [marc_input2 ... marc_inputN] marc_output\n"
              << "       Collects information about which superior/collective works are serials from the various\n"
              << "       MARC inputs and then patches up records in \"marc_input1\" which have been marked as a book\n"
              << "       component and changes them to be flagged as an article instead.  The patched up version is\n"
              << "       written to \"marc_output\".\n";
    std::exit(EXIT_FAILURE);
}


static std::unordered_set<std::string> monograph_control_numbers;


bool RecordMonographControlNumbers(MarcUtil::Record * const record, XmlWriter * const /*xml_writer*/,
                                   std::string * const /* err_msg */)
{
    const Leader &leader(record->getLeader());
    if (leader[7] == 'm') {
        const std::vector<std::string> &fields(record->getFields());
        monograph_control_numbers.insert(fields[0]);
    }

    return true;
}


void CollectMonographs(const bool verbose, const std::vector<File *> &inputs) {
    std::string err_msg;
    for (auto &input : inputs) {
        if (verbose)
            std::cout << "Extracting serial control numbers from \"" << input->getPath() << "\".\n";
        if (not MarcUtil::ProcessRecords(input, RecordMonographControlNumbers, /* xml_writer = */nullptr, &err_msg))
            Error("error while looking for serials: " + err_msg);
    }

    if (verbose)
        std::cout << "Found " << monograph_control_numbers.size() << " serial records.\n";
}


static unsigned patch_count;


bool HasMonographParent(const std::string &subfield, const MarcUtil::Record &record) {
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
    return monograph_control_numbers.find(parent_id) != monograph_control_numbers.cend();
}


bool HasAtLeastOneMonographParent(const std::string &subfield_list, const MarcUtil::Record &record) {
    std::vector<std::string> subfields;
    StringUtil::Split(subfield_list, ':', &subfields);
    for (const auto &subfield : subfields) {
        if (HasMonographParent(subfield, record))
            return true;
    }

    return false;
}


// Changes the bibliographic level of a record from 'a' to 'b' (= serial component part) if the parent is not a monograph.
// Also writes all records to "output_ptr".
bool PatchUpArticle(MarcUtil::Record * const record, XmlWriter * const xml_writer, std::string * const /*err_msg*/) {
    Leader &leader(record->getLeader());
    if (leader[7] != 'a') {
        record->write(xml_writer);
        return true;
    }

    if (HasAtLeastOneMonographParent("800w:810w:830w:773w", *record)) {
        record->write(xml_writer);
        return true;
    }

    leader[7] = 'b';
    ++patch_count;
    record->write(xml_writer);

    return true;
}


// Iterates over all records in a collection and retags all book component parts as artcles
// unless the object has a monograph as a parent.
void PatchUpBookComponentParts(const bool verbose, File * const input, File * const output) {
    XmlWriter xml_writer(output);
    xml_writer.openTag("marc:collection",
                       { std::make_pair("xmlns:marc", "http://www.loc.gov/MARC21/slim"),
                         std::make_pair("xmlns:xsi", "http://www.w3.org/2001/XMLSchema-instance"),
                         std::make_pair("xsi:schemaLocation", "http://www.loc.gov/standards/marcxml/schema/MARC21slim.xsd")});

    std::string err_msg;
    if (not MarcUtil::ProcessRecords(input, PatchUpArticle, &xml_writer, &err_msg))
        Error("error while patching up article records: " + err_msg);

    xml_writer.closeTag();

    if (verbose)
        std::cout << "Fixed the bibliographic level of " << patch_count << " article records.\n";
}


int main(int argc, char **argv) {
    progname = argv[0];

    if (argc == 1)
        Usage();

    const bool verbose(std::strcmp(argv[1], "--verbose") == 0);

    if (verbose and argc < 4 or not verbose and argc < 3)
        Usage();

    std::vector<File *> marc_inputs;
    for (int arg_no(verbose ? 2 : 1); arg_no < (argc - 1) ; ++arg_no) {
        const std::string marc_input_filename(argv[arg_no]);
        File *marc_input = new File(marc_input_filename, "r");
        if (not *marc_input)
            Error("can't open \"" + marc_input_filename + "\" for reading!");
        marc_inputs.push_back(marc_input);
    }

    const std::string marc_output_filename(argv[argc - 1]);
    File marc_output(marc_output_filename, "w");
    if (not marc_output)
        Error("can't open \"" + marc_output_filename + "\" for writing!");

    try {
        CollectMonographs(verbose, marc_inputs);

        marc_inputs[0]->rewind();
        PatchUpBookComponentParts(verbose, marc_inputs[0], &marc_output);
    } catch (const std::exception &x) {
        Error("caught exception: " + std::string(x.what()));
    }

    for (const auto &marc_input : marc_inputs)
        marc_input->close();
}

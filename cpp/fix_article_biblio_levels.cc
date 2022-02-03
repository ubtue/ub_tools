/** \file    fix_article_biblio_levels.cc
 *  \author  Dr. Johannes Ruscheinski
 *
 *  A tool for patching up the bibliographic level of article records.
 *  Many, possibly all, article records that we get have an 'a' in leader position 7 instead of a 'b'.
 *  If the referenced parent is not a monograph this tool changes the 'a' to a 'b'.
 */

/*
    Copyright (C) 2015-2019, Library of the University of Tübingen

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
#include "MARC.h"
#include "RegexMatcher.h"
#include "StringUtil.h"
#include "util.h"


namespace {


[[noreturn]] void Usage() {
    std::cerr << "Usage: " << ::progname << " marc_input1 [marc_input2 ... marc_inputN] marc_output\n"
              << "       Collects information about which superior/collective works are serials from the various\n"
              << "       MARC inputs and then patches up records in \"marc_input1\" which have been marked as a book\n"
              << "       component and changes them to be flagged as an article instead.  The patched up version is\n"
              << "       written to \"marc_output\".\n";
    std::exit(EXIT_FAILURE);
}


void CollectMonographs(const std::vector<std::unique_ptr<MARC::Reader>> &marc_readers,
                       std::unordered_set<std::string> * const monograph_control_numbers) {
    for (auto &marc_reader : marc_readers) {
        LOG_INFO("Extracting serial control numbers from \"" + marc_reader->getPath() + "\".");
        while (const auto record = marc_reader->read()) {
            if (record.isMonograph())
                monograph_control_numbers->insert(record.getControlNumber());
        }
    }

    LOG_INFO("Found " + std::to_string(monograph_control_numbers->size()) + " serial records.");
}


bool HasMonographParent(const std::string &subfield, const MARC::Record &record,
                        std::unordered_set<std::string> * const monograph_control_numbers) {
    const std::string tag(subfield.substr(0, 3));
    const char subfield_code(subfield[3]);
    const auto field(record.findTag(tag));
    if (field == record.end())
        return false;

    const std::string &subfield_contents(field->getSubfields().getFirstSubfieldWithCode(subfield_code));
    if (subfield_contents.empty())
        return false;

    static RegexMatcher * const matcher(RegexMatcher::RegexMatcherFactory("\\(.+\\)(\\d{8}[\\dX])"));
    if (not matcher->matched(subfield_contents))
        return false;

    const std::string parent_id((*matcher)[1]);
    return monograph_control_numbers->find(parent_id) != monograph_control_numbers->cend();
}


bool HasAtLeastOneMonographParent(const std::string &subfield_list, const MARC::Record &record,
                                  std::unordered_set<std::string> * const monograph_control_numbers) {
    std::vector<std::string> subfields;
    StringUtil::Split(subfield_list, ':', &subfields, /* suppress_empty_components = */ true);
    for (const auto &subfield : subfields) {
        if (HasMonographParent(subfield, record, monograph_control_numbers))
            return true;
    }

    return false;
}


// Iterates over all records in a collection and retags all book component parts as articles
// unless the object has a monograph as a parent.
// Changes the bibliographic level of a record from 'a' to 'b' (= serial component part) if the parent is not a
// monograph.  Also writes all records to "output_ptr".
void PatchUpBookComponentParts(MARC::Reader * const marc_reader, MARC::Writer * const marc_writer,
                               std::unordered_set<std::string> * const monograph_control_numbers) {
    unsigned patch_count(0);
    while (auto record = marc_reader->read()) {
        if (record.isArticle() and not HasAtLeastOneMonographParent("800w:810w:830w:773w", record, monograph_control_numbers)) {
            record.setBibliographicLevel(MARC::Record::SERIAL_COMPONENT_PART);
            ++patch_count;
        }
        marc_writer->write(record);
    }

    LOG_INFO("Fixed the bibliographic level of " + std::to_string(patch_count) + " article records.");
}


} // unnamed namespace


int Main(int argc, char **argv) {
    if (argc < 3)
        Usage();

    std::vector<std::unique_ptr<MARC::Reader>> marc_readers;
    for (int arg_no(1); arg_no < (argc - 1); ++arg_no)
        marc_readers.emplace_back(MARC::Reader::Factory(argv[arg_no]));
    std::unique_ptr<MARC::Writer> marc_writer(MARC::Writer::Factory(argv[argc - 1]));

    std::unordered_set<std::string> monograph_control_numbers;

    CollectMonographs(marc_readers, &monograph_control_numbers);
    marc_readers[0]->rewind();
    PatchUpBookComponentParts(marc_readers[0].get(), marc_writer.get(), &monograph_control_numbers);

    return EXIT_SUCCESS;
}

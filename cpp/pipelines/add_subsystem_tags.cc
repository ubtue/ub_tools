/** \file    add_subsystem_tags.cc
 *  \brief   Add additional tags for interfaces to identitify subset views of
             IxTheo like RelBib and Bibstudies
 *  \author  Johannes Riedl
 */

/*
    Copyright (C) 2018,2019 Library of the University of Tübingen

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

#include <fstream>
#include <iostream>
#include <map>
#include <vector>
#include <cstdlib>
#include "Compiler.h"
#include "File.h"
#include "FileUtil.h"
#include "MARC.h"
#include "RegexMatcher.h"
#include "StringUtil.h"
#include "util.h"


namespace {


const std::string RELBIB_TAG("REL");
const std::string BIBSTUDIES_TAG("BIB");

enum SubSystem { RELBIB, BIBSTUDIES };
const unsigned NUM_OF_SUBSYSTEMS(2);


[[noreturn]] void Usage() {
    std::cerr << "Usage: " << ::progname << " marc_input marc_output\n";
    std::exit(EXIT_FAILURE);
}


bool HasRelBibSSGN(const MARC::Record &record) {
    for (const auto& field : record.getTagRange("084")) {
        const MARC::Subfields subfields(field.getSubfields());
        if (subfields.hasSubfieldWithValue('2', "ssgn") and subfields.hasSubfieldWithValue('a', "0"))
            return true;
    }
    return false;
}


bool HasRelBibIxTheoNotation(const MARC::Record &record) {
    // Integrate IxTheo Notations A*.B*,T*,V*,X*,Z*
    static const std::string RELBIB_IXTHEO_NOTATION_PATTERN("^[ABTVXZ][A-Z].*|.*:[ABTVXZ][A-Z].*");
    static RegexMatcher * const relbib_ixtheo_notations_matcher(RegexMatcher::RegexMatcherFactory(RELBIB_IXTHEO_NOTATION_PATTERN));
    for (const auto& field : record.getTagRange("652")) {
        for (const auto &subfield_a : field.getSubfields().extractSubfields("a")) {
            if (relbib_ixtheo_notations_matcher->matched(subfield_a))
                return true;
        }
    }
    return false;
}


bool HasPlausibleDDCPrefix(const std::string &ddc_string) {
    // Exclude records that where the entry in the DCC field is not plausible
    static const std::string PLAUSIBLE_DDC_PREFIX_PATTERN("^\\d\\d");
    static RegexMatcher * const plausible_ddc_prefix_matcher(RegexMatcher::RegexMatcherFactoryOrDie(PLAUSIBLE_DDC_PREFIX_PATTERN));
    return plausible_ddc_prefix_matcher->matched(ddc_string);
}


// Additional criteria that prevent the exclusion of a record that has a 220-289 field
bool HasAdditionalRelbibAdmissionDDC(const MARC::Record &record) {
    static const std::string RELBIB_ADMIT_DDC_PATTERN("^([12][01][0-9]|2[9][0-9]|[3-9][0-9][0-9]).*$");
    static RegexMatcher * const relbib_admit_ddc_range_matcher(RegexMatcher::RegexMatcherFactoryOrDie(RELBIB_ADMIT_DDC_PATTERN));
    for (const auto &field : record.getTagRange("082")) {
        for (const auto &subfield_a : field.getSubfields().extractSubfields("a")) {
            if (HasPlausibleDDCPrefix(subfield_a) and relbib_admit_ddc_range_matcher->matched(subfield_a))
                return true;
        }
    }
    return false;
}


bool HasRelBibExcludeDDC(const MARC::Record &record) {
    if (not record.hasTag("082"))
        return true;
    // Exclude DDC 220-289
    static const std::string RELBIB_EXCLUDE_DDC_RANGE_PATTERN("^2[2-8][0-9](/|\\.){0,2}[^.]*$");
    static RegexMatcher * const relbib_exclude_ddc_range_matcher(RegexMatcher::RegexMatcherFactoryOrDie(RELBIB_EXCLUDE_DDC_RANGE_PATTERN));

    // Make sure we have 082-fields to examine
    if (not record.hasTag("082"))
        return false;

    // In general we exclude if the exclude range is matched
    // But we include it anyway if we find another reasonable DDC-code
    for (const auto &field : record.getTagRange("082")) {
        for (const auto &subfield_a : field.getSubfields().extractSubfields("a")) {
            if (relbib_exclude_ddc_range_matcher->matched(subfield_a)) {
                if (not HasAdditionalRelbibAdmissionDDC(record))
                    return true;
            }
        }
    }

    // Exclude item if it has only a 400 or 800 DDC notation
    static const std::string RELBIB_EXCLUDE_DDC_CATEGORIES_PATTERN("^[48][0-9][0-9]$");
    static RegexMatcher * const relbib_exclude_ddc_categories_matcher(RegexMatcher::RegexMatcherFactoryOrDie(RELBIB_EXCLUDE_DDC_CATEGORIES_PATTERN));
    for (const auto &field : record.getTagRange("082")) {
        for (const auto &subfield_a : field.getSubfields().extractSubfields('a')) {
            if (HasPlausibleDDCPrefix(subfield_a) and not relbib_exclude_ddc_categories_matcher->matched(subfield_a))
                return false;
        }
    }
    return true;
}


bool MatchesRelBibDDC(const MARC::Record &record) {
    return not HasRelBibExcludeDDC(record);
}


bool IsDefinitelyRelBib(const MARC::Record &record) {
   return HasRelBibSSGN(record) or HasRelBibIxTheoNotation(record) or MatchesRelBibDDC(record);
}


bool IsProbablyRelBib(const MARC::Record &record) {
    for (const auto& field : record.getTagRange("191")) {
        for (const auto& subfield : field.getSubfields().extractSubfields("a")) {
            if (subfield == "1")
                return true;
        }
    }
    return false;
}


std::set<std::string> GetTemporarySuperiorRelBibList() {
    const std::string relbib_superior_temporary_file("/usr/local/ub_tools/cpp/data/relbib_superior_temporary.txt");
    std::set<std::string> superior_temporary_list;
    File superior_temporary(relbib_superior_temporary_file, "r");
    std::string line;
    while (superior_temporary.getline(&line) and not superior_temporary.eof())
        superior_temporary_list.emplace(line);
    return superior_temporary_list;
}


bool IsTemporaryRelBibSuperior(const MARC::Record &record) {
    static std::set<std::string> superior_temporary_list(GetTemporarySuperiorRelBibList());
    if (superior_temporary_list.find(record.getControlNumber()) != superior_temporary_list.end())
        return true;
    return false;
}


bool ExcludeBecauseOfRWEX(const MARC::Record &record) {
    for (const auto &field : record.getTagRange("LOK")) {
        const auto &subfields(field.getSubfields());
        for (const auto &subfield0: subfields.extractSubfields('0')) {
            if (not StringUtil::StartsWith(subfield0, "935"))
                continue;
            for (const auto &subfield_a : subfields.extractSubfields('a')) {
                if (subfield_a == "rwex")
                    return true;
            }
        }
    }
    return false;
}


bool IsRelBibRecord(const MARC::Record &record) {
    return ((IsDefinitelyRelBib(record) or
             IsProbablyRelBib(record) or
             IsTemporaryRelBibSuperior(record))
             and not ExcludeBecauseOfRWEX(record));
}


bool HasBibStudiesIxTheoNotation(const MARC::Record &record) {
    static const std::string BIBSTUDIES_IXTHEO_PATTERN("^[H][A-Z].*|.*:[H][A-Z].*");
    static RegexMatcher * const relbib_ixtheo_notations_matcher(RegexMatcher::RegexMatcherFactory(BIBSTUDIES_IXTHEO_PATTERN));
    for (const auto &field : record.getTagRange("652")) {
        for (const auto &subfield_a : field.getSubfields().extractSubfields("a")) {
            if (relbib_ixtheo_notations_matcher->matched(subfield_a))
                return true;
        }
    }
    return false;
}


bool IsBibStudiesRecord(const MARC::Record &record) {
    return HasBibStudiesIxTheoNotation(record);
}


void AddSubsystemTag(MARC::Record * const record, const std::string &tag) {
    // Don't insert twice
    if (record->getFirstField(tag) != record->end())
        return;
    MARC::Subfields subfields;
    subfields.addSubfield('a', "1");
    record->insertField(tag, subfields);
}


void CollectSuperiorOrParallelWorks(const MARC::Record &record, std::unordered_set<std::string> * const superior_or_parallel_works) {
    const std::set<std::string> parallel(MARC::ExtractCrossReferencePPNs(record));
    superior_or_parallel_works->insert(parallel.begin(), parallel.end());
    superior_or_parallel_works->insert(record.getSuperiorControlNumber());
}


// Get set of immediately belonging or superior or parallel records
void GetSubsystemPPNSet(MARC::Reader * const marc_reader, MARC::Writer * const marc_writer,
                        std::vector<std::unordered_set<std::string>> * const subsystem_sets)
{
    while (const MARC::Record record = marc_reader->read()) {
        if (IsRelBibRecord(record)) {
            ((*subsystem_sets)[RELBIB]).emplace(record.getControlNumber());
            CollectSuperiorOrParallelWorks(record, &((*subsystem_sets)[RELBIB]));
        }
        if (IsBibStudiesRecord(record)) {
            ((*subsystem_sets)[BIBSTUDIES]).emplace(record.getControlNumber());
            CollectSuperiorOrParallelWorks(record, &((*subsystem_sets)[BIBSTUDIES]));
        }
        marc_writer->write(record);
    }
}


void AddSubsystemTags(MARC::Reader * const marc_reader, MARC::Writer * const marc_writer,
                      const std::vector<std::unordered_set<std::string>> &subsystem_sets) {
    unsigned record_count(0), modified_count(0);
    while (MARC::Record record = marc_reader->read()) {
        ++record_count;
        bool modified_record(false);
        if ((subsystem_sets[RELBIB]).find(record.getControlNumber()) != subsystem_sets[RELBIB].end()) {
            AddSubsystemTag(&record, RELBIB_TAG);
            modified_record = true;
        }
        if ((subsystem_sets[BIBSTUDIES]).find(record.getControlNumber()) != subsystem_sets[RELBIB].end()) {
            AddSubsystemTag(&record, BIBSTUDIES_TAG);
            modified_record = true;
        }
        if (modified_record)
            ++modified_count;
         marc_writer->write(record);
    }
    LOG_INFO("Modified " + std::to_string(modified_count) + " of " + std::to_string(record_count) + " records.");
}


void InitializeSubsystemPPNSets(std::vector<std::unordered_set<std::string>> * const subsystem_ppn_sets) {
    for (unsigned i(0); i < NUM_OF_SUBSYSTEMS; ++i)
        subsystem_ppn_sets->push_back(std::unordered_set<std::string>());
}


} //unnamed namespace


int Main(int argc, char **argv) {
    if (argc != 3)
        Usage();

    const std::string marc_input_filename(argv[1]);
    const std::string marc_output_filename(argv[2]);
    if (unlikely(marc_input_filename == marc_output_filename))
        LOG_ERROR("Title data input file name equals output file name!");

    std::vector<std::unordered_set<std::string>> subsystem_sets;
    InitializeSubsystemPPNSets(&subsystem_sets);

    std::unique_ptr<MARC::Reader> marc_reader(MARC::Reader::Factory(marc_input_filename));
    FileUtil::AutoTempFile tmp_marc_file("/dev/shm/", ".mrc");
    std::unique_ptr<MARC::Writer> marc_tmp_writer(MARC::Writer::Factory(tmp_marc_file.getFilePath()));
    GetSubsystemPPNSet(marc_reader.get(), marc_tmp_writer.get(), &subsystem_sets);
    if (not marc_tmp_writer->flush())
        LOG_ERROR("Could not flush to " + tmp_marc_file.getFilePath());
    std::unique_ptr<MARC::Reader> marc_tmp_reader(MARC::Reader::Factory(tmp_marc_file.getFilePath()));
    std::unique_ptr<MARC::Writer> marc_writer(MARC::Writer::Factory(marc_output_filename));
    AddSubsystemTags(marc_tmp_reader.get(), marc_writer.get(), subsystem_sets);

    return EXIT_SUCCESS;
}

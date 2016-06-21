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

#include <iomanip>
#include "File.h"
#include "MarcUtil.h"
#include <map>
#include "MarcXmlWriter.h"
#include <memory>
#include "PipelinePhase.h"
#include "StringUtil.h"
#include "util.h"
#include <vector>

#include "PhaseDeleteUnusedLocalData.h"
#include "PhaseExtractKeywordsForTranslation.h"
#include "PhaseAddSuperiorFlag.h"
#include "PhaseAddAuthorSynonyms.h"
#include "PhaseAddIsbnsOrIssnsToArticles.h"
#include "PhaseEnrichKeywordsWithTitleWords.h"
#include "PhaseAugmentBibleReferences.h"
#include "PhaseUpdateIxtheoNotations.h"
#include "PhaseMapDdcAndRvkToIxtheoNotations.h"
#include "PhaseAugment773a.h"


using PipelinePhaseList = std::vector <std::unique_ptr<PipelinePhase>>;


template<typename T>
std::unique_ptr<PipelinePhase> createInstance() { return std::unique_ptr<T>(new T()); }


static std::vector <std::pair<const std::string, std::unique_ptr<PipelinePhase>(*)(void)>> phase_store{
        { "DeleteUnusedLocalData",         &createInstance<PhaseDeleteUnusedLocalData> },
        {"ExtractKeywordsForTranslation", &createInstance<PhaseExtractKeywordsForTranslation>},
        {"AddSuperiorFlag",               &createInstance<PhaseAddSuperiorFlag>},
        {"AddAuthorSynonyms",             &createInstance<PhaseAddAuthorSynonyms>},
        {"AddIsbnsOrIssnsToArticles",     &createInstance<PhaseAddIsbnsOrIssnsToArticles>},
        {"EnrichKeywordsWithTitleWords",  &createInstance<PhaseEnrichKeywordsWithTitleWords>},
        {"AugmentBibleReferences",        &createInstance<PhaseAugmentBibleReferences>},
        {"UpdateIxtheoNotations",         &createInstance<PhaseUpdateIxtheoNotations>},
        {"MapDdcAndRvkToIxtheoNotations", &createInstance<PhaseMapDdcAndRvkToIxtheoNotations>},
        {"Augment773a",                   &createInstance<PhaseAugment773a>}
};

static std::map<const PipelinePhase * const, std::string> phase_to_name_map;

static bool debug = false, verbose = false;

static PipelineMonitor monitor;


void Usage() {
    std::cerr << "Usage: " << ::progname << " marc_input norm_data_input marc_output [active phases]\n";
    std::cerr << "marc_output has to be a xml file\n";
    std::cerr << "Valid values for active phases are:\n";
    for (auto &phase : phase_store) {
        std::cerr << "\t" << phase.first << "\n";
    }
    std::exit(EXIT_FAILURE);
}


void handleError(const std::string &error_message, const std::unique_ptr <PipelinePhase> &phase, const MarcUtil::Record &record) {
    File marc_output("./" + record.getControlNumber() + ".xml", "w");
    MarcXmlWriter xml_writer(&marc_output);
    record.write(&xml_writer);
    xml_writer.closeTag();

    Error("Error while processing record '" + record.getControlNumber() + "' in phase '" + phase_to_name_map[phase.get()] + "':\n"
                  + error_message + "\nFailing record was written to ./" + record.getControlNumber() + ".xml"
    );
}


void initPhase(PipelinePhaseList &phases, const std::string name, std::unique_ptr<PipelinePhase>(*createPhase)(void)) {
    if (debug) std::cout << "Activated #" << phases.size() + 1 << ": " << name << "\n";

    std::unique_ptr <PipelinePhase> phase = createPhase();
    phase->verbose = verbose;
    phase->debug = debug;
    phase->monitor = &monitor;
    phases.emplace_back(std::move(phase));
    phase_to_name_map[phases.back().get()] = name;
}

/**
 * /param active_phases comma separated list of phase names, or empty to activate all phases. See phase_store for valid names.
 */
void initPhases(PipelinePhaseList &phases, const std::string &active_phases) {
    if (active_phases.empty()) {  // all phases are active.
        for (auto iter = phase_store.begin(); iter != phase_store.end(); ++iter)
            initPhase(phases, iter->first, iter->second);
    } else { // only some phases are active.
        std::vector <std::string> active_phases_names;
        StringUtil::Split(active_phases, ",", &active_phases_names);
        for (const auto &active_phases_name : active_phases_names) {
            for (auto &phase_store_entry : phase_store) {
                if (phase_store_entry.first == active_phases_name && phase_store_entry.second != nullptr) {
                    initPhase(phases, active_phases_name, phase_store_entry.second);
                    phase_store_entry.second = nullptr;
                    break;
                }
            }
        }
        if (phases.size() != active_phases_names.size())
            Error("You named unknown phases. Check your arguments.");
    }
}


void deletePhases(PipelinePhaseList &phases) {
    for (auto &phase : phases) {
        delete phase.release();
    }
    phases.clear();
}


inline MarcUtil::Record read(File * const marc_input, const bool is_xml_marc_input) {
    return is_xml_marc_input ? MarcUtil::Record::XmlFactory(marc_input) : MarcUtil::Record::BinaryFactory(marc_input);
}


template<typename RecordType>
bool ProcessRecord(const PipelinePhaseList &phases, MarcUtil::Record &record,
                   PipelinePhaseState (PipelinePhase::*phaseStep)(RecordType &record, std::string * const)) {
    size_t phase_count(0);
    std::string error_message;

    if (debug) std::cerr << record.getControlNumber() << " ";

    for (const std::unique_ptr <PipelinePhase> &phase : phases) {
        if (debug) std::cerr << ++phase_count;

        error_message.clear();
        const PipelinePhaseState state = (*phase.*phaseStep)(record, &error_message);

        if (debug) std::cerr << ", ";

        if (state == ERROR) {
            handleError(error_message, phase, record);
        } else if (state == PURGE_RECORD) {
            return false;
        }
    }
    if (debug) std::cerr << "\r";
    return true;
}


unsigned PreprocessFile(const PipelinePhaseList &phases, File * const norm_data_input, const bool is_xml_norm_input,
                    PipelinePhaseState (PipelinePhase::*phaseStep)(const MarcUtil::Record &record, std::string * const)) {
    size_t count(0);
    while (MarcUtil::Record record = read(norm_data_input, is_xml_norm_input)) {
        ++count;
        if (debug) std::cout << std::setw(8) << count << " " << std::flush;
        ProcessRecord<const MarcUtil::Record>(phases, record, phaseStep);
    }
    return count;
}


unsigned ProcessFile(const PipelinePhaseList &phases, File * const marc_input, File * const marc_output, const bool is_xml_marc_input,
                 PipelinePhaseState (PipelinePhase::*phaseStep)(MarcUtil::Record &record, std::string * const)) {
    MarcXmlWriter xml_writer(marc_output);
    unsigned count(0);
    while (MarcUtil::Record record = read(marc_input, is_xml_marc_input)) {
        ++count;
        record.setRecordWillBeWrittenAsXml(true);
        if (ProcessRecord<MarcUtil::Record>(phases, record, phaseStep)) {
            record.write(&xml_writer);
        }
    }
    return count;
}


void RunPipeline(const PipelinePhaseList &phases, File * const marc_input, File * const norm_data_input, File * const marc_output,
                 const bool is_xml_marc_input, const bool is_xml_norm_input) {
    std::cout << "Preprocess...\n";
    unsigned recordCount = PreprocessFile(phases, marc_input, is_xml_marc_input, &PipelinePhase::preprocess);
    monitor.setCounter("Pipeline", "# records", recordCount);

    std::cout << "Preprocess norm data...\n";
    unsigned normRecordCount = PreprocessFile(phases, norm_data_input, is_xml_norm_input, &PipelinePhase::preprocessNormData);
    monitor.setCounter("Pipeline", "# norm records", normRecordCount);

    if (not marc_input->seek(0))
        Error("Failed to seek.");

    std::cout << "Process ...\n";
    ProcessFile(phases, marc_input, marc_output, is_xml_marc_input, &PipelinePhase::process);

    std::cout << "================================\n";
}


int main(int argc, char **argv) {
    progname = argv[0];
/*
    bool flag_found = false;
    do {
        flag_found = false;
        if (strcmp(argv[1], "-d") == 0 || strcmp(argv[1], "--debug") == 0) {
            debug = true;
            ++argv;
            --argc;
            flag_found = true;
        } else if (strcmp(argv[1], "-v") == 0 || strcmp(argv[1], "--verbose") == 0) {
            verbose = true;
            ++argv;
            --argc;
            flag_found = true;
        }
    } while (flag_found);
*/
    if (argc < 3 || argc > 4)
        Usage();

    const std::string marc_input_filename(argv[1]);
    const std::string norm_input_filename(argv[2]);
    const std::string marc_output_filename("GesamtTiteldaten-post-pipeline.xml");

    // Tests for file extensions
    if (unlikely(not StringUtil::EndsWith(marc_input_filename, ".mrc") and not StringUtil::EndsWith(marc_input_filename, ".marc")
                         and not StringUtil::EndsWith(marc_input_filename, ".xml")))
        Error("Unexpected file extension for master input file. Expected 'mrc', 'marc' or 'xml': " + marc_input_filename);
    if (unlikely(not StringUtil::EndsWith(norm_input_filename, ".mrc") and not StringUtil::EndsWith(norm_input_filename, ".marc")
                         and not StringUtil::EndsWith(norm_input_filename, ".xml")))
        Error("Unexpected file extension for norm data input file. Expected 'mrc', 'marc' or 'xml': " + norm_input_filename);
    if (unlikely(not StringUtil::EndsWith(marc_output_filename, ".xml")))
        Error("Unexpected file extension for marc output file. Expected 'xml': " + marc_output_filename);

    // Tests to protect overriding of files or using the wrong files
    if (unlikely(marc_input_filename == norm_input_filename))
        Error("Master input file name equals norm data file name!");
    if (unlikely(norm_input_filename == marc_output_filename))
        Error("Norm data input file name equals output file name!");
    if (unlikely(marc_input_filename == marc_output_filename))
        Error("Master input file name equals output file name!");

    File marc_input(marc_input_filename, "rmb");
    if (not marc_input)
        Error("can't open \"" + marc_input_filename + "\" for reading!");

    File norm_data_input(norm_input_filename, "rb");
    if (not norm_data_input)
        Error("can't open \"" + marc_input_filename + "\" for reading!");

    File marc_output(marc_output_filename, "w");
    if (not marc_output)
        Error("can't open \"" + marc_output_filename + "\" for writing!");

    bool is_xml_marc_input = StringUtil::EndsWith(marc_input_filename, ".xml");
    bool is_xml_norm_input = StringUtil::EndsWith(norm_input_filename, ".xml");

    try {
        PipelinePhaseList phases;
        const std::string active_phases = (argc == 5) ? argv[4] : "";
        initPhases(phases, active_phases);
        RunPipeline(phases, &marc_input, &norm_data_input, &marc_output, is_xml_marc_input, is_xml_norm_input);
        deletePhases(phases);
    } catch (const std::exception &x) {
        Error("caught exception: " + std::string(x.what()));
    }
}

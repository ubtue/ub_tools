#include "PhaseUpdateIxtheoNotations.h"

#include <iostream>
#include <unordered_map>
#include <cctype>
#include <cstdlib>
#include "DirectoryEntry.h"
#include "Leader.h"
#include "MarcUtil.h"
#include "MarcXmlWriter.h"
#include "Subfields.h"
#include "util.h"

static const std::string CODE_TO_DESCRIPTION_MAP_FILENAME("/usr/local/ub_tools/cpp/data/IxTheo_Notation.csv");
static std::unordered_map<std::string, std::string> code_to_description_map;
unsigned ixtheo_notation_count(0), records_with_ixtheo_notations(0);


void LoadCodeToDescriptionMap(File * const code_to_description_map_file)
{
    unsigned line_no(0);
    while (not code_to_description_map_file->eof()) {
        const std::string line(code_to_description_map_file->getline());
        ++line_no;
        if (line.length() < 4) // Need at least a 2 character code, a comma and some text.
            continue;

        const size_t comma_pos(line.find(','));
        if (comma_pos == std::string::npos)
            Error("malformed line " + std::to_string(line_no) + " in \"" + code_to_description_map_file->getPath() + "\"! (1)");

        const std::string code(line.substr(0, comma_pos));
        if (code.length() != 2 and code.length() != 3)
            Error("malformed line " + std::to_string(line_no) + " in \"" + code_to_description_map_file->getPath() + "\"! (2)");

        code_to_description_map[code] = line.substr(comma_pos + 1);
    }
}


bool LocalBlockIsFromUbTueTheologians(const std::pair<size_t, size_t> &local_block_begin_and_end,
                                      const MarcUtil::Record &record)
{
    std::vector<size_t> _852_indices;
    record.findFieldsInLocalBlock("852", "  ", local_block_begin_and_end, &_852_indices);

    const std::vector<std::string> &fields(record.getFields());
    for (const auto index : _852_indices) {
        const Subfields subfields(fields[index]);
        if (subfields.hasSubfieldWithValue('a', "Tü 135"))
            return true;
    }

    return false;
}


unsigned ExtractIxTheoNotations(const std::pair<size_t, size_t> &local_block_begin_and_end,
                                const MarcUtil::Record &record,
                                std::string * const ixtheo_notations_list)
{
    std::vector<size_t> _936ln_indices;
    record.findFieldsInLocalBlock("936", "ln", local_block_begin_and_end, &_936ln_indices);

    size_t found_count(0);
    const std::vector<std::string> &fields(record.getFields());
    for (const auto index : _936ln_indices) {
        const Subfields subfields(fields[index]);
        const std::string ixtheo_notation_candidate(subfields.getFirstSubfieldValue('a'));
        if (code_to_description_map.find(ixtheo_notation_candidate) != code_to_description_map.end()) {
            ++found_count;
            if (ixtheo_notations_list->empty())
                *ixtheo_notations_list = ixtheo_notation_candidate;
            else
                *ixtheo_notations_list += ":" + ixtheo_notation_candidate;
        }
    }

    return found_count;
}


PipelinePhaseState PhaseUpdateIxtheoNotations::process(MarcUtil::Record &record, std::string * const) {
    std::vector<std::pair<size_t, size_t>> local_block_boundaries;
    if (record.findAllLocalDataBlocks(&local_block_boundaries) == 0)
        return SUCCESS;

    std::string ixtheo_notations_list; // Colon-separated list of ixTheo notations.
    for (const auto &local_block_begin_and_end : local_block_boundaries) {
        if (not LocalBlockIsFromUbTueTheologians(local_block_begin_and_end, record))
            continue;

        const unsigned notation_count(ExtractIxTheoNotations(local_block_begin_and_end, record, &ixtheo_notations_list));
        if (notation_count > 0) {
            ++records_with_ixtheo_notations;
            ixtheo_notation_count += notation_count;
        }
    }

    if (not ixtheo_notations_list.empty()) // Insert a new 652 field w/ a $a subfield.
        record.insertField("652", "  ""\x1F""a" + ixtheo_notations_list);
    return SUCCESS;
};


PhaseUpdateIxtheoNotations::PhaseUpdateIxtheoNotations() {
    File code_to_description_map_file(CODE_TO_DESCRIPTION_MAP_FILENAME, "r");
    LoadCodeToDescriptionMap(&code_to_description_map_file);
}


PhaseUpdateIxtheoNotations::~PhaseUpdateIxtheoNotations() {
    std::cerr << "Update Ixtheo Notations:\n";
    std::cerr << "\t" << records_with_ixtheo_notations << " records had ixTheo notations.\n";
    std::cerr << "\tFound " << code_to_description_map.size() << " code to description mappings.\n";
    std::cerr << "\tFound " << ixtheo_notation_count << " ixTheo notations overall.\n";
}
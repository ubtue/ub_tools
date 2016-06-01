#include "PhaseAugment773a.h"

#include <iostream>
#include <unordered_map>
#include <vector>
#include <cstdlib>
#include "Compiler.h"
#include "DirectoryEntry.h"
#include "Leader.h"
#include "MarcUtil.h"
#include "RegexMatcher.h"
#include "StringUtil.h"
#include "Subfields.h"
#include "util.h"

static std::unordered_map <std::string, std::string> control_numbers_to_titles_map;
static unsigned patch_count;


PipelinePhaseState PhaseAugment773a::preprocess(const MarcUtil::Record &record, std::string *const) {
    ssize_t _245_index;
    if (likely((_245_index = record.getFieldIndex("245")) != -1)) {
        const std::vector <std::string> &fields(record.getFields());
        const Subfields _245_subfields(fields[_245_index]);
        std::string title(_245_subfields.getFirstSubfieldValue('a'));
        if (_245_subfields.hasSubfield('b'))
            title += " " + _245_subfields.getFirstSubfieldValue('b');
        StringUtil::RightTrim(" \t/", &title);
        if (likely(not title.empty()))
            control_numbers_to_titles_map[record.getControlNumber()] = title;
    }
    return SUCCESS;
};


PipelinePhaseState PhaseAugment773a::process(MarcUtil::Record &record, std::string *const) {
    ssize_t _773_index;
    if ((_773_index = record.getFieldIndex("773")) != -1) {
        const std::vector <std::string> &fields(record.getFields());
        const Subfields _773_subfields(fields[_773_index]);
        if (not _773_subfields.hasSubfield('a') and _773_subfields.hasSubfield('w')) {
            const std::string w_subfield(_773_subfields.getFirstSubfieldValue('w'));
            if (StringUtil::StartsWith(w_subfield, "(DE-576)")) {
                const std::string parent_control_number(w_subfield.substr(8));
                const auto control_number_and_title(control_numbers_to_titles_map.find(parent_control_number));
                if (control_number_and_title != control_numbers_to_titles_map.end()) {
                    record.updateField(_773_index, fields[_773_index] + "\x1F""a" + control_number_and_title->second);
                    ++patch_count;
                }
            }
        }
    }
    return SUCCESS;
};


PhaseAugment773a::~PhaseAugment773a() {
    std::cerr << "Augment 773a:\n";
    std::cerr << "\tFound " << control_numbers_to_titles_map.size() << " control number to title mappings.\n";
    std::cerr << "\tAdded 773$a subfields to " << patch_count << " records.\n";
}
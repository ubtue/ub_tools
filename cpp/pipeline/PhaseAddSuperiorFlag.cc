#include "PhaseAddSuperiorFlag.h"

#include <fstream>
#include <iostream>
#include <memory>
#include <unordered_map>
#include <cstdlib>
#include <cstring>
#include "DirectoryEntry.h"
#include "File.h"
#include "Leader.h"
#include "MarcUtil.h"
#include "MarcXmlWriter.h"
#include "StringUtil.h"
#include "Subfields.h"
#include "util.h"
#include <exception>


static unsigned modified_count(0);
static std::set <std::string> superior_ppns;
static std::string superior_subfield_data;


PipelinePhaseState PhaseAddSuperiorFlag::preprocess(const MarcUtil::Record &record, std::string *const) {
    std::vector <std::string> subfields;
    record.extractSubfields("800:810:830:773", "w", &subfields);

    for (auto subfield : subfields) {
        if (StringUtil::StartsWith(subfield, "(DE-576)"))
            superior_ppns.insert(subfield.substr(8));
    }
    return SUCCESS;
};


PipelinePhaseState PhaseAddSuperiorFlag::process(MarcUtil::Record &record, std::string *const error_message) {
    // Don't add the flag twice
    if (record.getFieldIndex("SPR") != -1)
        return SUCCESS;

    const std::vector <std::string> &field_data(record.getFields());
    const auto iter(superior_ppns.find(field_data.at(0)));
    if (iter != superior_ppns.end()) {
        if (not record.insertField("SPR", superior_subfield_data)) {
            return MakeError("Not enough room to add a SPR field! (Control number: " + field_data[0] + ")", error_message);
        }
        ++modified_count;
    }
    return SUCCESS;
};


PhaseAddSuperiorFlag::PhaseAddSuperiorFlag() {
    Subfields superior_subfield(/* indicator1 = */' ', /* indicator2 = */' ');
    superior_subfield.addSubfield('a', "1"); // Could be anything but we can't have an empty field.
    superior_subfield_data = superior_subfield.toString();
}


PhaseAddSuperiorFlag::~PhaseAddSuperiorFlag() {
    std::cerr << "Add superior flag:\n";
    std::cerr << "\tFound " << superior_ppns.size() << " superior ppns.\n";
}
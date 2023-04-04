/**
 * \brief Utility for updating issn information
 * \author Steven Lolong (steven.lolong@uni-tuebingen.de)
 *
 * \copyright 2023 Tübingen University Library.  All rights reserved.
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

#include <fstream>
#include <iostream>
#include "CORE.h"
#include "FileUtil.h"
#include "MARC.h"
#include "util.h"

namespace {
[[noreturn]] void Usage() {
    ::Usage(
        " issn_source input output"
        "\n"
        "- issn_source is the file contain ... \n"
        "- inpu is the file ... \n"
        "- output is the file ... \n");

    std::exit(EXIT_FAILURE);
}
struct SubFieldInfo {
    std::string i;
    std::string t;
    std::string w;
    // std::string x;
    bool is_found;

    SubFieldInfo(bool b) { this->is_found = b; }

    void Builder(MARC::Record &record) {
        for (auto &field : record) {
            if (field.getTag() == "245") {
                MARC::Subfields subfields(field.getSubfields());
                std::string subfield_a(subfields.getFirstSubfieldWithCode('a'));
                std::string subfield_b(subfields.getFirstSubfieldWithCode('b'));
                if (subfield_a != "" && subfield_b != "")
                    this->t = subfield_a + " " + subfield_b;
                else if (subfield_a != "" && subfield_b == "")
                    this->t = subfield_a;
                else if (subfield_a == "" && subfield_b != "")
                    this->t = subfield_b;
                else
                    this->t = "";
            }
        }
        this->i = "In:";
        this->w = "DE-627";
        this->is_found = true;
    }
};

bool IsOnlineISSN(const MARC::Record &record) {
    for (auto &field : record) {
        if (field.getTag() == "300") {
            const std::string sub_field_a(field.getFirstSubfieldWithCode('a'));
            if (sub_field_a == "Online-Ressource")
                return true;
            else
                return false;
        }
    }
    return false;
}

SubFieldInfo LookupISSNInDataSouce(const std::string ds_filename, const std::string issn) {
    auto data_source(MARC::Reader::Factory(ds_filename));
    SubFieldInfo sub_field_info = SubFieldInfo(false);

    while (MARC::Record record = data_source->read()) {
        for (auto &field : record) {
            if (field.getTag() == "022") {
                const std::string sub_field_a(field.getFirstSubfieldWithCode('a'));
                if ((sub_field_a != "") && (sub_field_a == issn)) {
                    sub_field_info.Builder(record);
                    // the issn is online version, no need to check further
                    if (IsOnlineISSN(record))
                        return sub_field_info;
                } else {
                    // maybe the issn is in subfield l
                    const std::string sub_field_l(field.getFirstSubfieldWithCode('l'));
                    if ((sub_field_l != "") && (sub_field_l == issn)) {
                        sub_field_info.Builder(record);
                        // the issn is online version, no need to check further
                        if (IsOnlineISSN(record))
                            return sub_field_info;
                    }
                }
            }
        }
    }
    return sub_field_info;
}

void UpdateSubfield(MARC::Subfields &subfields, const SubFieldInfo &sub_field_info, const std::string &ppn) {
    if (!subfields.replaceFirstSubfield('i', sub_field_info.i))
        subfields.addSubfield('i', sub_field_info.i);
    // if (!subfields.replaceFirstSubfield('x', sub_field_info.x))
    //     subfields.addSubfield('x', sub_field_info.x);
    if (!subfields.replaceFirstSubfield('w', sub_field_info.w + "-" + ppn))
        subfields.addSubfield('w', sub_field_info.w + "-" + ppn);

    if (sub_field_info.t != "") {
        if (!subfields.replaceFirstSubfield('t', sub_field_info.t))
            subfields.addSubfield('t', sub_field_info.t);
    }
}

void ISSNLookup(char **argv) {
    auto input_file(MARC::Reader::Factory(argv[2]));
    auto output_file(MARC::Writer::Factory(argv[3]));
    int onprogress_counter(1);
    std::string ppn("");

    std::cout << "Updating in progress...\n";
    while (MARC::Record record = input_file->read()) {
        std::cout << "Record - " << onprogress_counter << "\r";
        ppn = "";

        for (auto &field : record) {
            if (field.getTag() == "001")
                ppn = field.getContents();

            if (field.getTag() == "773") {
                const std::string issn(field.getFirstSubfieldWithCode('x'));
                if (issn != "") {
                    SubFieldInfo sub_field_info(LookupISSNInDataSouce(argv[1], issn));
                    if (sub_field_info.is_found) {
                        MARC::Subfields subfields(field.getSubfields());

                        // update subfield
                        UpdateSubfield(subfields, sub_field_info, ppn);
                        field.setSubfields(subfields);
                    }
                }
            }
        }
        ++onprogress_counter;
        output_file->write(record);
    }
}
} // end of namespace

int Main(int argc, char **argv) {
    if (argc < 4)
        Usage();


    ISSNLookup(argv);

    return EXIT_SUCCESS;
}
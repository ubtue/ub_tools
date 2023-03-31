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
        "- source_data is the \n"
        "- input_file is the \n"
        "- output_file is the \n");
}
struct SubFieldInfo {
    std::string i;
    std::string t;
    std::string w;
    std::string x;
    bool is_found;
    SubFieldInfo(bool b) { this->is_found = b; }
    void Builder(MARC::Record::Field &field, const char code) {
        this->i = "In:";
        this->w = "DE-627";
        this->x = field.getFirstSubfieldWithCode(code);
        this->is_found = true;
    }
};

bool IsOnlineISSN(const MARC::Record::Field &field) {
}

SubFieldInfo LookupISSNInDataSouce(const std::string ds_filename, const std::string issn) {
    auto data_source(MARC::Reader::Factory(ds_filename));
    SubFieldInfo sub_field_info = SubFieldInfo(false);

    while (MARC::Record record = data_source->read()) {
        for (auto &field : record) {
            if (field.getTag() == "022") {
                const std::string sub_field_a(field.getFirstSubfieldWithCode('a'));
                if ((sub_field_a != "") && (sub_field_a == issn)) {
                    sub_field_info.Builder(field, 'a');
                    if (IsOnlineISSN(field)) {
                        // the issn is online version, no need to check further
                        return sub_field_info;
                    }
                } else {
                    // maybe the issn is in subfield l
                    const std::string sub_field_l(field.getFirstSubfieldWithCode('l'));
                    if ((sub_field_l != "") && (sub_field_a == issn)) {
                        sub_field_info.Builder(field, 'l');
                    }
                }
            }
        }
    }
    return sub_field_info;
}

void UpdateField(MARC::Record::Field const &field, const SubFieldInfo &sub_field_info) {
}

void ISSNLookup(char **argv) {
    auto input_file(MARC::Reader::Factory(argv[2]));
    // auto output_file(MARC::Writer::Factory(argv[3]));
    while (MARC::Record recd = input_file->read()) {
        for (auto &field : recd) {
            if (field.getTag() == "773") {
                const std::string issn(field.getFirstSubfieldWithCode('x'));
                if (issn != "") {
                    SubFieldInfo sub_field_info = LookupISSNInDataSouce(argv[1], issn);
                    if (sub_field_info.is_found) {
                        // update record
                    }
                }
            }
        }
        // output_file->write(record);
    }
    std::cout << "\n souce data\n" << std::endl;
}
} // namespace

int Main(int argc, char **argv) {
    if (argc < 2)
        Usage();


    ISSNLookup(argv);

    return EXIT_SUCCESS;
}
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
        "marc_input_articles marc_input_journals marc_output_articles"
        "\n"
        "- marc_input_articles is a file containing all article information taken from CORE.\n"
        "- marc_input_journals is a file containing journal information. Please use issn_lookup.py to generate this file.\n"
        "- marc_output_articles is an output file generated by this tool.\n");

    std::exit(EXIT_FAILURE);
}
struct SubFieldInfo {
    std::string i_;
    std::string t_;
    std::string w_;
    std::string x_;
    int online_version_counter_;
    int printed_version_counter_;
    bool is_online_;
    bool is_valid_;


    void Builder(MARC::Record &record) {
        for (auto &field : record) {
            if (field.getTag() == "001")
                w_ = "(DE-627)" + field.getContents();

            if (field.getTag() == "245") {
                MARC::Subfields subfields(field.getSubfields());
                std::string subfield_a(subfields.getFirstSubfieldWithCode('a'));
                std::string subfield_b(subfields.getFirstSubfieldWithCode('b'));
                if (subfield_a != "" && subfield_b != "")
                    t_ = subfield_a + " " + subfield_b;
                else if (subfield_a != "" && subfield_b == "")
                    t_ = subfield_a;
                else if (subfield_a == "" && subfield_b != "")
                    t_ = subfield_b;
                else
                    t_ = "";
            }

            if (field.getTag() == "300")
                ((field.getFirstSubfieldWithCode('a') == "Online-Ressource") ? is_online_ = true : is_online_ = false);


            if (field.getTag() == "773")
                x_ = field.getFirstSubfieldWithCode('x');
        }

        i_ = "In:";
    }
};


void UpdateSubfield(MARC::Subfields &subfields, const SubFieldInfo &sub_field_info) {
    if (!subfields.replaceFirstSubfield('i', sub_field_info.i_))
        subfields.addSubfield('i', sub_field_info.i_);
    if (!subfields.replaceFirstSubfield('x', sub_field_info.x_))
        subfields.addSubfield('x', sub_field_info.x_);
    if (!subfields.replaceFirstSubfield('w', sub_field_info.w_))
        subfields.addSubfield('w', sub_field_info.w_);

    if (sub_field_info.t_ != "") {
        if (!subfields.replaceFirstSubfield('t', sub_field_info.t_))
            subfields.addSubfield('t', sub_field_info.t_);
    }
}

void UpdateJournalValidity(std::map<std::string, SubFieldInfo> &journal_cache) {
    // check whether the issn is occur more than once
    for (auto sfi : journal_cache) {
        if (sfi.second.online_version_counter_ == 1)
            journal_cache[sfi.first].is_valid_ = true;
        else if (sfi.second.online_version_counter_ > 1)
            journal_cache[sfi.first].is_valid_ = false;
        else if (sfi.second.online_version_counter_ == 0)
            if (sfi.second.printed_version_counter_ == 1)
                journal_cache[sfi.first].is_valid_ = true;
            else
                journal_cache[sfi.first].is_valid_ = false;
    }
}

std::map<std::string, SubFieldInfo> JournalCacheBuilder(const std::string &input_journal_filename) {
    std::map<std::string, SubFieldInfo> journal_cache;
    auto input_journal_file(MARC::Reader::Factory(input_journal_filename));
    int record_counter(0);

    std::cout << "Build a cache for journal \n";
    while (MARC::Record record = input_journal_file->read()) {
        SubFieldInfo sub_field_info_of_record;
        sub_field_info_of_record.Builder(record);

        // if sub_info is exist in the cache
        if (auto sub_info_search = journal_cache.find(sub_field_info_of_record.x_); sub_info_search != journal_cache.end()) {
            if (journal_cache[sub_field_info_of_record.x_].is_online_) {
                if (sub_field_info_of_record.is_online_)
                    ++journal_cache[sub_field_info_of_record.x_].online_version_counter_;
                else
                    ++journal_cache[sub_field_info_of_record.x_].printed_version_counter_;
            } else {
                if (sub_field_info_of_record.is_online_) {
                    journal_cache[sub_field_info_of_record.x_] = sub_field_info_of_record;
                    ++journal_cache[sub_field_info_of_record.x_].online_version_counter_;
                } else
                    ++journal_cache[sub_field_info_of_record.x_].printed_version_counter_;
            }
        }
        // sub_info is not in the cache
        else
        {
            journal_cache.emplace(sub_field_info_of_record.x_, sub_field_info_of_record);
            (sub_field_info_of_record.is_online_ ? ++journal_cache[sub_field_info_of_record.x_].online_version_counter_
                                                 : ++journal_cache[sub_field_info_of_record.x_].printed_version_counter_);
        }
        ++record_counter;
    }
    std::cout << "Total record processed - " << record_counter << "\n";
    UpdateJournalValidity(journal_cache);

    return journal_cache;
}

void ISSNLookup(char **argv, std::map<std::string, SubFieldInfo> &journal_cache) {
    auto input_file(MARC::Reader::Factory(argv[1]));
    auto output_file(MARC::Writer::Factory(argv[3]));
    int onprogress_counter(1);
    for (auto sfi : journal_cache) {
        std::cout << sfi.first << std::endl;
    }
    std::cout << "Updating in progress...\n\n";
    while (MARC::Record record = input_file->read()) {
        std::cout << "Record - " << onprogress_counter << "\r";
        for (auto &field : record) {
            if (field.getTag() == "773") {
                const std::string issn(field.getFirstSubfieldWithCode('x'));
                if (not issn.empty()) {
                    // data is found
                    if (auto ji_search = journal_cache.find(issn); ji_search != journal_cache.end()) {
                        std::cout << issn << std::endl;
                        if (journal_cache[issn].is_valid_) {
                            MARC::Subfields subfields(field.getSubfields());
                            UpdateSubfield(subfields, journal_cache[issn]);
                            field.setSubfields(subfields);
                        }
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
    if (argc != 4)
        Usage();


    std::map<std::string, SubFieldInfo> journal_cache = JournalCacheBuilder(argv[2]);
    ISSNLookup(argv, journal_cache);

    return EXIT_SUCCESS;
}
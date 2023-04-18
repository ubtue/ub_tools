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

#include <iostream>
#include "CORE.h"
#include "FileUtil.h"
#include "MARC.h"
#include "StringUtil.h"
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

// avoiding duplication in issns's cache
void InsertIssnIfNotExist(const std::string &issn, std::vector<std::string> * const issns) {
    if (std::find(issns->begin(), issns->end(), StringUtil::ASCIIToUpper(issn)) == issns->end())
        issns->emplace_back(StringUtil::ASCIIToUpper(issn));
}


void InsertIfNotExist(const std::vector<std::string> &issns_input, std::vector<std::string> * const issns) {
    for (const auto &issn : issns_input)
        InsertIssnIfNotExist(issn, issns);
}


struct SubFieldInfo {
    std::string t_;
    std::string w_;
    std::string x_;
    std::vector<std::string> issns_;
    bool is_online_;
    bool is_valid_;

    SubFieldInfo() {
        is_valid_ = false;
        is_online_ = false;
        t_ = "";
        w_ = "";
        x_ = "";
    }

    SubFieldInfo(MARC::Record &record) {
        is_valid_ = false;
        is_online_ = false;
        for (auto &field : record) {
            if (field.getTag() == "001")
                w_ = "(DE-627)" + field.getContents();

            if (field.getTag() == "022") {
                x_ = StringUtil::ASCIIToUpper(field.getFirstSubfieldWithCode('a'));
                InsertIssnIfNotExist(x_, &issns_);

                if (not field.getFirstSubfieldWithCode('l').empty())
                    InsertIssnIfNotExist(StringUtil::ASCIIToUpper(field.getFirstSubfieldWithCode('l')), &issns_);
            }

            if (field.getTag() == "245") {
                MARC::Subfields subfields(field.getSubfields());
                std::string subfield_a(subfields.getFirstSubfieldWithCode('a'));
                std::string subfield_b(subfields.getFirstSubfieldWithCode('b'));
                if ((not subfield_a.empty()) && (not subfield_b.empty()))
                    t_ = subfield_a + " " + subfield_b;
                else if ((not subfield_a.empty()) && subfield_b.empty())
                    t_ = subfield_a;
                else if (subfield_a.empty() && (not subfield_b.empty()))
                    t_ = subfield_b;
                else
                    t_ = "";
            }

            if (field.getTag() == "300")
                is_online_ = (field.getFirstSubfieldWithCode('a') == "Online-Ressource");
        }
    }
};


void PrettyPrintSubFieldInfo(const SubFieldInfo &sfi) {
    std::cout << "t: " << sfi.t_ << std::endl;
    std::cout << "w: " << sfi.w_ << std::endl;
    std::cout << "x: " << sfi.x_ << std::endl;
    std::cout << "online: " << (sfi.is_online_ ? "yes" : "no") << std::endl;
    std::cout << "valid: " << (sfi.is_valid_ ? "yes" : "no") << std::endl;
    std::cout << "related issn(s): " << std::endl;
    for (const auto &issn : sfi.issns_)
        if (issn != sfi.x_)
            std::cout << "* " << issn << std::endl;
}


void PrettyPrintCache(const std::vector<SubFieldInfo> &journal_cache) {
    unsigned i(1);
    std::cout << "********* Cache *********" << std::endl;
    for (const auto &jc : journal_cache) {
        std::cout << "=== Record - " << i << std::endl;
        PrettyPrintSubFieldInfo(jc);
        std::cout << std::endl;
        ++i;
    }
    std::cout << "******** End of Cache ***********" << std::endl;
}


bool IsInISSNs(const std::string &issn, const std::vector<std::string> &issns) {
    return (std::find(issns.begin(), issns.end(), StringUtil::ASCIIToUpper(issn)) != issns.end() ? true : false);
}


bool IsInISSNs(const std::vector<std::string> &issns, const std::vector<std::string> &issns_input) {
    for (const auto &issn : issns_input)
        if (std::find(issns.begin(), issns.end(), StringUtil::ASCIIToUpper(issn)) != issns.end())
            return true;

    return false;
}


void UpdateSubfield(MARC::Subfields &subfields, const SubFieldInfo &sub_field_info) {
    if (!subfields.replaceFirstSubfield('i', "In:"))
        subfields.addSubfield('i', "In:");
    if (!subfields.replaceFirstSubfield('x', sub_field_info.x_))
        subfields.addSubfield('x', sub_field_info.x_);
    if (!subfields.replaceFirstSubfield('w', sub_field_info.w_))
        subfields.addSubfield('w', sub_field_info.w_);

    if (not sub_field_info.t_.empty())
        if (!subfields.replaceFirstSubfield('t', sub_field_info.t_))
            subfields.addSubfield('t', sub_field_info.t_);
}


void UpdateSubFieldInfo(SubFieldInfo &sfi, const SubFieldInfo &new_sfi, const bool is_online) {
    sfi.t_ = new_sfi.t_;
    sfi.w_ = new_sfi.w_;
    sfi.x_ = new_sfi.x_;
    sfi.is_online_ = is_online;
    InsertIfNotExist(new_sfi.issns_, &sfi.issns_);
}


void UpdateSubFieldAndCombineIssn(SubFieldInfo * const sfi, const SubFieldInfo &new_sfi) {
    bool subset(true);

    for (const auto &v1 : new_sfi.issns_)
        subset = (subset || IsInISSNs(v1, sfi->issns_));

    if (not subset) {
        if (sfi->is_online_ && new_sfi.is_online_) {
            sfi->is_valid_ = false;
        }

        if (!sfi->is_online_ && new_sfi.is_online_) {
            sfi->is_valid_ = true;
            sfi->t_ = new_sfi.t_;
            sfi->w_ = new_sfi.w_;
            sfi->x_ = new_sfi.x_;
        }

        if (!sfi->is_online_ && !new_sfi.is_online_)
            sfi->is_valid_ = false;

        InsertIfNotExist(new_sfi.issns_, &sfi->issns_);
    }
}


std::vector<SubFieldInfo> MergeIssn(const std::vector<SubFieldInfo> &journal_cache) {
    std::vector<SubFieldInfo> tmp_cache = journal_cache, new_journal_cache;
    std::vector<SubFieldInfo>::iterator iter;
    SubFieldInfo content;
    bool restart(false);

    // Iteration of multiple checking (multi-pass checking)
    for (unsigned i = 0; i < tmp_cache.size(); i++) {
        content = tmp_cache[i];

        do {
            restart = false;

            for (unsigned j = i + 1; j < tmp_cache.size(); j++) {
                if (IsInISSNs(content.issns_, tmp_cache[j].issns_)) {
                    UpdateSubFieldAndCombineIssn(&content, tmp_cache[j]);
                    iter = tmp_cache.begin() + j;
                    tmp_cache.erase(iter);
                    restart = true;
                    break;
                }
            }

        } while (restart);
        new_journal_cache.emplace_back(content);
        restart = false;
    }

    return new_journal_cache;
}


std::vector<SubFieldInfo> BuildJournalCache(const std::string &input_journal_filename) {
    std::vector<SubFieldInfo> journal_cache;
    auto input_journal_file(MARC::Reader::Factory(input_journal_filename));

    std::cout << "building..." << std::endl;
    while (MARC::Record record = input_journal_file->read()) {
        SubFieldInfo sub_field_info_of_record(record);
        bool exist_in_journal_cache(false);

        for (auto &elemt : journal_cache) {
            if (IsInISSNs(sub_field_info_of_record.issns_, elemt.issns_)) {
                exist_in_journal_cache = true;
                if (elemt.is_online_) {
                    if (sub_field_info_of_record.is_online_) {
                        InsertIfNotExist(sub_field_info_of_record.issns_, &elemt.issns_);
                        elemt.is_valid_ = false;
                    } else
                        InsertIfNotExist(sub_field_info_of_record.issns_, &elemt.issns_);
                } else {
                    if (sub_field_info_of_record.is_online_) {
                        UpdateSubFieldInfo(elemt, sub_field_info_of_record, true);
                        elemt.is_valid_ = true;
                    } else {
                        // print issn refer to other print issn
                        elemt.is_valid_ = false;
                        InsertIssnIfNotExist(sub_field_info_of_record.x_, &elemt.issns_);
                    }
                }
            }
        }

        if (not exist_in_journal_cache) {
            sub_field_info_of_record.is_valid_ = true;
            journal_cache.emplace_back(sub_field_info_of_record);
        }
    }

    return MergeIssn(journal_cache);
}


void ISSNLookup(char **argv, std::vector<SubFieldInfo> &journal_cache) {
    auto input_file(MARC::Reader::Factory(argv[1]));
    auto output_file(MARC::Writer::Factory(argv[3]));
    std::vector<std::string> updated_ppn, ignored_ppn;

    while (MARC::Record record = input_file->read()) {
        std::string ppn("");
        for (auto &field : record) {
            if (field.getTag() == "001")
                ppn = field.getContents();

            if (field.getTag() == "773") {
                const std::string issn(StringUtil::ASCIIToUpper(field.getFirstSubfieldWithCode('x')));
                if (not issn.empty()) {
                    // data is found
                    for (const auto &elemt : journal_cache) {
                        bool is_in_l = (std::find(elemt.issns_.begin(), elemt.issns_.end(), issn) != elemt.issns_.end() ? true : false);
                        if (((elemt.x_ == issn) || is_in_l) && elemt.is_valid_) {
                            MARC::Subfields subfields(field.getSubfields());
                            UpdateSubfield(subfields, elemt);
                            field.setSubfields(subfields);
                        }
                    }
                }
            }
        }
        output_file->write(record);
    }
}


} // end of namespace


int Main(int argc, char **argv) {
    if (argc != 4)
        Usage();

    std::vector<SubFieldInfo> journal_cache(BuildJournalCache(argv[2]));
    ISSNLookup(argv, journal_cache);

    PrettyPrintCache(journal_cache);
    return EXIT_SUCCESS;
}

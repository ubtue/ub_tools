/** \brief Utility for harvesting metadata from NACJD (ICPSR) - Publication.
 *  \author Steven Lolong (steven.lolong@uni-tuebingen.de)
 *
 *  \copyright 2024 Tübingen University Library.  All rights reserved.
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
#include <nlohmann/json.hpp>
#include "MARC.h"
#include "StringUtil.h"


using json = nlohmann::json;

namespace {

[[noreturn]] void Usage() {
    ::Usage(
        "[mode] [mode_params]\n"
        "construct input_file output_file\n"
        "\t- input_file: source of data in JSON format (taken from NACJD website).\n"
        "\t- output_file: will contain all icpsr records as MARC21.\n"
        "\n");
}

struct NacjdDoc {
    std::string ref_id_, // REF_ID
        title_,          // TITLE
        issn_,           // ISSN
        volume_,         // VOLUME
        journal_,        // JOURNAL
        i_number_,       // INUMBER
        year_pub_,       // YEAR_PUB
        place_pub_,      // PLACE_PUB
        sec_title_,      // SEC_TITLE
        ris_type_,       // RIS_TYPE, ex.: RPRT, JOUR, etc.
        page_start_,     // PAGE_START
        page_end_,       // PAGE_END
        doi_,            // DOI
        url_,            // URL
        url_pdf_,        // URL_PDF
        url_abs_,        // URL_ABS
        type_work_,      // TYPE_WORK
        publisher_;      // PUBLISHER

    std::vector<std::string> author_split_, // AUTHORS_SPLIT
        study_title_;                       // STUDYTITLE
};
void WriteMarc(const std::string &marc_path, const std::vector<NacjdDoc> &nacjd_docs) {
    auto marc_file(MARC::Writer::Factory(marc_path));
    MARC::Writer * const marc_writer(marc_file.get());

    for (auto nacjd_doc : nacjd_docs) {
        if (nacjd_doc.ref_id_.empty())
            continue;

        // construct author_info
        std::vector<MARC::Subfield> authors;
        if (not nacjd_doc.author_split_.empty()) {
            for (auto author_ : nacjd_doc.author_split_) {
                authors.emplace_back(MARC::Subfield('a', author_));
            }
        }
        // construct publishing information
        std::vector<MARC::Subfield> publishing_info, publishing_260_info;
        if (not nacjd_doc.place_pub_.empty()) {
            publishing_260_info.emplace_back(MARC::Subfield('a', nacjd_doc.place_pub_));
        }
        if (not nacjd_doc.publisher_.empty()) {
            publishing_260_info.emplace_back(MARC::Subfield('b', nacjd_doc.publisher_));
        }
        if (not nacjd_doc.year_pub_.empty()) {
            publishing_info.emplace_back(MARC::Subfield('g', "year:" + nacjd_doc.year_pub_));
            publishing_260_info.emplace_back(MARC::Subfield('c', nacjd_doc.year_pub_));
        }
        if (not nacjd_doc.volume_.empty()) {
            publishing_info.emplace_back(MARC::Subfield('g', "volume:" + nacjd_doc.volume_));
        }
        if (not nacjd_doc.i_number_.empty()) {
            publishing_info.emplace_back(MARC::Subfield('g', "number:" + nacjd_doc.i_number_));
        }
        if (not nacjd_doc.page_start_.empty() && not nacjd_doc.page_end_.empty()) {
            publishing_info.emplace_back(MARC::Subfield('g', "pages:" + nacjd_doc.page_start_ + "-" + nacjd_doc.page_end_));
        }

        // construct url_info
        std::vector<MARC::Subfield> urls;
        if (not nacjd_doc.url_.empty()) {
            urls.emplace_back(MARC::Subfield('a', nacjd_doc.url_));
        }
        if (not nacjd_doc.url_pdf_.empty()) {
            urls.emplace_back(MARC::Subfield('a', nacjd_doc.url_pdf_));
        }
        if (not nacjd_doc.url_abs_.empty()) {
            urls.emplace_back(MARC::Subfield('a', nacjd_doc.url_abs_));
        }

        // create a new record
        MARC::Record new_record(MARC::Record::TypeOfRecord::LANGUAGE_MATERIAL, MARC::Record::BibliographicLevel::MONOGRAPH_OR_ITEM,
                                "ICPSR" + nacjd_doc.ref_id_);


        new_record.insertControlField("007", "cr||||");

        if (not nacjd_doc.issn_.empty()) {
            new_record.insertField("022", { { 'a', nacjd_doc.issn_ } });
        }

        if (not nacjd_doc.doi_.empty()) {
            new_record.insertField("024", { { 'a', nacjd_doc.doi_ }, { '2', "doi" } }, '7');
            new_record.insertField("856", { { 'u', "https://doi.org/" + nacjd_doc.doi_ }, { 'x', "R" }, { 'z', "LF" } },
                                   /*indicator1 = */ '4',
                                   /*indicator2 = */ '0');
        }

        if (not publishing_260_info.empty()) {
            new_record.insertField("260", publishing_260_info, ' ', ' ');
        }

        new_record.insertField("041", { { 'a', "eng" } });
        new_record.insertField("084", { { 'a', "2,1" }, { '2', "ssgn" } });
        if (not authors.empty()) {
            new_record.insertField("100", authors, '1');
        }
        if (not nacjd_doc.title_.empty()) {
            new_record.insertField("245", { { 'a', nacjd_doc.title_ } }, '1', '0');
        } else if (not nacjd_doc.sec_title_.empty()) {
            new_record.insertField("245", { { 'a', nacjd_doc.sec_title_ } }, '1', '0');
        }

        if (not nacjd_doc.year_pub_.empty()) {
            new_record.insertField(
                "264",
                { { 'a', "[Erscheinungsort nicht ermittelbar]" }, { 'b', "[Verlag nicht ermittelbar]" }, { 'c', nacjd_doc.year_pub_ } },
                /*indicator1=*/' ', /*indicator2=*/'1');
        } else {
            new_record.insertField("264", { { 'a', "[Erscheinungsort nicht ermittelbar]" }, { 'b', "[Verlag nicht ermittelbar]" } },
                                   /*indicator1=*/' ', /*indicator2=*/'1');
        }


        new_record.insertField("655",
                               { { 'a', "Forschungsdaten" },
                                 { '0', "(DE-588)1098579690" },
                                 { '0', "(DE-627)857755366" },
                                 { '0', "(DE-576)469182156" },
                                 { '2', "gnd-content" } },
                               ' ', '7');

        if (not publishing_info.empty()) {
            new_record.insertField("773", publishing_info, '1', '8');
        }

        if (not urls.empty()) {
            new_record.insertField("856", urls, '4', ' ');
        }

        // Disable Match & Merge
        new_record.insertField("912", { { 'a', "NOMM" } });

        new_record.insertField("935", { { 'a', "mkri" } });
        new_record.insertField("935", { { 'a', "nacj" }, { '2', "LOK" } });
        new_record.insertField("935", { { 'a', "foda" }, { '2', "LOK" } });

        marc_writer->write(new_record);
    }
}

void ExtractInfoFromNACJD(const std::string &json_path, std::vector<NacjdDoc> * const nacjd_docs) {
    std::ifstream f(json_path, std::ifstream::in);
    json jdat;
    f >> jdat;


    if (jdat.at("searchResults").at("response").at("docs").is_structured()) {
        for (const auto &doc : jdat.at("searchResults").at("response").at("docs")) {
            NacjdDoc nacjd_doc;

            if (doc.contains("REF_ID")) {
                nacjd_doc.ref_id_ = std::to_string(doc["REF_ID"].template get<int>());
            }
            if (doc.contains("TITLE")) {
                nacjd_doc.title_ = doc["TITLE"].template get<std::string>();
            }
            if (doc.contains("ISSN")) {
                nacjd_doc.issn_ = doc["ISSN"].template get<std::string>();
            }
            if (doc.contains("PUBLISHER")) {
                nacjd_doc.publisher_ = doc["PUBLISHER"].template get<std::string>();
            }
            if (doc.contains("TYPE_WORK")) {
                nacjd_doc.type_work_ = doc["TYPE_WORK"].template get<std::string>();
            }
            if (doc.contains("VOLUME")) {
                nacjd_doc.volume_ = doc["VOLUME"].template get<std::string>();
            }
            if (doc.contains("JOURNAL")) {
                nacjd_doc.journal_ = doc["JOURNAL"].template get<std::string>();
            }
            if (doc.contains("INUMBER")) {
                nacjd_doc.i_number_ = doc["INUMBER"].template get<std::string>();
            }
            if (doc.contains("YEAR_PUB")) {
                nacjd_doc.year_pub_ = doc["YEAR_PUB"].template get<std::string>();
            }
            if (doc.contains("PLACE_PUB")) {
                nacjd_doc.place_pub_ = doc["PLACE_PUB"].template get<std::string>();
            }
            if (doc.contains("SEC_TITLE")) {
                nacjd_doc.sec_title_ = doc["SEC_TITLE"].template get<std::string>();
            }
            if (doc.contains("RIS_TYPE")) {
                nacjd_doc.ris_type_ = doc["RIS_TYPE"].template get<std::string>();
            }
            if (doc.contains("PAGE_START")) {
                nacjd_doc.page_start_ = doc["PAGE_START"].template get<std::string>();
            }
            if (doc.contains("PAGE_END")) {
                nacjd_doc.page_end_ = doc["PAGE_END"].template get<std::string>();
            }
            if (doc.contains("DOI")) {
                nacjd_doc.doi_ = doc["DOI"].template get<std::string>();
            }
            if (doc.contains("URL")) {
                nacjd_doc.url_ = doc["URL"].template get<std::string>();
            }
            if (doc.contains("URL_PDF")) {
                nacjd_doc.url_pdf_ = doc["URL_PDF"].template get<std::string>();
            }
            if (doc.contains("URL_ABS")) {
                nacjd_doc.url_abs_ = doc["URL_ABS"].template get<std::string>();
            }
            if (doc.contains("PUBLISHER")) {
                nacjd_doc.publisher_ = doc["PUBLISHER"].template get<std::string>();
            }
            if (doc.contains("AUTHORS_SPLIT")) {
                nacjd_doc.author_split_ = doc["AUTHORS_SPLIT"].template get<std::vector<std::string>>();
            }
            if (doc.contains("STUDYTITLE")) {
                nacjd_doc.study_title_ = doc["STUDYTITLE"].template get<std::vector<std::string>>();
            }


            nacjd_docs->emplace_back(nacjd_doc);
        }
    }
}

} // unnamed namespace

int Main(int argc, char **argv) {
    if (argc < 2)
        Usage();

    // const std::string mode(argv[1]);

    std::vector<NacjdDoc> nacjd_docs;
    ExtractInfoFromNACJD(argv[1], &nacjd_docs);
    WriteMarc(argv[2], nacjd_docs);

    return EXIT_SUCCESS;
}
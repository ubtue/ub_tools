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

    std::vector<std::string> authors_split, // AUTHORS_SPLIT
        study_titles_;                      // STUDYTITLE
};

MARC::Record WriteAnArticleContent(const NacjdDoc &nacjd_doc) {
    // construct author_info
    std::vector<MARC::Subfield> authors;
    if (not nacjd_doc.authors_split.empty()) {
        for (auto author_ : nacjd_doc.authors_split) {
            authors.emplace_back(MARC::Subfield('a', author_));
        }
    }
    // construct publishing information
    std::vector<MARC::Subfield> publishing_info_773, publishing_info_936;
    std::string info_773_g(nacjd_doc.volume_ + " (" + nacjd_doc.year_pub_ + "), " + nacjd_doc.i_number_ + ", " + nacjd_doc.page_start_ + "-"
                           + nacjd_doc.page_end_);

    publishing_info_773.emplace_back(MARC::Subfield('i', "In:"));
    publishing_info_773.emplace_back(MARC::Subfield('w', "(DE-267)" + nacjd_doc.ref_id_));
    publishing_info_773.emplace_back(MARC::Subfield('g', info_773_g));

    if (not nacjd_doc.journal_.empty())
        publishing_info_773.emplace_back(MARC::Subfield('t', nacjd_doc.journal_));

    if (not nacjd_doc.issn_.empty())
        publishing_info_773.emplace_back(MARC::Subfield('x', nacjd_doc.issn_));


    if (not nacjd_doc.volume_.empty())
        publishing_info_936.emplace_back(MARC::Subfield('d', nacjd_doc.volume_));

    if (not nacjd_doc.i_number_.empty())
        publishing_info_936.emplace_back(MARC::Subfield('e', nacjd_doc.i_number_));

    if (not nacjd_doc.page_start_.empty())
        publishing_info_936.emplace_back(MARC::Subfield('h', nacjd_doc.page_start_ + "-" + nacjd_doc.page_end_));

    if (not nacjd_doc.year_pub_.empty())
        publishing_info_936.emplace_back(MARC::Subfield('j', nacjd_doc.year_pub_));


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
            "264", { { 'a', "[Erscheinungsort nicht ermittelbar]" }, { 'b', "[Verlag nicht ermittelbar]" }, { 'c', nacjd_doc.year_pub_ } },
            /*indicator1=*/' ', /*indicator2=*/'1');
    }


    new_record.insertField("655",
                           { { 'a', "Forschungsdaten" },
                             { '0', "(DE-588)1098579690" },
                             { '0', "(DE-627)857755366" },
                             { '0', "(DE-576)469182156" },
                             { '2', "gnd-content" } },
                           ' ', '7');

    if (not publishing_info_773.empty()) {
        new_record.insertField("773", publishing_info_773, '0', '8');
    }

    if (not urls.empty()) {
        new_record.insertField("856", urls, '4', ' ');
    }

    // Disable Match & Merge
    new_record.insertField("912", { { 'a', "NOMM" } });

    new_record.insertField("935", { { 'a', "mkri" } });
    new_record.insertField("935", { { 'a', "nacj" }, { '2', "LOK" } });
    new_record.insertField("935", { { 'a', "foda" }, { '2', "LOK" } });

    if (not publishing_info_936.empty()) {
        new_record.insertField("936", publishing_info_936, 'u', 'w');
    }

    return new_record;
}

void WriteMarc(const std::string &marc_path, const std::vector<NacjdDoc> &nacjd_docs) {
    auto marc_file(MARC::Writer::Factory(marc_path));
    MARC::Writer * const marc_writer(marc_file.get());

    for (auto nacjd_doc : nacjd_docs) {
        if (nacjd_doc.ref_id_.empty())
            LOG_ERROR("No ID found on Title: " + nacjd_doc.title_ + ", Sec Title: " + nacjd_doc.sec_title_);

        // Journal / Article
        if (nacjd_doc.ris_type_.compare("JOUR") == 0) {
            marc_writer->write(WriteAnArticleContent(nacjd_doc));
        }
    }
}

void ExtractInfoFromNACJD(const std::string &json_path, std::vector<NacjdDoc> * const nacjd_docs) {
    std::ifstream f(json_path, std::ifstream::in);
    nlohmann::json jdat = nlohmann::json::parse(f);
    const auto docs(jdat.at("searchResults").at("response").at("docs"));

    if (not docs.is_structured())
        LOG_ERROR("docs is empty");

    for (const auto &doc : jdat.at("searchResults").at("response").at("docs")) {
        NacjdDoc nacjd_doc;

        if (doc.contains("REF_ID")) {
            nacjd_doc.ref_id_ = std::to_string(doc.at("REF_ID").get<int>());
        }
        if (doc.contains("TITLE")) {
            nacjd_doc.title_ = doc.at("TITLE").get<std::string>();
        }
        if (doc.contains("ISSN")) {
            nacjd_doc.issn_ = doc.at("ISSN").get<std::string>();
        }
        if (doc.contains("PUBLISHER")) {
            nacjd_doc.publisher_ = doc.at("PUBLISHER").get<std::string>();
        }
        if (doc.contains("TYPE_WORK")) {
            nacjd_doc.type_work_ = doc.at("TYPE_WORK").get<std::string>();
        }
        if (doc.contains("VOLUME")) {
            nacjd_doc.volume_ = doc.at("VOLUME").get<std::string>();
        }
        if (doc.contains("JOURNAL")) {
            nacjd_doc.journal_ = doc.at("JOURNAL").get<std::string>();
        }
        if (doc.contains("INUMBER")) {
            nacjd_doc.i_number_ = doc.at("INUMBER").get<std::string>();
        }
        if (doc.contains("YEAR_PUB")) {
            nacjd_doc.year_pub_ = doc.at("YEAR_PUB").get<std::string>();
        }
        if (doc.contains("PLACE_PUB")) {
            nacjd_doc.place_pub_ = doc.at("PLACE_PUB").get<std::string>();
        }
        if (doc.contains("SEC_TITLE")) {
            nacjd_doc.sec_title_ = doc.at("SEC_TITLE").get<std::string>();
        }
        if (doc.contains("RIS_TYPE")) {
            nacjd_doc.ris_type_ = doc.at("RIS_TYPE").get<std::string>();
        }
        if (doc.contains("PAGE_START")) {
            nacjd_doc.page_start_ = doc.at("PAGE_START").get<std::string>();
        }
        if (doc.contains("PAGE_END")) {
            nacjd_doc.page_end_ = doc.at("PAGE_END").get<std::string>();
        }
        if (doc.contains("DOI")) {
            nacjd_doc.doi_ = doc.at("DOI").get<std::string>();
        }
        if (doc.contains("URL")) {
            nacjd_doc.url_ = doc.at("URL").get<std::string>();
        }
        if (doc.contains("URL_PDF")) {
            nacjd_doc.url_pdf_ = doc.at("URL_PDF").get<std::string>();
        }
        if (doc.contains("URL_ABS")) {
            nacjd_doc.url_abs_ = doc.at("URL_ABS").get<std::string>();
        }
        if (doc.contains("PUBLISHER")) {
            nacjd_doc.publisher_ = doc.at("PUBLISHER").get<std::string>();
        }
        if (doc.contains("authors_splitSPLIT")) {
            nacjd_doc.authors_split = doc.at("authors_splitSPLIT").get<std::vector<std::string>>();
        }
        if (doc.contains("STUDYTITLE")) {
            nacjd_doc.study_titles_ = doc.at("STUDYTITLE").get<std::vector<std::string>>();
        }


        nacjd_docs->emplace_back(nacjd_doc);
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
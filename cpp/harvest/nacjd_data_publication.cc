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
        "input_file source_file output_file\n"
        "\t- input_file: source of data in JSON format (taken from NACJD website).\n"
        "\t- source_file: source data needed for augmenting.\n"
        "\t- output_file: will contain all icpsr records as MARC21.\n"
        "\n");
}

std::string ConstructPPN(std::string const issn, std::map<std::string, std::string> const &issn_cache) {
    if (issn_cache.find(issn) != issn_cache.end()) {
        return "(DE-627)" + issn_cache.find(issn)->second;
    }
    return "";
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

    // using set to avoid redundancy
    std::set<std::string> authors_split, // AUTHORS_SPLIT
        study_titles_;                   // STUDYTITLE

    const std::vector<MARC::Subfield> ConstructPublishingInfo_773(std::map<std::string, std::string> const &issn_cache) {
        std::vector<MARC::Subfield> publishing_info;
        publishing_info.emplace_back(MARC::Subfield('i', "In:"));

        if (!volume_.empty() && !year_pub_.empty() && !i_number_.empty() && !page_start_.empty())
            if (not page_end_.empty()) {
                if ((page_end_.compare("-") == 0) && (page_end_.compare("unknown") == 0)) {
                    publishing_info.emplace_back(MARC::Subfield('g', volume_ + " (" + year_pub_ + "), " + i_number_ + ", " + page_start_));
                } else {
                    publishing_info.emplace_back(
                        MARC::Subfield('g', volume_ + " (" + year_pub_ + "), " + i_number_ + ", " + page_start_ + "-" + page_end_));
                }
            } else {
                publishing_info.emplace_back(MARC::Subfield('g', volume_ + " (" + year_pub_ + "), " + i_number_ + ", " + page_start_));
            }

        if (not journal_.empty())
            publishing_info.emplace_back(MARC::Subfield('t', journal_));

        if (not issn_.empty()) {
            std::string ppn(ConstructPPN(issn_, issn_cache));
            if (not ppn.empty()) {
                publishing_info.emplace_back(MARC::Subfield('w', ppn));
            }
            publishing_info.emplace_back(MARC::Subfield('x', issn_));
        }

        return publishing_info;
    }

    const std::vector<MARC::Subfield> ConstructPublishingInfo_936() {
        std::vector<MARC::Subfield> publishing_info;
        if (not volume_.empty())
            publishing_info.emplace_back(MARC::Subfield('d', volume_));

        if (not i_number_.empty())
            publishing_info.emplace_back(MARC::Subfield('e', i_number_));

        if (not page_start_.empty()) {
            if (not page_end_.empty()) {
                if (page_end_.compare("-") == 0) {
                    publishing_info.emplace_back(MARC::Subfield('h', page_start_));
                } else {
                    publishing_info.emplace_back(MARC::Subfield('h', page_start_ + "-" + page_end_));
                }
            } else {
                publishing_info.emplace_back(MARC::Subfield('h', page_start_));
            }
        }

        if (not year_pub_.empty())
            publishing_info.emplace_back(MARC::Subfield('j', year_pub_));


        return publishing_info;
    }

    void ConvertUrl(MARC::Record * const record) {
        if (not url_.empty()) {
            record->insertField("856", { { 'a', url_ } }, '4', ' ');
        }
        if (not url_pdf_.empty()) {
            record->insertField("856", { { 'a', url_pdf_ } }, '4', ' ');
        }
        if (not url_abs_.empty()) {
            record->insertField("856", { { 'a', url_abs_ } }, '4', ' ');
        }
    }

    std::vector<MARC::Subfield> ConstructAuthors() {
        std::vector<MARC::Subfield> authors;
        if (not authors_split.empty()) {
            for (const auto &author_ : authors_split) {
                authors.emplace_back(MARC::Subfield('a', author_));
            }
        }

        return authors;
    }

    void ConvertDOI(MARC::Record * const record) {
        if (not doi_.empty()) {
            record->insertField("024", { { 'a', doi_ }, { '2', "doi" } }, '7');
            record->insertField("856", { { 'u', "https://doi.org/" + doi_ }, { 'x', "R" }, { 'z', "LF" } },
                                /*indicator1 = */ '4',
                                /*indicator2 = */ '0');
        }
    }

    void ConvertISSN(MARC::Record * const record) {
        if (not issn_.empty()) {
            record->insertField("022", { { 'a', issn_ } });
        }
    }

    void ConvertTitle(MARC::Record * const record) {
        if (not title_.empty()) {
            record->insertField("245", { { 'a', title_ } }, '1', '0');
        } else if (not sec_title_.empty()) {
            record->insertField("245", { { 'a', sec_title_ } }, '1', '0');
        }
    }

    void ConvertYear(MARC::Record * const record) {
        if (not year_pub_.empty()) {
            record->insertField("264", 'c', year_pub_, ' ', '1');
        }
    }
};

MARC::Record WriteABookContent(NacjdDoc * const nacjd_doc) {
    MARC::Record new_record(MARC::Record::TypeOfRecord::LANGUAGE_MATERIAL, MARC::Record::BibliographicLevel::MONOGRAPH_OR_ITEM,
                            "ICPSR" + nacjd_doc->ref_id_);

    new_record.insertControlField("007", "cr||||");
    new_record.insertField("084", { { 'a', "2,1" }, { '2', "ssgn" } });
    new_record.insertField("591", 'a', "Metadaten maschinell erstellt (TUKRIM)");
    new_record.insertField("912", 'a', "NOMM");

    nacjd_doc->ConvertTitle(&new_record);
    nacjd_doc->ConvertDOI(&new_record);
    nacjd_doc->ConvertUrl(&new_record);
    nacjd_doc->ConvertYear(&new_record);

    return new_record;
}
MARC::Record WriteAJournalContent(NacjdDoc * const nacjd_doc, std::map<std::string, std::string> const &issn_cache) {
    // create a new record

    MARC::Record new_record("00000naa a22000002  4500");

    new_record.insertControlField("001", "ICPSR" + nacjd_doc->ref_id_);
    new_record.insertControlField("007", "cr||||");

    // nacjd_doc->ConvertISSN(&new_record);
    nacjd_doc->ConvertDOI(&new_record);

    new_record.insertField("084", { { 'a', "2,1" }, { '2', "ssgn" } });

    if (not nacjd_doc->ConstructAuthors().empty()) {
        new_record.insertField("100", nacjd_doc->ConstructAuthors(), '1');
    }

    nacjd_doc->ConvertTitle(&new_record);
    nacjd_doc->ConvertYear(&new_record);


    new_record.insertField("655",
                           { { 'a', "Forschungsdaten" },
                             { '0', "(DE-588)1098579690" },
                             { '0', "(DE-627)857755366" },
                             { '0', "(DE-576)469182156" },
                             { '2', "gnd-content" } },
                           ' ', '7');
    std::vector<MARC::Subfield> publishing_info(nacjd_doc->ConstructPublishingInfo_773(issn_cache));
    if (not publishing_info.empty()) {
        new_record.insertField("773", publishing_info, '0', '8');
    }
    nacjd_doc->ConvertUrl(&new_record);

    // Disable Match & Merge
    new_record.insertField("912", { { 'a', "NOMM" } });

    new_record.insertField("935", { { 'a', "mkri" } });
    new_record.insertField("935", { { 'a', "nacj" }, { '2', "LOK" } });
    new_record.insertField("935", { { 'a', "foda" }, { '2', "LOK" } });

    if (not nacjd_doc->ConstructPublishingInfo_936().empty()) {
        new_record.insertField("936", nacjd_doc->ConstructPublishingInfo_936(), 'u', 'w');
    }
    return new_record;
}

void WriteMarc(const std::string &marc_path, const std::vector<NacjdDoc> &nacjd_docs,
               std::map<std::string, std::string> const &issn_cache) {
    auto marc_file(MARC::Writer::Factory(marc_path));
    MARC::Writer * const marc_writer(marc_file.get());

    for (auto nacjd_doc : nacjd_docs) {
        if (nacjd_doc.ref_id_.empty())
            LOG_ERROR("No ID found on Title: " + nacjd_doc.title_ + ", Sec Title: " + nacjd_doc.sec_title_);

        // please see the list of RIS type here: https://en.wikipedia.org/wiki/RIS_(file_format)#Type_of_reference
        // Audiovisual
        if (nacjd_doc.ris_type_.compare("ADVS") == 0) {
        }
        // Book
        if (nacjd_doc.ris_type_.compare("BOOK") == 0) {
            marc_writer->write(WriteABookContent(&nacjd_doc));
        }
        // Book section / chapter
        if (nacjd_doc.ris_type_.compare("CHAP") == 0) {
        }
        // Conference Proceedings
        if (nacjd_doc.ris_type_.compare("CONF") == 0) {
        }
        // Web page / electronic citation
        if (nacjd_doc.ris_type_.compare("ELEC") == 0) {
        }
        // Generic
        if (nacjd_doc.ris_type_.compare("GEN") == 0) {
            // marc_writer->write(WriteAnGenericContent(nacjd_doc));
        }
        // Journal / Article
        if (nacjd_doc.ris_type_.compare("JOUR") == 0) {
            marc_writer->write(WriteAJournalContent(&nacjd_doc, issn_cache));
        }
        // Magazine
        if (nacjd_doc.ris_type_.compare("MGZN") == 0) {
        }
        // Newspaper
        if (nacjd_doc.ris_type_.compare("NEWS") == 0) {
        }
        // Report
        if (nacjd_doc.ris_type_.compare("RPRT") == 0) {
        }
        // Thesis / Dissertation
        if (nacjd_doc.ris_type_.compare("THES") == 0) {
        }
    }
}

void ExtractInfoFromNACJD(const std::string &json_path, std::vector<NacjdDoc> * const nacjd_docs) {
    std::ifstream f(json_path, std::ifstream::in);
    nlohmann::json jdat(nlohmann::json::parse(f));
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
            nacjd_doc.authors_split = doc.at("authors_splitSPLIT").get<std::set<std::string>>();
        }
        if (doc.contains("STUDYTITLE")) {
            nacjd_doc.study_titles_ = doc.at("STUDYTITLE").get<std::set<std::string>>();
        }


        nacjd_docs->emplace_back(nacjd_doc);
    }
}

void BuildISSNCache(std::map<std::string, std::string> * const issn_cache, const std::string &source_file_name) {
    auto input_file(MARC::Reader::Factory(source_file_name));

    while (MARC::Record record = input_file->read()) {
        std::set<std::string> issns(record.getISSNs());

        for (auto const &issn : issns) {
            issn_cache->insert({ StringUtil::ASCIIToUpper(issn), record.getControlNumber() });
        }
    }
}

} // unnamed namespace

int Main(int argc, char **argv) {
    if (argc < 3)
        Usage();


    std::vector<NacjdDoc> nacjd_docs;
    std::map<std::string, std::string> issn_cache;

    ExtractInfoFromNACJD(argv[1], &nacjd_docs);
    BuildISSNCache(&issn_cache, argv[2]);
    WriteMarc(argv[3], nacjd_docs, issn_cache);


    return EXIT_SUCCESS;
}
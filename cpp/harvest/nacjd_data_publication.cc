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
#include "UrlUtil.h"


namespace {

[[noreturn]] void Usage() {
    ::Usage(
        "[--verbose] input_file source_file output_file \n"
        "--verbose, print to standard output the summary.\n"
        "\t- input_file: source of data in JSON format (taken from NACJD website).\n"
        "\t- source_file: source data needed for augmenting (taken from K10Plus).\n"
        "\t- output_file: will contain all icpsr records as MARC21.\n"
        "\n");
}


struct DebugInfo {
    std::set<std::string> superior_work_not_found, // issn
        unknown_type;
    std::map<std::string, std::string> superior_work_found; // ppn -> issn
    unsigned long counter_advs = 0, counter_book = 0, counter_chap = 0, counter_conf = 0, counter_elec = 0, counter_generic = 0,
                  counter_jour = 0, counter_mgzn = 0, counter_news = 0, counter_rprt = 0, counter_thes = 0, counter_unknown = 0,
                  counter_journal_without_issn = 0, counter_doi_open_access = 0, counter_doi_close_access = 0, counter_doi_without_issn = 0,
                  counter_doi_issn_without_access_info = 0, data_found_in_k10_plus = 0, data_not_found_in_k10_plus = 0;
    DebugInfo() = default;
    unsigned long counter_total() const {
        return (counter_advs + counter_book + counter_chap + counter_conf + counter_elec + counter_generic + counter_jour + counter_mgzn
                + counter_news + counter_rprt + counter_thes + counter_unknown);
    };
};

std::string URLBJSResolver(const std::string &ori_url) {
    /**
     * Mapping
     * http(s)://bjs.ojp.usdoj.gov/ -> https://bjs.ojp.gov/redirect-legacy/
     * http(s)://bjs.gov/ ->  https://bjs.ojp.gov/redirect-legacy/
     * http(s)://www.bjs.gov/ -> https://bjs.ojp.gov/redirect-legacy/
     * http(s)://www.bjs.gov/ -> https://bjs.ojp.gov/redirect-legacy/
     */

    const std::string new_url_address("https://bjs.ojp.gov/redirect-legacy");
    std::string scheme, username_password, authority, port, path, params, query, fragment, relative_url;
    static ThreadSafeRegexMatcher legacy_bjs_authority_matcher("(www.)?bjs(.ojp.usdoj)?.gov");


    UrlUtil::ParseUrl(ori_url, &scheme, &username_password, &authority, &port, &path, &params, &query, &fragment, &relative_url);
    const std::string uri_full(path + "?" + query);

    if (legacy_bjs_authority_matcher.match(authority))
        return (new_url_address + uri_full);


    return ori_url;
}

// This will act as an adapter for the URL resolver function.
std::string URLResolver(const std::string &ori_url) {
    const std::string bjs_resolver(URLBJSResolver(ori_url));
    if (ori_url.compare(bjs_resolver) != 0)
        return bjs_resolver;

    return ori_url;
}
struct K10PlusInfo {
    std::string ppn_;
    bool is_open_access_;
    K10PlusInfo() = default;
    K10PlusInfo(const std::string &ppn, const bool &is_open_access) {
        ppn_ = ppn;
        is_open_access_ = is_open_access;
    }
};

std::string GetLicenceFlag(const std::string &issn, DebugInfo * const debug_info, const std::map<std::string, K10PlusInfo> &k10_plus_info) {
    if (k10_plus_info.find(issn) != k10_plus_info.end() && k10_plus_info.find(issn)->second.is_open_access_) {
        debug_info->counter_doi_open_access++;
        return "LF";
    }

    debug_info->counter_doi_close_access++;
    // Set the value to ZZ when the criteria is not met
    return "ZZ";
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
        url_,            // resolved url_old
        url_old_,        // URL
        url_pdf_,        // resolved url_pdf_old
        url_pdf_old_,    // URL_PDF
        url_abs_,        // resolved url_abs_old
        url_abs_old_,    // URL_ABS
        type_work_,      // TYPE_WORK
        publisher_;      // PUBLISHER

    // using set to avoid redundancy
    std::set<std::string> authors_split, // AUTHORS_SPLIT
        study_titles_;                   // STUDYTITLE

    const std::vector<MARC::Subfield> ConstructPublishingInfo_773(const std::map<std::string, K10PlusInfo> &k10_plus_info,
                                                                  DebugInfo * const debug_info) {
        std::vector<MARC::Subfield> publishing_info;
        publishing_info.emplace_back(MARC::Subfield('i', "In:"));

        if (!volume_.empty() && !year_pub_.empty() && !i_number_.empty() && !page_start_.empty())
            if (not page_end_.empty()) {
                if ((page_end_.compare("-") == 0) && (page_end_.compare("unknown") == 0)) {
                    publishing_info.emplace_back(
                        MARC::Subfield('g', volume_ + " (" + year_pub_ + "), " + i_number_ + ", Seite " + page_start_));
                } else {
                    publishing_info.emplace_back(
                        MARC::Subfield('g', volume_ + " (" + year_pub_ + "), " + i_number_ + ", Seite " + page_start_ + "-" + page_end_));
                }
            } else {
                publishing_info.emplace_back(
                    MARC::Subfield('g', volume_ + " (" + year_pub_ + "), " + i_number_ + ", Seite " + page_start_));
            }

        if (not journal_.empty())
            publishing_info.emplace_back(MARC::Subfield('t', journal_));

        if (not issn_.empty()) {
            const auto is_exist_in_k10_plus = k10_plus_info.find(issn_);
            if (is_exist_in_k10_plus != k10_plus_info.end()) {
                publishing_info.emplace_back(MARC::Subfield('w', "(DE-627)" + is_exist_in_k10_plus->second.ppn_));
                debug_info->superior_work_found.insert({ is_exist_in_k10_plus->second.ppn_, issn_ });
                debug_info->data_found_in_k10_plus++;
            } else {
                debug_info->data_not_found_in_k10_plus++;
                debug_info->superior_work_not_found.insert(issn_);
            }

            publishing_info.emplace_back(MARC::Subfield('x', issn_));
        } else {
            debug_info->counter_journal_without_issn++;
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

    void ConvertUrl(MARC::Record * const record, DebugInfo * const debug_info,
                    const std::map<std::string, K10PlusInfo> k10_plus_info = {}) {
        if (not url_.empty()) {
            record->insertField("856", { { 'u', url_ } }, '4', '0');
            if (url_.compare(url_old_) != 0) {
                record->insertField("887", { { 'a', "Invalid original URL from the original site: " + url_old_ } }, ' ', ' ');
            }
        }
        if (not url_pdf_.empty()) {
            record->insertField("856", { { 'u', url_pdf_ }, { 'q', "application/pdf" }, { '3', "Volltext" } }, '4', '0');
            if (url_pdf_.compare(url_pdf_old_) != 0) {
                record->insertField("887", { { 'a', "Invalid original URL from the original site: " + url_pdf_old_ } }, ' ', ' ');
            }
        }
        if (not url_abs_.empty()) {
            record->insertField("856", { { 'u', url_abs_ }, { 'x', "Abstracts" } }, '4', '0');
            if (url_abs_.compare(url_abs_old_) != 0) {
                record->insertField("887", { { 'a', "Invalid original URL from the original site: " + url_abs_old_ } }, ' ', ' ');
            }
        }
        if (not doi_.empty()) {
            record->insertField("024", { { 'a', doi_ }, { '2', "doi" } }, '7');

            if (not issn_.empty()) {
                const auto is_exist_in_k10_plus(k10_plus_info.find(issn_));
                if (is_exist_in_k10_plus != k10_plus_info.end()) {
                    record->insertField("856",
                                        { { 'u', "https://doi.org/" + doi_ },
                                          { 'x', "Resolving-System" },
                                          { 'z', GetLicenceFlag(issn_, debug_info, k10_plus_info) },
                                          { '3', "Volltext" } },
                                        '4', '0');
                } else {
                    record->insertField("856", { { 'u', "https://doi.org/" + doi_ }, { 'x', "Resolving-System" } }, '4', '0');
                    debug_info->counter_doi_issn_without_access_info++;
                }
            } else {
                record->insertField("856", { { 'u', "https://doi.org/" + doi_ }, { 'x', "Resolving-System" } }, '4', '0');
                debug_info->counter_doi_without_issn++;
            }
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

MARC::Record WriteABookContent(NacjdDoc * const nacjd_doc, DebugInfo * const debug_info) {
    MARC::Record new_record(MARC::Record::TypeOfRecord::LANGUAGE_MATERIAL, MARC::Record::BibliographicLevel::MONOGRAPH_OR_ITEM,
                            "ICPSR" + nacjd_doc->ref_id_);

    new_record.insertControlField("007", "cr||||");
    new_record.insertField("084", { { 'a', "2,1" }, { '2', "ssgn" } });
    new_record.insertField("591", 'a', "Metadaten maschinell erstellt (TUKRIM)");
    new_record.insertField("912", 'a', "NOMM");

    nacjd_doc->ConvertTitle(&new_record);
    nacjd_doc->ConvertUrl(&new_record, debug_info);
    nacjd_doc->ConvertYear(&new_record);

    return new_record;
}


MARC::Record WriteAJournalContent(NacjdDoc * const nacjd_doc, std::map<std::string, K10PlusInfo> const &k10_plus_info,
                                  DebugInfo * const debug_info) {
    // create a new record

    MARC::Record new_record("00000naa a22000002  4500");

    new_record.insertControlField("001", "ICPSR" + nacjd_doc->ref_id_);
    new_record.insertControlField("007", "cr||||");


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
    std::vector<MARC::Subfield> publishing_info(nacjd_doc->ConstructPublishingInfo_773(k10_plus_info, debug_info));
    if (not publishing_info.empty()) {
        new_record.insertField("773", publishing_info, '0', '8');
    }

    nacjd_doc->ConvertUrl(&new_record, debug_info, k10_plus_info);

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
               const std::map<std::string, K10PlusInfo> &k10_plus_info, DebugInfo * const debug_info) {
    auto marc_file(MARC::Writer::Factory(marc_path));
    MARC::Writer * const marc_writer(marc_file.get());

    for (auto nacjd_doc : nacjd_docs) {
        if (nacjd_doc.ref_id_.empty())
            LOG_ERROR("No ID found on Title: " + nacjd_doc.title_ + ", Sec Title: " + nacjd_doc.sec_title_);

        // please see the list of RIS type here: https://en.wikipedia.org/wiki/RIS_(file_format)#Type_of_reference
        if (nacjd_doc.ris_type_.compare("ADVS") == 0) {
            // Audiovisual
            debug_info->counter_advs++;
        } else {
            if (nacjd_doc.ris_type_.compare("BOOK") == 0) {
                // Book
                debug_info->counter_book++;
                marc_writer->write(WriteABookContent(&nacjd_doc, debug_info));
            } else if (nacjd_doc.ris_type_.compare("CHAP") == 0) {
                // Book section / chapter
                debug_info->counter_chap++;
            } else if (nacjd_doc.ris_type_.compare("CONF") == 0) {
                // Conference Proceedings
                debug_info->counter_conf++;
            } else if (nacjd_doc.ris_type_.compare("ELEC") == 0) {
                // Web page / electronic citation
                debug_info->counter_elec++;
            } else if (nacjd_doc.ris_type_.compare("GEN") == 0) {
                // Generic
                /**
                 * Generic content by assumption (based on observation of the link given and converted output by Zotero)
                 * is a journal article without ISSN; typically, it is a version of pre-print or author version or draft, etc.
                 * However, for statistic it should encoded as statistic.
                 */
                debug_info->counter_generic++;
            } else if (nacjd_doc.ris_type_.compare("JOUR") == 0) {
                // Journal
                debug_info->counter_jour++;
                marc_writer->write(WriteAJournalContent(&nacjd_doc, k10_plus_info, debug_info));
            } else if (nacjd_doc.ris_type_.compare("MGZN") == 0) {
                // Magazine
                debug_info->counter_mgzn += 1;
            } else if (nacjd_doc.ris_type_.compare("NEWS") == 0) {
                // Newspaper
                debug_info->counter_news++;
            } else if (nacjd_doc.ris_type_.compare("RPRT") == 0) {
                // Report
                debug_info->counter_rprt++;
            } else if (nacjd_doc.ris_type_.compare("THES") == 0) {
                // Thesis / Dissertation
                debug_info->counter_thes++;
            } else {
                debug_info->unknown_type.insert(nacjd_doc.ris_type_);
                debug_info->counter_unknown++;
            }
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
            nacjd_doc.url_old_ = doc.at("URL").get<std::string>();
            nacjd_doc.url_ = URLResolver(nacjd_doc.url_old_);
        }
        if (doc.contains("URL_PDF")) {
            nacjd_doc.url_pdf_old_ = doc.at("URL_PDF").get<std::string>();
            nacjd_doc.url_pdf_ = URLResolver(nacjd_doc.url_pdf_old_);
        }
        if (doc.contains("URL_ABS")) {
            nacjd_doc.url_abs_old_ = doc.at("URL_ABS").get<std::string>();
            nacjd_doc.url_abs_ = URLResolver(nacjd_doc.url_abs_old_);
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


void BuildISSNCache(std::map<std::string, K10PlusInfo> * const issn_to_k10_plus_info, const std::string &source_file_name) {
    auto input_file(MARC::Reader::Factory(source_file_name));

    while (MARC::Record record = input_file->read()) {
        const std::set<std::string> issns(record.getISSNs());
        const K10PlusInfo k10_plus_info(record.getControlNumber(), MARC::IsOpenAccess(record));

        for (auto const &issn : issns) {
            issn_to_k10_plus_info->insert(std::make_pair(StringUtil::ASCIIToUpper(issn), k10_plus_info));
        }
    }
}

void ShowInfoForDebugging(const DebugInfo &debug_info) {
    std::cout << "=== ISSN Found in K10Plus ===" << std::endl;
    for (auto db_found : debug_info.superior_work_found) {
        std::cout << "superior: " << db_found.first << " , ISSN: " << db_found.second << std::endl;
    }
    std::cout << "=== ISSN not found in K10Plus ===" << std::endl;
    for (auto db_not_found : debug_info.superior_work_not_found) {
        std::cout << "- " << db_not_found << std::endl;
    }

    std::cout << "=== Unknown type ===" << std::endl;
    for (auto unknown_type_ : debug_info.unknown_type) {
        std::cout << "- " << unknown_type_ << std::endl;
    }

    std::cout << "=== Summary ===" << std::endl;
    std::cout << "Audio visual: " << debug_info.counter_advs << std::endl;
    std::cout << "Book: " << debug_info.counter_book << std::endl;
    std::cout << "Chapter/ section: " << debug_info.counter_chap << std::endl;
    std::cout << "Conference proceeding: " << debug_info.counter_conf << std::endl;
    std::cout << "Electronic/ web page: " << debug_info.counter_elec << std::endl;
    std::cout << "Generic: " << debug_info.counter_generic << std::endl;
    std::cout << "Journal: " << debug_info.counter_jour << std::endl;
    std::cout << "Magazine: " << debug_info.counter_mgzn << std::endl;
    std::cout << "Newspaper: " << debug_info.counter_news << std::endl;
    std::cout << "Report: " << debug_info.counter_rprt << std::endl;
    std::cout << "Thesis/ Dissertation: " << debug_info.counter_thes << std::endl;
    std::cout << "Unknown: " << debug_info.counter_unknown << std::endl;
    std::cout << "Total: " << debug_info.counter_total() << std::endl << std::endl;

    std::cout << "The number of updated journal  using data from K-10-Plus: " << debug_info.data_found_in_k10_plus << std::endl;
    std::cout << "The number of journal that did not update: " << debug_info.data_not_found_in_k10_plus << std::endl;
    std::cout << "The number of journal without ISSN: " << debug_info.counter_journal_without_issn << std::endl << std::endl;

    std::cout << "The number of doi with open access: " << debug_info.counter_doi_open_access << std::endl;
    std::cout << "The number of doi with close access: " << debug_info.counter_doi_close_access << std::endl;
    std::cout << "The number of doi with issn and no access information: " << debug_info.counter_doi_issn_without_access_info << std::endl;
    std::cout << "The number of doi without issn and no access information: " << debug_info.counter_doi_without_issn << std::endl
              << std::endl;

    std::cout << "ISSN found in K10-Plus (unique): " << debug_info.superior_work_found.size() << std::endl;
    std::cout << "ISSN not found in K10-Plus (unique): " << debug_info.superior_work_not_found.size() << std::endl;
}

} // unnamed namespace

int Main(int argc, char **argv) {
    if (argc < 3)
        Usage();


    std::vector<NacjdDoc> nacjd_docs;
    std::map<std::string, K10PlusInfo> issn_to_k10_plus_info;
    DebugInfo debug_info;
    bool debug_mode(false);


    if (std::strcmp(argv[1], "--verbose") == 0) {
        debug_mode = true;
        --argc, ++argv;
    }

    ExtractInfoFromNACJD(argv[1], &nacjd_docs);
    BuildISSNCache(&issn_to_k10_plus_info, argv[2]);
    WriteMarc(argv[3], nacjd_docs, issn_to_k10_plus_info, &debug_info);

    if (debug_mode) {
        ShowInfoForDebugging(debug_info);
    }


    return EXIT_SUCCESS;
}
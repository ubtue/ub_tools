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
#include "FileUtil.h"
#include "MARC.h"
#include "MiscUtil.h"
#include "StringUtil.h"
#include "UrlUtil.h"

namespace {

[[noreturn]] void Usage() {
    ::Usage(
        "[--verbose] mode mode_params\n"
        "\n"
        "--verbose, print to standard output the summary.\n"
        "\n"
        "convert input_file source_file output_file\n"
        "\t- input_file: source of data in JSON format (taken from NACJD website).\n"
        "\t- source_file: source data needed for augmenting (taken from K10Plus).\n"
        "\t- output_file: will contain all icpsr records as MARC21.\n"
        "\n"
        "augment_open_access input_file source_file output_file\n"
        "\t- input_file: source of data.\n"
        "\t- source_file: source data needed for augmenting (taken from https://api.openalex.org/works).\n"
        "\t- output_file: target file after augmenting with the information of open access."
        "\n");
}


struct DebugInfo {
    std::set<std::string> superior_work_not_found, // issn
        unknown_type;
    std::map<std::string, std::string> superior_work_found; // ppn -> issn
    unsigned long counter_advs = 0, counter_book = 0, counter_chap = 0, counter_conf = 0, counter_elec = 0, counter_generic = 0,
                  counter_jour = 0, counter_mgzn = 0, counter_news = 0, counter_rprt = 0, counter_thes = 0, counter_unknown = 0,
                  counter_data_without_issn = 0, counter_doi_open_access = 0, counter_doi_close_access = 0, counter_doi_without_issn = 0,
                  counter_doi_with_issn = 0, data_found_in_k10_plus = 0, data_not_found_in_k10_plus = 0, k10plus_issn_online,
                  k10plus_issn_printed;
    DebugInfo() = default;
    unsigned long counter_total() const {
        return (counter_advs + counter_book + counter_chap + counter_conf + counter_elec + counter_generic + counter_jour + counter_mgzn
                + counter_news + counter_rprt + counter_thes + counter_unknown);
    };
};

struct AugmentedOpenAccessDebugInfo {
    unsigned updated_lf = 0, updated_zz = 0, total_not_in_openalex = 0;
    std::set<std::string> not_found_in_openalex;
    AugmentedOpenAccessDebugInfo() = default;
};

std::map<std::string, std::string> ConstructAVDSCategory() {
    std::map<std::string, std::string> _935a_category_map;
    _935a_category_map.emplace(StringUtil::ASCIIToUpper("video file"), "vika");
    _935a_category_map.emplace(StringUtil::ASCIIToUpper("videocassette"), "vika");
    _935a_category_map.emplace(StringUtil::ASCIIToUpper("video tape"), "vika");
    _935a_category_map.emplace(StringUtil::ASCIIToUpper("Workshop recording"), "vika");
    _935a_category_map.emplace(StringUtil::ASCIIToUpper("videorecording"), "vika");
    _935a_category_map.emplace(StringUtil::ASCIIToUpper("Videocassette, 1 hour"), "vika");
    _935a_category_map.emplace(StringUtil::ASCIIToUpper("VHS videotape"), "vika");
    _935a_category_map.emplace(StringUtil::ASCIIToUpper("VHS"), "vika");
    _935a_category_map.emplace(StringUtil::ASCIIToUpper("Podcast"), "cofz");
    _935a_category_map.emplace(StringUtil::ASCIIToUpper("webinar"), "medi");
    _935a_category_map.emplace(StringUtil::ASCIIToUpper("webinar videorecording"), "scha");
    _935a_category_map.emplace(StringUtil::ASCIIToUpper("infographic"), "kunstbl");
    _935a_category_map.emplace(StringUtil::ASCIIToUpper("graphic"), "kunstbl");
    _935a_category_map.emplace(StringUtil::ASCIIToUpper("audio podcast"), "scha");
    _935a_category_map.emplace(StringUtil::ASCIIToUpper("audio recording"), "scha");
    _935a_category_map.emplace(StringUtil::ASCIIToUpper("audio recording with transcript"), "scha");
    _935a_category_map.emplace(StringUtil::ASCIIToUpper("presentation slides"), "dias");
    _935a_category_map.emplace(StringUtil::ASCIIToUpper("Powerpoint presentation"), "dias");
    _935a_category_map.emplace(StringUtil::ASCIIToUpper("Slideshow Presentation"), "dias");
    _935a_category_map.emplace(StringUtil::ASCIIToUpper("Webinar slides"), "dias");
    _935a_category_map.emplace(StringUtil::ASCIIToUpper("Slideshow (PDF)"), "dias");

    return _935a_category_map;
}

static std::map<std::string, std::string> avds_category(ConstructAVDSCategory());

bool IsAnonymousOrUnknown(const std::string &author) {
    const std::set<std::string> unknown_author({ "ANONYMOUS", "(AUTHOR UNKNOWN)", "AUTHOR UNKNOWN", "UNKNOWN" });
    const auto search_it(unknown_author.find(StringUtil::ASCIIToUpper(author)));

    return (search_it != unknown_author.end() ? true : false);
}

std::string URLBJSResolver(const std::string &ori_url) {
    /**
     * Mapping
     * http(s)://bjs.ojp.usdoj.gov/ -> https://bjs.ojp.gov/redirect-legacy/
     * http(s)://bjs.gov/ ->  https://bjs.ojp.gov/redirect-legacy/
     * http(s)://www.bjs.gov/ -> https://bjs.ojp.gov/redirect-legacy/
     */

    const std::string new_url_address("https://bjs.ojp.gov/redirect-legacy");
    std::string scheme, username_password, authority, port, path, params, query, fragment, relative_url;
    static ThreadSafeRegexMatcher legacy_bjs_authority_matcher("(www.)?bjs(.ojp.usdoj)?.gov");


    UrlUtil::ParseUrl(ori_url, &scheme, &username_password, &authority, &port, &path, &params, &query, &fragment, &relative_url);

    return (legacy_bjs_authority_matcher.match(authority) ? (new_url_address + path + "?" + query) : ori_url);
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

// std::string GetLicenceFlag(const std::string &issn, DebugInfo * const debug_info, const std::map<std::string, K10PlusInfo>
// &k10_plus_info) {
//     if (k10_plus_info.find(issn) != k10_plus_info.end() && k10_plus_info.find(issn)->second.is_open_access_) {
//         debug_info->counter_doi_open_access++;
//         return "LF";
//     }

//     debug_info->counter_doi_close_access++;
//     // Set the value to ZZ when the criteria is not met
//     return "ZZ";
// }
struct NACJDDoc {
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

    // using vector to keep the order of the authors
    std::vector<std::string> authors_split; // AUTHORS_SPLIT
    // using set to avoid redundancy
    std::set<std::string> study_titles_; // STUDYTITLE

    bool StatisticTypePredictionBaseOnURL() {
        static ThreadSafeRegexMatcher url_matcher("http(s)?://(www.)?bjs.(ojp.)?(usdoj.)?gov");
        if (url_matcher.match(url_) || url_matcher.match(url_pdf_) || url_matcher.match(url_abs_))
            return true;

        return false;
    }

    // an adapter to decide wheather it is a statistic or not
    bool IsDocTypeStatistic() {
        if (StatisticTypePredictionBaseOnURL())
            return true;


        return false;
    }

    const MARC::Subfields ConstructPublishingInfo_773(const std::map<std::string, K10PlusInfo> &k10_plus_info,
                                                      DebugInfo * const debug_info) {
        MARC::Subfields publishing_info;


        if (not volume_.empty() && not year_pub_.empty() && not i_number_.empty() && not page_start_.empty()) {
            std::string field_info(volume_ + " (" + year_pub_ + "), " + i_number_ + ", Seite " + page_start_);

            if (not page_end_.empty() && not(page_end_.compare("-") == 0) && not(page_end_.compare("unknown") == 0))
                field_info.append("-" + page_end_);


            publishing_info.appendSubfield('i', "In:");
            publishing_info.appendSubfield('g', field_info);
        }

        if (not journal_.empty())
            publishing_info.appendSubfield('t', journal_);

        if (not issn_.empty()) {
            const auto is_exist_in_k10_plus = k10_plus_info.find(issn_);
            if (is_exist_in_k10_plus != k10_plus_info.end()) {
                publishing_info.appendSubfield('w', "(DE-627)" + is_exist_in_k10_plus->second.ppn_);
                debug_info->superior_work_found.insert({ issn_, is_exist_in_k10_plus->second.ppn_ });
                debug_info->data_found_in_k10_plus++;
            } else {
                debug_info->data_not_found_in_k10_plus++;
                debug_info->superior_work_not_found.insert(issn_);
            }

            publishing_info.appendSubfield('x', issn_);
        } else {
            debug_info->counter_data_without_issn++;
        }


        return publishing_info;
    }

    const MARC::Subfields ConstructPublishingInfo_936() {
        MARC::Subfields publishing_info;
        if (not volume_.empty())
            publishing_info.appendSubfield('d', volume_);

        if (not i_number_.empty())
            publishing_info.appendSubfield('e', i_number_);

        if (not page_start_.empty()) {
            std::string pages = page_start_;
            if (not page_end_.empty() && not(page_end_.compare("-") == 0) && not(page_end_.compare("unknown") == 0))
                pages.append("-" + page_end_);

            publishing_info.appendSubfield('h', pages);
        }

        if (not year_pub_.empty())
            publishing_info.appendSubfield('j', year_pub_);


        return publishing_info;
    }

    void InsertUrl(MARC::Record * const record, const std::string &original_url, const MARC::Subfields &additional_subfields = {}) {
        const std::string valid_url(URLResolver(original_url));
        MARC::Subfields _856subfields({ { 'u', valid_url } });
        std::move(additional_subfields.begin(), additional_subfields.end(), std::back_inserter(_856subfields));
        record->insertField("856", _856subfields, '4', '0');
        if (valid_url != original_url)
            record->insertField("887", { { 'a', "Invalid original URL from the original site: " + original_url } }, ' ', ' ');
    }

    void ConvertUrl(MARC::Record * const record, DebugInfo * const debug_info) {
        if (not url_.empty())
            InsertUrl(record, url_);

        if (not url_pdf_.empty()) {
            const MARC::Subfields additional_subfields({ { 'q', "application/pdf" }, { '3', "Volltext" } });
            InsertUrl(record, url_pdf_, additional_subfields);
        }

        if (not url_abs_.empty()) {
            const MARC::Subfields additional_subfields({ { 'x', "Abstract" } });
            InsertUrl(record, url_abs_, additional_subfields);
        }

        if (not doi_.empty()) {
            record->insertField("024", { { 'a', doi_ }, { '2', "doi" } }, '7');

            (not issn_.empty() ? debug_info->counter_doi_with_issn++ : debug_info->counter_doi_without_issn++);

            record->insertField("856", { { 'u', "https://doi.org/" + doi_ }, { 'x', "Resolving-System" } }, '4', '0');
        }
    }


    void ConvertISSN(MARC::Record * const record) {
        if (not issn_.empty())
            record->insertField("022", { { 'a', issn_ } });
    }

    void ConvertTitle(MARC::Record * const record) {
        if (not title_.empty())
            record->insertField("245", { { 'a', title_ } }, '1', '0');
        else if (not sec_title_.empty())
            record->insertField("245", { { 'a', sec_title_ } }, '1', '0');
    }

    void ConvertYear(MARC::Record * const record) {
        if (not year_pub_.empty())
            record->insertField("264", 'c', year_pub_, ' ', '1');
    }

    void ConvertPublisher(MARC::Record * const record) {
        if (not publisher_.empty())
            record->insertField("264", 'c', publisher_);
    }

    void ConvertAuthor(MARC::Record * const record) {
        if (authors_split.empty()) {
            record->insertField("500", { { 'a', "Author Unknown" } }, ' ', ' ');
            return;
        }

        if (IsAnonymousOrUnknown(authors_split[0])) {
            record->insertField("500", { { 'a', authors_split[0] } }, ' ', ' ');
            return;
        }

        bool is_first_author(true);
        for (const auto &author_ : authors_split) {
            std::string tag;
            if (is_first_author) {
                tag = MiscUtil::IsCorporateAuthor(author_) ? "110" : "100";
                is_first_author = false;
            } else
                tag = MiscUtil::IsCorporateAuthor(author_) ? "710" : "700";

            record->insertField(tag, { { 'a', author_ }, { 'e', "VerfasserIn" }, { '4', "aut" } }, '1', ' ');
        }
    }
};

MARC::Record *GenerateRecord(const std::string &leader, const std::string &_007_content) {
    MARC::Record *record(new MARC::Record(leader));
    record->insertControlField("007", _007_content);

    return record;
}


void InsertGeneralFieldInfo(MARC::Record * const record, NACJDDoc * const nacjd_doc,
                            std::map<std::string, K10PlusInfo> const &k10_plus_info, DebugInfo * const debug_info) {
    record->insertControlField("001", "ICPSR" + nacjd_doc->ref_id_);
    record->insertField("084", { { 'a', "2,1" }, { '2', "ssgn" } });
    nacjd_doc->ConvertTitle(record);
    nacjd_doc->ConvertAuthor(record);
    nacjd_doc->ConvertPublisher(record);
    nacjd_doc->ConvertYear(record);
    nacjd_doc->ConvertUrl(record, debug_info);

    record->insertField("041", { { 'a', "eng" } });
    record->insertField("591", 'a', "Metadaten maschinell erstellt (TUKRIM)");


    if (nacjd_doc->ris_type_.compare("BOOK") != 0) {
        MARC::Subfields publishing_info(nacjd_doc->ConstructPublishingInfo_773(k10_plus_info, debug_info));
        if (not publishing_info.empty())
            record->insertField("773", publishing_info, '0', '8');
    }


    // Disable Match & Merge
    record->insertField("912", { { 'a', "NOMM" } });


    record->insertField("935", { { 'a', "mkri" } });
    record->insertField("935", { { 'a', "nacj" }, { '2', "LOK" } });
}

MARC::Record *GenerateMarcForStatistic(NACJDDoc * const nacjd_doc, std::map<std::string, K10PlusInfo> const &k10_plus_info,
                                       DebugInfo * const debug_info) {
    MARC::Record *record(GenerateRecord("00000cam a22000000  4500", "tu"));

    InsertGeneralFieldInfo(record, nacjd_doc, k10_plus_info, debug_info);

    record->insertField("655",
                        { { 'a', "Forschungsdaten" },
                          { '0', "(DE-588)1098579690" },
                          { '0', "(DE-627)857755366" },
                          { '0', "(DE-576)469182156" },
                          { '2', "gnd-content" } },
                        ' ', '7');

    record->insertField("935", { { 'a', "foda" }, { '2', "LOK" } });

    return record;
}

MARC::Record *GenerateMarcForNews(NACJDDoc * const nacjd_doc, std::map<std::string, K10PlusInfo> const &k10_plus_info,
                                  DebugInfo * const debug_info) {
    if (nacjd_doc->IsDocTypeStatistic())
        return GenerateMarcForStatistic(nacjd_doc, k10_plus_info, debug_info);

    MARC::Record *record(GenerateRecord("00000cas a2200000   4500", "tu"));
    InsertGeneralFieldInfo(record, nacjd_doc, k10_plus_info, debug_info);


    return record;
}

MARC::Record *GenerateMarcForMagazine(NACJDDoc * const nacjd_doc, std::map<std::string, K10PlusInfo> const &k10_plus_info,
                                      DebugInfo * const debug_info) {
    if (nacjd_doc->IsDocTypeStatistic())
        return GenerateMarcForStatistic(nacjd_doc, k10_plus_info, debug_info);


    // The magazine is categorised as Serial, and the header is taken from Vufind demo for serial
    MARC::Record *record(GenerateRecord("00000nas a2200000   4500", "tu"));
    InsertGeneralFieldInfo(record, nacjd_doc, k10_plus_info, debug_info);


    return record;
}

MARC::Record *GenerateMarcForThesis(NACJDDoc * const nacjd_doc, std::map<std::string, K10PlusInfo> const &k10_plus_info,
                                    DebugInfo * const debug_info) {
    if (nacjd_doc->IsDocTypeStatistic())
        return GenerateMarcForStatistic(nacjd_doc, k10_plus_info, debug_info);


    // Using header from vufind
    MARC::Record *record(GenerateRecord("00000nam a2200000   4500", "tu"));
    InsertGeneralFieldInfo(record, nacjd_doc, k10_plus_info, debug_info);

    return record;
}

MARC::Record *GenerateMarcForConference(NACJDDoc * const nacjd_doc, std::map<std::string, K10PlusInfo> const &k10_plus_info,
                                        DebugInfo * const debug_info) {
    if (nacjd_doc->IsDocTypeStatistic())
        return GenerateMarcForStatistic(nacjd_doc, k10_plus_info, debug_info);


    // The header code for Conference proceeding
    MARC::Record *record(GenerateRecord("00000cam a22000000  4500", "tu"));
    InsertGeneralFieldInfo(record, nacjd_doc, k10_plus_info, debug_info);
    MARC::Subfields _655_subfields;

    _655_subfields.appendSubfield('a', "Konferenzschrift");
    if (not nacjd_doc->year_pub_.empty())
        _655_subfields.appendSubfield('y', nacjd_doc->year_pub_);
    if (not nacjd_doc->place_pub_.empty())
        _655_subfields.appendSubfield('z', nacjd_doc->place_pub_);

    record->insertField("655", _655_subfields, ' ', '7');

    return record;
}

MARC::Record *GenerateMarcForChapter(NACJDDoc * const nacjd_doc, std::map<std::string, K10PlusInfo> const &k10_plus_info,
                                     DebugInfo * const debug_info) {
    if (nacjd_doc->IsDocTypeStatistic())
        return GenerateMarcForStatistic(nacjd_doc, k10_plus_info, debug_info);


    // This header took from vufind live demo
    MARC::Record *record(GenerateRecord("00000naa a22000008i 4500", "tu"));
    InsertGeneralFieldInfo(record, nacjd_doc, k10_plus_info, debug_info);

    return record;
}


MARC::Record *GenerateMarcForAudioVisual(NACJDDoc * const nacjd_doc, std::map<std::string, K10PlusInfo> const &k10_plus_info,
                                         DebugInfo * const debug_info) {
    if (nacjd_doc->IsDocTypeStatistic())
        return GenerateMarcForStatistic(nacjd_doc, k10_plus_info, debug_info);


    const std::set<std::string> video_(
        { "video file", "videocassette", "video tape", "VHS videotape", "Podcast", "podcast", "videorecording", "Videorecording" }),
        audio_({ "audio podcast", "audio recording", "audio recording with transcript" }),
        webinar({ "Webinar", "webinar videorecording", "webinar" }), infographic_({ "infographic", "graphic" }),
        slide_({ "presentation slides", "Powerpoint presentation", "Webinar slides", "Slideshow (PDF)" });

    // 935a type map, please see the mapping on TueFindBiblio.java
    const std::string _935a(avds_category.find(nacjd_doc->type_work_) != avds_category.end() ? avds_category[nacjd_doc->type_work_] : "");
    std::string record_header("00000cgm a22000002  4500");

    if (video_.find(nacjd_doc->type_work_) != video_.end())
        record_header = "00000cgm a22000002  4500";
    else if (audio_.find(nacjd_doc->type_work_) != audio_.end())
        record_header = "00000cim a22000002  4500";
    else if (webinar.find(nacjd_doc->type_work_) != webinar.end())
        record_header = "00000cjm a22000002  4500";
    else if (slide_.find(nacjd_doc->type_work_) != slide_.end())
        record_header = "00000cgm a22000002  4500";
    else if (infographic_.find(nacjd_doc->type_work_) != infographic_.end())
        record_header = "00000ckm a22000002  4500";


    MARC::Record *record(GenerateRecord(record_header, "tu"));
    InsertGeneralFieldInfo(record, nacjd_doc, k10_plus_info, debug_info);

    if (not _935a.empty())
        record->insertField("935", { { 'a', _935a } });
    else if (not nacjd_doc->type_work_.empty())
        LOG_INFO("Need to add this category: " + nacjd_doc->type_work_ + " for ADVS");


    return record;
}

MARC::Record *GenerateMarcForWebsite(NACJDDoc * const nacjd_doc, std::map<std::string, K10PlusInfo> const &k10_plus_info,
                                     DebugInfo * const debug_info) {
    if (nacjd_doc->IsDocTypeStatistic())
        return GenerateMarcForStatistic(nacjd_doc, k10_plus_info, debug_info);


    MARC::Record *record(GenerateRecord("00000cai a22000000  4500", "tu"));
    InsertGeneralFieldInfo(record, nacjd_doc, k10_plus_info, debug_info);

    record->insertField("300", { { 'a', "Online-Ressource " } });

    return record;
}

MARC::Record *GenerateMarcForGeneric(NACJDDoc * const nacjd_doc, std::map<std::string, K10PlusInfo> const &k10_plus_info,
                                     DebugInfo * const debug_info) {
    if (nacjd_doc->IsDocTypeStatistic())
        return GenerateMarcForStatistic(nacjd_doc, k10_plus_info, debug_info);


    MARC::Record *record(GenerateRecord("00000caa a22000000  4500", "tu"));
    InsertGeneralFieldInfo(record, nacjd_doc, k10_plus_info, debug_info);

    return record;
}


MARC::Record *GenerateMarcForBook(NACJDDoc * const nacjd_doc, std::map<std::string, K10PlusInfo> const &k10_plus_info,
                                  DebugInfo * const debug_info) {
    MARC::Record *record(GenerateRecord("00000cam a22000000  4500", "tu"));
    InsertGeneralFieldInfo(record, nacjd_doc, k10_plus_info, debug_info);

    return record;
}


MARC::Record *GenerateMarcForReport(NACJDDoc * const nacjd_doc, std::map<std::string, K10PlusInfo> const &k10_plus_info,
                                    DebugInfo * const debug_info) {
    if (nacjd_doc->IsDocTypeStatistic())
        return GenerateMarcForStatistic(nacjd_doc, k10_plus_info, debug_info);

    MARC::Record *record(GenerateRecord("00000cam a22000002  4500", "cr||||"));
    InsertGeneralFieldInfo(record, nacjd_doc, k10_plus_info, debug_info);


    return record;
}


MARC::Record *GenerateMarcForJournal(NACJDDoc * const nacjd_doc, std::map<std::string, K10PlusInfo> const &k10_plus_info,
                                     DebugInfo * const debug_info) {
    // create a new record
    MARC::Record *record(GenerateRecord("00000naa a22000002  4500", "cr||||"));
    const MARC::Subfields _936_content(nacjd_doc->ConstructPublishingInfo_936());

    InsertGeneralFieldInfo(record, nacjd_doc, k10_plus_info, debug_info);


    record->insertField("655",
                        { { 'a', "Forschungsdaten" },
                          { '0', "(DE-588)1098579690" },
                          { '0', "(DE-627)857755366" },
                          { '0', "(DE-576)469182156" },
                          { '2', "gnd-content" } },
                        ' ', '7');


    record->insertField("935", { { 'a', "foda" }, { '2', "LOK" } });

    if (not _936_content.empty())
        record->insertField("936", _936_content, 'u', 'w');


    return record;
}

void WriteMarcRecords(const std::string &marc_path, const std::vector<NACJDDoc> &nacjd_docs,
                      const std::map<std::string, K10PlusInfo> &k10_plus_info, DebugInfo * const debug_info) {
    std::unique_ptr<MARC::Writer> marc_writer(MARC::Writer::Factory(marc_path));

    for (auto nacjd_doc : nacjd_docs) {
        if (nacjd_doc.ref_id_.empty())
            LOG_ERROR("No ID " + nacjd_doc.ref_id_ + " found on Title: " + nacjd_doc.title_ + ", Sec Title: " + nacjd_doc.sec_title_);


        // please see the list of RIS type here: https://en.wikipedia.org/wiki/RIS_(file_format)#Type_of_reference
        MARC::Record *record(nullptr);
        if (nacjd_doc.ris_type_.compare("ADVS") == 0) {
            // Audiovisual
            debug_info->counter_advs++;
            record = GenerateMarcForAudioVisual(&nacjd_doc, k10_plus_info, debug_info);
        } else if (nacjd_doc.ris_type_.compare("BOOK") == 0) {
            // Book
            debug_info->counter_book++;
            record = GenerateMarcForBook(&nacjd_doc, k10_plus_info, debug_info);
        } else if (nacjd_doc.ris_type_.compare("CHAP") == 0) {
            // Book section / chapter
            debug_info->counter_chap++;
            record = GenerateMarcForChapter(&nacjd_doc, k10_plus_info, debug_info);
        } else if (nacjd_doc.ris_type_.compare("CONF") == 0) {
            // Conference Proceedings
            debug_info->counter_conf++;
            record = GenerateMarcForConference(&nacjd_doc, k10_plus_info, debug_info);
        } else if (nacjd_doc.ris_type_.compare("ELEC") == 0) {
            // Web page / electronic citation
            debug_info->counter_elec++;
            record = GenerateMarcForWebsite(&nacjd_doc, k10_plus_info, debug_info);
        } else if (nacjd_doc.ris_type_.compare("GEN") == 0) {
            // Generic
            /**
             * Generic content by assumption (based on observation of the link given and converted output by Zotero)
             * is a journal article without ISSN; typically, it is a version of pre-print or author version or draft, etc.
             * However, for statistic it should encoded as statistic.
             */
            debug_info->counter_generic++;
            record = GenerateMarcForGeneric(&nacjd_doc, k10_plus_info, debug_info);
        } else if (nacjd_doc.ris_type_.compare("JOUR") == 0) {
            // Journal
            debug_info->counter_jour++;
            record = GenerateMarcForJournal(&nacjd_doc, k10_plus_info, debug_info);
        } else if (nacjd_doc.ris_type_.compare("MGZN") == 0) {
            // Magazine
            debug_info->counter_mgzn += 1;
            record = GenerateMarcForMagazine(&nacjd_doc, k10_plus_info, debug_info);
        } else if (nacjd_doc.ris_type_.compare("NEWS") == 0) {
            // Newspaper
            debug_info->counter_news++;
            record = GenerateMarcForNews(&nacjd_doc, k10_plus_info, debug_info);
        } else if (nacjd_doc.ris_type_.compare("RPRT") == 0) {
            // Report
            debug_info->counter_rprt++;
            record = GenerateMarcForReport(&nacjd_doc, k10_plus_info, debug_info);
        } else if (nacjd_doc.ris_type_.compare("THES") == 0) {
            // Thesis / Dissertation
            debug_info->counter_thes++;
            record = GenerateMarcForThesis(&nacjd_doc, k10_plus_info, debug_info);
        } else {
            debug_info->unknown_type.insert(nacjd_doc.ris_type_);
            debug_info->counter_unknown++;
            LOG_INFO("Unknown RIS TYPE: " + nacjd_doc.ris_type_);
        }

        // avoid signal 11 caused by nullptr on writing marc file
        if (record)
            marc_writer.get()->write(*record);
        else
            LOG_ERROR("Generator can't generate record for type: " + nacjd_doc.ris_type_);

        delete record;
    }
}

void ExtractInfoFromNACJD(const std::string &json_path, std::vector<NACJDDoc> * const nacjd_docs) {
    std::ifstream f(json_path, std::ifstream::in);
    nlohmann::json jdat(nlohmann::json::parse(f));
    const auto docs(jdat.at("searchResults").at("response").at("docs"));

    if (not docs.is_structured())
        LOG_ERROR("docs is empty");

    for (const auto &doc : jdat.at("searchResults").at("response").at("docs")) {
        NACJDDoc nacjd_doc;

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
            nacjd_doc.type_work_ = StringUtil::ASCIIToUpper(doc.at("TYPE_WORK").get<std::string>());
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
        if (doc.contains("AUTHORS_SPLIT")) {
            nacjd_doc.authors_split = doc.at("AUTHORS_SPLIT").get<std::vector<std::string>>();
        }
        if (doc.contains("STUDYTITLE")) {
            nacjd_doc.study_titles_ = doc.at("STUDYTITLE").get<std::set<std::string>>();
        }


        nacjd_docs->emplace_back(nacjd_doc);
    }
}

void BuildISSNCache(std::map<std::string, K10PlusInfo> * const issn_to_k10_plus_info, const std::string &source_file_name,
                    DebugInfo * const debug_info) {
    auto input_file(MARC::Reader::Factory(source_file_name));
    while (MARC::Record record = input_file->read()) {
        const std::set<std::string> issns(record.getISSNs());
        const bool is_open_access(MARC::IsOpenAccess(record));
        const K10PlusInfo k10_plus_info(record.getControlNumber(), is_open_access);
        bool is_online_resource(false);
        std::string control_number_or_superior_number(record.getControlNumber());

        for (const auto &fiedl300 : record.getTagRange("300")) {
            const std::string subfield_a = fiedl300.getFirstSubfieldWithCode('a');
            if (subfield_a == "Online-Ressource") {
                is_online_resource = true;
                break;
            }
        }

        if (not is_online_resource) {
            for (const auto &field338 : record.getTagRange("338")) {
                const std::string subfield_a = field338.getFirstSubfieldWithCode('a');
                if (subfield_a == "Online-Ressource") {
                    is_online_resource = true;
                    break;
                }
            }
        }

        if (not is_online_resource) {
            for (const auto &field776 : record.getTagRange("776")) {
                if (field776.getIndicator1() == '0' && field776.getIndicator2() == '8') {
                    const std::string sub_i = field776.getFirstSubfieldWithCode('i');
                    const std::string sub_n = field776.getFirstSubfieldWithCode('n');

                    if (sub_i == "Erscheint auch" && sub_n == "Online-Ausgabe") {
                        control_number_or_superior_number = record.getSuperiorControlNumber();
                        is_online_resource = true;
                    }
                }
            }
        }

        if (is_online_resource) {
            for (auto const &issn : issns) {
                issn_to_k10_plus_info->insert({ StringUtil::ASCIIToUpper(issn), { control_number_or_superior_number, is_open_access } });
                debug_info->k10plus_issn_online++;
            }
        } else {
            debug_info->k10plus_issn_printed += issns.size();
        }
    }
}

void ShowInfoForDebugging(const DebugInfo &debug_info) {
    std::cout << "=== ISSN Found in K10Plus ===" << std::endl;
    for (auto db_found : debug_info.superior_work_found) {
        std::cout << "ISSN: " << db_found.first << " , PPN: " << db_found.second << std::endl;
    }
    std::cout << "=== ISSN not found in K10Plus ===" << std::endl;
    for (auto db_not_found : debug_info.superior_work_not_found) {
        std::cout << db_not_found << std::endl;
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

    std::cout << "K10 Plus, issn online: " << debug_info.k10plus_issn_online << std::endl;
    std::cout << "K10 Plus, issn printed: " << debug_info.k10plus_issn_printed << std::endl << std::endl;

    std::cout << "The number of updated data using information from K-10-Plus: " << debug_info.data_found_in_k10_plus << std::endl;
    std::cout << "The number of data that did not update: " << debug_info.data_not_found_in_k10_plus << std::endl;
    std::cout << "The number of data without ISSN: " << debug_info.counter_data_without_issn << std::endl << std::endl;

    std::cout << "The number of doi with issn: " << debug_info.counter_doi_with_issn << std::endl;
    std::cout << "The number of doi without issn: " << debug_info.counter_doi_without_issn << std::endl << std::endl;

    std::cout << "ISSN found in K10-Plus (unique): " << debug_info.superior_work_found.size() << std::endl;
    std::cout << "ISSN not found in K10-Plus (unique) including the printed version: " << debug_info.superior_work_not_found.size()
              << std::endl;
}

void BuildOpenAccessCache(const std::string &file_path, std::map<std::string, std::string> * const open_access_info_cache) {
    std::vector<std::vector<std::string>> lines;
    TextUtil::ParseCSVFileOrDie(file_path, &lines);

    auto line(lines.cbegin());
    if (line == lines.cend())
        LOG_ERROR("Open Access file is empty");

    // Construct open access cache
    for (line = lines.cbegin(); line != lines.cend(); ++line) {
        if (line->size() != 3)
            LOG_ERROR("Logical line #" + std::to_string(line - lines.cbegin()) + " doesn't contain 3 values!");

        if (unlikely((*line)[0].empty()))
            LOG_ERROR("Logical line #" + std::to_string(line - lines.cbegin()) + " is missing the DOI!");
        if (unlikely((*line)[1].empty() and (*line)[2].empty()))
            LOG_ERROR("Logical line #" + std::to_string(line - lines.cbegin()) + " is missing Open Access Information!");

        if (not(*line)[1].empty() && not(*line)[2].empty())
            open_access_info_cache->insert({ (*line)[0], (*line)[1] });
    }
}

void FindAndReplaceOpenAccessInfo(MARC::Record * const record, const std::map<std::string, std::string> &open_access_info_cache,
                                  AugmentedOpenAccessDebugInfo * const debug_info) {
    for (auto &field : *record) {
        if (field.getTag() != "856")
            continue;

        if (field.getIndicator1() != '4' && field.getIndicator2() != '0')
            continue;

        MARC::Subfields subfields(field.getContents());
        if (not subfields.hasSubfield('u'))
            continue;

        const std::string subfield_u(subfields.getFirstSubfieldWithCode('u'));

        if (subfield_u.substr(0, 16) != "https://doi.org/")
            continue;


        const auto oa_info(open_access_info_cache.find(subfield_u));
        if (oa_info == open_access_info_cache.end()) {
            debug_info->not_found_in_openalex.emplace(subfield_u);
            debug_info->total_not_in_openalex++;
            continue;
        }

        subfields.appendSubfield('z', (oa_info->second == "true" ? "LF" : "ZZ"));
        field.setSubfields(subfields);

        (oa_info->second == "true" ? debug_info->updated_lf++ : debug_info->updated_zz++);
    }
}

void AugmentOpenAccessInfo(int argc, char **argv, const bool &debug_mode) {
    if (argc < 5)
        Usage();


    AugmentedOpenAccessDebugInfo augmented_oa_debug_info;
    std::map<std::string, std::string> open_access_info_cache;
    std::unique_ptr<MARC::Reader> marc_reader(MARC::Reader::Factory(argv[2]));
    std::unique_ptr<MARC::Writer> marc_writer(MARC::Writer::Factory(argv[4]));

    if (unlikely((argv[2] == (argv[4]))))
        LOG_ERROR("The input file name equals the output file name!");

    BuildOpenAccessCache(argv[3], &open_access_info_cache);

    while (MARC::Record record = marc_reader.get()->read()) {
        // 856u
        FindAndReplaceOpenAccessInfo(&record, open_access_info_cache, &augmented_oa_debug_info);
        marc_writer.get()->write(record);
    }

    if (debug_mode) {
        for (auto const &doi : augmented_oa_debug_info.not_found_in_openalex) {
            std::cout << doi << std::endl;
        }

        std::cout << "\n\n";
        std::cout << "Not found in OpenAlex: " << augmented_oa_debug_info.total_not_in_openalex << std::endl;
        std::cout << "Augmented: " << augmented_oa_debug_info.updated_lf + augmented_oa_debug_info.updated_zz << std::endl;
        std::cout << "Augmented with LF: " << augmented_oa_debug_info.updated_lf << std::endl;
        std::cout << "Augmented with ZZ: " << augmented_oa_debug_info.updated_zz << std::endl;
    }
}

void Convert(int argc, char **argv, const bool &debug_mode) {
    if (argc < 5)
        Usage();

    std::vector<NACJDDoc> nacjd_docs;
    std::map<std::string, K10PlusInfo> issn_to_k10_plus_info;
    DebugInfo debug_info;
    ExtractInfoFromNACJD(argv[2], &nacjd_docs);
    BuildISSNCache(&issn_to_k10_plus_info, argv[3], &debug_info);
    WriteMarcRecords(argv[4], nacjd_docs, issn_to_k10_plus_info, &debug_info);

    if (debug_mode) {
        ShowInfoForDebugging(debug_info);
    }
}

} // unnamed namespace

int Main(int argc, char **argv) {
    if (argc < 5)
        Usage();

    bool debug_mode(false);

    if (std::strcmp(argv[1], "--verbose") == 0) {
        debug_mode = true;
        --argc, ++argv;
    }

    const std::string mode(argv[1]);

    if (mode == "convert") {
        Convert(argc, argv, debug_mode);
    } else if (mode == "augment_open_access") {
        AugmentOpenAccessInfo(argc, argv, debug_mode);
    } else
        Usage();


    return EXIT_SUCCESS;
}
/** \brief A tool for rewriting  information in ssoar data
 *  \author Johannes Riedl
 *
 *  \copyright 2018 Universitätsbibliothek Tübingen.  All rights reserved.
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
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "Compiler.h"
#include "MARC.h"
#include "RegexMatcher.h"
#include "StringUtil.h"
#include "util.h"


namespace {


[[noreturn]] void Usage() {
    std::cerr << "Usage: " << ::progname << " marc_input marc_output\n";
    std::exit(EXIT_FAILURE);
}


bool ParseVolInfo(const std::string &volinfo, std::string * const volinfo_vol, std::string * const volinfo_year,
                  std::string * const volinfo_edition) {
    static const std::string volinfo_regex("(\\d+)\\s+\\((\\d{4})\\)\\s+([\\d\\-+/]+)"); // vol (year) edition
    static RegexMatcher * const vol_info_matcher(RegexMatcher::RegexMatcherFactoryOrDie(volinfo_regex));
    if (not vol_info_matcher->matched(volinfo))
        return false;

    *volinfo_vol = (*vol_info_matcher)[1];
    *volinfo_year = (*vol_info_matcher)[2];
    *volinfo_edition = (*vol_info_matcher)[3];
    // Normalize range representation in edition ranges because RDA requires it
    StringUtil::Map(volinfo_edition, "-+", "//");
    return true;
}


void Assemble773Article(MARC::Subfields * const _773subfields, const std::string &title = "", const std::string &year = "",
                        const std::string &pages = "", const std::string &volinfo = "", const std::string &edition = "") {
    if (not(title.empty() and volinfo.empty() and pages.empty() and year.empty() and edition.empty()))
        _773subfields->appendSubfield('i', "In:");
    if (not title.empty())
        _773subfields->appendSubfield('t', StringUtil::Trim(title));

    std::string subfield_g_content;
    std::string volinfo_vol, volinfo_year, volinfo_edition;

    // Generate $g with vol(year), edition, pages
    if (not volinfo.empty())
        ParseVolInfo(StringUtil::Trim(volinfo), &volinfo_vol, &volinfo_year, &volinfo_edition);

    if (not(year.empty() and volinfo_year.empty()))
        _773subfields->appendSubfield('d', (not year.empty()) ? year : volinfo_year);

    if (not volinfo_vol.empty())
        subfield_g_content += volinfo_vol;

    if (not volinfo_year.empty())
        subfield_g_content += "(" + volinfo_year + ")";

    if (not(edition.empty() and volinfo_edition.empty()))
        subfield_g_content += (not edition.empty()) ? ", " + edition : ", " + volinfo_edition;

    if (not pages.empty()) {
        subfield_g_content += not subfield_g_content.empty() ? ", " : subfield_g_content;
        subfield_g_content += "S. " + pages;
    }

    if (not subfield_g_content.empty())
        _773subfields->appendSubfield('g', subfield_g_content);
}


void Assemble773Book(MARC::Subfields * const _773subfields, const std::string &title = "", const std::string &authors = "",
                     const std::string &year = "", const std::string &pages = "", const std::string &isbn = "") {
    if (not(title.empty() and authors.empty() and year.empty() and pages.empty() and isbn.empty()))
        _773subfields->addSubfield('i', "In:");
    if (not title.empty()) {
        _773subfields->addSubfield('t', StringUtil::Trim(title));
    }
    if (not authors.empty()) {
        // Do not add editors
        static std::string editor_regex("\\(Hg\\.\\)");
        static RegexMatcher * const editor_matcher(RegexMatcher::RegexMatcherFactory(editor_regex));
        if (not editor_matcher->matched(authors))
            _773subfields->appendSubfield('a', authors);
    }
    if (not year.empty())
        _773subfields->appendSubfield('d', year);
    if (not pages.empty()) {
        const std::string _773g_year(_773subfields->hasSubfield('d') ? "(" + _773subfields->getFirstSubfieldWithCode('d') + ")" : "");
        _773subfields->appendSubfield('g', _773g_year + ", " + "S. " + pages);
    }
    if (not isbn.empty())
        _773subfields->appendSubfield('o', isbn);
}


void Assemble936Article(MARC::Subfields * const _936subfields, const std::string &year = "", const std::string &pages = "",
                        const std::string &volinfo = "", const std::string &edition = "") {
    if (volinfo.empty() and pages.empty() and year.empty() and edition.empty() and volinfo.empty())
        return;

    std::string volinfo_vol;
    std::string volinfo_year;
    std::string volinfo_edition;
    // Volinfo might also contain vol year edition in the format "vol (year) edition"
    if (not volinfo.empty()) {
        if (ParseVolInfo(volinfo, &volinfo_vol, &volinfo_year, &volinfo_edition))
            _936subfields->addSubfield('d', volinfo_vol);
        else
            _936subfields->addSubfield('d', StringUtil::Trim(volinfo));
    }
    if (not(year.empty() and volinfo_year.empty()))
        _936subfields->addSubfield('j', (not year.empty()) ? year : volinfo_year);
    if (not pages.empty())
        _936subfields->addSubfield('h', pages);
    if (not(edition.empty() and volinfo_edition.empty()))
        _936subfields->addSubfield('e', (not edition.empty()) ? edition : volinfo_edition);
}


void Assemble936Book(MARC::Subfields * const _936subfields, const std::string &year = "", const std::string &pages = "") {
    if (year.empty() and pages.empty())
        return;
    if (not year.empty())
        _936subfields->addSubfield('j', year);
    if (not pages.empty())
        _936subfields->addSubfield('h', pages);
}


void Parse500Content(MARC::Record * const record, bool * const modified_record, const std::string &_500a_content,
                     MARC::Subfields * const _773subfields, MARC::Subfields * const _936subfields) {
    // Belegung nach BSZ-Konkordanz
    // 773 $a "Geistiger Schöpfer"
    // 773 08 $i "Beziehungskennzeichnung" (== Übergerordnetes Werk)
    // 773 $d Jahr
    // 773 $t Titel (wenn Autor nicht vorhanden, dann stattdessen $a)
    // 773 $g Bandzählung [und weitere Angaben]
    // 773 $o "Sonstige Identifier für die andere Ausgabe" (ISBN)

    // 500 Structure for books
    // Must be checked first since it is more explicit
    // Normally it is Author(s) : Title. Year. S. xxx. ISBN
    static const std::string book_regex_1("^([^:]*):\\s*(.+)?\\s*(\\d{4})\\.(?=\\s*S\\.\\s*([\\d\\-]+)\\.\\s*ISBN\\s*([\\d\\-X]+))");
    static RegexMatcher * const book_matcher_1(RegexMatcher::RegexMatcherFactoryOrDie(book_regex_1));
    // Authors : Title. Year. Pages
    static const std::string book_regex_2("^([^:]*):\\s*(.+)?\\s*(\\d{4})\\.(?=\\sS\\.\\s([\\d\\-]+))");
    static RegexMatcher * const book_matcher_2(RegexMatcher::RegexMatcherFactoryOrDie(book_regex_2));
    // Authors : Title. Year. ISBN
    static const std::string book_regex_3("^([^:]*):\\s*(.+)?\\s*(\\d{4})\\.(?=\\s*ISBN\\s*([\\d\\-X]+))");
    static RegexMatcher * const book_matcher_3(RegexMatcher::RegexMatcherFactoryOrDie(book_regex_3));
    // Title. Year. Pages
    static const std::string book_regex_4("^(.+)\\.\\s+(\\d{4})\\.\\s+S\\.\\s+([\\d-]+)");
    static RegexMatcher * const book_matcher_4(RegexMatcher::RegexMatcherFactoryOrDie(book_regex_4));

    // 500 Structure fields for articles
    // Normally Journal ; Edition String ; Page (??)
    static const std::string article_regex_1("^([^;]*)\\s*;\\s*([^;]*)\\s*;\\s*([\\d\\-]*)\\s*");
    static RegexMatcher * const article_matcher_1(RegexMatcher::RegexMatcherFactoryOrDie(article_regex_1));
    // Journal; Pages
    static const std::string article_regex_2("^([^;]*)\\s*;\\s*([\\d\\-]*)\\s*");
    static RegexMatcher * const article_matcher_2(RegexMatcher::RegexMatcherFactoryOrDie(article_regex_2));
    // Journal (Year)
    static const std::string article_regex_3("^(.*)\\s*\\((\\d{4})\\)");
    static RegexMatcher * const article_matcher_3(RegexMatcher::RegexMatcherFactoryOrDie(article_regex_3));

    if (book_matcher_1->matched(_500a_content)) {
        const std::string authors((*book_matcher_1)[1]);
        const std::string title((*book_matcher_1)[2]);
        const std::string year((*book_matcher_1)[3]);
        const std::string pages((*book_matcher_1)[4]);
        const std::string isbn((*book_matcher_1)[5]);
        Assemble773Book(_773subfields, title, authors, year, pages, isbn);
        Assemble936Book(_936subfields, year, pages);
    } else if (book_matcher_2->matched(_500a_content)) {
        const std::string authors((*book_matcher_2)[1]);
        const std::string title((*book_matcher_2)[2]);
        const std::string year((*book_matcher_2)[3]);
        Assemble773Book(_773subfields, title, authors, year);
        Assemble936Book(_936subfields, year);
    } else if (book_matcher_3->matched(_500a_content)) {
        const std::string authors((*book_matcher_3)[1]);
        const std::string title((*book_matcher_3)[2]);
        const std::string year((*book_matcher_3)[3]);
        const std::string isbn((*book_matcher_3)[4]);
        Assemble773Book(_773subfields, title, authors, year, "", isbn);
        Assemble936Book(_936subfields, year);
    } else if (book_matcher_4->matched(_500a_content)) {
        const std::string title((*book_matcher_4)[1]);
        const std::string year((*book_matcher_4)[2]);
        const std::string pages((*book_matcher_4)[3]);
        Assemble773Book(_773subfields, title, "", year, pages);
        Assemble936Book(_936subfields, year, pages);
    } else if (article_matcher_1->matched(_500a_content)) {
        const std::string title((*article_matcher_1)[1]);
        const std::string volinfo((*article_matcher_1)[2]);
        const std::string page((*article_matcher_1)[3]);
        Assemble773Article(_773subfields, title, "", page, volinfo, "");
        Assemble936Article(_936subfields, "", page, volinfo, "");

        // Flag the record as an article:
        if (record->getBibliographicLevel() != MARC::Record::SERIAL_COMPONENT_PART) {
            record->setBibliographicLevel(MARC::Record::SERIAL_COMPONENT_PART);
            *modified_record = true;
        }
    } else if (article_matcher_2->matched(_500a_content)) {
        // See whether we can extract further information
        const std::string title_and_spec((*article_matcher_2)[1]);
        const std::string pages((*article_matcher_2)[2]);
        static const std::string title_and_spec_regex("^([^(]*)\\s*\\((\\d{4})\\)\\s*(\\d+)\\s*");
        static RegexMatcher * const title_and_spec_matcher(RegexMatcher::RegexMatcherFactoryOrDie(title_and_spec_regex));
        if (title_and_spec_matcher->matched(title_and_spec)) {
            const std::string title((*title_and_spec_matcher)[1]);
            const std::string year((*title_and_spec_matcher)[2]);
            const std::string edition((*title_and_spec_matcher)[3]);
            Assemble773Article(_773subfields, title, year, pages, "", edition);
            Assemble936Article(_936subfields, year, pages, "", edition);
        } else {
            Assemble773Article(_773subfields, title_and_spec, "", pages);
            Assemble936Article(_936subfields, "", pages);
        }

        // Flag the record as an article:
        if (record->getBibliographicLevel() != MARC::Record::SERIAL_COMPONENT_PART) {
            record->setBibliographicLevel(MARC::Record::SERIAL_COMPONENT_PART);
            *modified_record = true;
        }
    } else if (article_matcher_3->matched(_500a_content)) {
        const std::string title((*article_matcher_3)[1]);
        const std::string year((*article_matcher_3)[2]);
        Assemble773Article(_773subfields, title, year);
        Assemble936Article(_936subfields, year);

        // Flag the record as an article:
        if (record->getBibliographicLevel() != MARC::Record::SERIAL_COMPONENT_PART) {
            record->setBibliographicLevel(MARC::Record::SERIAL_COMPONENT_PART);
            *modified_record = true;
        }
    } else
        LOG_WARNING("No matching regex for " + _500a_content);
}


void InsertSigilInto003And852(MARC::Record * const record, bool * const modified_record) {
    static const std::string ISIL_KRIMDOK("DE-2619");

    record->insertField("003", ISIL_KRIMDOK);
    auto field_852(record->findTag("852"));
    if (field_852 != record->end()) {
        auto subfields(field_852->getSubfields());
        subfields.replaceFirstSubfield('a', ISIL_KRIMDOK);
        field_852->setSubfields(subfields);
    }

    *modified_record = true;
}


// Rewrite to 041$h or get date from 008
void InsertLanguageInto041(MARC::Record * const record, bool * const modified_record) {
    for (auto &field : record->getTagRange("041")) {
        // Check whether the information is already in the $a field
        static const std::string valid_language_regex("([a-zA-Z]{3})$");
        static RegexMatcher * const valid_language_matcher(RegexMatcher::RegexMatcherFactoryOrDie(valid_language_regex));
        std::string language;
        if (valid_language_matcher->matched(field.getFirstSubfieldWithCode('a')))
            return;
        else {
            const std::string _008_field(record->getFirstFieldContents("008"));
            if (not valid_language_matcher->matched(_008_field)) {
                LOG_WARNING("Invalid language code " + language);
                continue;
            }
            field.insertOrReplaceSubfield('a', language);
            *modified_record = true;
            return;
        }
    }
}


void InsertYearInto264c(MARC::Record * const record, bool * const modified_record) {
    for (const auto &field : record->getTagRange("264")) {
        if (field.hasSubfield('c'))
            return;
        // Extract year from 008 if available
        const std::string _008_field(record->getFirstFieldContents("008"));
        const std::string year(_008_field.substr(7, 4));
        record->addSubfield("264", 'c', year);
        *modified_record = true;
        return;
    }
}


// Write to the MARC correspondence of PICA 8520 (Field for local SWB projects for monographies)
void WriteLocal938L8(MARC::Record * const record, const std::string &subfield_8_content, const std::string &content) {
    record->insertField("938", { { '2', "LOK" }, { 'l', content }, { '8', subfield_8_content } });
}


// Transfer the original 500 data to a "parking field", i.e. make sure that the content description delivered in 500 fields
// is kept in a field that "survives" potential transformations"
void Copy500SuperiorToLocal938Field(MARC::Record * const record, const std::string &_500a_superior_content) {
    WriteLocal938L8(record, "0", _500a_superior_content);
}


void Move024OIAIdentifierToLocal938Field(MARC::Record * const record, bool * const modified_record) {
    static const std::string oai_regex("^oai:gesis.izsoz.de:document/.*");
    static RegexMatcher * const oai_matcher(RegexMatcher::RegexMatcherFactory(oai_regex));

    for (const auto &field : record->getTagRange("024")) {
        if (oai_matcher->matched(field.getFirstSubfieldWithCode('a'))) {
            WriteLocal938L8(record, "1", (*oai_matcher)[0]);
            *modified_record = true;
        }
    }

    auto field(record->getFirstField("024"));
    while (field != record->end() and field->getTag() == "024") {
        if (oai_matcher->matched(field->getFirstSubfieldWithCode('a')))
            field = record->erase(field);
        else
            ++field;
    }
}


void Create773And936From500(MARC::Record * const record, bool * const modified_record) {
    if (record->findTag("773") != record->end())
        LOG_ERROR("We were erroneously called for PPN " + record->getControlNumber() + " although a 773 field is already present");

    // Check if we have matching 500 field
    const std::string superior_string("^In:[\\s]*(.*)");
    static RegexMatcher * const superior_matcher(RegexMatcher::RegexMatcherFactory(superior_string));

    std::vector<std::string> new_773_fields;
    std::vector<std::string> new_936_fields;
    for (auto &field : record->getTagRange("500")) {
        const auto subfields(field.getSubfields());
        for (const auto &subfield : subfields) {
            if (subfield.code_ == 'a' and superior_matcher->matched(subfield.value_)) {
                MARC::Subfields new_773_Subfields;
                MARC::Subfields new_936_Subfields;
                // Parse Field Contents
                Parse500Content(record, modified_record, (*superior_matcher)[1], &new_773_Subfields, &new_936_Subfields);
                if (not new_773_Subfields.empty())
                    new_773_fields.emplace_back(new_773_Subfields.toString());
                if (not new_936_Subfields.empty())
                    new_936_fields.emplace_back(new_936_Subfields.toString());
                Copy500SuperiorToLocal938Field(record, subfield.value_);
            }
        }
    }

    // Delete all 500 fields since relevant data has been copied away
    auto field(record->getFirstField("500"));
    while (field != record->end() and field->getTag() == "500")
        field = record->erase(field);

    for (const auto &new_773_field : new_773_fields)
        record->insertField("773", "08" + new_773_field);

    for (const auto &new_936_field : new_936_fields)
        record->insertField("936", "uw" + new_936_field);

    if (not(new_773_fields.empty() and new_936_fields.empty()))
        *modified_record = true;
}


void RewriteExisting773FieldAndAdd936(MARC::Record * const record, bool * const modified_record) {
    // Data from SSOAR contains data where $g is split up which it should not be
    for (auto &field : record->getTagRange("773")) {
        std::map<std::string, std::string> new_g_subfield_map;
        for (auto &subfield : field.getSubfields()) {
            if (subfield.code_ == 'g') {
                for (const auto target : { "volume", "number", "year", "pages" }) {
                    if (StringUtil::StartsWith(subfield.value_, (target + std::string(":")).c_str()))
                        new_g_subfield_map[target] = subfield.value_.substr(__builtin_strlen((target + std::string(":")).c_str()));
                }
            }
        }
        field.deleteAllSubfieldsWithCode('g');
        field.insertOrReplaceSubfield('g', new_g_subfield_map["volume"] + " (" + new_g_subfield_map["year"] + ") "
                                               + new_g_subfield_map["number"] + "; " + new_g_subfield_map["pages"]);
        field.deleteAllSubfieldsWithCode('i');
        field.insertOrReplaceSubfield('i', "In:");


        MARC::Subfields new_936_subfields;
        Assemble936Article(&new_936_subfields, new_g_subfield_map["year"], new_g_subfield_map["pages"], new_g_subfield_map["volume"],
                           new_g_subfield_map["number"]);

        std::vector<std::string> new_936_fields;
        if (not new_936_subfields.empty())
            new_936_fields.emplace_back(new_936_subfields.toString());

        for (const auto &new_936_field : new_936_fields)
            record->insertField("936", "uw" + new_936_field);

        *modified_record = true;
    }
}


void RewriteSuperiorReference(MARC::Record * const record, bool * const modified_record) {
    // Case 1: We already have 773 => Rewrite and generate 936
    if (record->findTag("773") != record->end()) {
        RewriteExisting773FieldAndAdd936(record, modified_record);
        return;
    }

    // Case 2: Create 773 and 936 from 500
    Create773And936From500(record, modified_record);
}


void RemoveExtraneousHyphensFrom653(MARC::Record * const record, bool * const modified_record) {
    for (auto &field : record->getTagRange("653")) {
        auto subfields(field.getSubfields());
        for (auto &subfield : subfields) {
            if (subfield.value_.find('-') != std::string::npos) {
                (void)std::remove(subfield.value_.begin(), subfield.value_.end(), '-');
                *modified_record = true;
            }
        }
        field.setSubfields(subfields);
    }
}


void RemoveExtraneousPublisherNames(MARC::Record * const record, bool * const modified_record) {
    static const std::vector<std::string> tags_to_clean{ "700", "710" };

    std::vector<size_t> field_indices_to_remove;
    for (const auto &tag : tags_to_clean) {
        const auto tag_range(record->getTagRange(tag));
        for (auto field(tag_range.begin()); field != tag_range.end(); ++field) {
            bool is_publisher(field->getFirstSubfieldWithCode('4') == "edt");
            if (record->isArticle() and is_publisher)
                field_indices_to_remove.push_back(field - record->begin());
        }
    }

    if (not field_indices_to_remove.empty()) {
        record->deleteFields(field_indices_to_remove);
        *modified_record = true;
    }
}


void MovePageNumbersFrom300(MARC::Record * const record, bool * const modified_record) {
    static RegexMatcher * const page_range_matcher(RegexMatcher::RegexMatcherFactoryOrDie("(.*)\\.S\\b"));

    auto field_300(record->findTag("300"));
    if (field_300 == record->end())
        return;

    const auto first_subfield(field_300->getFirstSubfieldWithCode('a'));
    if (page_range_matcher->matched(first_subfield)) {
        *modified_record = true;
        // move to 936$h and 773$g, if not already present
        auto page_string((*page_range_matcher)[1]);
        page_string = StringUtil::Trim(page_string);

        auto field_936(record->findTag("936")), field_773(record->findTag("773"));
        if (field_936 == record->end())
            record->insertField("936", { { 'h', page_string } });
        else if (not field_936->hasSubfield('h'))
            field_936->insertOrReplaceSubfield('h', page_string);

        if (field_773 == record->end())
            record->insertField("773", { { 'g', page_string } });
        else {
            bool page_number_present(false);
            for (const auto &subfield : field_773->getSubfields()) {
                if (subfield.code_ == 'g' and subfield.value_.find(page_string) != std::string::npos) {
                    page_number_present = true;
                    break;
                }
            }

            if (not page_number_present)
                field_773->insertOrReplaceSubfield('g', page_string + " .S");
        }

        record->erase(field_300);
    }
}


void FixArticleLeader(MARC::Record * const record, bool * const modified_record) {
    // SSOAR delivers a wrong leader for articles in journals: leader[7]=m instead of b
    // For chapters in books it is correctly done: leader[7]=a
    // So rewrite to b if we have a component part that is not part of a book
    // The criterion is that we do not have both "In:" and "(Hg.)"
    for (const auto &_500_a_subfield : record->getSubfieldValues("500", 'a')) {
        static const std::string is_book_component_regex("^(.+)\\(Hg.\\)(.+)");
        static RegexMatcher * const is_book_component_matcher(RegexMatcher::RegexMatcherFactoryOrDie(is_book_component_regex));
        // Skip fields that are definitely not relevant
        if (not StringUtil::StartsWith(_500_a_subfield, "In:"))
            continue;
        if (not is_book_component_matcher->matched(_500_a_subfield)) {
            record->setBibliographicLevel(MARC::Record::BibliographicLevel::SERIAL_COMPONENT_PART);
            *modified_record = true;
            return;
        }
    }
}


void RemoveLicenseField540(MARC::Record * const record, bool * const modified_record) {
    auto field_540(record->findTag("540"));
    if (field_540 != record->end()) {
        *modified_record = true;
        record->erase(field_540);
    }
}


void Rewrite856OpenAccess(MARC::Record * const record, bool * const modified_record) {
    for (auto &field : record->getTagRange("856")) {
        if (field.getIndicator1() == '4' and field.getIndicator2() == ' ' and field.hasSubfieldWithValue('z', "Open Access")) {
            field.insertOrReplaceSubfield('z', "Kostenfrei");
            *modified_record = true;
        }
    }
}


void Fix024DOIAndTransferTo856(MARC::Record * const record, bool * const modified_record) {
    static const std::string doi_regex("^http[s]?://doi.org/(.*$)");
    static RegexMatcher * const doi_matcher(RegexMatcher::RegexMatcherFactoryOrDie(doi_regex));

    for (auto &field : record->getTagRange("024")) {
        if (field.getIndicator1() == '7' and field.getIndicator2() == ' ' and doi_matcher->matched(field.getFirstSubfieldWithCode('a'))) {
            // Remove Resolver Prefix in 024
            field.insertOrReplaceSubfield('a', (*doi_matcher)[1]);
            // Create new information
            record->insertField("856",
                                MARC::Subfields({ MARC::Subfield('u', (*doi_matcher)[0]), MARC::Subfield('x', "Resolving System"),
                                                  MARC::Subfield('z', "Kostenfrei") }),
                                '4' /*indicator1*/, '0' /*indicator 2*/);
            *modified_record = true;
        }
    }
}


void ProcessRecords(MARC::Reader * const marc_reader, MARC::Writer * const marc_writer) {
    unsigned record_count(0), modified_count(0);
    while (MARC::Record record = marc_reader->read()) {
        ++record_count;
        bool modified_record(false);

        InsertSigilInto003And852(&record, &modified_record);
        InsertLanguageInto041(&record, &modified_record);
        InsertYearInto264c(&record, &modified_record);
        RewriteSuperiorReference(&record, &modified_record);
        RemoveExtraneousHyphensFrom653(&record, &modified_record);
        RemoveExtraneousPublisherNames(&record, &modified_record);
        MovePageNumbersFrom300(&record, &modified_record);
        FixArticleLeader(&record, &modified_record);
        RemoveLicenseField540(&record, &modified_record);
        Fix024DOIAndTransferTo856(&record, &modified_record);
        Rewrite856OpenAccess(&record, &modified_record);
        Move024OIAIdentifierToLocal938Field(&record, &modified_record);

        marc_writer->write(record);
        if (modified_record)
            ++modified_count;
    }

    LOG_INFO("Modified " + std::to_string(modified_count) + " of " + std::to_string(record_count) + " records");
}


} // unnamed namespace


int Main(int argc, char **argv) {
    if (argc != 3)
        Usage();

    const std::string marc_input_filename(argv[1]);
    const std::string marc_output_filename(argv[2]);
    if (unlikely(marc_input_filename == marc_output_filename))
        LOG_ERROR("Title data input file name equals output file name!");
    auto marc_reader(MARC::Reader::Factory(marc_input_filename));
    auto marc_writer(MARC::Writer::Factory(marc_output_filename));
    ProcessRecords(marc_reader.get(), marc_writer.get());

    return EXIT_SUCCESS;
}

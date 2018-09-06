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
#include "FileUtil.h"
#include "MARC.h"
#include "MiscUtil.h"
#include "RegexMatcher.h"
#include "StringUtil.h"
#include "TextUtil.h"
#include "util.h"


namespace {


[[noreturn]] void Usage() {
    std::cerr << "Usage: " << ::progname << " marc_input marc_output\n";
    std::exit(EXIT_FAILURE);
}


void Assemble773Article(MARC::Subfields * const _773subfields, const std::string &title = "",
                        const std::string &year = "", const std::string &pages = "",
                        const std::string &volinfo = "", const std::string &edition = "")
{
    if (not (title.empty() and volinfo.empty() and pages.empty() and year.empty() and edition.empty()))
       _773subfields->addSubfield('i', "In:");
    if (not title.empty())
        _773subfields->addSubfield('a', StringUtil::Trim(title));
    if (not volinfo.empty())
        _773subfields->addSubfield('g', "volume: " + volinfo);
    if (not pages.empty())
        _773subfields->addSubfield('g', "pages: " + pages);
    if (not year.empty())
        _773subfields->addSubfield('g', "year: " + year);
    if (not edition.empty())
        _773subfields->addSubfield('g', "edition: "  + edition);
}


void Assemble773Book(MARC::Subfields * const _773subfields, const std::string &title = "",
                     const std::string &authors = "", const std::string &year = "",
                     const std::string &pages = "", const std::string &isbn = "")
{
    if (not (title.empty() and authors.empty() and year.empty() and pages.empty() and isbn.empty()))
       _773subfields->addSubfield('i', "In:");
    if (not title.empty()) {
        if (not authors.empty())
            _773subfields->addSubfield('t', StringUtil::Trim(title));
        else
            _773subfields->addSubfield('a', StringUtil::Trim(title));
    }
    if (not authors.empty())
        _773subfields->addSubfield('a', authors);
    if (not year.empty())
        _773subfields->addSubfield('d', year);
    if ( not pages.empty())
        _773subfields->addSubfield('g', "pages:" + pages);
    if (not isbn.empty())
        _773subfields->addSubfield('o', isbn);
}


void ParseSuperior(const std::string &_500a_content, MARC::Subfields * const _773subfields) {
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
   static RegexMatcher * const book_matcher_2(RegexMatcher::RegexMatcherFactoryOrDie(book_regex_1));
   // Authors : Title. Year. ISBN
   static const std::string book_regex_3("^([^:]*):\\s*(.+)?\\s*(\\d{4})\\.(?=\\s*ISBN\\s*([\\d\\-X]+))");
   static RegexMatcher * const book_matcher_3(RegexMatcher::RegexMatcherFactoryOrDie(book_regex_2));

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
   } else if (book_matcher_2->matched(_500a_content)) {
       const std::string authors((*book_matcher_2)[1]);
       const std::string title((*book_matcher_2)[2]);
       const std::string year((*book_matcher_2)[3]);
       Assemble773Book(_773subfields, title, authors, year);
   } else if (book_matcher_3->matched(_500a_content)) {
       const std::string authors((*book_matcher_3)[1]);
       const std::string title((*book_matcher_3)[2]);
       const std::string year((*book_matcher_3)[3]);
       const std::string isbn((*book_matcher_3)[4]);
       Assemble773Book(_773subfields, title, authors, year, "", isbn);
   } else if (article_matcher_1->matched(_500a_content)) {
       const std::string title((*article_matcher_1)[1]);
       const std::string volinfo((*article_matcher_1)[2]);
       const std::string page((*article_matcher_1)[3]);
       Assemble773Article(_773subfields, title, "", page, volinfo, "");
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
       } else
          Assemble773Article(_773subfields, title_and_spec, "", pages);
   } else if (article_matcher_3->matched(_500a_content)) {
       const std::string title((*article_matcher_3)[1]);
       const std::string year((*article_matcher_3)[2]);
       Assemble773Article(_773subfields, title, year);
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
         if (field.hasSubfield('h'))
             return;

         // Check whether the information is already in the $a field
         static const std::string valid_language_regex("([a-zA-Z]{3})$");
         static RegexMatcher * const valid_language_matcher(RegexMatcher::RegexMatcherFactoryOrDie(valid_language_regex));
         std::string language;
         if (valid_language_matcher->matched(field.getFirstSubfieldWithCode('a'))) {
             field.replaceSubfieldCode('a', 'h');
             *modified_record = true;
             return;
         } else {
             const std::string _008_field(record->getFirstFieldContents("008"));
             if (not valid_language_matcher->matched(_008_field)) {
                 LOG_WARNING("Invalid language code " + language);
                 continue;
             }
             record->addSubfield("041", 'h', language);
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
        const std::string year(_008_field.substr(7,4));
        record->addSubfield("264", 'c', year);
        *modified_record = true;
        return;
    }
}


void RewriteSuperiorReference(MARC::Record * const record, bool * const modified_record) {
    if (record->findTag("773") != record->end())
        return;

    // Check if we have matching 500 field
    const std::string superior_string("^In:[\\s]*(.*)");
    RegexMatcher * const superior_matcher(RegexMatcher::RegexMatcherFactory(superior_string));

    std::vector<std::string> new_773_fields;
    for (auto &field : record->getTagRange("500")) {
        const auto subfields(field.getSubfields());
        for (const auto &subfield : subfields) {
            if (subfield.code_ == 'a' and superior_matcher->matched(subfield.value_)) {
                MARC::Subfields new_773_Subfields;
                // Parse Field Contents
                ParseSuperior((*superior_matcher)[1], &new_773_Subfields);
                if (not new_773_Subfields.empty())
                    new_773_fields.emplace_back(new_773_Subfields.toString());
            }
        }
    }

    for (const auto &new_773_field : new_773_fields)
        record->insertField("773", "08" + new_773_field);

    if (not new_773_fields.empty())
        *modified_record = true;
}


void RemoveExtraneousHyphensFrom653(MARC::Record * const record, bool * const modified_record) {
    for (auto &field : record->getTagRange("653")) {
        auto subfields(field.getSubfields());
        for (auto &subfield : subfields) {
            if (subfield.value_.find('-') != std::string::npos) {
                std::remove(subfield.value_.begin(), subfield.value_.end(), '-');
                *modified_record = true;
            }
        }
        field.setSubfields(subfields);
    }
}


void RemoveExtraneousPublisherNames(MARC::Record * const record, bool * const modified_record) {
    static const std::vector<std::string> tags_to_clean{ "700", "710" };

    std::vector<MARC::Record::iterator> fields_to_remove;
    for (const auto &tag : tags_to_clean) {
        const auto tag_range(record->getTagRange(tag));
        for (auto field(tag_range.begin()); field != tag_range.end(); ++field) {
            bool is_publisher(field->getFirstSubfieldWithCode('4') == "edt");
            if (record->isArticle() and is_publisher)
                fields_to_remove.push_back(field);
        }
    }

    for (auto &field_to_remove : fields_to_remove)
        record->erase(field_to_remove);

    if (not fields_to_remove.empty())
        *modified_record = true;
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
            record->insertField( "936" , { { 'h', page_string } });
        else if (not field_936->hasSubfield('h'))
            field_936->insertOrReplaceSubfield('h', page_string);

        if (field_773 == record->end())
            record->insertField( "773" , { { 'g', page_string } });
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


void FixArticleLeader(MARC::Record * const record,  bool * const modified_record) {
     // SSOAR delivers a wrong leader for articles in journals: leader[7]=m instead of b
     // For chapters in books it is correctly done: leader[7]=a
     // So rewrite to b if we have a component part that is not part of a book
     // We use the fact that we have already rewritten the 773 fields where $i contains "In:"
     for (const auto field : record->getTagRange("773")) {
          if (field.hasSubfieldWithValue('i', "In:") and record->getLeader()[7] != 'a') {
              record->setBibliographicLevel('b');
              *modified_record = true;
              return;
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
    ProcessRecords(marc_reader.get() , marc_writer.get());

    return EXIT_SUCCESS;
}

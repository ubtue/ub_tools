/** \brief A tool for rewriting superior information in ssoar data
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
    std::cerr << "Usage: " << ::progname << " [--input-format=(marc-21|marc-xml)] marc_input marc_output\n";
    std::exit(EXIT_FAILURE);
}


void ParseSuperior(const std::string &_500aContent, MARC::Subfields * const _773subfields) {
   // Belegung nach BSZ-Konkordanz
   // 773 $a "Geistiger Schöpfer"
   // 773 08 $i "Beziehungskennzeichnung" (== Übergerordnetes Werk)
   // 773 $d Jahr
   // 773 $t Titel
   // 773 $g Bandzählung
   // 773 $o "Sonstige Identifier für die andere Ausgabe" (ISBN)

   // Structure 500a fields for articles
   // Normally Journal ; Edition String ; Page (??)
   const std::string article_regex("^([^;]*)\\s*;\\s*([^;]*)\\s*;\\s*([\\d\\-]*)\\s*");
   static RegexMatcher * const article_matcher(RegexMatcher::RegexMatcherFactoryOrDie(article_regex));
   // Structure 500a for books
   // Normally it is Author(s) : Title. Year. S. xxx. ISBN
   const std::string book_regex("^([^:]*):\\s*(.+)?\\s*(\\d{4})\\.(?=\\s*S\\.\\s*([\\d\\-]+)\\.\\s*ISBN\\s*([\\d\\-X]+))");
   static RegexMatcher * const book_matcher(RegexMatcher::RegexMatcherFactoryOrDie(book_regex));
   // Authors : Title. Year. Pages
   const std::string book_regex_1("^([^:]*):\\s*(.+)?\\s*(\\d{4})\\.(?=\\sS\\.\\s([\\d\\-]+))");
   static RegexMatcher * const book_matcher_1(RegexMatcher::RegexMatcherFactoryOrDie(book_regex_1));

   if (article_matcher->matched(_500aContent)) {
       const std::string title((*article_matcher)[1]);
       const std::string volinfo((*article_matcher)[2]);
       const std::string page((*article_matcher)[3]);
       _773subfields->addSubfield('t', title);
       _773subfields->addSubfield('g', volinfo);
   } else if (book_matcher->matched(_500aContent)) {
       const std::string authors((*book_matcher)[1]);
       const std::string title((*book_matcher)[2]);
       const std::string year((*book_matcher)[3]);
       const std::string pages((*book_matcher)[4]);
       const std::string isbn((*book_matcher)[5]);
       _773subfields->addSubfield('t', title);
       _773subfields->addSubfield('a', authors);
       _773subfields->addSubfield('d', year);
       _773subfields->addSubfield('o', isbn);
       // FIXME pages

   } else if (book_matcher_1->matched(_500aContent)) {
       const std::string authors((*book_matcher_1)[1]);
       const std::string title((*book_matcher_1)[2]);
       const std::string year((*book_matcher_1)[3]);
       _773subfields->addSubfield('t', title);
       _773subfields->addSubfield('a', authors);
       _773subfields->addSubfield('d', year);
   }
}


void RewriteSSOARSuperiorReference(MARC::Reader * const marc_reader, MARC::Writer * const marc_writer) {
    unsigned record_count(0), modified_count(0);
    while (MARC::Record record = marc_reader->read()) {
        ++record_count;
        bool modified_record(false);

        if (record.findTag("773") != record.end()) {
            marc_writer->write(record);
            continue;
        }

        // Check if we have matching 500 field
        const std::string superior_string("^In:[\\s]*(.*)");
        RegexMatcher * const superior_matcher(RegexMatcher::RegexMatcherFactory(superior_string));

        for (auto &field : record.getTagRange("500")) {
            const auto subfields(field.getSubfields());
            for (const auto &subfield : subfields) {
                if (subfield.code_ == 'a' && superior_matcher->matched(subfield.value_)) {
                    MARC::Subfields new773Subfields;
                    // Parse Field Contents
                    ParseSuperior((*superior_matcher)[1], &new773Subfields);
                    // Write 773 Field
                    if (not new773Subfields.empty()) {
                        record.insertField("773", new773Subfields, '0', '8');
                        modified_record = true;
                    }
                }
            }
        }
        marc_writer->write(record);
        if (modified_record)
            ++modified_count;
    }

    LOG_INFO("Modified " + std::to_string(modified_count) + " of " + std::to_string(record_count) + " records");
}


} // unnamed namespace

int Main(int argc, char **argv) {
    ::progname = argv[0];

    MARC::FileType reader_type(MARC::FileType::AUTO);
    if (argc == 4) {
        if (std::strcmp(argv[1], "--input-format=marc-21") == 0)
            reader_type = MARC::FileType::BINARY;
        else if (std::strcmp(argv[1], "--input-format=marc-xml") == 0)
            reader_type = MARC::FileType::XML;
        else
            Usage();
        ++argv, --argc;
    }

    if (argc != 3)
        Usage();

    const std::string marc_input_filename(argv[1]);
    const std::string marc_output_filename(argv[2]);
    if (unlikely(marc_input_filename == marc_output_filename))
        LOG_ERROR("Title data input file name equals output file name!");
    std::unique_ptr<MARC::Reader> marc_reader(MARC::Reader::Factory(marc_input_filename, reader_type));
    std::unique_ptr<MARC::Writer> marc_writer(MARC::Writer::Factory(marc_output_filename));
    RewriteSSOARSuperiorReference(marc_reader.get() , marc_writer.get());
    return(EXIT_SUCCESS);
}

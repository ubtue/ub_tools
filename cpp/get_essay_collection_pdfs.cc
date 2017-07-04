/** \brief Utility for downloading PDFs of essay collections.
 *  \author Dr. Johannes Ruscheinski (johannes.ruscheinski@uni-tuebingen.de)
 *
 *  \copyright 2017 Universitätsbiblothek Tübingen.  All rights reserved.
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
#include <map>
#include <stdexcept>
#include <vector>
#include <cstdio>
#include <cstdlib>
#include "Compiler.h"
#include "MarcReader.h"
#include "MarcRecord.h"
#include "PerlCompatRegExp.h"
#include "Subfields.h"
#include "StringUtil.h"
#include "util.h"


static void Usage() __attribute__((noreturn));


static void Usage() {
    std::cerr << "Usage: " << ::progname << " marc_data\n";
    std::exit(EXIT_FAILURE);
}


void ProcessRecords(MarcReader * const marc_reader) {
    unsigned record_count(0), until1999_count(0), from2000_to_2009_count(0), after2009_count(0),
             unhandled_url_count(0), good_count(0);
    PerlCompatRegExp year_reg_exp(PerlCompatRegExp("(\\d\\d\\d\\d)"));
    while (const MarcRecord record = marc_reader->read()) {
        ++record_count;

        const std::string _655_contents(record.getFieldData("655"));
        if (_655_contents.empty())
            continue;
        const Subfields _655_subfields(_655_contents);
        if (_655_subfields.getIndicator1() != ' ' or _655_subfields.getIndicator2() != '7')
            continue;
        if (not _655_subfields.hasSubfieldWithValue('a', "Aufsatzsammlung"))
            continue;

        const std::string _264_contents(record.getFieldData("264"));
        if (_264_contents.empty())
            continue;
        const Subfields _264_subfields(_264_contents);
        if (not _264_subfields.hasSubfield('c'))
            continue;
        if (not year_reg_exp.match(_264_subfields.getFirstSubfieldValue('c')))
            continue;
        const std::string year(year_reg_exp.getMatchedSubstring(1));

        const std::string _856_contents(record.getFieldData("856"));
        if (_856_contents.empty())
            continue;
        const Subfields _856_subfields(_856_contents);
        if (unlikely(not _856_subfields.hasSubfield('u'))
            or not _856_subfields.hasSubfieldWithValue('3', "Inhaltsverzeichnis"))
            continue;
        const std::string url(_856_subfields.getFirstSubfieldValue('u'));
        std::string pdf_url;
        if (StringUtil::StartsWith(url, "http://swbplus.bsz-bw.de/bsz") and StringUtil::EndsWith(url, ".htm"))
            pdf_url = url.substr(0, url.length() - 3) + "pdf";
        else if (StringUtil::StartsWith(url, "http://d-nb.info/"))
            pdf_url = url;
        if (pdf_url.empty()) {
            std::cout << "Bad URL: " << url << '\n';
            ++unhandled_url_count;
            continue;
        }

        // Classify the hits by year:
        if (year < "2000")
            ++until1999_count;
        else if (year > "2009")
            ++after2009_count;
        else
            ++from2000_to_2009_count;

//        const std::string &control_number(record.getControlNumber());
        ++good_count;
    }

    std::cout << "Data set contains " << record_count << " MARC record(s).\n";
    std::cout << good_count << " records survived all conditions.\n";
    std::cout << "Didn't know how to handle " << unhandled_url_count << " URLs.\n";
    std::cout << until1999_count << " came before 2000, " << after2009_count << " after 2009, and "
              << from2000_to_2009_count << " inbetween.\n";
}


int main(int argc, char *argv[]) {
    ::progname = argv[0];

    if (argc != 2)
        Usage();

    std::unique_ptr<MarcReader> marc_reader(MarcReader::Factory(argv[1]));

    try {
        ProcessRecords(marc_reader.get());
    } catch (const std::exception &e) {
        Error("Caught exception: " + std::string(e.what()));
    }
}

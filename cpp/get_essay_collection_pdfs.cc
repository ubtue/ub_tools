/** \brief Utility for downloading PDFs of essay collections.
 *  \author Dr. Johannes Ruscheinski (johannes.ruscheinski@uni-tuebingen.de)
 *
 *  \copyright 2017 Universitätsbibliothek Tübingen.  All rights reserved.
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
#include "Downloader.h"
#include "FileUtil.h"
#include "MARC.h"
#include "MediaTypeUtil.h"
#include "PerlCompatRegExp.h"
#include "StringUtil.h"
#include "util.h"


static void Usage() __attribute__((noreturn));


static void Usage() {
    std::cerr << "Usage: " << ::progname << " [--download-pdfs limit_count] marc_data\n";
    std::exit(EXIT_FAILURE);
}


bool IsEssayCollection(const std::string &tag, const MARC::Record &record) {
    for (const std::string &value : record.getSubfieldValues(tag, 'a')) {
        if (value.find("Aufsatzsammlung") != std::string::npos)
            return true;
    }
    return false;
}


bool IsEssayCollection(const MARC::Record &record) {
    if (IsEssayCollection("650", record))
        return true;
    if (IsEssayCollection("655", record))
        return true;
    if (IsEssayCollection("689", record))
        return true;
    return false;
}


std::string GetTOC_URL(const MARC::Record &record) {
    for (const MARC::Record::Field &field : record.getTagRange("856")) {
        if (field.getContents().empty())
            continue;
        const MARC::Subfields subfields(field.getSubfields());
        if (likely(subfields.hasSubfield('u')) and subfields.hasSubfieldWithValue('3', "Inhaltsverzeichnis"))
            return subfields.getFirstSubfieldWithCode('u');
    }

    return ""; // Found no TOC URL.
}


std::string GetYear(const std::string &tag, const MARC::Record &record) {
    static PerlCompatRegExp year_reg_exp(PerlCompatRegExp("(\\d\\d\\d\\d)"));
    const std::string field_contents(record.getFirstFieldContents(tag));
    if (field_contents.empty())
        return "";
    const MARC::Subfields subfields(field_contents);
    if (not subfields.hasSubfield('c'))
        return "";
    if (not year_reg_exp.match(subfields.getFirstSubfieldWithCode('c')))
        return "";
    return year_reg_exp.getMatchedSubstring(1);
}


std::string GetYear(const MARC::Record &record) {
    std::string year(GetYear("264", record));
    if (year.empty())
        return GetYear("260", record);
    return year;
}


void ProcessRecords(MARC::Reader * const marc_reader, const unsigned pdf_limit_count) {
    unsigned record_count(0), until1999_count(0), from2000_to_2009_count(0), after2009_count(0), unhandled_url_count(0), good_count(0),
        pdf_success_count(0);
    while (const MARC::Record record = marc_reader->read()) {
        ++record_count;

        if (not IsEssayCollection(record))
            continue;

        const std::string year(GetYear(record));
        if (year.empty())
            continue;

        const std::string url(GetTOC_URL(record));
        if (url.empty())
            continue;

        std::string pdf_url;
        if (StringUtil::StartsWith(url, "http://swbplus.bsz-bw.de/bsz") and StringUtil::EndsWith(url, ".htm"))
            pdf_url = url.substr(0, url.length() - 3) + "pdf";
        else if (StringUtil::StartsWith(url, "http://d-nb.info/"))
            pdf_url = url;
        else if (StringUtil::StartsWith(url, "http://digitool.hbz-nrw.de:1801/webclient/DeliveryManager?pid=")) {
            pdf_url = url;
            const size_t first_ampersand_pos(pdf_url.find('&'));
            if (first_ampersand_pos != std::string::npos)
                pdf_url.resize(first_ampersand_pos);
        } else if (StringUtil::EndsWith(url, ".pdf") or StringUtil::EndsWith(url, ".PDF"))
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

        ++good_count;

        if (pdf_success_count < pdf_limit_count) {
            std::string document;
            if (Download(pdf_url, 10000 /* ms */, &document)) {
                const std::string media_type(MediaTypeUtil::GetMediaType(document));
                if (media_type != "application/pdf") {
                    std::cout << url << " has wrong media type: " << media_type << '\n';
                    continue;
                }

                const std::string &control_number(record.getControlNumber());
                const std::string output_filename(control_number + ".pdf");
                if (not FileUtil::WriteString(output_filename, document))
                    logger->error("failed to write \"" + output_filename + "\"!");
                ++pdf_success_count;
            }
        }
    }

    std::cout << "Data set contains " << record_count << " MARC record(s).\n";
    std::cout << good_count << " records survived all conditions and " << pdf_success_count << " PDFs were sucessfully downloaded.\n";
    std::cout << "Didn't know how to handle " << unhandled_url_count << " URLs.\n";
    std::cout << until1999_count << " came before 2000, " << after2009_count << " after 2009, and " << from2000_to_2009_count
              << " inbetween.\n";
}


int main(int argc, char *argv[]) {
    ::progname = argv[0];

    if (argc != 2 and argc != 4)
        Usage();

    unsigned pdf_limit_count;
    if (argc == 4) {
        if (std::strcmp(argv[1], "--download-pdfs") != 0)
            Usage();
        if (not StringUtil::ToUnsigned(argv[2], &pdf_limit_count))
            logger->error(std::string(argv[2]) + " is not a valid PDF limit count!");
    } else
        pdf_limit_count = 0;

    std::unique_ptr<MARC::Reader> marc_reader(MARC::Reader::Factory(argv[argc == 2 ? 1 : 3]));

    try {
        ProcessRecords(marc_reader.get(), pdf_limit_count);
    } catch (const std::exception &e) {
        logger->error("Caught exception: " + std::string(e.what()));
    }
}

/** \file job_grep.cc
 *  \brief jop_grep is a command-line utility for the extration of JOP-relevant field and subfield values from
 *         MARC-21 records.
 *
 *  \author Dr. Johannes Ruscheinski (johannes.ruscheinski@uni-tuebingen.de)
 *
 *  \copyright 2015-2017 Universitätsbibliothek Tübingen.  All rights reserved.
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
#include <memory>
#include <unordered_set>
#include <climits>
#include <cstdlib>
#include <cstring>
#include <getopt.h>
#include "MARC.h"
#include "StringUtil.h"
#include "util.h"


void Usage() {
    std::cerr << "Usage: " << progname << " input_filename [optional_max_result_count]\n";
    std::exit(EXIT_FAILURE);
}


void JOP_Grep(const std::string &input_filename, const unsigned max_result_count) {
    std::unique_ptr<MARC::Reader> marc_reader(MARC::Reader::Factory(input_filename, MARC::FileType::BINARY));

    unsigned count(0), result_count(0);
    while (const MARC::Record record = marc_reader->read()) {
        ++count;

        const bool is_article(record.isArticle());
        const bool is_serial(record.isSerial());
        if (not is_article and not is_serial)
            continue;

        std::string isbn, issn;
        for (const auto &field : record) {
            const MARC::Tag tag(field.getTag());
            if (tag == "020" or tag == "022") {
                const std::string a_value(field.getFirstSubfieldWithCode('a'));
                if (not a_value.empty()) {
                    if (tag == "020")
                        isbn = a_value;
                    else // Assume tag == "022".
                        issn = a_value;
                }
            } else if (is_article and tag == "773") {
                const std::string x_value(field.getFirstSubfieldWithCode('x'));
                if (not x_value.empty())
                    issn = x_value;
                const std::string z_value(field.getFirstSubfieldWithCode('z'));
                if (not z_value.empty())
                    isbn = z_value;
            }

            if (not issn.empty() or not isbn.empty()) {
                ++result_count;
                if (result_count > max_result_count)
                    goto done;
                break;
            }
        }

        if (not issn.empty())
            std::cout << (is_serial ? "journal" : "article") << ", ISSN: " << issn << '\n';
        else if (not isbn.empty())
            std::cout << (is_serial ? "journal" : "article") << ", ISBN: " << isbn << '\n';
    }

done:
    std::cerr << "Matched " << result_count << " records of " << count << " overall records.\n";
}


int main(int argc, char **argv) {
    progname = argv[0];

    if (argc != 2 and argc != 3)
        Usage();

    unsigned max_result_count(UINT_MAX);
    if (argc == 3) {
        if (not StringUtil::ToUnsigned(argv[2], &max_result_count) or max_result_count == 0)
            Usage();
    }

    try {
        JOP_Grep(argv[1], max_result_count);
    } catch (const std::exception &x) {
        logger->error("caught exception: " + std::string(x.what()));
    }
}

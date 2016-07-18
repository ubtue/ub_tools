/** \file make_marc_xml.cc
 *  \brief Converts XML blobs downloaded from the BSZ into proper MARC-XML records.
 *  \author Dr. Johannes Ruscheinski (johannes.ruscheinski@uni-tuebingen.de)
 *
 *  \copyright 2016 Universitätsbiblothek Tübingen.  All rights reserved.
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
#include <unordered_map>
#include "Compiler.h"
#include "FileUtil.h"
#include "util.h"


void Usage() {
    std::cerr << "usage: " << progname << " [--append] input_blob output_marc_xml\n";
    std::exit(EXIT_FAILURE);
}


const std::unordered_map<std::string, std::string> from_to{
    { "<record>", "<marc:record>" },
    { "</record>", "</marc:record>" },
    { "<leader>", "<marc:leader>" },
    { "</leader>", "</marc:leader>" },
    { "<controlfield", "<marc:controlfield" },
    { "</controlfield>", "</marc:controlfield>" },
    { "<datafield", "<marc:datafield" },
    { "</datafield>", "</marc:datafield>" },
    { "<subfield", "<marc:subfield" },
    { "</subfield>", "</marc:subfield>" },
};


std::string GetNextToken(File * const input) {
    std::string token;

    const int first_ch(input->get());
    if (first_ch == EOF)
        return token; // Empty string signal EOF.

    token += static_cast<char>(first_ch);
    if (likely(first_ch != '<'))
        return token;

    int ch;
    while ((ch = input->get()) != EOF and (islower(ch) or ch == '>' or ch == '/'))
        token += static_cast<char>(ch);
    if (likely(ch != EOF))
        input->putback(static_cast<char>(ch));

    return token;
}


void Convert(File * const input, File * const output) {
    bool converting(false);

    std::string token(GetNextToken(input));
    bool leader_open_seen(false); // We only like to see one of these.
    while (not token.empty()) {
        if (token == "<record>")
            converting = true;

        if (converting) {
            if (token == "</record>")
                converting = false;
            if (token == "<leader>") {
                if (leader_open_seen) {
                    (*output) << "</marc:record>\n";
                    return;
                }
                leader_open_seen = true;
            }
            const auto pair(from_to.find(token));
            (*output) << (pair == from_to.cend() ? token : pair->second);
        }

        token = GetNextToken(input);
    }
}


int main(int argc, char *argv[]) {
    ::progname = argv[0];

    if (argc < 2)
        Usage();

    bool append(false);
    if (std::strcmp("--append", argv[1]) == 0) {
        append = true;
        --argc, ++argv;
    }

    if (argc != 3)
        Usage();

    const std::unique_ptr<File> input(FileUtil::OpenInputFileOrDie(argv[1]));
    const std::unique_ptr<File> output(append ? FileUtil::OpenForAppeningOrDie(argv[2])
                                              : FileUtil::OpenOutputFileOrDie(argv[2]));

    try {
        Convert(input.get(), output.get());
    } catch (const std::exception &x) {
        Error("caught exception: " + std::string(x.what()));
    }
}


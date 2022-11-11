/** \brief Wrapper for SWB GND author lookup for theological authors
 *  \author Johannes Riedl
 *
 *  \copyright 2022 Universitätsbibliothek Tübingen.  All rights reserved.
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
#include "BSZUtil.h"
#include "HtmlUtil.h"
#include "util.h"


namespace {

const std::string author_swb_lookup_url(
    "https://swb.bsz-bw.de/DB=2.104/SET=70/TTL=1/"
    "CMD?SGE=&ACT=SRCHM&MATCFILTER=Y&MATCSET=Y&NOSCAN=Y&PARSE_MNEMONICS=N&PARSE_OPWORDS=N&PARSE_OLDSETS=N&IMPLAND=Y&NOABS=Y&ACT0=SRCHA&"
    "SHRTST=50&IKT0=3040&ACT1=*&IKT1=2057&TRM1=*&ACT2=*&IKT2=8991&TRM2=(theolog*|neutestament*|alttestament*|kirchenhist*|evangelisch*|"
    "religions*|pädagog*)&"
    "ACT3=-&IKT3=8991&TRM3=1[0%2C1%2C2%2C3%2C4%2C5%2C6%2C7%2C8][0%2C1%2C2%2C3%2C4%2C5%2C6%2C7%2C8%2C9][0%2C1%2C2%2C3%2C4%2C5%2C6%2C7%2C8%"
    "2C9]&TRM0=");


[[noreturn]] void Usage() {
    ::Usage("author");
}

std::string LookupAuthor(const std::string &author) {
    const std::string gnd_number(HtmlUtil::StripHtmlTags(BSZUtil::GetAuthorGNDNumber(author, author_swb_lookup_url)));
    return gnd_number;
}


} // end unnamed namespace


int Main(int argc, char *argv[]) {
    if (argc != 2)
        Usage();
    std::string author(argv[1]);
    // Make sure, we have space after comma. Otherwise results do not match
    std::vector<std::string> author_parts;
    StringUtil::SplitThenTrimWhite(author, ',', &author_parts);
    author = StringUtil::Join(author_parts, ", ");
    const std::string gnd_number(LookupAuthor('"' + author + '"'));
    if (gnd_number.empty()) {
        LOG_WARNING("Unable to determine GND for author \"" + author + "\"");
        return EXIT_FAILURE;
    }
    std::cout << gnd_number << '\n';
    return EXIT_SUCCESS;
}

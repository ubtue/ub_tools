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

const std::string author_swb_lookup_url_sloppy(
    "https://swb.bsz-bw.de/DB=2.104/SET=70/TTL=1/"
    "CMD?SGE=&ACT=SRCHM&MATCFILTER=Y&MATCSET=Y&NOSCAN=Y&PARSE_MNEMONICS=N&PARSE_OPWORDS=N&PARSE_OLDSETS=N&IMPLAND=Y&NOABS=Y&ACT0=SRCHA&"
    "SHRTST=50&IKT0=3040&ACT1=*&IKT1=2057&TRM1=*&ACT2=*&IKT2=8991&"
    "ACT3=-&IKT3=8991&TRM3=1[0%2C1%2C2%2C3%2C4%2C5%2C6%2C7%2C8][0%2C1%2C2%2C3%2C4%2C5%2C6%2C7%2C8%2C9][0%2C1%2C2%2C3%2C4%2C5%2C6%2C7%2C8%"
    "2C9]&TRM0=");


const std::string author_swb_lookup_url_bibwiss_ixtheo(
    "https://swb.bsz-bw.de/DB=2.104/SET=70/TTL=1/"
    "CMD?SGE=&ACT=SRCHM&MATCFILTER=Y&MATCSET=Y&NOSCAN=Y&PARSE_MNEMONICS=N&PARSE_OPWORDS=N&PARSE_OLDSETS=N&IMPLAND=Y&NOABS=Y&ACT0=SRCHA&"
    "SHRTST=50&IKT0=3040&ACT1=*&IKT1=2057&TRM1=*&ACT2=*&IKT2=8991&"
    "ACT3=-&IKT3=8991&"
    "TRM2=(theolog*|neutestament*|alttestament*|kirchenhist*|evangelisch*|"
    "religions*|pädagog*)&"
    "TRM3=1[0%2C1%2C2%2C3%2C4%2C5%2C6%2C7%2C8][0%2C1%2C2%2C3%2C4%2C5%2C6%2C7%2C8%2C9][0%2C1%2C2%2C3%2C4%2C5%2C6%2C7%2C8%"
    "2C9]&TRM0=");

const std::string author_swb_lookup_url_krimdok(
    "https://swb.bsz-bw.de/DB=2.104/SET=70/TTL=1/"
    "CMD?SGE=&ACT=SRCHM&MATCFILTER=Y&MATCSET=Y&NOSCAN=Y&PARSE_MNEMONICS=N&PARSE_OPWORDS=N&PARSE_OLDSETS=N&IMPLAND=Y&NOABS=Y&ACT0=SRCHA&"
    "SHRTST=50&IKT0=3040&ACT1=-&IKT1=8991&TRM1=1[0%2C1%2C2%2C3%2C4%2C5%2C6%2C7%2C8][0%2C1%2C2%2C3%2C4%2C5%2C6%2C7%2C8%2C9]"
    "[0%2C1%2C2%2C3%2C4%2C5%2C6%2C7%2C8%2C9]&TRM0=");

const std::string author_swb_lookup_url_no_restrictions(
    "https://swb.bsz-bw.de/DB=2.104/SET=1/TTL=1/"
    "CMD?RETRACE=0&TRM_OLD=&ACT=SRCHA&IKT=1&SRT=RLV&"
    "&MATCFILTER=N&MATCSET=N&NOABS=Y&SHRTST=50&TRM="

);

[[noreturn]] void Usage() {
    ::Usage("[--sloppy-filter|--krimdok|--no-restrictions] [--all-matches] author");
}

enum author_lookup_filter { SLOPPY, BIBWISS_IXTHEO, KRIMDOK, NO_RESTRICTIONS };


std::string LookupAuthor(const std::string &author, author_lookup_filter filter, const bool &all_matches) {
    std::string author_swb_lookup_url;
    switch (filter) {
    case SLOPPY:
        author_swb_lookup_url = author_swb_lookup_url_sloppy;
        break;
    case BIBWISS_IXTHEO:
        author_swb_lookup_url = author_swb_lookup_url_bibwiss_ixtheo;
        break;
    case KRIMDOK:
        author_swb_lookup_url = author_swb_lookup_url_krimdok;
        break;
    case NO_RESTRICTIONS:
        author_swb_lookup_url = author_swb_lookup_url_no_restrictions;
        break;
    default:
        LOG_ERROR("Unknown lookup filter");
    }

    if (not all_matches) {
        const std::string gnd_number(HtmlUtil::StripHtmlTags(BSZUtil::GetAuthorGNDNumber(author, author_swb_lookup_url)));
        return gnd_number;
    }

    const std::string all_gnd_numbers(HtmlUtil::StripHtmlTags(BSZUtil::GetAllAuthorGNDNumberCandidates(author, author_swb_lookup_url)));
    return all_gnd_numbers;
}


} // end unnamed namespace


int Main(int argc, char *argv[]) {
    if (argc < 2 or argc > 5)
        Usage();


    author_lookup_filter filter(BIBWISS_IXTHEO);

    if (std::strcmp(argv[1], "--sloppy-filter") == 0) {
        filter = SLOPPY;
        ++argv;
        --argc;
    }

    if (std::strcmp(argv[1], "--krimdok-filter") == 0) {
        filter = KRIMDOK;
        ++argv;
        --argc;
    }

    if (std::strcmp(argv[1], "--no-restrictions") == 0) {
        filter = NO_RESTRICTIONS;
        ++argv;
        --argc;
    }

    bool all_matches(false); /*Get all GND numbers for a name */
    if (std::strcmp(argv[1], "--all-matches") == 0) {
        all_matches = true;
        ++argv;
        --argc;
    }


    std::string author(argv[1]);
    // Make sure, we have space after comma. Otherwise results do not match
    std::vector<std::string> author_parts;
    StringUtil::SplitThenTrimWhite(author, ',', &author_parts);
    author = StringUtil::Join(author_parts, ", ");
    const std::string gnd_number_or_numbers(LookupAuthor('"' + author + '"', filter, all_matches));
    if (gnd_number_or_numbers.empty()) {
        LOG_WARNING("Unable to determine GND for author \"" + author + "\"");
        return EXIT_FAILURE;
    }
    std::cout << gnd_number_or_numbers << '\n';
    return EXIT_SUCCESS;
}

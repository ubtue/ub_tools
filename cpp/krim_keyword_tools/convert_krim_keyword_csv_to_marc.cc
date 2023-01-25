/** \file    convert_krim_keyword_csv_to_marc.cc
 *  \brief   Convert the manually crafted CSV converted Excel sheet of relevant
 *           KrimDok keywords to a marc authority file to be processed
 *           by the standard translation tool import machinery
 *           (e.g. extract_keywords_for_translation)
 *  \author  Johannes Riedl
 */

/*
    Copyright (C) 2023 Library of the University of Tübingen

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU Affero General Public License as
    published by the Free Software Foundation, either version 3 of the
    License, or (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Affero General Public License for more details.

    You should have received a copy of the GNU Affero General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/


#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <tuple>
#include <unordered_set>
#include <utility>
#include <vector>
#include <cstdlib>
#include "Compiler.h"
#include "DbConnection.h"
#include "IniFile.h"
#include "MARC.h"
#include "StringUtil.h"
#include "TextUtil.h"
#include "TimeUtil.h"
#include "TranslationUtil.h"
#include "UBTools.h"
#include "util.h"

[[noreturn]] void Usage() {
    ::Usage("krim_keyword_csv_export marc_out");
    std::exit(EXIT_FAILURE);
}

enum column_name {
    BEGRIFF = 2,
    HÄUFIGKEIT,
    PPN,
    GND_aut,
    GND_man,
    ÜBERBEGRIFF,
    PICA3,
    WÖRTERTRENNUNG_1,
    WÖRTERTRENNUNG_2,
    WÖRTERTRENNUNG_3,
    ALTERNATIVE_1,
    ALTERNATIVE_2,
    ALTERNATIVE_3,
    ALTERNATIVE_4,
    ALTERNATIVE_5,
    ALTERNATIVE_6,
    ALTERNATIVE_7,
    ALTERNATIVE_8,
    ALTERNATIVE_9,
    ALTERNATIVE_10
};

const std::string PSEUDO_PPN_PREFIX("KRI");
const std::string PSEUDO_PPN_SIGIL("KRIM");


namespace {

void GetCSVEntries(const std::string &csv_file, std::vector<std::vector<std::string>> * const lines) {
    TextUtil::ParseCSVFileOrDie(csv_file, lines);
    // Needed since ParseCSVFileOrDie() cannot cope with empty fields at the end
    auto has_more_columns = [](const std::vector<std::string> &a, const std::vector<std::string> &b) { return a.size() < b.size(); };
    auto max_columns_element(std::max_element(lines->begin(), lines->end(), has_more_columns));
    unsigned max_columns(max_columns_element->size());
    for (auto &line : *lines)
        line.resize(max_columns);
}

std::string GetPPN(const std::string &csv_ppn) {
    static unsigned pseudo_ppn_index(0);
    if (not csv_ppn.empty())
        return csv_ppn;

    std::ostringstream pseudo_ppn;
    pseudo_ppn << PSEUDO_PPN_PREFIX << std::setfill('0') << std::setw(7) << ++pseudo_ppn_index;
    return pseudo_ppn.str();
}

bool IsPriorityEntry(const std::vector<std::string> &line) {
    if (line[HÄUFIGKEIT].empty())
        return false;
    unsigned freq(std::atoi(line[HÄUFIGKEIT].c_str()));
    return freq >= 10; /* criterion defined by the criminologians*/
}

} // end unnamed namespace


int Main(int argc, char **argv) {
    if (argc != 3)
        Usage();

    std::vector<std::vector<std::string>> lines;
    GetCSVEntries(argv[1], &lines);

    // Skip column names in first line
    lines.erase(lines.begin());
    std::unique_ptr<MARC::Writer> authority_marc_writer(MARC::Writer::Factory(argv[2], MARC::FileType::BINARY));
    unsigned generated_records(0);

    for (auto &line : lines) {
        if (line[BEGRIFF].empty())
            continue;

        const std::string ppn(GetPPN(line[PPN]));
        MARC::Record *new_record = new MARC::Record("00000nz  a2210000n  4500");
        new_record->insertField("001", ppn);
        const std::string sigil(StringUtil::StartsWith(ppn, PSEUDO_PPN_PREFIX) ? PSEUDO_PPN_SIGIL : "DE-627");
        new_record->insertField("003", sigil);
        new_record->insertField("005", TimeUtil::GetCurrentDateAndTime("%Y%m%d%H%M%S") + ".0");
        new_record->insertField("008", TimeUtil::GetCurrentDateAndTime("%y%m%d") + "n||azznnabbn           | ana    |c");
        std::string gnd(not line[GND_man].empty() ? line[GND_man] : line[GND_aut]);
        if (not gnd.empty())
            new_record->insertField("024", { { 'a', "http://d-nb.info/gnd/" + gnd }, { '2', "uri" } });
        new_record->insertField("035", 'a', "(" + sigil + ")" + ppn);
        new_record->insertField("035", 'a', not gnd.empty() ? "(DE-588)" + gnd : "(KRIM)" + ppn);

        new_record->insertField("150", 'a', line[BEGRIFF]);
        for (auto alternative = line.begin() + ALTERNATIVE_1; alternative <= line.begin() + ALTERNATIVE_10; ++alternative) {
            if (alternative->empty())
                continue;
            new_record->insertField("450", 'a', *alternative);
        }
        if (IsPriorityEntry(line))
            new_record->insertField("PRI", 'a', "1");

        authority_marc_writer->write(*new_record);
        ++generated_records;
    }

    std::cerr << "Generated " << generated_records << " MARC records\n";

    return EXIT_SUCCESS;
}

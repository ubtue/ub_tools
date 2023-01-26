/** \file    convert_ezw_to_marc
 *  \brief   Convert fixed CSV-Input for EZW reference work to MARC
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

enum column_name { TITLE, AUTHOR, URL_REFERENCE, DESCRIPTION };

const std::string PSEUDO_PPN_PREFIX("EZW");
const std::string EZW_BASE_URL("https://www.ezw-berlin.de/");

namespace {

[[noreturn]] void Usage() {
    ::Usage("ezw.csv marc_output");
}

void GetCSVEntries(const std::string &csv_file, std::vector<std::vector<std::string>> * const lines) {
    TextUtil::ParseCSVFileOrDie(csv_file, lines);
    // Needed since ParseCSVFileOrDie() cannot cope with empty fields at the end
    auto has_more_columns = [](const std::vector<std::string> &a, const std::vector<std::string> &b) { return a.size() < b.size(); };
    auto max_columns_element(std::max_element(lines->begin(), lines->end(), has_more_columns));
    unsigned max_columns(max_columns_element->size());
    for (auto &line : *lines)
        line.resize(max_columns);
}

std::string GetPPN(const std::string &csv_ppn = "") {
    static unsigned pseudo_ppn_index(0);
    if (not csv_ppn.empty())
        return csv_ppn;

    std::ostringstream pseudo_ppn;
    pseudo_ppn << PSEUDO_PPN_PREFIX << std::setfill('0') << std::setw(7) << ++pseudo_ppn_index;
    return pseudo_ppn.str();
}


MARC::Record *CreateNewRecord() {
    return new MARC::Record(MARC::Record::TypeOfRecord::LANGUAGE_MATERIAL, MARC::Record::BibliographicLevel::SERIAL_COMPONENT_PART,
                            GetPPN());
}


MARC::Subfields GetSuperiorWorkDescription(const std::string &publication_year) {
    return MARC::Subfields({ { 'i', "Enhalten in" },
                             { 't', "Lexikon für Religion und Weltanschauungen" },
                             { 'd', "Berlin : Evangelische Zentralstelle für Weltanschauungsfragen, 2014" },
                             { 'g', publication_year },
                             { 'h', "Online-Ressource" },
                             { 'w', "(DE-627)779918967" },
                             { 'w', "(DE-576)401993256" } });
}


void InsertCreationDates(MARC::Record * const record, const std::string &year) {
    if (not year.empty())
        record->insertField("936", { { 'j', year } }, 'u', 'w');
    record->insertField("264", { { 'c', year } });
}


void InsertAuthor(MARC::Record * const record, const std::string &data) {
    if (data.length())
        record->insertField("100", { { 'a', data }, { '4', "aut" }, { 'e', "VerfasserIn" } });
    else
        LOG_WARNING("No author for " + record->getControlNumber());
}


void InsertTitle(MARC::Record * const record, const std::string &data) {
    if (data.length())
        record->insertField("245", { { 'a', data } });
    else
        LOG_WARNING("No title for " + record->getControlNumber());
}


void InsertURL(MARC::Record * const record, const std::string &data) {
    if (data.length())
        record->insertField("856", { { 'u', EZW_BASE_URL + data }, { 'z', "LF" } }, '4', '0');
    else
        LOG_WARNING("No URL for " + record->getControlNumber());
}


void InsertAbstract(MARC::Record * const record, const std::string &data) {
    if (data.length())
        record->insertField("520", { { 'a', data } });
    else
        LOG_WARNING("No abstract for " + record->getControlNumber());
}


} // end unnamed namespace


int Main(int argc, char **argv) {
    if (argc != 3)
        Usage();

    std::vector<std::vector<std::string>> lines;
    GetCSVEntries(argv[1], &lines);
    std::unique_ptr<MARC::Writer> marc_writer(MARC::Writer::Factory(argv[2]));
    unsigned generated_records(0);

    for (auto &line : lines) {
        MARC::Record *new_record = CreateNewRecord();
        new_record->insertField("003", "DE-Tue135");
        new_record->insertField("005", TimeUtil::GetCurrentDateAndTime("%Y%m%d%H%M%S") + ".0");
        new_record->insertField("007", "cr|||||");
        new_record->insertField("041", { { 'a', "ger" } });
        new_record->insertField("084", { { 'a', "1" }, { '2', "ssgn" } });
        new_record->insertField("084", { { 'a', "0" }, { '2', "ssgn" } });
        InsertAuthor(new_record, line[AUTHOR]);
        InsertTitle(new_record, line[TITLE]);
        InsertAbstract(new_record, line[DESCRIPTION]);
        InsertCreationDates(new_record, "XXXX");
        new_record->insertField("773", GetSuperiorWorkDescription("XXXX"), '0', '8');
        InsertURL(new_record, line[URL_REFERENCE]);
        new_record->insertField("TYP", { { 'a', "EZW" } });
        marc_writer->write(*new_record);
        ++generated_records;
    }

    std::cerr << "Generated " << generated_records << " MARC records\n";

    return EXIT_SUCCESS;
}

/** \file    convert_de_gruyter_csv_to_marc
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
#include "ExecUtil.h"
#include "MARC.h"
#include "StringUtil.h"
#include "TextUtil.h"
#include "TimeUtil.h"
#include "TranslationUtil.h"
#include "UBTools.h"
#include "util.h"


namespace {

[[noreturn]] void Usage() {
    ::Usage("pseudo_ppn_prefix, degruyter_refwork.csv marc_output");
}


std::string GetPPN(const std::string &pseudo_ppn_prefix, const std::string &csv_ppn = "") {
    static unsigned pseudo_ppn_index(0);
    if (not csv_ppn.empty())
        return csv_ppn;

    const unsigned complete_ppn_length(10);
    std::ostringstream pseudo_ppn;
    pseudo_ppn << pseudo_ppn_prefix << std::setfill('0') << std::setw(complete_ppn_length - pseudo_ppn_prefix.length())
               << ++pseudo_ppn_index;
    return pseudo_ppn.str();
}


MARC::Record *CreateNewRecord(const std::string &prefix, const std::string &ppn = "") {
    return new MARC::Record(MARC::Record::TypeOfRecord::LANGUAGE_MATERIAL, MARC::Record::BibliographicLevel::SERIAL_COMPONENT_PART,
                            GetPPN(prefix, ppn));
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


void InsertAuthors(MARC::Record * const record, const std::string &author1, const std::string &author_etal) {
    if (author1.length()) {
        MARC::Subfields author_subfields({ { 'a', author1 }, { '4', "aut" }, { 'e', "VerfasserIn" } });
        record->insertField("100", author_subfields, '1');
    } else
        LOG_WARNING("No author for " + record->getControlNumber());

    if (author_etal.length()) {
        std::vector<std::string> further_authors;
        StringUtil::SplitThenTrim(author_etal, ";", "\n ", &further_authors);
        for (const std::string &further_author : further_authors) {
            if (not further_author.length())
                continue;
            MARC::Subfields further_author_subfields({ { 'a', further_author }, { '4', "aut" }, { 'e', "VerfasserIn" } });
            record->insertField("700", further_author_subfields, '1');
        }
    }
}


void InsertTitle(MARC::Record * const record, const std::string &data) {
    if (data.length())
        record->insertField("245", { { 'a', data } }, '1', '0');
    else
        LOG_WARNING("No title for " + record->getControlNumber());
}


void InsertCreationDates(MARC::Record * const record, const std::string &year) {
    if (not year.empty())
        record->insertField("264", { { 'c', year } }, ' ', '1');
}


void InsertDOI(MARC::Record * const record, const std::string &doi) {
    if (doi.empty())
        return;
    record->insertField("024", { { 'a', doi }, { '2', "doi" } }, '7');
    record->insertField("856", { { 'u', "https://doi.org/" + doi }, { 'z', "ZZ" } }, '4', '0');
}


void InsertURL(MARC::Record * const record, const std::string &data) {
    if (data.length())
        record->insertField("856", { { 'u', data }, { 'z', "ZZ" } }, '4', '0');
    else
        LOG_WARNING("No URL for " + record->getControlNumber());
}


void InsertReferenceHint(MARC::Record * const record, const std::string &data) {
    if (data.length())
        record->insertField("500", { { 'a', "Verweis auf \"" + data + "\"" } });
}


void InsertLanguage(MARC::Record * const record, const std::string &data) {
    if (not TranslationUtil::IsValidInternational2LetterCode(data))
        LOG_ERROR("Invalid language code \"" + data + "\"");
    const std::string german_language_code(TranslationUtil::MapInternational2LetterCodeToGerman3Or4LetterCode(data));
    const std::string language_code(TranslationUtil::MapGermanLanguageCodesToFake3LetterEnglishLanguagesCodes(german_language_code));
    record->insertField("041", { { 'a', language_code } });
}


void InsertVolume(MARC::Record * const record, const std::string &data) {
    if (not data.empty())
        record->insertField("VOL", { { 'a', data } });
}

void InsertKeywords(MARC::Record * const record, const std::string &data) {
    if (not data.empty()) {
        for (const auto &keyword : StringUtil::Split(data, ';', '\\', true))
            record->insertField("650", { { 'a', keyword } });
    }
}


std::string TestValidPseudoPPNPrefix(const std::string &prefix) {
    if (prefix.length() > 6)
        LOG_ERROR("prefix is too long (>6)");
    return prefix;
}


using column_names_to_offsets_map = std::map<std::string, unsigned>;
static column_names_to_offsets_map column_names_to_offsets;


void GenerateColumnOffsetMap(const std::string &columns_line) {
    std::vector<std::string> column_names;
    StringUtil::Split(columns_line, ',', &column_names);
    unsigned offset(0);
    for (const auto &column_name : column_names)
        column_names_to_offsets.emplace(StringUtil::ASCIIToLower(column_name), offset++);
}


unsigned GetColumnOffset(const std::string &column_name) {
    try {
        return column_names_to_offsets.at(StringUtil::ASCIIToLower(column_name));
    } catch (const std::exception) {
        LOG_ERROR("Invalid column \"" + column_name + "\"");
    }
}


bool HasColumn(const std::string &column_name) {
    return column_names_to_offsets.find(StringUtil::ASCIIToLower(column_name)) != column_names_to_offsets.end();
}


} // end unnamed namespace


int Main(int argc, char **argv) {
    if (argc != 4)
        Usage();

    const std::string pseudo_ppn_prefix(TestValidPseudoPPNPrefix(argv[1]));
    std::vector<std::vector<std::string>> lines;
    GetCSVEntries(argv[2], &lines);
    GenerateColumnOffsetMap(StringUtil::Join(lines[0], ','));
    lines.erase(lines.begin());

    std::unique_ptr<MARC::Writer> marc_writer(MARC::Writer::Factory(argv[3]));
    unsigned generated_records(0);

    for (const auto &line : lines) {
        MARC::Record *new_record = CreateNewRecord(pseudo_ppn_prefix, line[GetColumnOffset("BOOKPARTID")]);
        new_record->insertField("005", TimeUtil::GetCurrentDateAndTime("%Y%m%d%H%M%S") + ".0");
        new_record->insertField("007", "cr|||||");
        InsertAuthors(new_record, line[GetColumnOffset("AUTHOR1")], line[GetColumnOffset("AUTHOR-ETAL")]);
        InsertTitle(new_record, line[GetColumnOffset("TITLE")]);
        InsertDOI(new_record, line[GetColumnOffset("DOI")]);
        InsertLanguage(new_record, line[GetColumnOffset("LANG")]);
        InsertCreationDates(new_record, line[GetColumnOffset("EPUB")]);
        InsertURL(new_record, line[GetColumnOffset("URL")]);
        if (HasColumn("ZIELSTICHWORT"))
            InsertReferenceHint(new_record, line[GetColumnOffset("ZIELSTICHWORT")]);
        if (HasColumn("VOL"))
            InsertVolume(new_record, line[GetColumnOffset("VOL")]);
        if (HasColumn("SUBJECT-DG"))
            InsertKeywords(new_record, line[GetColumnOffset("SUBJECT-DG")]);
        new_record->insertField("TYP", { { 'a', pseudo_ppn_prefix } });
        marc_writer->write(*new_record);
        ++generated_records;
    }

    std::cerr << "Generated " << generated_records << " MARC records\n";

    return EXIT_SUCCESS;
}

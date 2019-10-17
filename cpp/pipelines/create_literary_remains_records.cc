/** \file    create_literary_remains_records.cc
 *  \author  Dr. Johannes Ruscheinski
 *
 *  A tool for creating literary remains MARC records from Beacon files.
 */

/*
    Copyright (C) 2019, Library of the University of Tübingen

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

#include <unordered_map>
#include <unordered_set>
#include "FileUtil.h"
#include "MARC.h"
#include "StringUtil.h"
#include "TimeUtil.h"
#include "util.h"


namespace {


// use marc reader/writer to copy instead of using FileUtil::Copy
// so we don't lose file handler (FIFO compatibility)
void CopyMarc(MARC::Reader * const reader, MARC::Writer * const writer) {
    while (auto record = reader->read())
        writer->write(record);
}


struct LiteraryRemainsInfo {
    std::string author_name_;
    std::string url_;
    std::string source_name_;
public:
    LiteraryRemainsInfo() = default;
    LiteraryRemainsInfo(const LiteraryRemainsInfo &other) = default;
    LiteraryRemainsInfo(const std::string &author_name, const std::string &url, const std::string &source_name)
        : author_name_(author_name), url_(url), source_name_(source_name) { }

    LiteraryRemainsInfo &operator=(const LiteraryRemainsInfo &rhs) = default;
};


void LoadAuthorGNDNumbers(
    const std::string &filename,
    std::unordered_map<std::string, std::vector<LiteraryRemainsInfo>> * const gnd_numbers_to_literary_remains_infos_map)
{
    auto reader(MARC::Reader::Factory(filename));

    unsigned total_count(0), references_count(0);
    while (auto record = reader->read()) {
        ++total_count;

        auto beacon_field(record.findTag("BEA"));
        if (beacon_field == record.end())
            continue;

        const auto _100_field(record.findTag("100"));
        if (_100_field == record.end() or not _100_field->hasSubfield('a'))
            continue;

        std::string gnd_number;
        if (not MARC::GetGNDCode(record, &gnd_number))
            continue;

        const auto subfield_c(_100_field->getFirstSubfieldWithCode('c'));
        const std::string author_name(not subfield_c.empty() ? _100_field->getFirstSubfieldWithCode('a') + " (" + subfield_c + ")"
                                                             : _100_field->getFirstSubfieldWithCode('a'));

        std::vector<LiteraryRemainsInfo> literary_remains_infos;
        while (beacon_field != record.end() and beacon_field->getTag() == "BEA") {
            literary_remains_infos.emplace_back(author_name, beacon_field->getFirstSubfieldWithCode('u'),
                                                beacon_field->getFirstSubfieldWithCode('a'));
            ++beacon_field;
        }
        (*gnd_numbers_to_literary_remains_infos_map)[gnd_number] = literary_remains_infos;
        references_count += literary_remains_infos.size();
    }

    LOG_INFO("Loaded " + std::to_string(references_count) + " literary remains references from \"" + filename
             + "\" which contained a total of " + std::to_string(total_count) + " records.");
}


std::string NormaliseAuthorName(std::string author_name) {
    const auto comma_pos(author_name.find(','));
    if (comma_pos == std::string::npos)
        return author_name;

    std::string auxillary_info;
    const auto open_paren_pos(author_name.find('('));
    if (open_paren_pos != std::string::npos) {
        auxillary_info = " " + author_name.substr(open_paren_pos);
        author_name.resize(open_paren_pos);
    }

    return StringUtil::TrimWhite(author_name.substr(comma_pos + 1)) + " " + StringUtil::TrimWhite(author_name.substr(0, comma_pos))
           + auxillary_info;
}


void AppendLiteraryRemainsRecords(
    MARC::Writer * const writer,
    const std::unordered_map<std::string, std::vector<LiteraryRemainsInfo>> &gnd_numbers_to_literary_remains_infos_map)
{
    unsigned creation_count(0);
    for (const auto &gnd_numbers_and_literary_remains_infos : gnd_numbers_to_literary_remains_infos_map) {
        MARC::Record new_record(MARC::Record::TypeOfRecord::MIXED_MATERIALS, MARC::Record::BibliographicLevel::COLLECTION,
                                "LR" + StringUtil::ToString(++creation_count, /* base = */10, /* width= */6, /* padding_char = */'0'));
        const std::string &author_name(gnd_numbers_and_literary_remains_infos.second.front().author_name_);
        new_record.insertField("003", "PipeLineGenerated");
        new_record.insertField("005", TimeUtil::GetCurrentDateAndTime("%Y%m%d%H%M%S") + ".0");
        new_record.insertField("008", "190606s2019    xx |||||      00| ||ger c");
        new_record.insertField("100", { { 'a', author_name }, { '0', "(DE-588)" + gnd_numbers_and_literary_remains_infos.first } });
        new_record.insertField("245", { { 'a', "Nachlass von " + NormaliseAuthorName(author_name) } });

        for (const auto &literary_remains_info : gnd_numbers_and_literary_remains_infos.second)
            new_record.insertField("856",
                                   { { 'u', literary_remains_info.url_ },
                                       { '3', "Nachlassdatenbank (" + literary_remains_info.source_name_ + ")" } });

        writer->write(new_record);
    }

    LOG_INFO("Appended a total of " + std::to_string(creation_count) + " record(s).");
}


} // unnamed namespace


int Main(int argc, char **argv) {
    if (argc != 4)
        ::Usage("marc_input marc_output authority_records");

    auto reader(MARC::Reader::Factory(argv[1]));
    auto writer(MARC::Writer::Factory(argv[2]));
    CopyMarc(reader.get(), writer.get());

    std::unordered_map<std::string, std::vector<LiteraryRemainsInfo>> gnd_numbers_to_literary_remains_infos_map;
    LoadAuthorGNDNumbers(argv[3], &gnd_numbers_to_literary_remains_infos_map);
    AppendLiteraryRemainsRecords(writer.get(), gnd_numbers_to_literary_remains_infos_map);

    return EXIT_SUCCESS;
}

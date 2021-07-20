/** \brief Utility for converting Dublin Core data downloaded from ICPSR to MARC.
 *  \author Dr. Johannes Ruscheinski (johannes.ruscheinski@uni-tuebingen.de)
 *
 *  \copyright 2021 Universitätsbibliothek Tübingen.  All rights reserved.
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

#include <cstdlib>
#include "MARC.h"
#include "StringUtil.h"
#include "TimeUtil.h"
#include "util.h"
#include "XMLParser.h"


bool IsCriminologyRecord(const MARC::Record &record) {
    for (const auto &field : record.getTagRange("653")) {
        if (StringUtil::StartsWith(field.getFirstSubfieldWithCode('a'), "crime"))
            return true;
    }

    return false;
}


// "american_date" is expected to contain the pattern mm-dd-yyyy.
std::string AmericanDateToGermanDate(const std::string &american_date) {
    const auto month(StringUtil::ToUnsigned(american_date.substr(0, 2)));
    if (unlikely(month == 0 or month > 12))
        LOG_ERROR("bad month in \"" + american_date + "\"!");

    const auto day(StringUtil::ToUnsigned(american_date.substr(3, 2)));
    if (unlikely(day == 0 or day > 31))
        LOG_ERROR("bad day in \"" + american_date + "\"!");

    const auto year(StringUtil::ToUnsigned(american_date.substr(6)));
    if (unlikely(year < 1000 or year > 2099))
        LOG_ERROR("bad year in \"" + american_date + "\"!");

    return StringUtil::ToString(day) + "." + StringUtil::ToString(month) + "." + StringUtil::ToString(year);
}


// Mostly uses the mapping found at https://www.loc.gov/marc/dccross.html to map DC to MARC.
bool ParseRecord(XMLParser * const xml_parser, MARC::Writer * const marc_writer) {
    static unsigned record_number;
    ++record_number;
    MARC::Record new_record(MARC::Record::TypeOfRecord::LANGUAGE_MATERIAL, MARC::Record::BibliographicLevel::UNDEFINED,
                            "ICPSR" + StringUtil::ToString(record_number, /* radix = */10, /* width = */6,
                                                           /* padding_char = */'0'));
    static const std::string today(TimeUtil::GetCurrentDateAndTime("%y%m%d"));
    static const std::string current_year(TimeUtil::GetCurrentYear());
    static const std::string _008_contents(today + 's' + current_year);
    new_record.insertField(MARC::Tag("008"), _008_contents);
    new_record.insertField(MARC::Tag("935"), { { 'a', "icpsr" }, { '2', "LOK" } });

    XMLParser::XMLPart xml_part;
    std::string last_data;
    while (xml_parser->getNext(&xml_part)) {
        if (xml_part.type_ == XMLParser::XMLPart::CHARACTERS)
            xml_part.data_.swap(last_data);
        else if (xml_part.type_ == XMLParser::XMLPart::CLOSING_TAG and StringUtil::StartsWith(xml_part.data_, "dc:")) {
            const std::string tag(xml_part.data_.substr(3));
            if (tag == "title")
                new_record.insertField(MARC::Tag("245"), 'a', last_data);
            else if (tag == "contributor")
                new_record.insertField(MARC::Tag("720"), 'a', last_data);
            else if (tag == "creator")
                new_record.insertField(MARC::Tag("720"), { { 'a', last_data}, { 'e', "author" } });
            else if (tag == "description")
                new_record.insertField(MARC::Tag("520"), 'a', last_data);
            else if (tag == "identifier") {
                if (StringUtil::StartsWith(last_data, "http://doi.org/")) {
                    new_record.insertField(MARC::Tag("024"),
                                           { { 'a', last_data.substr(__builtin_strlen("http://doi.org/")) }, { '2', "doi" } });
                    new_record.insertField(MARC::Tag("856"), 'u', last_data);
                }
            } else if (tag == "date")
                new_record.insertField(MARC::Tag("260"), 'c', AmericanDateToGermanDate(last_data));
            else if (tag == "type")
                new_record.insertField(MARC::Tag("655"), 'a', last_data, ' ', '7');
            else if (tag == "source")
                new_record.insertField(MARC::Tag("786"), 'n', last_data, '0', ' ');
            else if (tag == "coverage" or tag == "subject")
                new_record.insertField(MARC::Tag("653"), 'a', last_data);
            else
                LOG_ERROR("Unhandled tag: \"" + xml_part.data_ + "\"!");
        } else if (not StringUtil::StartsWith(xml_part.data_, "dc:")) {
            if (IsCriminologyRecord(new_record)) {
                marc_writer->write(new_record);
                return true;
            } else
                return false;
        }
    }

    if (IsCriminologyRecord(new_record)) {
        marc_writer->write(new_record);
        return true;
    } else
        return false;
}


int Main(int argc, char *argv[]) {
    if (argc != 3)
        ::Usage("dc_xml_imput marc_output");

    XMLParser xml_parser(argv[1], XMLParser::XML_FILE);
    const auto marc_writer(MARC::Writer::Factory(argv[2]));

    unsigned total_record_count(0), selected_record_count(0);
    while (xml_parser.skipTo(XMLParser::XMLPart::OPENING_TAG, "oai_dc:dc")) {
        ++total_record_count;
        if (ParseRecord(&xml_parser, marc_writer.get()))
            ++selected_record_count;
    }

    LOG_INFO("Processed " + std::to_string(total_record_count) + " record(s) of which "
             + std::to_string(selected_record_count) + " record(s) where selected and converted to MARC.");

    return EXIT_SUCCESS;
}

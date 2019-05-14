/** \file    tag_canon_law_and_bible_studies_records.cc
 *  \author  Dr. Johannes Ruscheinski
 *
 *  A tool for tagging entries that are relevant for bible studies and canon law.
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

#include <iostream>
#include <unordered_map>
#include <cstdlib>
#include "Compiler.h"
#include "FileUtil.h"
#include "MARC.h"
#include "StringUtil.h"
#include "util.h"


namespace {


// See https://github.com/ubtue/tuefind/wiki/Daten-Abzugskriterien#abzugskriterien-bibelwissenschaften for the documentation.
bool IsBibleStudiesRecord(const MARC::Record &record) {
    // 1. Abrufzeichen
    for (const auto &field : record.getTagRange("935")) {
        if (field.hasSubfieldWithValue('a', "BIIN"))
            return true;
    }

    // 2. IxTheo-Klassen
    for (const auto &field : record.getTagRange("LOK")) {
        if (field.hasSubfieldWithValue('0', "936ln")) {
            for (const auto &subfield : field.getSubfields()) {
                if (subfield.code_ == 'a' and likely(not subfield.value_.empty()) and subfield.value_[0] == 'H')
                    return true;
            }
        }
    }

    // 3. DDC Klassen
    for (const auto &field : record.getTagRange("082")) {
        if (field.getIndicator1() != ' ' or field.getIndicator2() != '0')
            continue;
        for (const auto &subfield : field.getSubfields()) {
            if (subfield.code_ == 'a' and StringUtil::StartsWith(subfield.value_, "22"))
                return true;
        }
    }

    // 4. RVK Klassen
    for (const auto &field : record.getTagRange("084")) {
        if (not field.hasSubfieldWithValue('2', "rvk"))
            continue;
        for (const auto &subfield : field.getSubfields()) {
            if (subfield.code_ == 'a' and StringUtil::StartsWith(subfield.value_, "BC"))
                return true;
        }
    }

    // 5. Basisklassifikation (BK)
    for (const auto &field : record.getTagRange("936")) {
        if (field.getIndicator1() != 'b' or field.getIndicator2() != 'k')
            continue;
        for (const auto &subfield : field.getSubfields()) {
            if (subfield.code_ == 'a'
                and (StringUtil::StartsWith(subfield.value_, "11.3") or StringUtil::StartsWith(subfield.value_, "11.4")))
                return true;
        }
    }

    // 6. Titel, die mit einem Normsatz verknüpft sind, der die GND Systematik enthält
    for (const auto &field : record.getTagRange("065")) {
        if (not field.hasSubfieldWithValue('2', "sswd"))
            continue;
        for (const auto &subfield : field.getSubfields()) {
            if (subfield.code_ == 'a' and StringUtil::StartsWith(subfield.value_, "3.2"))
                return true;
        }
    }

    // 7. SSG-Kennzeichen für den Alten Orient
    for (const auto &field : record.getTagRange("084")) {
        if (not field.hasSubfieldWithValue('2', "ssgn"))
            continue;
        for (const auto &subfield : field.getSubfields()) {
            if (subfield.code_ == 'a' and StringUtil::StartsWith(subfield.value_, "6,22"))
                return true;
        }
    }

    return false;
}


// See https://github.com/ubtue/tuefind/wiki/Daten-Abzugskriterien#abzugskriterien-bibelwissenschaften for the documentation.
bool IsCanonLawRecord(const MARC::Record &record) {
    // 1. Abrufzeichen
    for (const auto &field : record.getTagRange("935")) {
        if (field.hasSubfieldWithValue('a', "KALD"))
            return true;
    }

    // 2. IxTheo-Klassen
    for (const auto &field : record.getTagRange("LOK")) {
        if (field.hasSubfieldWithValue('0', "936ln")) {
            for (const auto &subfield : field.getSubfields()) {
                if (subfield.code_ == 'a' and likely(not subfield.value_.empty()) and subfield.value_[0] == 'S')
                    return true;
            }
        }
    }

    // 3. DDC Klassen
    for (const auto &field : record.getTagRange("082")) {
        if (field.getIndicator1() != ' ' or field.getIndicator2() != '0')
            continue;
        for (const auto &subfield : field.getSubfields()) {
            if (subfield.code_ == 'a'
                and (StringUtil::StartsWith(subfield.value_, "262.91") or StringUtil::StartsWith(subfield.value_, "262.92")
                     or StringUtil::StartsWith(subfield.value_, "262.93") or StringUtil::StartsWith(subfield.value_, "262.94")
                     or StringUtil::StartsWith(subfield.value_, "262.98")))
                return true;
        }
    }

    // 4. RVK Klassen
    for (const auto &field : record.getTagRange("084")) {
        if (not field.hasSubfieldWithValue('2', "rvk"))
            continue;
        for (const auto &subfield : field.getSubfields()) {
            if (subfield.code_ == 'a' and StringUtil::StartsWith(subfield.value_, "BR"))
                return true;
        }
    }

    // 5. Basisklassifikation (BK)
    for (const auto &field : record.getTagRange("936")) {
        if (field.getIndicator1() != 'b' or field.getIndicator2() != 'k')
            continue;
        for (const auto &subfield : field.getSubfields()) {
            if (subfield.code_ == 'a' and subfield.value_ == "86.97")
                return true;
        }
    }

    // 6. Titel, die mit einem Normsatz verknüpft sind, der die GND Systematik enthält
    for (const auto &field : record.getTagRange("065")) {
        if (not field.hasSubfieldWithValue('2', "sswd"))
            continue;
        for (const auto &subfield : field.getSubfields()) {
            if (subfield.code_ == 'a' and StringUtil::StartsWith(subfield.value_, "7.13"))
                return true;
        }
    }

    return false;
}


const std::string BIBLE_STUDIES_TAG("BIB");
const std::string CANON_LAW_TAG("CAN");


void ProcessRecords(MARC::Reader * const marc_reader, MARC::Writer * const marc_writer) {
    unsigned bible_studies_tag_count(0), canon_law_tag_count(0), tagged_both_count(0), total_record_count(0);
    while (MARC::Record record = marc_reader->read()) {
        ++total_record_count;

        const bool is_bible_studies_record(IsBibleStudiesRecord(record));
        if (is_bible_studies_record) {
            record.insertField(BIBLE_STUDIES_TAG, { { 'a', "1" } });
            ++bible_studies_tag_count;
        }

        if (IsCanonLawRecord(record)) {
            record.insertField(CANON_LAW_TAG, { { 'a', "1" } });
            ++canon_law_tag_count;
            if (is_bible_studies_record)
                ++tagged_both_count;
        }

        marc_writer->write(record);
    }

    LOG_INFO("processed a total of " + std::to_string(total_record_count) + " record(s).");
    LOG_INFO("tagged " + std::to_string(bible_studies_tag_count) + " record(s) as (a) bible studies record(s).");
    LOG_INFO("tagged " + std::to_string(canon_law_tag_count) + " record(s) as (a) canon law record(s).");
    LOG_INFO("dual-tagged " + std::to_string(tagged_both_count) + " record(s).");
}


} // unnamed namespace


int Main(int argc, char **argv) {
    if (argc != 3)
        ::Usage("marc_input marc_output\n"
                "Tags entries that are relevant for bible studies and canon law.");

    const std::string marc_input_filename(argv[1]);
    const std::string marc_output_filename(argv[2]);
    if (unlikely(marc_input_filename == marc_output_filename))
        LOG_ERROR("MARC input file name equals MARC output file name!");

    const auto marc_reader(MARC::Reader::Factory(marc_input_filename));
    const auto marc_writer(MARC::Writer::Factory(marc_output_filename));
    ProcessRecords(marc_reader.get(), marc_writer.get());

    return EXIT_SUCCESS;
}

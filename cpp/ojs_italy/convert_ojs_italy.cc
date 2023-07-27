/** \brief Convert (UNI-)MARC Files from external sources to our standard import format
 *
 *  \copyright 2023 Universitätsbibliothek Tübingen.  All rights reserved.
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

#include <algorithm>
#include <functional>
#include <iomanip>
#include <iostream>
#include <map>
#include <sstream>
#include "ExecUtil.h"
#include "FileUtil.h"
#include "MARC.h"
#include "util.h"

namespace {


[[noreturn]] void Usage() {
    ::Usage("marc_in map_file_path marc_out");
}


using ConversionFunctor = std::function<void(const std::string, const char, MARC::Record * const, const std::string)>;
const char SEPARATOR_CHAR('|');
enum OJSITALY_TYPES { STUDIA_PATAVINA, RIVISTA_SCIENCE_DELL_EDUCAZIONE };

struct MARCToMARCMapping {
    const std::string marc_in_tag_and_subfield_;
    const std::string marc_tag_;
    const char subfield_code_;
    std::function<void(MARC::Record * const, const std::string)> extraction_function_;
    MARCToMARCMapping(const std::string &marc_in_tag_and_subfield, const std::string marc_tag, const char subfield_code,
                      ConversionFunctor extraction_function)
        : marc_in_tag_and_subfield_(marc_in_tag_and_subfield), marc_tag_(marc_tag), subfield_code_(subfield_code),
          extraction_function_(std::bind(extraction_function, marc_tag, subfield_code, std::placeholders::_1, std::placeholders::_2)) { }
};


const auto MARCToMARCMappingComparator = [](const MARCToMARCMapping &lhs, const MARCToMARCMapping &rhs) {
    return lhs.marc_in_tag_and_subfield_ < rhs.marc_in_tag_and_subfield_;
};

using MARCToMARCMappingMultiset = std::multiset<MARCToMARCMapping, decltype(MARCToMARCMappingComparator)>;


MARC::Record *CreateNewRecord(const std::string id) {
    std::ostringstream formatted_number;
    formatted_number << std::setfill('0') << std::setw(8) << std::atoi(id.c_str());
    const std::string prefix("STP");
    const std::string ppn(prefix + formatted_number.str());
    return new MARC::Record(MARC::Record::TypeOfRecord::LANGUAGE_MATERIAL, MARC::Record::BibliographicLevel::SERIAL_COMPONENT_PART, ppn);
}


std::string ExtractPublicationYear(const std::string &published_at) {
    static ThreadSafeRegexMatcher date_matcher("((\\d{4})-\\d{2}-\\d{2})");
    if (const auto &match_result = date_matcher.match(published_at)) {
        return match_result[2];
    }
    return "";
}


void InsertField(const std::string &tag, const char subfield_code, MARC::Record * const record, const std::string &data) {
    if (data.length())
        record->insertField(tag, subfield_code, data);
}


void InsertAuthors(const std::string, const char, MARC::Record * const record, const std::string &data) {
    if (data.length()) {
        std::vector<std::string> authors;
        std::string author, further_parts;
        std::string data_to_split(data);
        while (StringUtil::SplitOnString(data_to_split, " and ", &author, &further_parts)) {
            authors.emplace_back(author);
            data_to_split = further_parts;
        }
        authors.emplace_back(data_to_split);


        record->insertField("100", { { 'a', authors[0] }, { '4', "aut" }, { 'e', "VerfasserIn" } });
        for (auto further_author = authors.begin() + 1; further_author != authors.end(); ++further_author)
            record->insertField("700", { { 'a', *further_author }, { '4', "aut" }, { 'e', "VerfasserIn" } });
    }
}


void InsertCreationField(const std::string &tag, const char, MARC::Record * const record, const std::string &data) {
    if (data.length()) {
        static ThreadSafeRegexMatcher date_matcher("((\\d{4})-\\d{2}-\\d{2})");
        if (const auto &match_result = date_matcher.match(data)) {
            if (match_result[1] == "0000-00-00")
                record->insertField(tag, "000101s2000    x |||||      00| ||ger c");
            else
                record->insertField(
                    tag, StringUtil::Filter(match_result[1], "-").substr(2) + "s" + match_result[2] + "    xx |||||      00| ||ger c");
            return;
        } else
            LOG_ERROR("Invalid date format \"" + data + "\"");
    }
    // Fallback with dummy data
    record->insertField(tag, "000101s2000    xx |||||      00| ||ger c");
}


void InsertOrForceSubfield(const std::string &tag, const char subfield_code, MARC::Record * const record, const std::string &data) {
    if (data.length()) {
        if (not record->hasTag(tag)) {
            InsertField(tag, subfield_code, record, data);
            return;
        }
        for (auto &field : record->getTagRange(tag)) {
            // FIXME: Do not necessarily replace
            field.insertOrReplaceSubfield(subfield_code, data);
        }
    }
}


void InsertCreationDates(const std::string, const char, MARC::Record * const record, const std::string &data) {
    if (data.length()) {
        const std::string year(ExtractPublicationYear(data));
        if (not year.empty())
            record->insertField("936", { { 'j', year } }, 'u', 'w');
        record->insertField("264", { { 'c', year } });
    }
}


void AppendAuthorFirstName(const std::string, const char, MARC::Record * const record, const std::string &data) {
    const std::string author_last_name_with_comma(record->getFirstSubfieldValue("100", 'a'));
    for (auto &field : record->getTagRange("100")) {
        auto _100_subfields(field.getSubfields());
        if (not _100_subfields.replaceFirstSubfield('a', author_last_name_with_comma + ' ' + data))
            LOG_WARNING("Skip adding \"" + data + "\" to author field, because author not yet present for PPN "
                        + record->getControlNumber());
        field = MARC::Record::Field("100", _100_subfields, field.getIndicator1(), field.getIndicator2());
    }
}


const std::map<std::string, ConversionFunctor> name_to_functor_map{ { "InsertField", InsertField },
                                                                    { "InsertCreationField", InsertCreationField },
                                                                    { "InsertCreationDates", InsertCreationDates },
                                                                    { "InsertAuthors", InsertAuthors },
                                                                    { "InsertOrForceSubfield", InsertOrForceSubfield },
                                                                    { "AppendAuthorFirstName", AppendAuthorFirstName } };


ConversionFunctor GetConversionFunctor(const std::string &functor_name) {
    if (not name_to_functor_map.contains(functor_name))
        LOG_ERROR("Unknown functor " + functor_name);
    return name_to_functor_map.find(functor_name)->second;
}


void ExtractTagAndSubfield(const std::string combined, std::string *tag, char *subfield_code) {
    bool is_no_subfield_tag(StringUtil::StartsWith(combined, "00"));
    if (combined.length() != 4 and not is_no_subfield_tag)
        LOG_ERROR("Invalid Tag and Subfield format " + combined);
    *tag = combined.substr(0, 3);
    *subfield_code = is_no_subfield_tag ? ' ' : combined[3];
}


void CreateMARCToMARCMappings(File * const map_file, MARCToMARCMappingMultiset * const marc_to_marc_mappings) {
    unsigned linenum(0);
    while (not map_file->eof()) {
        ++linenum;
        std::string line;
        map_file->getline(&line);
        StringUtil::Trim(&line);
        if (line[0] == '#')
            continue;
        std::vector<std::string> mapping;
        StringUtil::SplitThenTrim(line, SEPARATOR_CHAR, " \t", &mapping);
        if (unlikely(mapping.size() < 2 and line.back() != SEPARATOR_CHAR)) {
            LOG_WARNING("Invalid line format in line " + std::to_string(linenum));
            continue;
        }
        static ThreadSafeRegexMatcher tag_subfield_and_functorname("(?i)([a-z0-9]{3,4})\\s+\\((\\p{L}+)\\)\\s*");
        const std::vector<std::string> extraction_rules(mapping.begin() + 1, mapping.end());
        for (const auto &extraction_rule : extraction_rules) {
            std::string tag;
            char subfield_code;
            ConversionFunctor conversion_functor;
            if (const auto match_result = tag_subfield_and_functorname.match(extraction_rule)) {
                ExtractTagAndSubfield(match_result[1], &tag, &subfield_code);
                conversion_functor = GetConversionFunctor(match_result[2]);
            } else if (extraction_rule.length() >= 3 && extraction_rule.length() <= 4) {
                ExtractTagAndSubfield(extraction_rule, &tag, &subfield_code);
                conversion_functor = GetConversionFunctor("InsertField");
            } else
                LOG_ERROR("Invalid extraction rule: " + extraction_rule);

            marc_to_marc_mappings->emplace(MARCToMARCMapping(mapping[0], tag, subfield_code, conversion_functor));
        }
    }
}


MARC::Subfields GetSuperiorWorkDescription(enum OJSITALY_TYPES type, const std::string &publication_year) {
    switch (type) {
    case OJSITALY_TYPES::STUDIA_PATAVINA:
        return MARC::Subfields({ { 'i', "Enhalten in" },
                                 { 't', "Studia patavina" },
                                 { 'd', "Padova : Facoltà teologica del Triveneto, 1954" },
                                 { 'g', publication_year },
                                 { 'w', "(DE-627)166751685" },
                                 { 'w', "(DE-600)301088-0" },
                                 { 'w', "(DE-576)015186075" } });
    case OJSITALY_TYPES::RIVISTA_SCIENCE_DELL_EDUCAZIONE:
        return MARC::Subfields({ { 'i', "Enhalten in" },
                                 { 't', "Rivista di scienze dell'educazione" },
                                 { 'd', "Roma, 1973" },
                                 { 'g', publication_year },
                                 { 'w', "(DE627)166430072 " },
                                 { 'w', "(DE600)188494-3" },
                                 { 'w', "(DE576)014791072" } });
    default:
        LOG_ERROR("Invalid OJSItaly type: " + std::to_string(type));
    }
}


void ConvertRecords(MARC::Reader * const marc_reader, MARC::Writer * const marc_writer,
                    const MARCToMARCMappingMultiset &marc_to_marc_mappings) {
    unsigned id(0);
    for (const auto ojsitaly_type : { OJSITALY_TYPES::STUDIA_PATAVINA }) {
        while (MARC::Record record = marc_reader->read()) {
            ++id;
            MARC::Record *new_record(CreateNewRecord(std::to_string(id)));
            for (auto marc_to_marc_mapping(marc_to_marc_mappings.begin()); marc_to_marc_mapping != marc_to_marc_mappings.end();
                 ++marc_to_marc_mapping) {
                std::string tag(marc_to_marc_mapping->marc_in_tag_and_subfield_.substr(0, 3));
                char subfield_code(marc_to_marc_mapping->marc_in_tag_and_subfield_[3]);
                marc_to_marc_mapping->extraction_function_(new_record, record.getFirstSubfieldValue(tag, subfield_code));
            }
            // Dummy entries
            new_record->insertField("003", "DE-Tue135");
            new_record->insertField("005", TimeUtil::GetCurrentDateAndTime("%Y%m%d%H%M%S") + ".0");
            new_record->insertField("007", "cr|||||");
            new_record->insertField("084", { { 'a', "1" }, { '2', "ssgn" } });
            new_record->insertField("773", GetSuperiorWorkDescription(ojsitaly_type, new_record->getFirstSubfieldValue("264", 'c')));
            marc_writer->write(*new_record);
            delete new_record;
        }
    }
}


} // namespace


int Main(int argc, char *argv[]) {
    if (argc != 4)
        Usage();

    const std::string marc_input_path(argv[1]);
    const std::string map_file_path(argv[2]);
    const std::string marc_output_path(argv[3]);

    const std::unique_ptr<MARC::Reader> marc_reader(MARC::Reader::Factory(marc_input_path));
    std::unique_ptr<File> map_file(FileUtil::OpenInputFileOrDie(map_file_path));
    const std::unique_ptr<MARC::Writer> marc_writer(MARC::Writer::Factory(marc_output_path));
    MARCToMARCMappingMultiset marc_to_marc_mappings(MARCToMARCMappingComparator);
    CreateMARCToMARCMappings(map_file.get(), &marc_to_marc_mappings);
    ConvertRecords(marc_reader.get(), marc_writer.get(), marc_to_marc_mappings);

    return EXIT_SUCCESS;
}

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
enum OJSITALY_TYPE { STUDIA_PATAVINA, RIVISTA_SCIENCE_DELL_EDUCAZIONE };
const std::map<std::string, enum OJSITALY_TYPE> OJSITALY_TYPE_NAMES_TO_ENUM {
    { "Studia_Patavina", STUDIA_PATAVINA },
    { "Revista_science_dell_educatione", RIVISTA_SCIENCE_DELL_EDUCAZIONE }
};


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


MARC::Record *CreateNewRecord(enum OJSITALY_TYPE type, const std::string id) {
    std::ostringstream formatted_number;
    formatted_number << std::setfill('0') << std::setw(8) << std::atoi(id.c_str());
    const std::string prefix(type == STUDIA_PATAVINA ? "STP" : "RSE");
    const std::string ppn(prefix + formatted_number.str());
    return new MARC::Record(MARC::Record::TypeOfRecord::LANGUAGE_MATERIAL, MARC::Record::BibliographicLevel::SERIAL_COMPONENT_PART, ppn);
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


        record->insertField("100", { { 'a', authors[0] }, { '4', "aut" }, { 'e', "VerfasserIn" } }, '1');
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


void InsertOrAppendToSubfield(const std::string &tag, const char subfield_code, MARC::Record * const record, const std::string &data) {
    if (data.length()) {
        if (not record->hasTag(tag)) {
            InsertField(tag, subfield_code, record, data);
            return;
        }
        for (auto &field : record->getTagRange(tag)) {
            if (field.hasSubfield(subfield_code)) {
                auto subfields(field.getSubfields());
                subfields.replaceFirstSubfield(subfield_code, subfields.getFirstSubfieldWithCode(subfield_code) + data);
                field = MARC::Record::Field(tag, subfields, field.getIndicator1(), field.getIndicator2());
            } else
                field.insertOrReplaceSubfield(subfield_code, data);
        }
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

void ExtractStudiaPatavinaVolumeYearAndPages(const std::string, const char, MARC::Record * const record, const std::string &data) {
    const std::string component_matcher_str("Vol[.]\\s+(\\d+)[(](\\d{4})[)](\\d+),\\s*(\\d+)-(\\d+)\\s*p.");
    static ThreadSafeRegexMatcher matcher((ThreadSafeRegexMatcher(component_matcher_str)));
    const auto matched(matcher.match(data));

    if (not matched)
        LOG_ERROR("Invalid volume/year/pages specification: \"" + data + "\"");

    MARC::Subfields _936_subfields;
    const std::string volume(matched[1]);
    _936_subfields.addSubfield('d', volume);

    const std::string year(matched[2]);
    _936_subfields.addSubfield('j', year);
    record->insertField("264", { { 'c', year } });

    const std::string issue(matched[3]);
    _936_subfields.addSubfield('e', issue);

    const std::string start_page(matched[4]);
    const std::string end_page(matched[5]);
    _936_subfields.addSubfield('h', start_page + "-" + end_page);

    record->insertField("936", _936_subfields, 'u', 'w');
}

void CreateOrAppendTo936IfPresent(MARC::Record * const record, const MARC::Subfields _936_subfields) {
    if (record->hasFieldWithTag("936")) {
        for (auto &field : record->getTagRange("936")) {
            if (not(field.getIndicator1() == 'u' and field.getIndicator2() == 'w'))
                continue;

            MARC::Subfields _936_subfields_present(field.getSubfields());
            for (const auto &new_subfield : _936_subfields)
                _936_subfields_present.appendSubfield(new_subfield);
            field = MARC::Record::Field("936", _936_subfields_present, field.getIndicator1(), field.getIndicator2());
        }
    } else
        record->insertField("936", _936_subfields, 'u', 'w');
}


void ExtractRivistaVolumeIssueAndYear(const std::string, const char, MARC::Record * const record, const std::string &data) {
    const std::string volume_issue_and_year_str("a[.](\\d+):n(\\d+(?:-\\d+)?)\\s+[(](\\d{4}).*");
    static ThreadSafeRegexMatcher matcher((ThreadSafeRegexMatcher(volume_issue_and_year_str)));
    const auto matched(matcher.match(data));

    if (matched) {
        MARC::Subfields _936_subfields;
        const std::string volume(matched[1]);
        _936_subfields.addSubfield('d', volume);

        const std::string issue(matched[2]);
        _936_subfields.addSubfield('e', issue);

        const std::string year(matched[3]);
        _936_subfields.addSubfield('j', year);
        record->insertField("264", { { 'c', year } });


        CreateOrAppendTo936IfPresent(record, _936_subfields);

    } else
        LOG_WARNING("Could not extract volume, issue and year from \"" + data + "\"");
}


void ExtractRevistaPages(const std::string, const char, MARC::Record * const record, const std::string &data) {
    const std::string page_str("(?:P|pp)[.]\\s+(\\d+)-(\\d+)");
    static ThreadSafeRegexMatcher matcher((ThreadSafeRegexMatcher(page_str)));
    const auto matched(matcher.match(data));

    if (matched) {
        MARC::Subfields _936_subfields;
        const std::string start_page(matched[1]);
        const std::string end_page(matched[2]);
        _936_subfields.addSubfield('h', start_page + "-" + end_page);
        CreateOrAppendTo936IfPresent(record, _936_subfields);
    } else
        LOG_WARNING("Could not extract pages from \"" + data + "\"");
}


void ExtractLanguageFrom008(const std::string, const char, MARC::Record * const record, const std::string &data) {
    // Extract three letter language code at position 35
    const std::string lang(data.substr(35, 3));
    InsertField("041", 'a', record, lang);
}


const std::map<std::string, ConversionFunctor> name_to_functor_map{
    { "InsertField", InsertField },
    { "InsertCreationField", InsertCreationField },
    { "InsertAuthors", InsertAuthors },
    { "InsertOrForceSubfield", InsertOrForceSubfield },
    { "InsertOrAppendToSubfield", InsertOrAppendToSubfield },
    { "AppendAuthorFirstName", AppendAuthorFirstName },
    { "ExtractStudiaPatavinaVolumeYearAndPages", ExtractStudiaPatavinaVolumeYearAndPages },
    { "ExtractRivistaVolumeIssueAndYear", ExtractRivistaVolumeIssueAndYear },
    { "ExtractRevistaPages", ExtractRevistaPages },
    { "ExtractLanguageFrom008", ExtractLanguageFrom008 }
};


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


void GetOJSItalyType(File * const map_file, enum OJSITALY_TYPE * const ojsitaly_type) {
    if (map_file->eof())
        LOG_ERROR("Could not determine OJSItaly type");
    std::string line;
    map_file->getline(&line);
    std::vector<std::string> type_line_components;
    StringUtil::SplitThenTrim(line, SEPARATOR_CHAR, " \t", &type_line_components);
    if (type_line_components.size() != 2 and type_line_components[0] != "TYPE")
        LOG_ERROR("Invalid type line : \"" + line + "\"");
    if (not OJSITALY_TYPE_NAMES_TO_ENUM.contains(type_line_components[1]))
        LOG_ERROR("Invalid OJSITALY type: \"" + type_line_components[1]);
    *ojsitaly_type = OJSITALY_TYPE_NAMES_TO_ENUM.at(type_line_components[1]);
}


void CreateMARCToMARCMappings(File * const map_file, MARCToMARCMappingMultiset * const marc_to_marc_mappings) {
    unsigned linenum(1); // We are called after GetOJSItalyType()
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
        static ThreadSafeRegexMatcher tag_subfield_and_functorname("(?i)([a-z0-9]{3,4})\\s+\\((\\p{L}[\\p{L}0-9]+)\\)\\s*");
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


MARC::Subfields GetSuperiorWorkDescription(enum OJSITALY_TYPE type, const std::string &subfield_g_content) {
    switch (type) {
    case OJSITALY_TYPE::STUDIA_PATAVINA:
        return MARC::Subfields({ { 'i', "Enhalten in" },
                                 { 't', "Studia patavina" },
                                 { 'd', "Padova : Facoltà teologica del Triveneto, 1954" },
                                 { 'g', subfield_g_content },
                                 { 'w', "(DE-627)166751685" },
                                 { 'w', "(DE-600)301088-0" },
                                 { 'w', "(DE-576)015186075" } });
    case OJSITALY_TYPE::RIVISTA_SCIENCE_DELL_EDUCAZIONE:
        return MARC::Subfields({ { 'i', "Enhalten in" },
                                 { 't', "Rivista di scienze dell'educazione" },
                                 { 'd', "Roma, 1973" },
                                 { 'g', subfield_g_content },
                                 { 'w', "(DE-627)166430072" },
                                 { 'w', "(DE-600)188494-3" },
                                 { 'w', "(DE-576)014791072" } });
    default:
        LOG_ERROR("Invalid OJSItaly type: " + std::to_string(type));
    }
}


void CleanTitles(MARC::Record * const record) {
    static ThreadSafeRegexMatcher matcher((ThreadSafeRegexMatcher("(?:<<(.*)>>)")));
    std::string title(record->getFirstSubfieldValue("245", 'a'));
    const std::string new_title = matcher.replaceWithBackreferences(title, "\\1");
    auto _245_field = record->getFirstField("245");
    MARC::Subfields _245_subfields(_245_field->getSubfields());
    _245_subfields.replaceFirstSubfield('a', new_title);
    record->replaceField("245", _245_subfields, _245_field->getIndicator1(), _245_field->getIndicator2());
}

std::string Assemble773gContent(const MARC::Record &record) {
    std::string _773g_content;
    for (const auto &_936field : record.getTagRange("936")) {
        if (not(_936field.getIndicator1() == 'u' and _936field.getIndicator2()))
            continue;
        const MARC::Subfields _936_subfields(_936field.getSubfields());
        const std::string volume(_936_subfields.getFirstSubfieldWithCode('d'));
        _773g_content += volume;

        std::string year(_936_subfields.getFirstSubfieldWithCode('j'));
        if (year.empty())
            year = record.getFirstSubfieldValue("264", 'c');
        if (not year.empty()) {
            _773g_content += _773g_content.size() ? " " : "";
            _773g_content += "(" + year + ")";
        }

        const std::string issue(_936_subfields.getFirstSubfieldWithCode('e'));
        if (not issue.empty()) {
            _773g_content += _773g_content.size() ? " " : "";
            _773g_content += issue;
        }

        const std::string pages(_936_subfields.getFirstSubfieldWithCode('h'));
        if (not pages.empty()) {
            _773g_content += _773g_content.size() ? ", " : "";
            _773g_content += "Seite " + pages;
        }
    }
    return _773g_content;
}


void AddSelectors(MARC::Record * const record) {
    record->insertFieldAtEnd("935", { { 'a', "itbk" }, { '2', "LOK" } });
    record->insertFieldAtEnd("935", { { 'a', "aixrk" }, { '2', "LOK" } });
    record->insertFieldAtEnd("935", { { 'a', "aixzs" }, { '2', "LOK" } });
}


void ConvertRecords(MARC::Reader * const marc_reader, MARC::Writer * const marc_writer, const enum OJSITALY_TYPE ojsitaly_type,
                    const MARCToMARCMappingMultiset &marc_to_marc_mappings) {
    unsigned id(0);
    while (MARC::Record record = marc_reader->read()) {
        ++id;
        MARC::Record *new_record(CreateNewRecord(ojsitaly_type, std::to_string(id)));
        for (auto marc_to_marc_mapping(marc_to_marc_mappings.begin()); marc_to_marc_mapping != marc_to_marc_mappings.end();
             ++marc_to_marc_mapping) {
            std::string tag(marc_to_marc_mapping->marc_in_tag_and_subfield_.substr(0, 3));
            char subfield_code(marc_to_marc_mapping->marc_in_tag_and_subfield_[3]);
            if (unlikely(!subfield_code))
                marc_to_marc_mapping->extraction_function_(new_record, record.getFirstFieldContents(tag));
            else
                marc_to_marc_mapping->extraction_function_(new_record, record.getFirstSubfieldValue(tag, subfield_code));
        }
        // Dummy entries
        new_record->insertField("003", "DE-Tue135");
        new_record->insertField("005", TimeUtil::GetCurrentDateAndTime("%Y%m%d%H%M%S") + ".0");
        new_record->insertField("007", "tu");
        new_record->insertField("084", { { 'a', "1" }, { '2', "ssgn" } });
        new_record->insertField("773", GetSuperiorWorkDescription(ojsitaly_type, Assemble773gContent(*new_record)));
        new_record->insertField("852", { { 'a', "DE-Tue135" } });
        new_record->insertField("912", { { 'a', "NOMM" } });
        CleanTitles(new_record);
        AddSelectors(new_record);
        marc_writer->write(*new_record);
        delete new_record;
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
    enum OJSITALY_TYPE ojsitaly_type;
    GetOJSItalyType(map_file.get(), &ojsitaly_type);
    MARCToMARCMappingMultiset marc_to_marc_mappings(MARCToMARCMappingComparator);
    CreateMARCToMARCMappings(map_file.get(), &marc_to_marc_mappings);
    ConvertRecords(marc_reader.get(), marc_writer.get(), ojsitaly_type, marc_to_marc_mappings);

    return EXIT_SUCCESS;
}

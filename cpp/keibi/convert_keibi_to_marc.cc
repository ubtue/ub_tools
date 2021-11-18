/** \brief Convert the KeiBi Database entries to MARC 21 Records
 *  \author Johannes Riedl
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

#include <functional>
#include <iostream>
#include <iomanip>
#include <map>
#include <sstream>
#include "DbConnection.h"
#include "FileUtil.h"
#include "IniFile.h"
#include "MARC.h"
#include "StringUtil.h"
#include "UBTools.h"
#include "util.h"


namespace {

using ConversionFunctor = std::function<void(const std::string, const char, MARC::Record * const, const std::string)>;
const std::string KEIBI_QUERY("SELECT * FROM citations LIMIT 100");
const char SEPARATOR_CHAR('|');


struct DbFieldToMARCMapping {
    const std::string db_field_name_;
    const std::string marc_tag_;
    const char subfield_code_;
    std::function<void(MARC::Record * const, const std::string)> extraction_function_;
    DbFieldToMARCMapping(const std::string &db_field_name, const std::string marc_tag, const char subfield_code,
                         ConversionFunctor extraction_function) :
                         db_field_name_(db_field_name), marc_tag_(marc_tag),
                         subfield_code_(subfield_code),
                         extraction_function_(std::bind(extraction_function, marc_tag, subfield_code, std::placeholders::_1, std::placeholders::_2)) {}
};

const auto DbFieldToMarcMappingComparator = [](const DbFieldToMARCMapping &lhs, const DbFieldToMARCMapping &rhs) { return lhs.db_field_name_ < rhs.db_field_name_;};
using DbFieldToMARCMappingMultiset = std::multiset<DbFieldToMARCMapping, decltype(DbFieldToMarcMappingComparator)>;


[[noreturn]] void Usage() {
    ::Usage("db_inifile map_file marc_output");
}


void InsertField(const std::string &tag, const char subfield_code, MARC::Record * const record, const std::string &data) {
    if (data.length())
        record->insertField(tag, subfield_code, data);
}


void ConvertCitations(DbConnection * const db_connection,
                      const DbFieldToMARCMappingMultiset &dbfield_to_marc_mappings,
                      MARC::Writer * const marc_writer)
{
    db_connection->queryOrDie(KEIBI_QUERY);
    DbResultSet result_set(db_connection->getLastResultSet());
    unsigned i(0);
    while (const auto row = result_set.getNextRow()) {
        ++i;
        std::ostringstream formatted_number;
        formatted_number << std::setfill('0') << std::setw(8) << i;
        MARC::Record new_record(MARC::Record::TypeOfRecord::MIXED_MATERIALS, MARC::Record::BibliographicLevel::COLLECTION,
                                "KEI" + formatted_number.str());
        for (auto dbfield_to_marc_mapping(dbfield_to_marc_mappings.begin());
             dbfield_to_marc_mapping != dbfield_to_marc_mappings.end();
             ++dbfield_to_marc_mapping)
        {
            dbfield_to_marc_mapping->extraction_function_(&new_record, row[dbfield_to_marc_mapping->db_field_name_]);

        }
        marc_writer->write(new_record);
    }
}


const std::map<std::string, ConversionFunctor> name_to_functor_map {
    {  "InsertField", InsertField }
};

ConversionFunctor GetConversionFunctor(const std::string &functor_name) {
    if (not name_to_functor_map.contains(functor_name))
        LOG_ERROR("Unknown functor " + functor_name);
    return name_to_functor_map.find(functor_name)->second;
}


void ExtractTagAndSubfield(const std::string combined, std::string * tag, char * subfield_code) {
    if (combined.length() != 4)
        LOG_ERROR("Invalid Tag and Subfield format " + combined);
    *tag = combined.substr(0,3);
    *subfield_code = combined[3];
}


void CreateDbFieldToMarcMappings(File * const map_file, DbFieldToMARCMappingMultiset * const dbfield_to_marc_mappings) {
    unsigned linenum(0);
    while (not map_file->eof()) {
        ++linenum;
        std::string line;
        map_file->getline(&line);
        StringUtil::Trim(&line);
        std::vector<std::string> mapping;
        StringUtil::SplitThenTrim(line, SEPARATOR_CHAR, " \t", &mapping);
        if (unlikely(mapping.size() < 2 and line.back() != SEPARATOR_CHAR)) {
            LOG_WARNING("Invalid line format in line " + std::to_string(linenum));
            continue;
        }
        static ThreadSafeRegexMatcher tag_subfield_and_functorname("(?i)([a-z0-9]{4})\\s+\\((\\p{L}+)\\)\\s*");
        const std::vector<std::string> extraction_rules(mapping.begin() + 1, mapping.end());
        for (const auto &extraction_rule : extraction_rules) {
             std::string tag;
             char subfield_code;
             ConversionFunctor conversion_functor;
             if (const auto match_result = tag_subfield_and_functorname.match(extraction_rule)) {
                 ExtractTagAndSubfield(match_result[1], &tag, &subfield_code);
                 conversion_functor = GetConversionFunctor(match_result[2]);
             } else {
                 ExtractTagAndSubfield(mapping[1], &tag, &subfield_code);
                 conversion_functor = GetConversionFunctor("InsertField");
             }
             dbfield_to_marc_mappings->emplace(DbFieldToMARCMapping(mapping[0], tag, subfield_code, GetConversionFunctor("InsertField")));
        }
    }
}

} // unnamed namespace


int Main(int argc, char *argv[]) {
    if (argc != 4)
        Usage();
    const std::string ini_file_path(argv[1]);
    const std::string map_file_path(argv[2]);
    const std::string marc_output_path(argv[3]);

    DbConnection db_connection(DbConnection::MySQLFactory(IniFile(ini_file_path)));
    std::unique_ptr<File> map_file(FileUtil::OpenInputFileOrDie(map_file_path));
    const std::unique_ptr<MARC::Writer> marc_writer(MARC::Writer::Factory(marc_output_path));
    DbFieldToMARCMappingMultiset dbfield_to_marc_mappings(DbFieldToMarcMappingComparator);
    CreateDbFieldToMarcMappings(map_file.get(), &dbfield_to_marc_mappings);
    ConvertCitations(&db_connection, dbfield_to_marc_mappings, marc_writer.get());

    return EXIT_SUCCESS;
}





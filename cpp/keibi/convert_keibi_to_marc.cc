/** \brief 
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
#include <sstream>
#include <map>
#include "DbConnection.h"
#include "IniFile.h"
#include "MARC.h"
#include "UBTools.h"
#include "util.h"

namespace {

[[noreturn]] void Usage() {
    ::Usage("db_inifile map_file marc_output");
}

struct DbFieldToMARCMapping {
    const std::string db_field_name_;
    const std::string marc_tag_;
    const char subfield_code_;
    std::function<void(MARC::Record * const, const std::string)> extraction_function_;
    DbFieldToMARCMapping(const std::string &db_field_name, const std::string marc_tag, const char subfield_code,
                         std::function<void(const std::string, const char, MARC::Record * const, const std::string)> extraction_function) : 
                         db_field_name_(db_field_name), marc_tag_(marc_tag),
                         subfield_code_(subfield_code),
                         extraction_function_(std::bind(extraction_function, marc_tag, subfield_code, std::placeholders::_1, std::placeholders::_2)) {}
};

const auto DbFieldToMarcMappingComparator = [](const DbFieldToMARCMapping &lhs, const DbFieldToMARCMapping &rhs) { return lhs.db_field_name_ < rhs.db_field_name_;};


void InsertField(const std::string &tag, const char subfield_code, MARC::Record * const record, const std::string &data) {
    if (data.length())
        record->insertField(tag, subfield_code, data);
}


void ConvertCitations(DbConnection * const db_connection, 
                      const std::multiset<DbFieldToMARCMapping, decltype(DbFieldToMarcMappingComparator)> &dbfield_to_marc_mappings,
                      MARC::Writer * const marc_writer) 
{
    db_connection->queryOrDie("SELECT * FROM citations");
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


void CreateDbFieldToMarcMappings(std::multiset<DbFieldToMARCMapping, decltype(DbFieldToMarcMappingComparator)> * const dbfield_to_marc_mappings) {
    dbfield_to_marc_mappings->emplace(DbFieldToMARCMapping("title", "265", 'c', InsertField));
    dbfield_to_marc_mappings->emplace(DbFieldToMARCMapping("author", "100", 'a', InsertField));
}

} // unnamed namespace


int Main(int argc, char *argv[]) {
    if (argc != 4)
        Usage();
    const std::string ini_file_path(argv[1]);
    const std::string map_file_path(argv[2]);
    const std::string marc_output_path(argv[3]);

    DbConnection db_connection(DbConnection::MySQLFactory(IniFile(ini_file_path)));
    const std::unique_ptr<MARC::Writer> marc_writer(MARC::Writer::Factory(marc_output_path));
    std::multiset<DbFieldToMARCMapping, decltype(DbFieldToMarcMappingComparator)> dbfield_to_marc_mappings(DbFieldToMarcMappingComparator);
    CreateDbFieldToMarcMappings(&dbfield_to_marc_mappings);
    ConvertCitations(&db_connection, dbfield_to_marc_mappings, marc_writer.get()); 

    return EXIT_SUCCESS;
}





/** \file    convert_tags_to_keywords.cc
 *  \brief   Inserts user tags from the SQL database into corresponding MARC records.
 *  \author  Dr. Johannes Ruscheinski (johannes.ruscheinski@uni-tuebingen.de)
 */
/*
    Copyright (C) 2017-2021 Library of the University of Tübingen

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
#include <memory>
#include <set>
#include <unordered_map>
#include <cstdlib>
#include "Compiler.h"
#include "DbConnection.h"
#include "DbResultSet.h"
#include "MARC.h"
#include "util.h"


namespace {


[[noreturn]] void Usage() {
    std::cerr << "Usage: " << ::progname << " marc_input marc_output\n";
    std::exit(EXIT_FAILURE);
}


void PopulateResourceIdToRecordIdMap(DbConnection * const connection,
                                     std::unordered_map<std::string, std::string> * const resource_id_to_record_id_map) {
    resource_id_to_record_id_map->clear();

    connection->queryOrDie("SELECT id,record_id FROM resource");
    DbResultSet result_set(connection->getLastResultSet());

    while (const auto db_row = result_set.getNextRow())
        resource_id_to_record_id_map->emplace(db_row["id"], db_row["record_id"]);

    std::cout << "Found " << resource_id_to_record_id_map->size() << " mappings from resource ID's to record ID's.\n";
}


void PopulateTagIdToResourceIdMap(DbConnection * const connection,
                                  std::unordered_map<std::string, std::string> * const tag_id_to_resource_id_map) {
    tag_id_to_resource_id_map->clear();

    connection->queryOrDie("SELECT tag_id,resource_id FROM resource_tags");
    DbResultSet result_set(connection->getLastResultSet());

    while (const auto db_row = result_set.getNextRow())
        tag_id_to_resource_id_map->emplace(db_row["tag_id"], db_row["resource_id"]);

    std::cout << "Found " << tag_id_to_resource_id_map->size() << " mappings from tag ID's to resource ID's.\n";
}


void ExtractTags(DbConnection * const connection, const std::unordered_map<std::string, std::string> &tag_id_to_resource_id_map,
                 const std::unordered_map<std::string, std::string> &resource_id_to_record_id_map,
                 std::unordered_map<std::string, std::set<std::string>> * const record_id_to_tags_map) {
    record_id_to_tags_map->clear();

    connection->queryOrDie("SELECT id,tag FROM tags");
    DbResultSet result_set(connection->getLastResultSet());

    unsigned tag_count(0);
    while (const auto db_row = result_set.getNextRow()) {
        ++tag_count;

        const auto tag_id_and_resource_id(tag_id_to_resource_id_map.find(db_row["id"]));
        if (unlikely(tag_id_and_resource_id == tag_id_to_resource_id_map.end())) {
            LOG_WARNING("no resource ID for tag ID \"" + db_row["id"] + "\" found!");
            continue;
        }

        const auto resource_id_and_record_id(resource_id_to_record_id_map.find(tag_id_and_resource_id->second));
        if (unlikely(resource_id_and_record_id == resource_id_to_record_id_map.end())) {
            LOG_WARNING("no record ID for resource ID \"" + tag_id_and_resource_id->second + "\" found!");
            continue;
        }

        const auto record_id_and_tags(record_id_to_tags_map->find(resource_id_and_record_id->second));
        if (record_id_and_tags == record_id_to_tags_map->end())
            record_id_to_tags_map->emplace(resource_id_and_record_id->second, std::set<std::string>{ db_row["tag"] });
        else
            record_id_and_tags->second.insert(db_row["tag"]);
    }

    std::cout << "Found " << tag_count << " tag(s).\n";
}


void AddTagsToRecords(MARC::Reader * const reader, MARC::Writer * const writer,
                      const std::unordered_map<std::string, std::set<std::string>> &record_id_to_tags_map) {
    unsigned total_count(0), modified_count(0);
    while (MARC::Record record = reader->read()) {
        ++total_count;

        const auto record_id_and_tags(record_id_to_tags_map.find(record.getControlNumber()));
        if (record_id_and_tags != record_id_to_tags_map.end()) {
            for (const auto &tag : record_id_and_tags->second)
                record.insertField("653", { { 'a', tag } });
            ++modified_count;
        }
        writer->write(record);
    }

    std::cout << "Processed a total of " << total_count << " record(s).\n";
    std::cout << "Added tags to " << modified_count << " record(s).\n";
}


} // unnamed namespace


int Main(int argc, char *argv[]) {
    if (argc != 3)
        Usage();

    auto reader(MARC::Reader::Factory(argv[1]));
    auto writer(MARC::Writer::Factory(argv[2]));

    auto db_connection(DbConnection::VuFindMySQLFactory());

    std::unordered_map<std::string, std::string> resource_id_to_record_id_map;
    PopulateResourceIdToRecordIdMap(&db_connection, &resource_id_to_record_id_map);

    std::unordered_map<std::string, std::string> tag_id_to_resource_id_map;
    PopulateTagIdToResourceIdMap(&db_connection, &tag_id_to_resource_id_map);

    std::unordered_map<std::string, std::set<std::string>> record_id_to_tags_map;
    ExtractTags(&db_connection, tag_id_to_resource_id_map, resource_id_to_record_id_map, &record_id_to_tags_map);

    AddTagsToRecords(reader.get(), writer.get(), record_id_to_tags_map);

    return EXIT_SUCCESS;
}

/** \file    create_match_db.cc
 *  \brief   Creates mapping databases from normalised author names and titles to control numbers.
 *  \author  Dr. Johannes Ruscheinski
 *
 *  Copyright (C) 2018, Library of the University of Tübingen
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
#include <iostream>
#include <set>
#include <unordered_set>
#include <cctype>
#include <cstdlib>
#include "DbConnection.h"
#include "Compiler.h"
#include "MARC.h"
#include "StringUtil.h"
#include "TextUtil.h"
#include "util.h"


namespace {


[[noreturn]] void Usage() {
    std::cerr << "Usage: " << ::progname << " marc_titles\n";
    std::exit(EXIT_FAILURE);
}


std::string NormaliseAuthorName(std::string author_name) {
    author_name = StringUtil::TrimWhite(author_name);
    const auto comma_pos(author_name.find(','));
    if (comma_pos != std::string::npos)
        author_name = StringUtil::TrimWhite(author_name.substr(comma_pos + 1) + " " + author_name.substr(0, comma_pos));

    std::string normalised_author_name;
    bool space_seen(false);
    for (const char ch : author_name) {
        switch (ch) {
        case '.':
            normalised_author_name += ch;
            normalised_author_name += ' ';
            space_seen = true;
            break;
        case ' ':
            if (not space_seen)
                normalised_author_name += ' ';
            space_seen = true;
            break;
        default:
            normalised_author_name += ch;
            space_seen = false;
        }
    }

    return normalised_author_name;
}


void ExtractAuthors(const MARC::Record &record, const std::string &tag, std::unordered_set<std::string> * const normlised_author_names) {
    for (const auto &field : record.getTagRange(tag)) {
        for (const auto &subfield : field.getSubfields()) {
            if (subfield.code_ == 'a')
                normlised_author_names->emplace(TextUtil::UTF8ToLower(NormaliseAuthorName(subfield.value_)));
        }
    }
}


std::string NormaliseTitle(const std::string &title) {
    std::string normalised_title;
    bool space_seen(false);
    for (const char ch : title) {
        if (std::ispunct(ch) or std::isspace(ch)) {
            if (not space_seen)
                normalised_title += ' ';
            space_seen = true;
        } else {
            space_seen = false;
            normalised_title += ch;
        }
    }

    return StringUtil::TrimWhite(&normalised_title);
}


void ExtractAllAuthors(const MARC::Record &record, std::unordered_set<std::string> * const normalised_author_names) {
    ExtractAuthors(record, "100", normalised_author_names);
    ExtractAuthors(record, "700", normalised_author_names);
}


const size_t MAX_TITLE_LENGTH(191); // Because of MySQL restrictions on VARCHAR.


void PopulateTables(DbConnection * const db_connection, MARC::Reader * const reader) {
    unsigned count(0);
    std::unordered_set<std::string> already_seen_control_numbers;
    while (const auto record = reader->read()) {
        const auto control_number(record.getControlNumber());
        if (already_seen_control_numbers.find(control_number) != already_seen_control_numbers.cend()) {
            LOG_WARNING("Duplicate control number: " + control_number);
            continue;
        }
        already_seen_control_numbers.emplace(control_number);

        std::unordered_set<std::string> normalised_author_names;
        ExtractAllAuthors(record, &normalised_author_names);
        for (const auto normalised_author_name : normalised_author_names)
            db_connection->queryOrDie("INSERT INTO normalised_authors SET author='" + db_connection->escapeString(normalised_author_name)
                                      + "', ppn='" + control_number + "'");

        auto normalised_title(TextUtil::UTF8ToLower(NormaliseTitle(record.getMainTitle())));
        if (unlikely(not TextUtil::UnicodeTruncate(&normalised_title, MAX_TITLE_LENGTH)))
            LOG_ERROR("failed to truncate the normalised title to a maximal length of " + std::to_string(MAX_TITLE_LENGTH)
                      + " Unicode code points!");
        if (unlikely(normalised_title.empty()))
            LOG_WARNING("Empty normalised title in record w/ control number: " + control_number);
        else
            db_connection->queryOrDie("INSERT INTO normalised_titles SET title='" + db_connection->escapeString(normalised_title)
                                      + "', ppn='" + control_number + "'");
    }

    LOG_INFO("Processed " + std::to_string(count) + " records.");
}


} // unnamed namespace


int Main(int argc, char **argv) {
    if (argc != 2)
        Usage();

    auto reader(MARC::Reader::Factory(argv[1]));
    DbConnection ub_tools_db;
    ub_tools_db.queryOrDie("TRUNCATE TABLE normalised_authors");
    ub_tools_db.queryOrDie("TRUNCATE TABLE normalised_titles");
    PopulateTables(&ub_tools_db, reader.get());

    return EXIT_SUCCESS;
}

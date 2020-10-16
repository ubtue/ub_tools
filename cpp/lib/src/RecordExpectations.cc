/** \file   RecordExpectations.cc
 *  \brief  Various bits relating to MARC record field and subfield presence requirements.
 *  \author Dr. Johannes Ruscheinski (johannes.ruscheinski@uni-tuebingen.de)
 *
 *  \copyright 2020 Universitätsbibliothek Tübingen.  All rights reserved.
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
#include "RecordExpectations.h"
#include "DbConnection.h"
#include "DbResultSet.h"
#include "DbRow.h"


namespace RecordExpectations {


bool StringToFieldPresence(const std::string &field_presence_str, FieldPresence * const field_presence) {
    if (field_presence_str == "ALWAYS") {
        *field_presence = ALWAYS;
        return true;
    }

    if (field_presence_str == "SOMETIMES") {
        *field_presence = SOMETIMES;
        return true;
    }

    if (field_presence_str == "IGNORE") {
        *field_presence = IGNORE;
        return true;
    }

    return false;
}


FieldPresence StringToFieldPresence(const std::string &s) {
    FieldPresence field_presence;
    if (StringToFieldPresence(s, &field_presence))
        return field_presence;

    LOG_ERROR("unknown enumerated value \"" + s + "\"!");
}


std::string FieldPresenceToString(const FieldPresence field_presence) {
    switch (field_presence) {
    case ALWAYS:
        return "always";
    case SOMETIMES:
        return "sometimes";
    case IGNORE:
        return "ignore";
    default:
        LOG_ERROR("we should *never get here!");
    }
}


RecordType StringToRecordType(const std::string &record_type_str) {
    if (record_type_str == "regular_article")
        return REGULAR_ARTICLE;
    if (record_type_str == "review")
        return REVIEW;
    LOG_ERROR("unknown record type \"" + record_type_str + "\"!");
}


std::string RecordTypeToString(const RecordType record_type) {
    switch (record_type) {
    case REGULAR_ARTICLE:
        return "regular_article";
    case REVIEW:
        return "review";
    }
}


bool FieldInfo::operator<(const FieldInfo &rhs) const {
    const auto retcode(::strcasecmp(name_.c_str(), rhs.name_.c_str()));
    if (retcode < 0)
        return true;
    if (retcode > 0)
        return false;

    if (subfield_code_ < rhs.subfield_code_)
        return true;
    if (subfield_code_ > rhs.subfield_code_)
        return false;

    if (presence_ < rhs.presence_)
        return true;
    if (presence_ > rhs.presence_)
        return false;

    if (record_type_ < rhs.record_type_)
        return true;

    return false;
}


static std::string GetJournalID(DbConnection * db_connection, const std::string &zeder_id, const std::string &zeder_instance) {
    if (zeder_id.empty() or zeder_instance.empty()) {
        if (not zeder_id.empty() or not zeder_instance.empty())
            LOG_ERROR("if zeder_id is empty, zeder_instance must also be empty!");
        return "";
    }

    if (zeder_instance != "ixtheo" and zeder_instance != "krimdok")
        LOG_ERROR("unknown Zeder instance \"" + zeder_instance + "\"!");

    db_connection->queryOrDie("SELECT id FROM zeder_journals WHERE zeder_id=" + db_connection->escapeAndQuoteString(zeder_id) +
                              " AND zeder_instance=" + db_connection->escapeAndQuoteString(zeder_instance));
    auto result_set(db_connection->getLastResultSet());
    if (result_set.empty())
        LOG_ERROR("no entry found in ub_tools.zeder_journals where zeder_id=\"" + zeder_id + "\" and zeder_instance=\""
                  + zeder_instance + "\"!");

    return result_set.getNextRow()[0];
}


bool AddRule(const std::string &zeder_id, const std::string &zeder_instance, const std::string &marc_tag,
             const std::string &marc_subfield_code, const FieldPresence field_presence,
             const RecordType record_type, std::string * const error_message)
{
    if (marc_subfield_code.length() > 1)
        LOG_ERROR("marc_subfield_code must be empty or a single character!");

    DbConnection db_connection;
    const auto journal_id(GetJournalID(&db_connection, zeder_id, zeder_instance));

    // Check for incompatibility w/ existing rules:
    db_connection.queryOrDie("SELECT subfield_code, field_presence FROM metadata_presence_tracer WHERE zeder_journal_id " +
                             (journal_id.empty() ? std::string("IS NULL") : "= " + journal_id) +
                             " metadata_field_name = '" + marc_tag + "' AND record_type='" + RecordTypeToString(record_type) + "'");
    auto result_set(db_connection.getLastResultSet());
    while (const auto row = result_set.getNextRow()) {
        const auto existing_marc_subfield_code(row.getValue("subfield_code"));
        const auto existing_field_presence(StringToFieldPresence(row["field_presence"]));

        if (marc_subfield_code.empty()) { // We have a rule for an entire field!
            if (existing_marc_subfield_code.empty()) {
                if (zeder_id.empty())
                    *error_message = "a rule already exist for tag " + marc_tag +
                                     "!  Please first delete the existing rule before adding a new rule.";
                else
                    *error_message = "a global rule already exist for Zeder ID " + zeder_id + ", Zeder instance " + zeder_instance +
                                     "tag " + marc_tag + "!  Please first delete the existing rule before adding a new rule.";
                return false;
            } else if (not marc_subfield_code.empty()) {
                if (field_presence == ALWAYS and (existing_field_presence == SOMETIMES or existing_field_presence == IGNORE)) {
                    *error_message = "you can't require that subfield " + marc_subfield_code + " must be present when the entrire field "
                                     + marc_tag + " is optional!";
                    if (not zeder_id.empty())
                        error_message->append("(Zeder ID " + zeder_id + ", Zeder instance " + zeder_instance + ")");
                    return false;
                }
            }
        } else { // We have an existing rule for the subfield
            if (existing_marc_subfield_code == marc_subfield_code) {
                if (zeder_id.empty())
                    *error_message = "a rule already exist for tag " + marc_tag + "$" + marc_subfield_code +
                                     "!  Please first delete the existing rule before adding a new rule.";
                else
                    *error_message = "a global rule already exist for Zeder ID " + zeder_id + ", Zeder instance " + zeder_instance +
                                     "tag " + marc_tag + "$" + marc_subfield_code +
                                     "!  Please first delete the existing rule before adding a new rule.";
                return false;
            } else if (marc_subfield_code.empty() and (field_presence == SOMETIMES or field_presence == IGNORE)) {
                if (existing_field_presence == ALWAYS) {
                    *error_message = "you can't add a rule to make the field " + marc_tag + " optional because subfield "
                                     + existing_marc_subfield_code + " is mandatory!";
                    if (not zeder_id.empty())
                        error_message->append("(Zeder ID " + zeder_id + ", Zeder instance " + zeder_instance + ")");
                    return false;
                }
            }
        }
    }

    db_connection.queryOrDie("INSERT INTO metadata_presence_tracer (zeder_journal_id, metadata_field_name, subfield_code, field_presence, record_type) "
                             " VALUES (" + (journal_id.empty() ? "NULL" : journal_id) +
                             ", " + db_connection.escapeAndQuoteString(marc_tag) + ", "
                             + (marc_subfield_code.empty() ? std::string("NULL") : db_connection.escapeAndQuoteString(marc_subfield_code)) +
                             ", " + db_connection.escapeAndQuoteString(RecordTypeToString(record_type)) +
                             ", " + db_connection.escapeAndQuoteString(FieldPresenceToString(field_presence)) + ")");

    return true;
}


void DeleteRule(const std::string &zeder_id, const std::string &zeder_instance, const std::string &marc_tag,
                const std::string &marc_subfield_code, const RecordType record_type)
{
    if (marc_subfield_code.length() > 1)
        LOG_ERROR("marc_subfield_code must be empty or a single character!");

    DbConnection db_connection;
    const auto journal_id(GetJournalID(&db_connection, zeder_id, zeder_instance));
    db_connection.queryOrDie("DELETE FROM metadata_presence_tracer "
                             "WHERE zeder_journal_id " +
                             (journal_id.empty() ? std::string("IS NULL") : "= " + db_connection.escapeAndQuoteString(journal_id)) +
                             " AND metadata_field_name = " + db_connection.escapeAndQuoteString(marc_tag) +
                             " AND subfield_code " +
                             (marc_subfield_code.empty() ? std::string("IS NULL") : "= " + db_connection.escapeAndQuoteString(marc_subfield_code)) +
                             " AND delete_record_type = " + db_connection.escapeAndQuoteString(RecordTypeToString(record_type)));
}


} // namespace RecordExpectations

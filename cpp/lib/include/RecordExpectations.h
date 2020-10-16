/** \file   RecordExpectations.h
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
#pragma once


#include <string>


namespace RecordExpectations {


enum FieldPresence { ALWAYS, SOMETIMES, IGNORE };
enum RecordType { REGULAR_ARTICLE, REVIEW };


bool StringToFieldPresence(const std::string &field_presence_str, FieldPresence * const field_presence);
FieldPresence StringToFieldPresence(const std::string &s);
std::string FieldPresenceToString(const FieldPresence field_presence);
RecordType StringToRecordType(const std::string &record_type_str);
std::string RecordTypeToString(const RecordType record_type);


struct FieldInfo {
    std::string name_;
    char subfield_code_;
    FieldPresence presence_;
    RecordType record_type_;
public:
    FieldInfo(const std::string &name, const char subfield_code, const FieldPresence presence, const RecordType record_type)
        : name_(name), subfield_code_(subfield_code), presence_(presence), record_type_(record_type) { }
    bool operator<(const FieldInfo &rhs) const;
    inline bool operator>(const FieldInfo &rhs) const { return rhs < *this; }
};


/** \brief Add a new rule to the metadata_presence_tracer table.
 *  \param zeder_id        If empty the rule is a global rule and not specific to an individual journal.
 *  \param zeder_instance  If empty the rule is a global rule and not specific to an individual journal.
 *                         Must be either "ixtheo" or "krimdok".  If not the function aborts the running program!
 *  \return True if the new rule was added and false if it was incompatible w/ existing rules and therefore not added.
 */
bool AddRule(const std::string &zeder_id, const std::string &zeder_instance, const std::string &marc_tag,
             const std::string &marc_subfield_code, const FieldPresence field_presence,
             const RecordType record_type, std::string * const error_message);


/** \brief Removes a rule from the metadata_presence_tracer table.
 *  \note  Aborts if the rule does not exist.
 */
void DeleteRule(const std::string &zeder_id, const std::string &zeder_instance, const std::string &marc_tag,
                const std::string &marc_subfield_code, const RecordType record_type);


} // namespace RecordExpectations

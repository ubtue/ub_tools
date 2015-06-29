/** \file   Leader.h
 *  \brief  Interface for the Leader class.
 *  \author Dr. Johannes Ruscheinski (johannes.ruscheinski@uni-tuebingen.de)
 *
 *  \copyright 2014 Universitätsbiblothek Tübingen.  All rights reserved.
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
#ifndef LEADER_H
#define LEADER_H


#include <string>


/** \class Leader
 *  \brief Encapsulates a MARC-21 record leader.
 */
class Leader {
public:
    static const size_t LEADER_LENGTH;
private:
    std::string raw_leader_;
    unsigned record_length_;
    unsigned base_address_of_data_;

    /** Constructs a leader from its constituent parts. */
    Leader(const std::string &raw_leader, unsigned record_length, unsigned base_address_of_data)
        : raw_leader_(raw_leader), record_length_(record_length), base_address_of_data_(base_address_of_data) {}
public:
    /** \brief Creates a "Leader" instance from a binary MARC-21 leader blob.
     *  \param leader_string  The binary blob that should be a leader from a MARC-21 record.
     *  \param leader         Must be the address of a "Leader*" (pointer-to-Leader). ParseLeader will allocate a
     *                        Leader instance and set "*leader" to point to this newly initialised object.
     *  \param err_msg        If not NULL and a parse error occurred an informational text will be returned here.
     *  \return True if the parse succeeded, else false.
     */
    static bool ParseLeader(const std::string &leader_string, Leader ** const leader,
                            std::string * const err_msg = NULL);

    /** \brief Index operator returning the n'th byte of the leader.
     *  \param pos  The offset of the byte to return.  Must be < Leader::LEADER_LENGTH.
     */
    char operator[](const std::string::size_type pos) const { return raw_leader_[pos]; }

    unsigned getRecordLength() const { return record_length_; }
    bool setRecordLength(const unsigned new_record_length, std::string * const err_msg = NULL);

    char getRecordStatus() const { return raw_leader_[5]; }
    char getRecordType() const { return raw_leader_[6]; }
    char getCharacterCodingScheme() const { return raw_leader_[9]; }
    std::string getImplementationDefined1() const { return raw_leader_.substr(7, 2); }
    std::string getImplementationDefined2() const { return raw_leader_.substr(17, 3); }
    unsigned getBaseAddressOfData() const { return base_address_of_data_; }
    void setBaseAddressOfData(const unsigned new_base_address_of_data);
    bool isSerial() const { return raw_leader_[7] == 's'; }
    bool isArticle() const { return raw_leader_[7] == 'a'; }

    /** \return A binary representation of the leader.  Can be used to construct a MARC-21 record. */ 
    std::string toString() const { return raw_leader_; }
};


#endif // ifndef LEADER_H

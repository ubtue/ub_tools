/** \file   LocalDataDB.h
 *  \brief  Interface for the LocalDataDB class.
 *  \author Dr. Johannes Ruscheinski (johannes.ruscheinski@uni-tuebingen.de)
 *
 *  \copyright 2020-2021 Universitätsbibliothek Tübingen.  All rights reserved.
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
#include <vector>
#include "DbConnection.h"


class LocalDataDB {
    mutable DbConnection db_connection_;
public:
    enum OpenMode { READ_ONLY, READ_WRITE };
private:
    bool single_transaction_; // By default SQLite uses a transaction for each INSERT or UPDATE!
public:
    explicit LocalDataDB(const OpenMode open_mode);
    ~LocalDataDB();

    // Empty out the database.
    void clear();

    /** \param title_ppn the PPN of the title data set that the local fields are associated with */
    void insertOrReplace(const std::string &title_ppn, const std::vector<std::string> &local_fields);

    /** \param title_ppn the PPN of the title data set that the local fields are associated with */
    std::vector<std::string> getLocalFields(const std::string &title_ppn) const;

    /** \param title_ppn the PPN of the title data set that the local fields are associated with */
    void removeTitleDataSet(const std::string &title_ppn);

    /** \param local_ppn a PPN of a local data set
     *  \note  The PPN "local_ppn" here is *not* the PPN of a title data set!
     */
    bool removeLocalDataSet(const std::string &local_ppn);
};

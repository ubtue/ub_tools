/** \brief Implementation of the Marc21OaiPmhClient class.
 *  \author Dr. Johannes Ruscheinski (johannes.ruscheinski@uni-tuebingen.de)
 *
 *  \copyright 2017 Universitätsbibliothek Tübingen.  All rights reserved.
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
#include "Marc21OaiPmhClient.h"
#include <iostream>


bool Marc21OaiPmhClient::processRecord(const OaiPmh::Record &record, const unsigned /*verbosity*/,
                                       Logger * const /*logger*/)
{
    ++record_count_;

    (void)marc_writer_;
    std::cout << "Got a record w/ identifier \"" << record.getIdentifier() << "\".\n";
    return true;
}

/** \brief Interface for the Marc21OaiPmhClient class.
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

#ifndef MARC21_OAI_PMH_CLIENT_H
#define MARC21_OAI_PMH_CLIENT_H


#include "OaiPmhClient.h"
#include "MarcWriter.h"


class Marc21OaiPmhClient final : public OaiPmh::Client {
    MarcWriter * const marc_writer_;
public:
    Marc21OaiPmhClient(const IniFile &ini_file, const std::string &section_name, MarcWriter * const marc_writer)
        : OaiPmh::Client(ini_file, section_name), marc_writer_(marc_writer) { }

    virtual bool processRecord(const OaiPmh::Record &record, const unsigned verbosity, Logger * const logger);
};


#endif // ifndef MARC21_OAI_PMH_CLIENT_H

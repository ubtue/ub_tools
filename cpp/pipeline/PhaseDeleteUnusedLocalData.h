/** \file    PhaseDeleteUnusedLocalData.h
 *  \brief   Local data blocks are embedded marc records inside of a record using LOK-Fields.
 *           Each local data block belongs to an institution and is marked by the institution's
 *           sigil. This tool filters for local data blocks of some institutions of the University
 *           of Tübingen and deletes all other local blocks.
 *  \author  Oliver Obenland
 */
/*
    Copyright (C) 2016, Library of the University of Tübingen

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

#ifndef PHASE_DELETE_UNUSED_LOCAL_DATA_H
#define PHASE_DELETE_UNUSED_LOCAL_DATA_H

#include "PipelinePhase.h"


class PhaseDeleteUnusedLocalData : public PipelinePhase {
public:
    PhaseDeleteUnusedLocalData() { };
    virtual ~PhaseDeleteUnusedLocalData();

    virtual PipelinePhaseState process(MarcUtil::Record &record, std::string * const error_message);
};


#endif

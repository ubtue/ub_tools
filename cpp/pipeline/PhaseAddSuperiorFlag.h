/** \file    PhaseAddSuperiorFlag.h
 *  \brief   A tool for marking superior records that have associated inferior records in our data sets.
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

#ifndef PHASE_ADD_SUPERIOR_FLAG_H
#define PHASE_ADD_SUPERIOR_FLAG_H

#include "PipelinePhase.h"


class PhaseAddSuperiorFlag : public PipelinePhase {
public:
    PhaseAddSuperiorFlag();
    virtual ~PhaseAddSuperiorFlag();

    virtual PipelinePhaseState preprocess(const MarcUtil::Record &record, std::string * const error_message) override;

    virtual PipelinePhaseState process(MarcUtil::Record &record, std::string * const error_message) override;
};


#endif

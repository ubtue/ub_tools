/** \file    PhaseAugment773a.h
 *  \brief   A tool for filling in 773$a if the 773 field exists and $a is missing.
 *  \author  Dr. Johannes Ruscheinski
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

#ifndef PHASE_AUGMENT_773A_H
#define PHASE_AUGMENT_773A_H

#include "PipelinePhase.h"


class PhaseAugment773a : public PipelinePhase {
public:
    PhaseAugment773a() { };
    virtual ~PhaseAugment773a();

    virtual PipelinePhaseState preprocess(const MarcUtil::Record &record, std::string * const error_message);

    virtual PipelinePhaseState process(MarcUtil::Record &record, std::string * const error_message);
};


#endif

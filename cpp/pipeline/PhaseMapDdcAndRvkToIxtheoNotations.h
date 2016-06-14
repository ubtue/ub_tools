/** \file    PhaseMapDdcAndRvkToIxtheoNotations.h
 *  \brief   Map certain DDC and RVK categories to ixTheo notations and add them to field 652a.
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

#ifndef PHASE_MAP_DDC_AND_RVK_TO_IXTHEO_NOTATIONS_H
#define PHASE_MAP_DDC_AND_RVK_TO_IXTHEO_NOTATIONS_H

#include "PipelinePhase.h"


class PhaseMapDdcAndRvkToIxtheoNotations : public PipelinePhase {
public:
    PhaseMapDdcAndRvkToIxtheoNotations();
    virtual ~PhaseMapDdcAndRvkToIxtheoNotations();

    virtual PipelinePhaseState process(MarcUtil::Record &record, std::string * const error_message);
};


#endif

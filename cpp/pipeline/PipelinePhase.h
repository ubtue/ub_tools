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

#ifndef PIPELINE_PHASE_H
#define PIPELINE_PHASE_H

#include <cstdlib>
#include <cstring>
#include <iostream>
#include "MarcUtil.h"
#include "PipelineMonitor.h"


enum PipelinePhaseState {
    SUCCESS,
    PURGE_RECORD,
    ERROR
};


class PipelinePhase {
public:
    bool verbose = false;
    bool debug = false;
    PipelineMonitor *monitor;


    PipelinePhase() { };
    virtual ~PipelinePhase() { };


    PipelinePhaseState MakeError(const std::string message, std::string * const error_message) {
        if (error_message != nullptr)
            *error_message = message;
        return ERROR;
    }


    /**
     * \param record the record for extracting data.
     */
    virtual PipelinePhaseState preprocess(const MarcUtil::Record &, std::string * const = nullptr) { return SUCCESS; };


    virtual PipelinePhaseState preprocessNormData(const MarcUtil::Record &, std::string * const = nullptr) { return SUCCESS; };

    /**
     * \param  record  the record to modify.
     * \param  error   if not equals nullptr something went wrong. An appropriate error message will be assigned.
     * \return true to keep the record, false to drop the record.
     */
    virtual PipelinePhaseState process(MarcUtil::Record &record, std::string * const error_message = nullptr) = 0;
};


#endif
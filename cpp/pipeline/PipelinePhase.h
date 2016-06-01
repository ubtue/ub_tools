//
// Created by quboo01 on 11.05.16.
//

#ifndef PIPELINE_PHASE_H
#define PIPELINE_PHASE_H

#include <cstdlib>
#include <cstring>
#include <iostream>
#include "MarcUtil.h"


enum PipelinePhaseState {
    SUCCESS,
    PURGE_RECORD,
    ERROR
};


class PipelinePhase {
public:
    bool verbose = false;
    bool debug = false;


    PipelinePhase() { };


    virtual ~PipelinePhase() { };


    PipelinePhaseState MakeError(const std::string message, std::string *const error_message) {
        if (error_message != nullptr)
            *error_message = message;
        return ERROR;
    }


    /**
     * \param record the record for extracting data.
     */
    virtual PipelinePhaseState preprocess(const MarcUtil::Record &, std::string *const = nullptr) { return SUCCESS; };


    virtual PipelinePhaseState preprocessNormData(const MarcUtil::Record &, std::string *const = nullptr) { return SUCCESS; };

    /**
     * \param  record  the record to modify.
     * \param  error   if not equals nullptr something went wrong. An appropriate error message will be assigned.
     * \return true to keep the record, false to drop the record.
     */
    virtual PipelinePhaseState process(MarcUtil::Record &record, std::string *const error_message = nullptr) = 0;
};


#endif
//
// Created by quboo01 on 11.05.16.
//

#ifndef PHASEADDAUTHORSYNONYMS_H
#define PHASEADDAUTHORSYNONYMS_H

#include "PipelinePhase.h"


class PhaseAddAuthorSynonyms : public PipelinePhase {
public:
    PhaseAddAuthorSynonyms();

    virtual ~PhaseAddAuthorSynonyms();

    virtual PipelinePhaseState preprocessNormData(const MarcUtil::Record &record, std::string *const error_message);

    virtual PipelinePhaseState process(MarcUtil::Record &record, std::string *const error_message);
};


#endif

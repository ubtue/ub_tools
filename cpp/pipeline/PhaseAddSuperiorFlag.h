//
// Created by quboo01 on 11.05.16.
//

#ifndef PHASEADDSUPERIORFLAG_H
#define PHASEADDSUPERIORFLAG_H

#include "PipelinePhase.h"


class PhaseAddSuperiorFlag : public PipelinePhase {
public:
    PhaseAddSuperiorFlag();

    virtual ~PhaseAddSuperiorFlag();

    virtual PipelinePhaseState preprocess(const MarcUtil::Record &record, std::string *const error_message) override;

    virtual PipelinePhaseState process(MarcUtil::Record &record, std::string *const error_message) override;
};


#endif

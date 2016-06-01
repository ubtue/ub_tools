//
// Created by quboo01 on 11.05.16.
//

#ifndef PHASEAUGMENT773A_H
#define PHASEAUGMENT773A_H

#include "PipelinePhase.h"


class PhaseAugment773a : public PipelinePhase {
public:
    PhaseAugment773a() { };

    virtual ~PhaseAugment773a();

    virtual PipelinePhaseState preprocess(const MarcUtil::Record &record, std::string *const error_message);

    virtual PipelinePhaseState process(MarcUtil::Record &record, std::string *const error_message);
};


#endif

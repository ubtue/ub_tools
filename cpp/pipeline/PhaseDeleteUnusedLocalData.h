//
// Created by quboo01 on 11.05.16.
//

#ifndef PHASEDELETEUNUSEDLOCALDATA_H
#define PHASEDELETEUNUSEDLOCALDATA_H

#include "PipelinePhase.h"


class PhaseDeleteUnusedLocalData : public PipelinePhase {
public:
    PhaseDeleteUnusedLocalData() { };

    virtual ~PhaseDeleteUnusedLocalData();

    virtual PipelinePhaseState process(MarcUtil::Record &record, std::string *const error_message);
};


#endif

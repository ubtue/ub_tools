//
// Created by quboo01 on 11.05.16.
//

#ifndef PHASEUPDATEIXTHEONOTATIONS_H
#define PHASEUPDATEIXTHEONOTATIONS_H

#include "PipelinePhase.h"


class PhaseUpdateIxtheoNotations : public PipelinePhase {
public:
    PhaseUpdateIxtheoNotations();

    virtual ~PhaseUpdateIxtheoNotations();

    virtual PipelinePhaseState process(MarcUtil::Record &record, std::string *const error_message);
};


#endif

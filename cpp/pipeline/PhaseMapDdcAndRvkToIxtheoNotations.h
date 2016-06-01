//
// Created by quboo01 on 11.05.16.
//

#ifndef PHASEMAPDDCANDRVKTOIXTHEONOTATIONS_H
#define PHASEMAPDDCANDRVKTOIXTHEONOTATIONS_H

#include "PipelinePhase.h"


class PhaseMapDdcAndRvkToIxtheoNotations : public PipelinePhase {
public:
    PhaseMapDdcAndRvkToIxtheoNotations();

    virtual ~PhaseMapDdcAndRvkToIxtheoNotations();

    virtual PipelinePhaseState process(MarcUtil::Record &record, std::string *const error_message);
};


#endif

//
// Created by quboo01 on 11.05.16.
//

#ifndef PHASEADDISBNSORISSNSTOARTICLES_H
#define PHASEADDISBNSORISSNSTOARTICLES_H

#include "PipelinePhase.h"


class PhaseAddIsbnsOrIssnsToArticles : public PipelinePhase {
public:
    PhaseAddIsbnsOrIssnsToArticles() { };

    virtual ~PhaseAddIsbnsOrIssnsToArticles();

    virtual PipelinePhaseState preprocess(const MarcUtil::Record &record, std::string *const error_message);

    virtual PipelinePhaseState process(MarcUtil::Record &record, std::string *const error_message);
};


#endif

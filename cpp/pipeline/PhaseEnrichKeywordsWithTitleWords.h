//
// Created by quboo01 on 11.05.16.
//

#ifndef PHASEENRICHKEYWORDSWITHTITLEWORDS_H
#define PHASEENRICHKEYWORDSWITHTITLEWORDS_H

#include "PipelinePhase.h"


class PhaseEnrichKeywordsWithTitleWords : public PipelinePhase {
public:
    PhaseEnrichKeywordsWithTitleWords();

    virtual ~PhaseEnrichKeywordsWithTitleWords();

    virtual PipelinePhaseState preprocess(const MarcUtil::Record &record, std::string *const error_message);

    virtual PipelinePhaseState process(MarcUtil::Record &record, std::string *const error_message);
};


#endif

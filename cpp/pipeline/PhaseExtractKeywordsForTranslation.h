//
// Created by quboo01 on 11.05.16.
//

#ifndef PHASEEXTRACTKEYWORDSFORTRANSLATION_H
#define PHASEEXTRACTKEYWORDSFORTRANSLATION_H

#include "PipelinePhase.h"


class PhaseExtractKeywordsForTranslation : public PipelinePhase {
public:
    PhaseExtractKeywordsForTranslation();

    virtual ~PhaseExtractKeywordsForTranslation();

    virtual PipelinePhaseState preprocess(const MarcUtil::Record &record, std::string *const error_message);

    virtual PipelinePhaseState preprocessNormData(const MarcUtil::Record &record, std::string *const error_message);

    virtual PipelinePhaseState process(MarcUtil::Record &record, std::string *const error_message);
};


#endif

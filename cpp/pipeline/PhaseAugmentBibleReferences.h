/** \file    PhaseAugmentBibleReferences.h
 *  \brief   A tool for adding numeric bible references to MARC-21 datasets.
 *  \author  Dr. Johannes Ruscheinski
 */
/*
    Copyright (C) 2016, Library of the University of Tübingen

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU Affero General Public License as
    published by the Free Software Foundation, either version 3 of the
    License, or (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Affero General Public License for more details.

    You should have received a copy of the GNU Affero General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef PHASE_AUGMENT_BIBLE_REFERENCES_H
#define PHASE_AUGMENT_BIBLE_REFERENCES_H

#include "PipelinePhase.h"
#include <unordered_set>

const std::unordered_set<std::string> books_of_the_bible { // Found in 130$a:100$t
        "matthäusevangelium", // -- start New Testament --
        "markusevangelium",
        "lukasevangelium",
        "johannesevangelium",
        "apostelgeschichte",
        "römerbrief",
        "korintherbrief", // 2 records "I." and "II." in $n
        "galaterbrief",
        "epheserbrief",
        "philipperbrief",
        "kolosserbrief",
        "thessalonicherbrief", // 2 records "I." and "II." in $n
        "timotheusbrief", // 2 records "I." and "II." in $n
        "titusbrief",
        "philemonbrief",
        "hebräerbrief",
        "jakobusbrief",
        "petrusbrief", // 2 records "I." and "II." in $n
        "johannesbrief", // 3 records "I.", "II." and "III." in $n
        "judasbrief",
        "offenbarung des Johannes", // a.k.a. "Johannes Apokalypse"
        "genesis", // -- start Old Testament --
        "exodus",
        "leviticus",
        "numeri",
        "deuteronomium",
        "josua",
        "richter",
        "rut",
        "samuel", // 2 records "I." and "II." in $n
        "könige", // 2 records "I." and "II." in $n
        "chronik", // 2 records "I." and "II." in $n
        "esra",
        "nehemia",
        "tobit",
        "judit",
        "ester",
        "makkabäer", // 4 records "I.", "II.", "III." and "IV." in $n
        "ijob",
        "psalmen",
        "sprichwörter",
        "kohelet",
        "hoheslied",
        "weisheit",
        "sirach",
        "jesaja",
        "jeremia",
        "klagelieder jeremias", // a.k.a. "Klagelieder"
        "baruch",
        "jeremiabrief", // a.k.a. "Epistola Jeremiae"
        "ezechiel",
        "daniel",
        "hosea",
        "joel",
        "amos",
        "obadja",
        "jona",
        "micha",
        "nahum",
        "habakuk",
        "zefanja",
        "haggai",
        "sacharja",
        "maleachi",
};


// Books of the bible that are flagged as "g:Buch.*" in 130$9:
const std::unordered_set<std::string> explicit_books {
        "josua", "richter", "rut", "samuel", "könige", "esra", "nehemia", "tobit", "judit", "ester",
        "makkabäer", "ijob", "weisheit", "sirach", "jesaja", "jeremia", "baruch", "ezechiel", "daniel", "hosea", "joel",
        "amos", "obadja", "jona", "micha", "nahum", "habakuk", "zefanja", "haggai", "sacharja", "maleachi"
};


// Books of the bible that have ordinal Roman numerals in 130$n:
const std::unordered_set<std::string> books_with_ordinals {
        "korintherbrief", "thessalonicherbrief", "timotheusbrief", "petrusbrief", "johannesbrief", "samuel", "könige",
        "chronik", "makkabäer"
};


class PhaseAugmentBibleReferences : public PipelinePhase {
public:
    PhaseAugmentBibleReferences();
    virtual ~PhaseAugmentBibleReferences();

    virtual PipelinePhaseState preprocessNormData(const MarcUtil::Record &record, std::string * const error_message);

    virtual PipelinePhaseState process(MarcUtil::Record &record, std::string * const error_message);
};


#endif

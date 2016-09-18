//
// Created by quboo01 on 16.09.16.
//

#ifndef UB_TOOLS_MARCREADER_H
#define UB_TOOLS_MARCREADER_H

#include "MarcRecord.h"

class MarcReader {
    friend class MarcRecord;

private:
    static MarcRecord ReadSingleRecord(File * const input);

public:
    static MarcRecord Read(File * const input);
    static MarcRecord ReadXML(File * const input);
};

#endif //UB_TOOLS_MARCREADER_H

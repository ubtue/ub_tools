//
// Created by quboo01 on 16.09.16.
//

#ifndef UB_TOOLS_MARCWRITER_H_H
#define UB_TOOLS_MARCWRITER_H_H

#include "MarcRecord.h"
#include "XmlWriter.h"

class MarcWriter {
public:
    static void Write(MarcRecord &record, File * const output);
    static void Write(MarcRecord &record, XmlWriter * const xml_writer);
};

#endif //UB_TOOLS_MARCWRITER_H_H

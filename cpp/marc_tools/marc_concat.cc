#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include "MARC.h"
#include "util.h"


namespace {


[[noreturn]] static void Usage() {
    ::Usage("marc_input1 marc_input2 ... --output-file marc_output");
}


void ProcessRecords(MARC::Reader* const marc_reader, MARC::Writer* const marc_writer) {
    while (MARC::Record record = marc_reader->read()) {
        marc_writer->write(record);
    }
}


} // unnamed namespace


int Main(int argc, char* argv[]) {
    std::vector<std::string> inputFiles;
    std::string outputFile;

    if (argc < 4)
        Usage();

    ++argv, --argc;

    while (argc > 0) {
        std::string arg = *argv;

        if (arg == "--output-file") {
            --argc, ++argv;

            if (argc == 0)
                Usage();

            outputFile = *argv;
            break;
        }

        inputFiles.push_back(arg);
        --argc, ++argv;
    }

    if (outputFile.empty())
        Usage();

    std::unique_ptr<MARC::Writer> marc_writer = MARC::Writer::Factory(outputFile);
    if (not marc_writer) {
        LOG_ERROR("Error: Could not create MARC writer for output file: " + outputFile);
    }

    for (const auto& file : inputFiles) {
        std::unique_ptr<MARC::Reader> marc_reader = MARC::Reader::Factory(file);

        if (not marc_reader) {
            LOG_ERROR("Error: Could not open input file: " + file);
        }
        ProcessRecords(marc_reader.get(), marc_writer.get());
    }

    return 0;
}

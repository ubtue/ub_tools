#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include "MARC.h"


namespace {


[[noreturn]] static void Usage() {
    std::cerr << "Usage: " << ::progname << " marc_input1 marc_input2 ... --output-file marc_output\n";
    std::exit(EXIT_FAILURE);
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

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];

        if (arg == "--output-file") {
            if (i + 1 >= argc) {
                std::cerr << "Error: Missing argument after --output-file\n";
                Usage();
            }
            outputFile = argv[++i];
        } else {
            inputFiles.push_back(arg);
        }
    }

    if (inputFiles.empty() || outputFile.empty())
        Usage();

    std::unique_ptr<MARC::Writer> marc_writer = MARC::Writer::Factory(outputFile);
    if (not marc_writer) {
        std::cerr << "Error: Could not create MARC writer for output file: " << outputFile << std::endl;
        return 1;
    }

    for (const auto& file : inputFiles) {
        std::unique_ptr<MARC::Reader> marc_reader = MARC::Reader::Factory(file);
        if (not marc_reader) {
            std::cerr << "Warning: Could not open input file: " << file << std::endl;
            continue;
        }

        ProcessRecords(marc_reader.get(), marc_writer.get());
    }

    return 0;
}

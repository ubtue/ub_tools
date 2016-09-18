//
// Created by quboo01 on 13.09.16.
//
#include <iostream>
#include <string>
#include <vector>

#include "util.h"
#include "File.h"
#include "MarcUtil.h"
#include "MarcWriter.h"
#include "MarcRecord.h"
#include "MarcReader.h"
#include "MarcWriter.h"
#include "WallClockTimer.h"

void Usage() {
    std::cerr << "usage: " << progname << " marc_input marc_output\n";

    std::exit(EXIT_FAILURE);
}

static const size_t INSERTED_FIELDS_COUNT = 20;

MarcRecord testMarc21(File * const input) {
    MarcRecord record = MarcReader::Read(input);
    if (not record) { return record; }

    record.getControlNumber();
    record.getFieldIndex("689");
    record.extractFirstSubfield("689", 't');

    std::vector<std::string> vector;
    size_t count = record.extractAllSubfields("100", &vector, "a0");

    count = record.extractSubfield("689", '0', &vector);

    count = record.extractSubfields("689", "02", &vector);

    std::vector<std::pair<size_t, size_t>> LOKs;
    count = record.findAllLocalDataBlocks(&LOKs);

    if (count > 0) {
        std::vector <size_t> indizes;
        count = record.findFieldsInLocalBlock("852", "?1", LOKs[0], &indizes);
    }

    record.filterTags({"LOK"});
    count = record.findAllLocalDataBlocks(&LOKs);

    Subfields subfields('x', 'y');
    subfields.addSubfield('a', "Test");
    ssize_t index = record.insertField("TST", subfields.toString());
    record.extractFirstSubfield("TST", 'a');

    Subfields newSubfields('a', 'b');
    newSubfields.addSubfield('a', "FooBar");
    record.updateField(index, newSubfields.toString());
    record.extractFirstSubfield("TST", 'a');

    record.deleteField(index);
    record.getFieldIndex("TST");

    for (size_t i = 0; i < INSERTED_FIELDS_COUNT; ++i) {
        record.insertField("TST", subfields.toString());
    }

    return record;
}

MarcUtil::Record testOldRecord(File * const input) {
    MarcUtil::Record record = MarcUtil::Record::BinaryFactory(input);
    if (not record) { return record; }

    record.getControlNumber();
    record.getFieldIndex("689");
    record.extractFirstSubfield("689", 't');

    std::vector<std::string> vector;
    size_t count = record.extractAllSubfields("100", &vector, "a0");

    count = record.extractSubfield("689", '0', &vector);

    count = record.extractSubfields("689", "02", &vector);

    std::vector<std::pair<size_t, size_t>> LOKs;
    count = record.findAllLocalDataBlocks(&LOKs);

    if (count > 0) {
        std::vector <size_t> indizes;
        count = record.findFieldsInLocalBlock("852", "?1", LOKs[0], &indizes);
    }

    record.filterTags({"LOK"});
    count = record.findAllLocalDataBlocks(&LOKs);

    Subfields subfields('x', 'y');
    subfields.addSubfield('a', "Test");
    record.insertField("TST", subfields.toString());
    ssize_t index = record.getFieldIndex("TST");
    record.extractFirstSubfield("TST", 'a');

    Subfields newSubfields('a', 'b');
    newSubfields.addSubfield('a', "FooBar");
    record.updateField(index, newSubfields.toString());
    record.extractFirstSubfield("TST", 'a');
    record.getFieldIndex("TST");

    record.deleteField(index);
    record.getFieldIndex("TST");

    for (size_t i = 0; i < INSERTED_FIELDS_COUNT; ++i) {
        record.insertField("TST", subfields.toString());
    }

    return record;
}

void speedTest(const std::string &inputFile) {
    const std::string marc_input_filename(inputFile);
    File marc_input(marc_input_filename, "r");
    if (not marc_input)
        Error("can't open \"" + marc_input_filename + "\" for reading!");

    File marc_output("Marc.test.mrc", "w");
    if (not marc_output)
        Error("can't open \"Marc21.test.mrc\" for writing!");

    WallClockTimer marc21Timer(WallClockTimer::CUMULATIVE_WITH_AUTO_START);
    while (MarcRecord marcRecord = testMarc21(&marc_input)) {
        MarcWriter::Write(marcRecord, &marc_output);
    }
    marc21Timer.stop();
    std::cout << "Marc21: " << marc21Timer.getTimeInMilliseconds() / 1000.0 << " sek\n";

    marc_input.rewind();
    marc_output.rewind();

    WallClockTimer oldRecordTimer(WallClockTimer::CUMULATIVE_WITH_AUTO_START);
    while (MarcUtil::Record oldRecord = testOldRecord(&marc_input)) {
        oldRecord.write(&marc_output);
    }
    oldRecordTimer.stop();
    std::cout << "Old Marc: " << oldRecordTimer.getTimeInMilliseconds() / 1000.0<< " sek\n";
}

void speedTestWithoutChanges(const std::string &inputFile) {
    const std::string marc_input_filename(inputFile);
    File marc_input(marc_input_filename, "r");
    if (not marc_input)
        Error("can't open \"" + marc_input_filename + "\" for reading!");

    File marc_output("Marc.test.mrc", "w");
    if (not marc_output)
        Error("can't open \"Marc21.test.mrc\" for writing!");

    WallClockTimer marc21Timer(WallClockTimer::CUMULATIVE_WITH_AUTO_START);
    while (MarcRecord marcRecord = MarcReader::Read(&marc_input)) {
        MarcWriter::Write(marcRecord, &marc_output);
    }
    marc21Timer.stop();
    std::cout << "Marc21: " <<marc21Timer.getTimeInMilliseconds() / 1000.0 << " sek\n";

    marc_input.rewind();
    marc_output.rewind();

    WallClockTimer oldRecordTimer(WallClockTimer::CUMULATIVE_WITH_AUTO_START);
    while (MarcUtil::Record oldRecord = MarcUtil::Record::BinaryFactory(&marc_input)) {
        oldRecord.write(&marc_output);
    }
    oldRecordTimer.stop();
    std::cout << "Old Marc: " << oldRecordTimer.getTimeInMilliseconds() / 1000.0<< " sek\n";
}

void writeTestWithoutChanges (const std::string &inputFile) {
    const std::string marc_input_filename(inputFile);
    File marc_input(marc_input_filename, "r");
    if (not marc_input)
        Error("can't open \"" + marc_input_filename + "\" for reading!");

    File marc21_output("Marc21.test.mrc", "w");
    if (not marc21_output)
        Error("can't open \"Marc21.test.mrc\" for writing!");

    File old_output("Marc_old.test.mrc", "w");
    if (not old_output)
        Error("can't open \"Marc_old.test.mrc\" for writing!");

    MarcRecord marcRecord = MarcReader::Read(&marc_input);
    MarcWriter::Write(marcRecord, &marc21_output);

    marc_input.seek(0);
    MarcUtil::Record oldRecord = MarcUtil::Record::BinaryFactory(&marc_input);
    oldRecord.write(&old_output);
}

void writeTestWithChanges(const std::string &inputFile) {
    const std::string marc_input_filename(inputFile);
    File marc_input(marc_input_filename, "r");
    if (not marc_input)
        Error("can't open \"" + marc_input_filename + "\" for reading!");

    File marc21_output("Marc21.test.mrc", "w");
    if (not marc21_output)
        Error("can't open \"Marc21.test.mrc\" for writing!");

    File old_output("Marc_old.test.mrc", "w");
    if (not old_output)
        Error("can't open \"Marc_old.test.mrc\" for writing!");

    MarcRecord marcRecord = testMarc21(&marc_input);
    MarcWriter::Write(marcRecord, &marc21_output);
    marc21_output.close();

    marc_input.seek(0);
    MarcUtil::Record oldRecord = testOldRecord(&marc_input);
    oldRecord.write(&old_output);
}

void writeBigMarcFile(const std::string &inputFile) {
    const std::string marc_input_filename(inputFile);
    File input(marc_input_filename, "r");
    if (not input)
        Error("can't open \"" + marc_input_filename + "\" for reading!");

    File output("Marc21.test.big.mrc", "w");
    if (not output)
        Error("can't open \"Marc21.test.big.mrc\" for writing!");

    MarcRecord record = MarcReader::Read(&input);
    Subfields subfields('x', 'y');
    subfields.addSubfield('a', "A very long String. FooBar. Erases the contents of the string, which becomes an empty string (with a length of 0 characters).");
    for (size_t i = 0; i < 5000; ++i)
        record.insertField("TST", subfields.toString());
    std::cout << "Write number of Fields: " << record.getNumberOfFields() << "\n";
    MarcWriter::Write(record, &output);
    output.close();

    File next_input("Marc21.test.big.mrc", "r");
    if (not next_input)
        Error("can't open \"Marc21.test.big.mrc\" for reading!");
    MarcRecord next_record = MarcReader::Read(&next_input);
    std::cout << "Read number of Fields: " << next_record.getNumberOfFields() << "\n";

    File next_output("Marc21.test.big2.mrc", "w");
    if (not next_output)
        Error("can't open \"Marc21.test.big2.mrc\" for writing!");
    MarcWriter::Write(next_record, &next_output);
}



int main(int argc, char **argv) {
    ::progname = argv[0];

    if (argc < 2)
        Usage();

    speedTestWithoutChanges(argv[1]);

    return 0;
}
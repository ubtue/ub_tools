#include "MarcWriter.h"
#include "util.h"

//
// Marc 21 stuff:
//

static const size_t MAX_MARC_21_RECORD_LENGTH = 99999;
static char write_buffer[MAX_MARC_21_RECORD_LENGTH];


static bool inline hasDirectoryEntryEnoughSpace(const size_t baseAddress, const size_t data_length, const size_t next_field_length) {
    return baseAddress + DirectoryEntry::DIRECTORY_ENTRY_LENGTH + data_length + next_field_length + 1 <= MAX_MARC_21_RECORD_LENGTH;
}


/**
 * Intentionally we want a copy of directory_iter, so it behaves like a lookahead.
 */
static void inline precalculateBaseAddressOfData(std::vector<DirectoryEntry>::const_iterator directory_iter, const std::vector<DirectoryEntry>::const_iterator &end_iter,
                                          size_t *number_of_directory_entries, size_t *baseAddress, size_t *record_length) {
    *baseAddress += Leader::LEADER_LENGTH + 1 /* for directory separator byte */;
    *record_length += 1 /* for end of record byte */;
    for (; directory_iter < end_iter && hasDirectoryEntryEnoughSpace(*baseAddress, *record_length, directory_iter->getFieldLength()); ++directory_iter) {
        *baseAddress += DirectoryEntry::DIRECTORY_ENTRY_LENGTH;
        *record_length += directory_iter->getFieldLength();
        ++*number_of_directory_entries;
    }
    *record_length += *baseAddress;
}


static void inline writeToBuffer(char *&dest, const std::string &data) {
    std::memcpy(dest, data.data(), data.size());
    dest += data.size();
}


static void inline writeToBuffer(char *&dest, const char* source, size_t offset, size_t length) {
    std::memcpy(dest, source + offset, length);
    dest += length;
}


void MarcWriter::Write(MarcRecord &record, File * const output) {
    size_t written_data_offset;
    size_t raw_data_offset;
    size_t raw_data_length;

    const std::string ppn = record.getControlNumber();
    const size_t ppn_field_length = ppn.size() + 1;


    auto directory_iter = record.directory_entries_.cbegin();
    if (directory_iter->getTag() != "001")
        Error("First directory entry has to be 001!");
    ++directory_iter;

    while (directory_iter < record.directory_entries_.cend()) {
        size_t number_of_directory_entries = 0;
        size_t record_length = ppn_field_length;
        size_t baseAddress = DirectoryEntry::DIRECTORY_ENTRY_LENGTH;
        precalculateBaseAddressOfData(directory_iter, record.directory_entries_.cend(), &number_of_directory_entries, &baseAddress, &record_length);

        char *leader_pointer = write_buffer;
        char *directory_pointer = write_buffer + Leader::LEADER_LENGTH;
        char *data_pointer = write_buffer + baseAddress;

        record.leader_.setBaseAddressOfData(baseAddress);
        record.leader_.setRecordLength(record_length);
        record.leader_.setMultiPartRecord(directory_iter + number_of_directory_entries + 1 < record.directory_entries_.cend());
        writeToBuffer(leader_pointer, record.leader_.toString());

        // Write PPN in each record as first directory entry.
        writeToBuffer(directory_pointer, "001");
        writeToBuffer(directory_pointer, StringUtil::PadLeading(std::to_string(ppn_field_length), 4, '0'));
        writeToBuffer(directory_pointer, "00000");

        raw_data_offset = 0;
        raw_data_length = ppn_field_length;
        written_data_offset = ppn_field_length;

        const std::vector<DirectoryEntry>::const_iterator &end_iter = directory_iter + number_of_directory_entries;
        for (; directory_iter < end_iter; ++directory_iter) {
            writeToBuffer(directory_pointer, directory_iter->getTag());
            writeToBuffer(directory_pointer, StringUtil::PadLeading(std::to_string(directory_iter->getFieldLength()), 4, '0'));
            writeToBuffer(directory_pointer, StringUtil::PadLeading(std::to_string(written_data_offset), 5, '0'));

            if (raw_data_offset + raw_data_length == directory_iter->getFieldOffset()) {
                raw_data_length += directory_iter->getFieldLength();
            } else {
                writeToBuffer(data_pointer, record.raw_data_.data(), raw_data_offset, raw_data_length);
                raw_data_offset = directory_iter->getFieldOffset();
                raw_data_length = directory_iter->getFieldLength();
            }

            written_data_offset += directory_iter->getFieldLength();
        }
        writeToBuffer(directory_pointer, "\x1E");
        writeToBuffer(data_pointer, record.raw_data_.data(), raw_data_offset, raw_data_length);
        writeToBuffer(data_pointer, "\x1D");
        output->write(write_buffer, record_length);
    }
}

//
// XML stuff:
//


void MarcWriter::Write(MarcRecord &record, XmlWriter * const xml_writer) {
    xml_writer->openTag("marc:record");

    record.leader_.setRecordLength(0);
    record.leader_.setBaseAddressOfData(0);
    xml_writer->writeTagsWithData("marc:leader", record.leader_.toString(), /* suppress_newline = */ true);

    for (unsigned entry_no(0); entry_no < record.directory_entries_.size(); ++entry_no) {
        const DirectoryEntry &dir_entry(record.directory_entries_[entry_no]);
        if (dir_entry.isControlFieldEntry())
            xml_writer->writeTagsWithData("marc:controlfield", { std::make_pair("tag", dir_entry.getTag()) }, record.getFieldData(entry_no),
                    /* suppress_newline = */ true);
        else { // We have a data field.
            const std::string data(record.getFieldData(entry_no));
            xml_writer->openTag("marc:datafield",
                                { std::make_pair("tag", dir_entry.getTag()),
                                  std::make_pair("ind1", std::string(1, data[0])),
                                  std::make_pair("ind2", std::string(1, data[1]))
                                });

            std::string::const_iterator ch(data.cbegin() + 2 /* Skip over the indicators. */);

            while (ch != data.cend()) {
                if (*ch != '\x1F')
                    std::runtime_error("in Record::write: expected subfield code delimiter not found! Found "
                                       + std::string(1, *ch) + "! (Control number is " + record.getControlNumber() + ".)");

                ++ch;
                if (ch == data.cend())
                    std::runtime_error("in Record::write: unexpected subfield data end while expecting a subfield code!");
                const std::string subfield_code(1, *ch++);

                std::string subfield_data;
                while (ch != data.cend() and *ch != '\x1F')
                    subfield_data += *ch++;
                if (subfield_data.empty())
                    continue;

                xml_writer->writeTagsWithData("marc:subfield", { std::make_pair("code", subfield_code) },
                                              subfield_data, /* suppress_newline = */ true);
            }

            xml_writer->closeTag(); // Close "marc:datafield".
        }
    }

    xml_writer->closeTag(); // Close "marc:record".
}
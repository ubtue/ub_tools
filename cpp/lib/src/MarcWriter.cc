/** \brief Implementation of writer classes for MARC files.
 *  \author Oliver Obenland (oliver.obenland@uni-tuebingen.de)
 *  \author Dr. Johannes Ruscheinski (johannes.ruscheinski@uni-tuebingen.de)
 *
 *  \copyright 2017 Universitätsbibliothek Tübingen.  All rights reserved.
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU Affero General Public License as
 *  published by the Free Software Foundation, either version 3 of the
 *  License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Affero General Public License for more details.
 *
 *  You should have received a copy of the GNU Affero General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "MarcRecord.h"
#include "Compiler.h"
#include "FileUtil.h"
#include "MarcWriter.h"
#include "StringUtil.h"
#include "util.h"


static const size_t MAX_MARC_21_RECORD_LENGTH(99999);
static char write_buffer[MAX_MARC_21_RECORD_LENGTH];


static bool inline DirectoryEntryHasEnoughSpace(const size_t base_address, const size_t data_length,
                                                const size_t next_field_length)
{
    return base_address + DirectoryEntry::DIRECTORY_ENTRY_LENGTH + data_length + next_field_length + 1
           <= MAX_MARC_21_RECORD_LENGTH;
}


/**
 * We need a copy of directory_iter so it behaves like a lookahead.
 */
static void inline PrecalculateBaseAddressOfData(std::vector<DirectoryEntry>::const_iterator directory_iter,
                                                 const std::vector<DirectoryEntry>::const_iterator &end_iter,
                                                 size_t *number_of_directory_entries, size_t *base_address,
                                                 size_t *record_length)
{
    *base_address += Leader::LEADER_LENGTH + 1 /* for directory separator byte */;
    *record_length += 1 /* for end of record byte */;
    for (/* empty */;
         directory_iter < end_iter
         and DirectoryEntryHasEnoughSpace(*base_address, *record_length, directory_iter->getFieldLength());
         ++directory_iter)
    {
        *base_address += DirectoryEntry::DIRECTORY_ENTRY_LENGTH;
        *record_length += directory_iter->getFieldLength();
        ++*number_of_directory_entries;
    }
    *record_length += *base_address;
}


static void inline WriteToBuffer(char *&dest, const std::string &data) {
    std::memcpy(dest, data.data(), data.size());
    dest += data.size();
}


static void inline WriteToBuffer(char *&dest, const char* source, size_t offset, size_t length) {
    std::memcpy(dest, source + offset, length);
    dest += length;
}


void BinaryMarcWriter::write(const MarcRecord &record) {
    size_t written_data_offset;
    size_t field_data_offset;
    size_t field_data_length;

    const std::string control_number(record.getControlNumber());
    const size_t control_number_field_length(control_number.size() + 1);

    auto directory_iter(record.directory_entries_.cbegin());
    if (unlikely(directory_iter == record.directory_entries_.cend()))
        Error("BinaryMarcWriter::write: can't write a record w/ an empty directory!");
    if (unlikely(directory_iter->getTag() != "001"))
        Error("BinaryMarcWriter::write: first directory entry has to be 001! Found: "
              + directory_iter->getTag().to_string() + " (Control number: " + record.getControlNumber() + ")");
    ++directory_iter;

    while (directory_iter < record.directory_entries_.cend()) {
        size_t number_of_directory_entries(0);
        size_t record_length(control_number_field_length);
        size_t base_address(DirectoryEntry::DIRECTORY_ENTRY_LENGTH);
        PrecalculateBaseAddressOfData(directory_iter, record.directory_entries_.cend(), &number_of_directory_entries,
                                      &base_address, &record_length);

        char *leader_pointer(write_buffer);
        char *directory_pointer(write_buffer + Leader::LEADER_LENGTH);
        char *data_pointer(write_buffer + base_address);

        record.leader_.setBaseAddressOfData(base_address);
        record.leader_.setRecordLength(record_length);
        record.leader_.setMultiPartRecord(directory_iter + number_of_directory_entries + 1
                                          < record.directory_entries_.cend());
        WriteToBuffer(leader_pointer, record.leader_.toString());

        // Write CONTROL_NUMBER in each record as first directory entry.
        WriteToBuffer(directory_pointer, "001");
        WriteToBuffer(directory_pointer, StringUtil::PadLeading(std::to_string(control_number_field_length), 4, '0'));
        WriteToBuffer(directory_pointer, "00000");

        field_data_offset = 0;
        field_data_length = control_number_field_length;
        written_data_offset = control_number_field_length;

        const std::vector<DirectoryEntry>::const_iterator &end_iter = directory_iter + number_of_directory_entries;
        for (; directory_iter < end_iter; ++directory_iter) {
            WriteToBuffer(directory_pointer, directory_iter->getTag().to_string());
            WriteToBuffer(directory_pointer, StringUtil::PadLeading(std::to_string(directory_iter->getFieldLength()),
                                                                    4, '0'));
            WriteToBuffer(directory_pointer, StringUtil::PadLeading(std::to_string(written_data_offset), 5, '0'));

            if (field_data_offset + field_data_length == directory_iter->getFieldOffset()) {
                field_data_length += directory_iter->getFieldLength();
            } else {
                WriteToBuffer(data_pointer, record.field_data_.data(), field_data_offset, field_data_length);
                field_data_offset = directory_iter->getFieldOffset();
                field_data_length = directory_iter->getFieldLength();
            }

            written_data_offset += directory_iter->getFieldLength();
        }
        WriteToBuffer(directory_pointer, "\x1E");
        WriteToBuffer(data_pointer, record.field_data_.data(), field_data_offset, field_data_length);
        WriteToBuffer(data_pointer, "\x1D");
        output_->write(write_buffer, record_length);
    }
}


XmlMarcWriter::XmlMarcWriter(File * const output_file, const unsigned indent_amount,
                             const XmlWriter::TextConversionType text_conversion_type)
{
    xml_writer_ = new MarcXmlWriter(output_file, indent_amount, text_conversion_type);
}


void XmlMarcWriter::write(const MarcRecord &record) {
    xml_writer_->openTag("marc:record");

    record.leader_.setRecordLength(0);
    record.leader_.setBaseAddressOfData(0);
    xml_writer_->writeTagsWithData("marc:leader", record.leader_.toString(), /* suppress_newline = */ true);

    for (unsigned entry_no(0); entry_no < record.directory_entries_.size(); ++entry_no) {
        const DirectoryEntry &dir_entry(record.directory_entries_[entry_no]);
        if (dir_entry.isControlFieldEntry())
            xml_writer_->writeTagsWithData("marc:controlfield",
                                           { std::make_pair("tag", dir_entry.getTag().to_string()) },
                                           record.getFieldData(entry_no),
                    /* suppress_newline = */ true);
        else { // We have a data field.
            const std::string data(record.getFieldData(entry_no));
            xml_writer_->openTag("marc:datafield",
                                { std::make_pair("tag", dir_entry.getTag().to_string()),
                                  std::make_pair("ind1", std::string(1, data[0])),
                                  std::make_pair("ind2", std::string(1, data[1]))
                                });

            std::string::const_iterator ch(data.cbegin() + 2 /* Skip over the indicators. */);

            while (ch != data.cend()) {
                if (*ch != '\x1F')
                    std::runtime_error("in XmlMarcWriter::write: expected subfield code delimiter not found! Found "
                                       + std::string(1, *ch) + "! (Control number is " + record.getControlNumber()
                                       + ".)");

                ++ch;
                if (ch == data.cend())
                    std::runtime_error("in XmlMarcWriter::write: unexpected subfield data end while expecting a "
                                       "subfield code!");
                const std::string subfield_code(1, *ch++);

                std::string subfield_data;
                while (ch != data.cend() and *ch != '\x1F')
                    subfield_data += *ch++;
                if (subfield_data.empty())
                    continue;

                xml_writer_->writeTagsWithData("marc:subfield", { std::make_pair("code", subfield_code) },
                                              subfield_data, /* suppress_newline = */ true);
            }

            xml_writer_->closeTag(); // Close "marc:datafield".
        }
    }

    xml_writer_->closeTag(); // Close "marc:record".
}


std::unique_ptr<MarcWriter> MarcWriter::Factory(const std::string &output_filename, WriterType writer_type,
                                                const WriterMode writer_mode)
{
    std::unique_ptr<File> output(writer_mode == WriterMode::OVERWRITE ? FileUtil::OpenOutputFileOrDie(output_filename)
                                                                      : FileUtil::OpenForAppeningOrDie(output_filename));
    if (writer_type == AUTO) {
        if (StringUtil::EndsWith(output_filename, ".mrc") or StringUtil::EndsWith(output_filename, ".marc"))
            writer_type = BINARY;
        else if (StringUtil::EndsWith(output_filename, ".xml"))
            writer_type = XML;
        else
            throw std::runtime_error("in MarcWriter::Factory: WriterType is AUTO but filename \""
                                     + output_filename + "\" does not end in \".mrc\" or \".xml\"!");
    }
    return (writer_type == XML) ? std::unique_ptr<MarcWriter>(new XmlMarcWriter(output.release()))
                                : std::unique_ptr<MarcWriter>(new BinaryMarcWriter(output.release()));
}

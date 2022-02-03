/** \brief Utility for patching up incorrect encodings in MARC files o/w assumed to be UTF8.
 *  \author Dr. Johannes Ruscheinski (johannes.ruscheinski@uni-tuebingen.de)
 *
 *  \copyright 2022 Universitätsbibliothek Tübingen.  All rights reserved.
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

#include <cstdlib>
#include "MARC.h"
#include "TextUtil.h"
#include "util.h"


namespace {


[[noreturn]] void Usage() {
    ::Usage(
        "(--all-fields|--non-utf8-fields-only) from_encoding marc_input marc_output\n"
        "\tIf --non-utf8-fields-only has been specified then only fields that would not be possibly valid UTF8 will be converted.\n"
        "\tIf --all-fields has been specified all fields will be unconditionally converted.\n"
        "\tTo get a list off all possible values for \"from_encoding\", run \"iconv --list\".\n");
}


// \return True if all records were successfully converted and false o/w.
bool ProcessRecords(const bool all_fields, TextUtil::EncodingConverter * const encoding_converter, MARC::Reader * const marc_reader,
                    MARC::Writer * const marc_writer) {
    unsigned total_count(0), patched_count(0), failure_count(0);

    while (MARC::Record record = marc_reader->read()) {
        ++total_count;

        bool patched_at_least_one_field(false), all_output_fields_are_fine(true);
        for (auto &field : record) {
            if (field.isControlField())
                continue;

            const std::string &field_contents(field.getContents());
            if (all_fields or not TextUtil::IsValidUTF8(field_contents)) {
                patched_at_least_one_field = true;
                std::string converted_field_contents;
                if (not encoding_converter->convert(field_contents, &converted_field_contents)) {
                    all_output_fields_are_fine = false;
                    ++failure_count;
                } else
                    field.setContents(converted_field_contents);
            }
        }

        if (patched_at_least_one_field)
            ++patched_count;

        if (all_output_fields_are_fine)
            marc_writer->write(record);
    }

    LOG_INFO("Converted at least one field in " + std::to_string(patched_count) + " record(s) out of " + std::to_string(total_count)
             + " record(s).");
    if (failure_count == 0)
        return true;

    LOG_WARNING("failed to convert " + std::to_string(failure_count) + " field(s)!");
    return false;
}


} // unnamed namespace


int Main(int argc, char *argv[]) {
    if (argc != 5)
        Usage();

    bool all_fields;
    if (__builtin_strcmp(argv[1], "--all-fields") == 0)
        all_fields = true;
    else if (__builtin_strcmp(argv[1], "--non-utf8-fields-only") == 0)
        all_fields = false;
    else
        Usage();

    std::string error_message;
    const std::unique_ptr<TextUtil::EncodingConverter> encoding_converter(
        TextUtil::EncodingConverter::Factory(argv[2], "utf8", &error_message));
    if (encoding_converter == nullptr)
        LOG_ERROR("failed to create an encoding converter: " + error_message);

    const auto marc_reader(MARC::Reader::Factory(argv[3]));
    const auto marc_writer(MARC::Writer::Factory(argv[4]));

    return ProcessRecords(all_fields, encoding_converter.get(), marc_reader.get(), marc_writer.get()) ? EXIT_SUCCESS : EXIT_FAILURE;
}

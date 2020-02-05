/** \brief Utility for extracting various bit of information from superior works
 *  \author Dr. Johannes Ruscheinski (johannes.ruscheinski@uni-tuebingen.de)
 *
 *  \copyright 2018 Universitätsbibliothek Tübingen.  All rights reserved.
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

#include <iostream>
#include <cstdlib>
#include "Compiler.h"
#include "FileUtil.h"
#include "MARC.h"
#include "MiscUtil.h"
#include "StringUtil.h"
#include "TextUtil.h"
#include "UBTools.h"
#include "util.h"
#include "VuFind.h"


namespace {


const std::string ISSN_TO_MISC_BITS_MAP_PATH_LOCAL(UBTools::GetTuelibPath() + "issn_to_misc_bits.map");
const std::string ISSN_TO_MISC_BITS_MAP_DIR_REMOTE("/mnt/ZE020150/FID-Entwicklung/issn_to_misc_bits");


[[noreturn]] void Usage() {
    std::cerr << "Usage: " << ::progname << " [--min-log-level=min_log_level] marc_input\n"
              << "       Generates map information from marc file and stores it in \"" << ISSN_TO_MISC_BITS_MAP_DIR_REMOTE << "\".\n\n";
    std::exit(EXIT_FAILURE);
}


const std::vector<std::string> ISSN_SUBFIELDS{ "022a", "029a", "440x", "490x", "730x", "773x", "776x", "780x", "785x" };


std::string Sanitize856z(std::string _856z_contents) {
    StringUtil::Map(&_856z_contents, ',', ' ');
    return TextUtil::CollapseAndTrimWhitespace(&_856z_contents);
}


std::string GetSanitized856zContents(const MARC::Record &record) {
    std::string _856_contents;
    for (const auto &_856_field : record.getTagRange("856")) {
        _856_contents = _856_field.getSubfields().getFirstSubfieldWithCode('z');
        if (not _856_contents.empty())
            break;
    }

    return Sanitize856z(_856_contents);
}


void PopulateISSNtoControlNumberMapFile(MARC::Reader * const marc_reader, File * const output) {
    unsigned total_count(0), written_count(0), malformed_count(0);
    while (const MARC::Record record = marc_reader->read()) {
        ++total_count;

        if (not record.isSerial())
            continue;

        std::string unique_language_code;
        std::set<std::string> language_codes;
        if (MARC::GetLanguageCodes(record, &language_codes) == 1)
            unique_language_code = *language_codes.cbegin();

        const std::string sanitized_856z_contents(GetSanitized856zContents(record));
        for (const std::string &issn_subfield : ISSN_SUBFIELDS) {
            for (const auto &field : record.getTagRange(issn_subfield.substr(0, MARC::Record::TAG_LENGTH))) {
                const MARC::Subfields subfields(field.getSubfields());
                for (const auto &subfield_value : subfields.extractSubfields(issn_subfield[MARC::Record::TAG_LENGTH])) {
                    std::string normalised_issn;
                    const std::string title(record.getMainTitle());
                    if (title.empty()) {
                        ++malformed_count;
                        LOG_WARNING("Empty title: \"" + record.getControlNumber() + "\"!");
                    } else if (MiscUtil::NormaliseISSN(subfield_value, &normalised_issn)) {
                        (*output) << normalised_issn << ',' << record.getControlNumber() << ',' << unique_language_code << ','
                                  << sanitized_856z_contents << ',' << title << '\n';
                        ++written_count;
                        goto skip_to_next_record; // Avoid to write the entry several times
                    } else {
                        ++malformed_count;
                        LOG_WARNING("Weird ISSN: \"" + subfield_value + "\"!");
                    }
                }
            }
        }
skip_to_next_record:;
    }

    LOG_INFO("Found " + std::to_string(written_count) + " ISSN's associated with " + std::to_string(total_count)
             + " record(s), " + std::to_string(malformed_count) + " had no title or ISSN's were malformed.");
}


} // unnamed namespace


int Main(int argc, char *argv[]) {
    if (argc != 2)
        Usage();

    const std::string TUEFIND_FLAVOUR(VuFind::GetTueFindFlavour());
    if (TUEFIND_FLAVOUR.empty())
        LOG_ERROR("TUEFIND_FLAVOUR not set, map file cannot be generated.");

    const std::string input_path(argv[1]);
    const std::string output_path(ISSN_TO_MISC_BITS_MAP_DIR_REMOTE + "/" + TUEFIND_FLAVOUR + ".map");

    LOG_INFO("Generating \"" + output_path + "\" from \"" + input_path + "\"...");
    FileUtil::AutoTempFile temp_file;
    std::unique_ptr<MARC::Reader> marc_reader(MARC::Reader::Factory(input_path));
    std::unique_ptr<File> output(FileUtil::OpenOutputFileOrDie(temp_file.getFilePath()));
    PopulateISSNtoControlNumberMapFile(marc_reader.get(), output.get());
    FileUtil::CopyOrDie(temp_file.getFilePath(), output_path);

    return EXIT_SUCCESS;
}

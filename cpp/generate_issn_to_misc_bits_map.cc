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
#include "util.h"
#include "VuFind.h"
#include "Zotero.h"


namespace {


[[noreturn]] void Usage() {
    std::cerr << "Usage: " << ::progname << " [--min-log-level=min_log_level] [--process-remote-files] marc_input [issn_to_ppn_map]\n"
              << "       If you omit the output filename, the file will be stored in \"" << Zotero::ISSN_TO_MISC_BITS_MAP_PATH_LOCAL << "\".\n\n"
              << "       If --process-remote-files is given, the result will also be stored in \"" << Zotero::ISSN_TO_MISC_BITS_MAP_DIR_REMOTE << "\".\n"
              << "          and other systems' files will be added to the regular output filename.\n\n";
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
                    if (MiscUtil::NormaliseISSN(subfield_value, &normalised_issn)) {
                        (*output) << normalised_issn << ',' << record.getControlNumber() << ',' << unique_language_code << ','
                                  << sanitized_856z_contents << ',' << record.getMainTitle() << '\n';
                        ++written_count;
                    } else {
                        ++malformed_count;
                        LOG_WARNING("Weird ISSN: \"" + subfield_value + "\"!");
                    }
                }
            }
        }
    }

    LOG_INFO("Found " + std::to_string(written_count) + " ISSN's associated with " + std::to_string(total_count)
             + " record(s), " + std::to_string(malformed_count) + " ISSN's were malformed.");
}


void ProcessRemoteMapFiles(const std::string &output_path) {
    const std::string TUEFIND_FLAVOUR(VuFind::GetTueFindFlavour());
    const std::string remote_write_path = Zotero::ISSN_TO_MISC_BITS_MAP_DIR_REMOTE + "/" + TUEFIND_FLAVOUR + ".map";
    const std::string remote_write_path_tmp = remote_write_path + ".tmp";
    LOG_INFO("Updating " + TUEFIND_FLAVOUR + " map at \"" + remote_write_path + "\"...");
    FileUtil::CopyOrDie(output_path, remote_write_path_tmp);
    FileUtil::RenameFileOrDie(remote_write_path_tmp, remote_write_path, true /*remove target if it exists*/);

    std::vector<std::string> remote_read_paths;
    FileUtil::GetFileNameList("(?<!" + TUEFIND_FLAVOUR + ")\\.map$", &remote_read_paths, Zotero::ISSN_TO_MISC_BITS_MAP_DIR_REMOTE);
    std::vector<std::string> concat_paths({ remote_write_path });
    for (const auto &remote_file : remote_read_paths) {
        const std::string remote_path(Zotero::ISSN_TO_MISC_BITS_MAP_DIR_REMOTE + "/" + remote_file);
        LOG_INFO("Adding \"" + remote_path + "\" to local map in \"" + output_path + "\"...");
        concat_paths.emplace_back(remote_path);
    }

for (const auto concat_path : concat_paths)
     std::cerr << "PATH: " << concat_path << '\n';


    FileUtil::ConcatFiles(output_path, concat_paths);
}


} // unnamed namespace


int Main(int argc, char *argv[]) {
    if (argc < 2 or argc > 4)
        Usage();

    bool process_remote_files(false);
    if (std::strcmp(argv[1], "--process-remote-files") == 0) {
        process_remote_files = true;
        --argc;++argv;
    }

    const std::string output_path(argc == 3 ? argv[2] : Zotero::ISSN_TO_MISC_BITS_MAP_PATH_LOCAL);

    LOG_INFO("Generating \"" + output_path + "\"...");
    std::unique_ptr<MARC::Reader> marc_reader(MARC::Reader::Factory(argv[1]));
    std::unique_ptr<File> output(FileUtil::OpenOutputFileOrDie(output_path));
    PopulateISSNtoControlNumberMapFile(marc_reader.get(), output.get());

    if (process_remote_files) {
        output->close();
        ProcessRemoteMapFiles(output_path);
    }

    return EXIT_SUCCESS;
}

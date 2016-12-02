/** \brief Generates a continous decompressed stream of data from a BASE tarball containing gzipped ListRecord files.
 *  \author Dr. Johannes Ruscheinski (johannes.ruscheinski@uni-tuebingen.de)
 *
 *  \copyright 2016 Universitätsbiblothek Tübingen.  All rights reserved.
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
#include "Archive.h"
#include "Compiler.h"
#include "FileUtil.h"
#include "GzStream.h"
#include "util.h"


__attribute__((noreturn)) void Usage() {
    std::cerr << "Usage: " << ::progname << " [--verbose] base_tarball_input output\n";
    std::exit(EXIT_FAILURE);
}


void ProcessTarball(const bool verbose, const std::string &input_filename, File * const output) {
    ArchiveReader reader(input_filename);

    unsigned member_count(0);
    ArchiveReader::EntryInfo entry_info;
    while (reader.getNext(&entry_info)) {
        ++member_count;

        GzStream gunzip_streamer(GzStream::GUNZIP);
        char compressed_data[8192];
        ssize_t n_read;
        unsigned bytes_consumed, bytes_produced;
        bool more(false);
        char decompressed_data[8192];
        while ((n_read = reader.read(compressed_data, sizeof compressed_data)) > 0) {
            unsigned total_processed(0);
            do {
                more = gunzip_streamer.decompress(compressed_data + total_processed,
                                                  static_cast<unsigned>(n_read) - total_processed, decompressed_data,
                                                  sizeof decompressed_data, &bytes_consumed, &bytes_produced);
                if (unlikely(output->write(decompressed_data, bytes_produced) != bytes_produced))
                    Error("unexpected error while writing to \"" + output->getPath() + "\"!");
                total_processed += bytes_consumed;
            } while (total_processed < n_read);
        }

        if (unlikely(n_read == -1))
            Error("unexpected error while reading tar member data! (" + reader.getLastErrorMessage() + ")");

        while (more) {
            more = gunzip_streamer.decompress(nullptr, n_read, decompressed_data, sizeof(decompressed_data),
                                              &bytes_consumed, &bytes_produced);
            if (unlikely(output->write(decompressed_data, bytes_produced) != bytes_produced))
                Error("unexpected error while writing to \"" + output->getPath() + "\"!");
        }
    }

    if (verbose)
        std::cerr << "The tarball contained " << member_count << " entries.\n";
}


int main(int argc, char *argv[]) {
    ::progname = argv[0];

    if (argc != 3 and argc != 4)
        Usage();

    bool verbose(false);
    if (argc == 4) {
        if (std::strcmp(argv[1], "--verbose") != 0)
            Usage();
        verbose = true;
        --argc, ++argv;
    }

    const std::unique_ptr<File> output(FileUtil::OpenOutputFileOrDie(argv[2]));

    try {
        ProcessTarball(verbose, argv[1], output.get());
    } catch (const std::exception &x) {
        Error("caught exception: " + std::string(x.what()));
    }
}

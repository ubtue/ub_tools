/** \brief Importer for full text documents.
 *  \author Dr. Johannes Ruscheinski (johannes.ruscheinski@uni-tuebingen.de)
 *
 *  \copyright 2018,2019 Universitätsbibliothek Tübingen.  All rights reserved.
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
#include <stdexcept>
#include <vector>
#include <cstdio>
#include <cstdlib>
#include "ControlNumberGuesser.h"
#include "FileUtil.h"
#include "FullTextCache.h"
#include "FullTextImport.h"
#include "util.h"


namespace {


[[noreturn]] void Usage() {
    ::Usage("[--force-overwrite] [--set-publisher-provided] [--verbose] fulltext_file1  [fulltext_file2 .. fulltext_fileN]");
}


bool ImportDocument(const ControlNumberGuesser &control_number_guesser, FullTextCache * const full_text_cache, const std::string &filename,
                    const bool force_overwrite = false, const bool is_publisher_provided = false, const bool verbose = false) {
    const auto input(FileUtil::OpenInputFileOrDie(filename));
    FullTextImport::FullTextData full_text_data;
    FullTextImport::ReadExtractedTextFromDisk(input.get(), &full_text_data);

    std::string ppn;
    if (not FullTextImport::CorrelateFullTextData(control_number_guesser, full_text_data, &ppn)) {
        if (verbose)
            LOG_INFO("Could not correlate data for file \"" + filename + "\"");
        return false;
    }
    FullTextCache::Entry entry;
    const bool entry_present(full_text_cache->hasEntryWithType(ppn, FullTextCache::FULLTEXT));
    if (not force_overwrite and entry_present) {
        LOG_WARNING("Skip inserting PPN \"" + ppn + "\" since entry already present");
        return true;
    }

    if (entry_present)
        full_text_cache->deleteEntry(ppn);

    full_text_cache->insertEntry(ppn, full_text_data.full_text_, /* entry_urls = */ {}, FullTextCache::FULLTEXT, is_publisher_provided);
    if (not full_text_data.full_text_location_.empty())
        full_text_cache->extractPDFAndImportHTMLPages(ppn, full_text_data.full_text_location_);
    LOG_INFO("Inserted text from \"" + filename + "\" as entry for PPN \"" + ppn + "\"");

    return true;
}


} // unnamed namespace


int Main(int argc, char *argv[]) {
    bool force_overwrite(false);
    if (argc > 1 and std::strcmp(argv[1], "--force-overwrite") == 0) {
        force_overwrite = true;
        --argc;
        ++argv;
    }

    bool publisher_provided(false);
    if (argc > 1 and std::strcmp(argv[1], "--set-publisher-provided") == 0) {
        publisher_provided = true;
        --argc;
        ++argv;
    }

    bool verbose(false);
    if (argc > 1 and std::strcmp(argv[1], "--verbose") == 0) {
        verbose = true;
        --argc;
        ++argv;
    }
    if (argc < 2)
        Usage();
    ControlNumberGuesser control_number_guesser;
    FullTextCache full_text_cache;

    unsigned total_count(0), failure_count(0);
    for (int arg_no(1); arg_no < argc; ++arg_no) {
        ++total_count;
        if (not ImportDocument(control_number_guesser, &full_text_cache, argv[arg_no], force_overwrite, publisher_provided, verbose))
            ++failure_count;
    }

    LOG_INFO("Failed to import " + std::to_string(failure_count) + " documents of " + std::to_string(total_count) + ".");

    return EXIT_SUCCESS;
}

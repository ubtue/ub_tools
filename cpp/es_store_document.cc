/** \brief Utility for indexing documents with Elasticsearch.
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
#include <stdexcept>
#include <cstdlib>
#include "Downloader.h"
#include "FileUtil.h"
#include "JSON.h"
#include "util.h"


namespace {


static void Usage() __attribute__((noreturn));

static std::string DEFAULT_SERVER_URL("http://localhost:9200");


static void Usage() {
    std::cerr
        << "Usage: " << ::progname
        << " [--debug] [--server-url=url] --title=title_data (--text=document_contents|--text-from-file=path) other_fields_to_submit\n"
        << "       If not specified with \"--server\" the default server URL is \"" << DEFAULT_SERVER_URL << "\".\n"
        << "       other_fields_to_submit must have the format --field-name=field_value.  \"field-name\" can be any\n"
        << "       name except for \"title\", \"text\", or \"text-from-file\".\n\n";
    std::exit(EXIT_FAILURE);
}


} // unnamed namespace


int main(int argc, char *argv[]) {
    ::progname = argv[0];

    bool debug(false);
    if (argc > 1 and std::strcmp(argv[1], "--debug") == 0) {
        debug = true;
        --argc, ++argv;
    }

    std::string server_url(DEFAULT_SERVER_URL);
    if (argc > 1 and std::strncmp(argv[1], "--server-url=", __builtin_strlen("--server-url=")) == 0) {
        server_url = argv[1] + __builtin_strlen("--server-url=");
        --argc, ++argv;
    }

    if (argc < 3)
        Usage();

    try {
        std::string put_data("{\n");

        if (std::strncmp(argv[1], "--title=", __builtin_strlen("--title=")) != 0)
            LOG_ERROR("missing title field!");
        put_data += "\t\"title\": \"" + std::string(argv[1] + __builtin_strlen("--title=")) + "\",\n";

        if (std::strncmp(argv[2], "--text=", __builtin_strlen("--text=")) != 0
            and std::strncmp(argv[2], "--text-from-file=", __builtin_strlen("--text-from-file=")) != 0)
            LOG_ERROR("missing text or text-from-file field!");
        if (std::strncmp(argv[2], "--text=", __builtin_strlen("--text=")) == 0)
            put_data += "\t\"text\": \"" + std::string(argv[2] + __builtin_strlen("--text=")) + "\"";
        else { // text-from-file
            const std::string load_path(argv[2] + __builtin_strlen("--text-from-file="));
            std::string text;
            if (not FileUtil::ReadString(load_path, &text))
                LOG_ERROR("failed to read text from \"" + load_path + "\"!");
            put_data += "\t\"text\": \"" + JSON::EscapeString(text) + "\"";
        }

        for (int arg_no(3); arg_no < argc; ++arg_no) {
            const char * const first_equal_sign(std::strchr(argv[arg_no], '='));
            if (std::strncmp(argv[arg_no], "--", 2) != 0 or first_equal_sign == nullptr)
                LOG_ERROR("arguments must start with a double dash and contain an equal sign!");
            put_data += ",\n\t\"";
            put_data += std::string(argv[arg_no] + 2, first_equal_sign - argv[arg_no] - 2) + "\": \"";
            put_data += first_equal_sign + 1;
            put_data += '"';
        }

        put_data += "\n}\n";
        if (debug)
            std::cout << put_data;
        else {
            Downloader downloader;
            if (not downloader.putData(server_url, put_data))
                LOG_ERROR(downloader.getLastErrorMessage());
        }
    } catch (const std::exception &e) {
        LOG_ERROR("caught exception: " + std::string(e.what()));
    }
}

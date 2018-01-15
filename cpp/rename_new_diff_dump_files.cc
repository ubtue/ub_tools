/** \file    rename_new_diff_dump_files.cc
 *  \brief   Throw-away utility to deal with renaming experimental files.
 *  \author  Dr. Johannes Ruscheinski
 */

/*
    Copyright (C) 2018 Library of the University of Tübingen

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU Affero General Public License as
    published by the Free Software Foundation, either version 3 of the
    License, or (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Affero General Public License for more details.

    You should have received a copy of the GNU Affero General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <iostream>
#include <stdexcept>
#include <vector>
#include <cstdlib>
#include <cstring>
#include "FileUtil.h"
#include "StringUtil.h"
#include "util.h"


namespace {


std::string default_email_recipient;
std::string email_server_address;
std::string email_server_user;
std::string email_server_password;


void Usage() {
    std::cerr << "Usage: " << ::progname << '\n';
    std::exit(EXIT_FAILURE);
}


unsigned GetListOfFilesToRename(std::vector<std::string> * const filenames) {
    filenames->clear();

    FileUtil::Directory directory(".", "^(TA-MARC-ixtheo1|TA-MARC-ixtheo_o1)-(\\d\\d\\d\\d\\d\\d).tar.gz$");
    for (const auto entry : directory) {
        const int entry_type(entry.getType());
        if (entry_type == DT_REG or entry_type == DT_UNKNOWN)
            filenames->emplace_back(entry.getName());
    }

    return filenames->size();
}


void RenameFile(const std::string &filename) {
    if (StringUtil::StartsWith(filename, "TA-MARC-ixtheo1"))
        FileUtil::RenameFileOrDie(filename, "TA-MARC-ixtheo" + filename.substr(__builtin_strlen("TA-MARC-ixtheo1")));
    else
        FileUtil::RenameFileOrDie(filename, "TA-MARC-ixtheo_o" + filename.substr(__builtin_strlen("TA-MARC-ixtheo_o1")));
}


} // unnamed namespace


int main(int argc, char *argv[]) {
    ::progname = argv[0];
    if (argc != 1)
        Usage();

    try {
        std::vector<std::string> filenames;
        GetListOfFilesToRename(&filenames);
        for (const auto &filename : filenames)
            RenameFile(filename);
    } catch (const std::exception &x) {
        logger->error("caught exception: " + std::string(x.what()));
    }
}

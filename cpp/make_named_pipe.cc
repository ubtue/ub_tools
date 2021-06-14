/** \file    make_named_pipe.cc
 *  \brief   Creates a FIFO in the file system.
 *  \author  Dr. Johannes Ruscheinski
 */

/*
    Copyright (C) 2021 Library of the University of Tübingen

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
#include <fcntl.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include "StringUtil.h"
#include "util.h"


[[noreturn]] void Usage() {
    ::Usage("[--buffer-size=buffer_size_in_bytes|--show-buffer-size] path");
}


int main(int argc, char *argv[]) {
    if (argc != 2 and argc != 3)
        Usage();

    bool show_buffer_size(false);
    int buffer_size(1048576); // NB: the fcntl system call requires an "int".
    if (argc == 3) {
        if (__builtin_strcmp(argv[1], "--show-buffer-size") == 0)
            show_buffer_size = true;
        else if (StringUtil::StartsWith(argv[1], "--buffer-size="))
            buffer_size = StringUtil::ToInt(argv[1] + __builtin_strlen("--buffer-size="));
        else
            Usage();
        --argc, ++argv;
    }

    const std::string filename(argv[1]);
    if (show_buffer_size) {
        const auto fd(::open(filename.c_str(), O_RDWR));
        if (fd == -1)
            LOG_ERROR("failed to open \"" + filename + "\" for reading!");
        std::cout << ::fcntl(fd, F_GETPIPE_SZ) << '\n';
    } else {
        ::unlink(filename.c_str());

        if (::mkfifo(filename.c_str(), 0600) != 0)
            LOG_ERROR("mkfifo(3) failed!");

        const auto fd(::open(filename.c_str(), O_RDWR));
        const int actual_buffer_size(::fcntl(fd, F_SETPIPE_SZ, buffer_size));
        if (actual_buffer_size < buffer_size)
            LOG_ERROR("failed to set the FIFO buffer size to at least \"" + std::to_string(buffer_size)
                      + " on \"" + filename + "\"!");
        else if (actual_buffer_size > buffer_size)
            LOG_ERROR("actually set the FIFO buffer size to \"" + std::to_string(actual_buffer_size)
                      + " on \"" + filename + "\"!");
    }

    return EXIT_SUCCESS;
}

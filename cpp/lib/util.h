/** \file   util.h
 *  \brief  Various utility functions that did not seem to logically fit anywhere else.
 *  \author Dr. Johannes Ruscheinski (johannes.ruscheinski@uni-tuebingen.de)
 *
 *  \copyright 2014 Universitätsbiblothek Tübingen.  All rights reserved.
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
#ifndef UTIL_H
#define UTIL_H


#include <string>
#include <vector>
#include <cstdio>


/** Must be set to point to argv[0] in main(). */
extern char *progname;


/** Emits "msg" on stderr and then calls exit(3). */
void Error(const std::string &msg) __attribute__((noreturn));


/** Emits "msg" on stderr. */
void Warning(const std::string &msg);


bool ReadFile(const std::string &filename, std::string * const contents);


/** \class DSVReader
 *  \brief A "reader" for delimiter-separated values.
 */
class DSVReader {
    char field_separator_;
    char field_delimiter_;
    unsigned line_no_;
    std::string filename_;
    FILE *input_;
public:
    explicit DSVReader(const std::string &filename, const char field_separator=',', const char field_delimiter='"');
    ~DSVReader();
    bool readLine(std::vector<std::string> * const values);
};


#endif // ifndef UTIL_H

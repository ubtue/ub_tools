/** \file   FileUtil.h
 *  \brief  File related utility classes and functions.
 *  \author Dr. Johannes Ruscheinski (johannes.ruscheinski@uni-tuebingen.de)
 *
 *  \copyright 2015 Universitätsbiblothek Tübingen.  All rights reserved.
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
#ifndef FILE_UTIL_H
#define FILE_UTIL_H


#include <string>
#include <unistd.h>


/** \class AutoDeleteFile
 *  \brief Deletes the file, specified by the path that was given in the constructor, when going out of scope.
 */
class AutoDeleteFile {
    std::string path_;
public:
    explicit AutoDeleteFile(const std::string &path): path_(path) { }
    ~AutoDeleteFile() { ::unlink(path_.c_str()); }
};


bool WriteString(const std::string &filename, const std::string &data);


#endif // ifndef FILE_UTIL_H

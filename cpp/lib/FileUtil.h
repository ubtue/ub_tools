/** \file    FileUtil.h
 *  \brief   Declaration of file-related utility functions.
 *  \author  Dr. Gordon W. Paynter
 *  \author  Dr. Johannes Ruscheinski
 *  \author  Artur Kedzierski
 */

/*
 *  Copyright 2002-2008 Project iVia.
 *  Copyright 2002-2008 The Regents of The University of California.
 *  Copyright 2015 Universitätsbiblothek Tübingen.  All rights reserved.
 *
 *  This file is part of the libiViaCore package.
 *
 *  The libiViaCore package is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public License as
 *  published by the Free Software Foundation; either version 2 of the License,
 *  or (at your option) any later version.
 *
 *  libiViaCore is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public License
 *  along with libiViaCore; if not, write to the Free Software Foundation, Inc.,
 *  59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
#ifndef FILE_UTIL_H
#define FILE_UTIL_H


#include <string>
#include <sys/types.h>
#include <unistd.h>


namespace FileUtil {


/** \class AutoDeleteFile
 *  \brief Deletes the file, specified by the path that was given in the constructor, when going out of scope.
 */
class AutoDeleteFile {
    std::string path_;
public:
    explicit AutoDeleteFile(const std::string &path): path_(path) { }
    ~AutoDeleteFile() { ::unlink(path_.c_str()); }
};


/** \return The size of the file named by "path".
 *  \note   Exits with an error message if "path" does not exist or we don't have the rights to stat it.
 */
off_t GetFileSize(const std::string &path);
    

/** \class AutoTempFile
 *  \brief Creates a temp file and removes it when going out of scope.
 */
class AutoTempFile {
    std::string path_;
public:
    explicit AutoTempFile(const std::string &path_prefix = "/tmp/AutoTempFile");
    ~AutoTempFile() { if (not path_.empty()) ::unlink(path_.c_str()); }

    const std::string &getFilePath() const { return path_; }
};
    

bool WriteString(const std::string &path, const std::string &data);
bool ReadString(const std::string &path, std::string * const data);


/** \brief  Does the named file (or directory) exist?.
 *  \param  path           The path of the file.
 *  \param  error_message  Where to store an error message if an error occurred.
 */
bool Exists(const std::string &path, std::string * const error_message = nullptr);


/** \brief  Makes a relative path absolute using an absolute reference path.
 *  \param  reference_path  An absolute path to use as the reference path for "relative_path".
 *  \param  relative_path   The path to make absolute (unless it already starts with a slash).
 *  \note   Unless "reference_path" path ends in a slash the last component is stripped off unconditionally.
 *          So if you plan to use all of "reference_path" as the path prefix for "relative_path" you must
 *          ensure that it ends in a slash!
 *  \return The absolute path equivalent of "relative_path".
 */
std::string MakeAbsolutePath(const std::string &reference_path, const std::string &relative_path);


/** Makes "relative_path" absolute using the current working directory as the reference path. */
std::string MakeAbsolutePath(const std::string &relative_path);


inline std::string MakeAbsolutePath(const char * const relative_path)
{ return MakeAbsolutePath(std::string(relative_path)); }


} // namespace FileUtil


#endif // ifndef FILE_UTIL_H

/** \file   FileUtil.cc
 *  \brief  Implementation of file related utility classes and functions.
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
#include "FileUtil.h"
#include <fstream>
#include <list>
#include <stdexcept>
#include <cassert>
#include <climits>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include "Compiler.h"
#include "util.h"


namespace FileUtil {


AutoTempFile::AutoTempFile(const std::string &path_prefix) {
    char * const path_template(strdupa((path_prefix + "XXXXXX").c_str()));
    const int fd(::mkstemp(path_template));
    if (fd == -1)
        throw std::runtime_error("in AutoTempFile::AutoTempFile: mkstemp(3) failed!");
    ::close(fd);
    path_ = path_template;
}


off_t GetFileSize(const std::string &path) {
    struct stat stat_buf;
    if (::stat(path.c_str(), &stat_buf) == -1)
        Error("in FileUtil::GetFileSize: can't stat(2) \"" + path + "\"!");

    return stat_buf.st_size;
}


bool WriteString(const std::string &path, const std::string &data) {
    std::ofstream output(path, std::ios_base::out | std::ios_base::binary | std::ios_base::trunc);
    if (output.fail())
        return false;

    output.write(data.data(), data.size());
    return not output.bad();
}
    

bool ReadString(const std::string &path, std::string * const data) {
    std::ifstream input(path, std::ios_base::in | std::ios_base::binary);
    if (input.fail())
        return false;

    const off_t file_size(GetFileSize(path));
    data->resize(file_size);
    input.read(const_cast<char *>(data->data()), file_size);
    return not input.bad();

}
    

// DirnameAndBasename -- Split a path into a directory name part and filename part.
//
void DirnameAndBasename(const std::string &path, std::string * const dirname, std::string * const basename) {
    if (unlikely(path.length() == 0)) {
        *dirname = *basename = "";
        return;
    }

    std::string::size_type last_slash_pos = path.rfind('/');
    if (last_slash_pos == std::string::npos) {
        *dirname  = "";
        *basename = path;
    }
    else {
        *dirname  = path.substr(0, last_slash_pos);
        *basename = path.substr(last_slash_pos + 1);
    }
}


// AccessErrnoToString -- Converts an errno set by access(2) to a string.
//                        The string values were copied and pasted from a Linux man page.
//
std::string AccessErrnoToString(int errno_to_convert, const std::string &pathname, const std::string &mode) {
    switch (errno_to_convert) {
    case 0: // Just in case...
        return "OK";
    case EACCES:
        return "The requested access would be denied to the file or search"
            " permission is denied to one of the directories in '" + pathname + "'";
    case EROFS:
        return "Write  permission  was  requested  for  a  file  on  a read-only filesystem.";
    case EFAULT:
        return "'" + pathname + "' points outside your accessible address space.";
    case EINVAL:
        return mode + " was incorrectly specified.";
    case ENAMETOOLONG:
        return "'" + pathname + "' is too long.";
    case ENOENT:
        return "A directory component in '" + pathname + "' would have been accessible but"
            " does not exist or was a dangling symbolic link.";
    case ENOTDIR:
        return "A component used as a directory in '" + pathname + "' is not, in fact, a directory.";
    case ENOMEM:
        return "Insufficient kernel memory was available.";
    case ELOOP:
        return "Too many symbolic links were encountered in resolving '" + pathname + "'.";
    case EIO:
        return "An I/O error occurred.";
    }

    throw std::runtime_error("Unknown errno code in FileUtil::AccessErrnoToString");
}


// Exists -- test whether a file exists
//
bool Exists(const std::string &path, std::string * const error_message) {
    errno = 0;
    int access_status = ::access(path.c_str(), F_OK);
    if (error_message != nullptr)
        *error_message = AccessErrnoToString(errno, path, "F_OK");

    return (access_status == 0);
}


namespace {


void MakeCanonicalPathList(const char * const path, std::list<std::string> * const canonical_path_list) {
    canonical_path_list->clear();

    const char *cp = path;
    if (*cp == '/') {
        canonical_path_list->push_back("/");
        ++cp;
    }

    while (*cp != '\0') {
        std::string directory;
        while (*cp != '\0' and *cp != '/')
            directory += *cp++;
        if (*cp == '/')
            ++cp;

        if (directory.empty() or directory == ".")
            continue;

        if (directory == ".." and not canonical_path_list->empty()) {
            if (canonical_path_list->size() != 1 or canonical_path_list->front() != "/")
                canonical_path_list->pop_back();
        }
        else
            canonical_path_list->push_back(directory);
    }
}


std::string ErrnoToString(const int error_code) {
    char buf[1024];
    return ::strerror_r(error_code, buf, sizeof buf); // GNU version of strerror_r.
}


} // unnamed namespace


std::string CanonisePath(const std::string &path)
{
    std::list<std::string> canonical_path_list;
    MakeCanonicalPathList(path.c_str(), &canonical_path_list);

    std::string canonised_path;
    for (std::list<std::string>::const_iterator path_component(canonical_path_list.begin());
         path_component != canonical_path_list.end(); ++path_component)
        {
            if (not canonised_path.empty() and canonised_path != "/")
                canonised_path += '/';
            canonised_path += *path_component;
        }

    return canonised_path;
}


std::string MakeAbsolutePath(const std::string &reference_path, const std::string &relative_path) {
    assert(not reference_path.empty() and reference_path[0] == '/');

    if (relative_path[0] == '/')
        return relative_path;

    std::string reference_dirname, reference_basename;
    DirnameAndBasename(reference_path, &reference_dirname, &reference_basename);

    std::list<std::string> resultant_dirname_components;
    MakeCanonicalPathList(reference_dirname.c_str(), &resultant_dirname_components);

    std::string relative_dirname, relative_basename;
    DirnameAndBasename(relative_path, &relative_dirname, &relative_basename);
    std::list<std::string> relative_dirname_components;
    MakeCanonicalPathList(relative_dirname.c_str(), &relative_dirname_components);

    // Now merge the two canonical path lists.
    for (std::list<std::string>::const_iterator component(relative_dirname_components.begin());
         component != relative_dirname_components.end(); ++component)
    {
        if (*component == ".." and (resultant_dirname_components.size() > 1 or
                                    resultant_dirname_components.front() != "/"))
            resultant_dirname_components.pop_back();
        else
            resultant_dirname_components.push_back(*component);
    }

    // Build the final path:
    std::string canonized_path;
    std::list<std::string>::const_iterator dir(resultant_dirname_components.begin());
    if (dir != resultant_dirname_components.end() and *dir == "/") {
        canonized_path = "/";
        ++dir;
    }
    for (/* empty */; dir != resultant_dirname_components.end(); ++dir)
        canonized_path += *dir + "/";
    canonized_path += relative_basename;

    return canonized_path;
}


std::string MakeAbsolutePath(const std::string &relative_path) {
    char buf[PATH_MAX];
    const char * const current_working_dir(::getcwd(buf, sizeof buf));
    if (unlikely(current_working_dir == nullptr))
        throw std::runtime_error("in FileUtil::MakeAbsolutePath: getcwd(3) failed (" + ErrnoToString(errno) + ")!");
    return MakeAbsolutePath(std::string(current_working_dir) + "/", relative_path);
}


bool MakeEmpty(const std::string &path) {
    int fd;
    if ((fd = ::open(path.c_str(), O_CREAT|O_TRUNC, S_IRUSR|S_IWUSR)) == -1)
	return false;

    ::close(fd);
    return true;
}
    

} // namespace FileUtil

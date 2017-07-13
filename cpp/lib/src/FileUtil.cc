/** \file   FileUtil.cc
 *  \brief  Implementation of file related utility classes and functions.
 *  \author Dr. Johannes Ruscheinski (johannes.ruscheinski@uni-tuebingen.de)
 *
 *  \copyright 2015-2017 Universitätsbibliothek Tübingen.  All rights reserved.
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
#include <memory>
#include <stdexcept>
#include <cassert>
#include <climits>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dirent.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include "Compiler.h"
#include "SocketUtil.h"
#include "StringUtil.h"
#include "RegexMatcher.h"
#include "StringUtil.h"
#include "util.h"


namespace FileUtil {


AutoTempFile::AutoTempFile(const std::string &path_prefix) {
    std::string path_template(path_prefix + "XXXXXX");
    const int fd(::mkstemp(const_cast<char *>(path_template.c_str())));
    if (fd == -1)
        throw std::runtime_error("in AutoTempFile::AutoTempFile: mkstemp(3) for path prefix \"" + path_prefix
                                 + "\" failed! (" + std::string(::strerror(errno)) + ")");
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


bool AppendString(const std::string &path, const std::string &data) {
    std::ofstream output(path, std::ios_base::out | std::ios_base::binary | std::ios_base::app);
    if (output.fail())
        return false;

    output.write(data.data(), data.size());
    return not output.bad();
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
        return "Write permission was requested for a file on a read-only filesystem.";
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
    errno = 0;
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


std::string GetFileName(const int fd) {
    char proc_path[25];
    std::sprintf(proc_path, "/proc/self/fd/%d", fd);
    struct stat stat_buf;
    if (::lstat(proc_path, &stat_buf) == -1)
        std::runtime_error("in FileUtil::GetFileName: lstat(2) failed on \"" + std::string(proc_path)
                           + "\"! (errno = " + std::to_string(errno) + ")");
    char * const linkname(reinterpret_cast<char *>(std::malloc(stat_buf.st_size + 1)));
    if (linkname == nullptr)
        std::runtime_error("in FileUtil::GetFileName: malloc(3) failed!");
    const ssize_t link_size(::readlink(proc_path, linkname, stat_buf.st_size + 1));
    if (link_size == -1)
        std::runtime_error("in FileUtil::GetFileName: readlink(2) failed on \"" + std::string(proc_path)
                           + "\"! (errno = " + std::to_string(errno) + ")");
    if (link_size > stat_buf.st_size)
        std::runtime_error("in FileUtil::GetFileName: symlink increased in size between call to lstat(2) and readlink(2)!");
    const std::string filename(linkname);
    std::free(linkname);

    return filename;
}


bool SetNonblocking(const int fd) {
    // First, retrieve current settings:
    int flags = ::fcntl(fd, F_GETFL, 0);
    if (flags == -1)
        return false;

    flags |= O_NONBLOCK;

    return ::fcntl(fd, F_SETFL, flags) != -1;
}


bool SetBlocking(const int fd) {
    // First, retrieve current settings:
    int flags = ::fcntl(fd, F_GETFL, 0);
    if (flags == -1)
        return false;

    flags &= ~O_NONBLOCK;

    return ::fcntl(fd, F_SETFL, flags) != -1;
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
    } else {
        *dirname  = path.substr(0, last_slash_pos);
        *basename = path.substr(last_slash_pos + 1);
    }
}


// IsDirectory -- Is the specified file a directory?
//
bool IsDirectory(const std::string &dir_name) {
    struct stat statbuf;
    if (::stat(dir_name.c_str(), &statbuf) != 0)
        return false;

    return S_ISDIR(statbuf.st_mode);
}


// MakeDirectory -- Create a directory.
//
bool MakeDirectory(const std::string &path, const bool recursive, const mode_t mode) {
    const bool absolute(path[0] == '/' ? true : false);
    // In NON-recursive mode we make a single attempt to create the directory:
    if (not recursive) {
        errno = 0;
        if (::mkdir(path.c_str(), mode) == 0)
            return true;
        const bool dir_exists(errno == EEXIST and IsDirectory(path));
        if (dir_exists)
            errno = 0;
        return dir_exists;
    }

    std::vector<std::string> path_components;
    StringUtil::Split(path, '/', &path_components);

    std::string path_so_far;
    if (absolute)
        path_so_far += "/";
    for (std::vector<std::string>::const_iterator path_component(path_components.begin());
         path_component != path_components.end(); ++path_component)
    {
        path_so_far += *path_component;
        path_so_far += '/';
        errno = 0;
        if (::mkdir(path_so_far.c_str(), mode) == -1 and errno != EEXIST)
            return false;
        if (errno == EEXIST and not IsDirectory(path_so_far))
            return false;
    }

    return true;
}


static void CloseDirWhilePreservingErrno(DIR * const dir_handle) {
    const int old_errno(errno);
    ::closedir(dir_handle);
    errno = old_errno;
}


bool RemoveDirectory(const std::string &dir_name) {
    DIR *dir_handle(::opendir(dir_name.c_str()));
    if (unlikely(dir_handle == nullptr))
        return false;

    struct dirent *entry;
    while ((entry = ::readdir(dir_handle)) != nullptr) {
        if (std::strcmp(entry->d_name, ".") == 0 or std::strcmp(entry->d_name, "..") == 0)
            continue;

        const std::string path(dir_name + "/" + std::string(entry->d_name));

        if (entry->d_type == DT_DIR) {
            if (unlikely(not RemoveDirectory(path))) {
                CloseDirWhilePreservingErrno(dir_handle);
                return false;
            }
        } else
            ::unlink(path.c_str());

        if (unlikely(errno != 0)) {
            CloseDirWhilePreservingErrno(dir_handle);
            return false;
        }
    }
    if (unlikely(errno != 0)) { // readdir(2) failed!
        CloseDirWhilePreservingErrno(dir_handle);
        return false;
    }

    if (::unlikely(::rmdir(dir_name.c_str()) != 0)) {
        CloseDirWhilePreservingErrno(dir_handle);
        return false;
    }

    return likely(::closedir(dir_handle) == 0);
}


ssize_t RemoveMatchingFiles(const std::string &filename_regex, const bool include_directories,
                            const std::string &directory_to_scan)
{
    if (unlikely(filename_regex.find('/') != std::string::npos))
        throw std::runtime_error("in FileUtil::RemoveMatchingFiles: filename regex contained a slash!");

    std::string err_msg;
    std::unique_ptr<RegexMatcher> matcher(RegexMatcher::RegexMatcherFactory(filename_regex, &err_msg));
    if (unlikely(not err_msg.empty()))
        throw std::runtime_error("in FileUtil::RemoveMatchingFiles: failed to compile regular expression \"" + filename_regex
                                 + "\"! (" + err_msg + ")");

    DIR *dir_handle(::opendir(directory_to_scan.c_str()));
    if (unlikely(dir_handle == nullptr))
        return -1;

    struct dirent *entry;
    ssize_t match_count(0);
    while ((entry = ::readdir(dir_handle)) != nullptr) {
        if (std::strcmp(entry->d_name, ".") == 0 or std::strcmp(entry->d_name, "..") == 0)
            continue;

        if (not matcher->matched((entry->d_name)))
            continue;

        const std::string path(directory_to_scan + "/" + std::string(entry->d_name));
        if (entry->d_type == DT_DIR) {
            if (unlikely(not include_directories or not RemoveDirectory(path))) {
                CloseDirWhilePreservingErrno(dir_handle);
                return -1;
            }
        } else
            ::unlink(path.c_str());

        if (unlikely(errno != 0)) {
            CloseDirWhilePreservingErrno(dir_handle);
            return false;
        }

        ++match_count;
    }
    if (unlikely(errno != 0)) { // readdir(2) failed!
        CloseDirWhilePreservingErrno(dir_handle);
        return -1;
    }

    if (unlikely(::closedir(dir_handle) != 0))
        return -1;
    return match_count;
}


bool Rewind(const int fd) {
    return ::lseek(fd, 0, SEEK_SET) == 0;
}


FileUtil::FileType GuessFileType(const std::string &filename) {
    if (filename.empty())
        return FILE_TYPE_UNKNOWN;

    // Cannot guess a mime type without an extension:
    const std::string::size_type extension_pos = filename.rfind('.');
    if (extension_pos == std::string::npos)
        return FILE_TYPE_UNKNOWN;

    std::string file_extension = filename.substr(extension_pos + 1);
    StringUtil::ToLower(&file_extension);
    if (file_extension.find("htm") != std::string::npos) // .phtml, .shtml, .html
        return FILE_TYPE_HTML;

    FileUtil::FileType file_type = FILE_TYPE_UNKNOWN;
    switch (file_extension[0]) {
    case 'c':
        if (file_extension == "c" or file_extension == "cc" or file_extension == "cpp"
            or file_extension == "cxx")
            file_type = FILE_TYPE_CODE;
        else if (file_extension == "cgi")
            file_type = FILE_TYPE_HTML;
        break;
    case 'd':
        if (file_extension == "dvi")
            file_type = FILE_TYPE_DVI;
        else if (file_extension == "divx")
            file_type = FILE_TYPE_MOVIE;
        else if (file_extension == "doc")
            file_type = FILE_TYPE_DOC;
        break;
    case 'e':
        if (file_extension == "eps")
            file_type = FILE_TYPE_PS;
        break;
    case 'g':
        if (file_extension == "gif")
            file_type = FILE_TYPE_GRAPHIC;
        else if (file_extension == "gz")
            file_type = FILE_TYPE_GZIP;
        break;
    case 'h':
        if (file_extension == "h")
            file_type = FILE_TYPE_CODE;
        break;
    case 'j':
        if (file_extension == "jpg")
            file_type = FILE_TYPE_GRAPHIC;
        break;
    case 'p':
        switch (file_extension[1]) {
        case 'd':
            if (file_extension == "pdf")
                file_type = FILE_TYPE_PDF;
            break;
        case 'h':
            if (file_extension == "phtml") // serverside parsed html
                file_type = FILE_TYPE_HTML;
            else if (file_extension == "php") //
                file_type = FILE_TYPE_HTML;
            break;
        case 'l':
            if (file_extension == "pl")
                file_type = FILE_TYPE_HTML; // it might be a source code too!
        case 'n':
            if (file_extension == "png")
                file_type = FILE_TYPE_GRAPHIC;
            break;
        case 'p':
            if (file_extension == "ppt")
                file_type = FILE_TYPE_SLIDES;
            break;
        case 's':
            if (file_extension == "ps")
                file_type = FILE_TYPE_PS;
            break;
        case 'y':
            if (file_extension == "py")
                file_type = FILE_TYPE_HTML; // it might be a source code too!
            break;
        }
        break;
    case 'r':
        if (file_extension == "rtf")
            file_type = FILE_TYPE_RTF;
        break;
    case 's':
        if (file_extension == "sxi")
            file_type = FILE_TYPE_SLIDES;
        else if (file_extension == "sxw")
            file_type = FILE_TYPE_DOC;
        break;
    case 't':
        switch (file_extension[1]) {
        case 'a':
            if (file_extension == "tar")
                file_type = FILE_TYPE_TAR;
            break;
        case 'e':
            if (file_extension == "tex")
                file_type = FILE_TYPE_TEX;
            break;
        case 'g':
            if (file_extension == "tgz")
                file_type = FILE_TYPE_GZIP;
            break;
        case 'x':
            if (file_extension == "txt")
                file_type = FILE_TYPE_TEXT;
            break;
        }
        break;
    case 'x':
        if (file_extension == "xhtml") // serverside parsed html.
            file_type = FILE_TYPE_HTML;
        break;
    }

    return file_type;
}


std::string FileTypeToString(const FileType file_type) {
    switch (file_type) {
    case FILE_TYPE_UNKNOWN:
        return "unknown";
    case FILE_TYPE_TEXT:
        return "text";
    case FILE_TYPE_HTML:
        return "html";
    case FILE_TYPE_PDF:
        return "pdf";
    case FILE_TYPE_PS:
        return "ps";
    case FILE_TYPE_DOC:
        return "doc";
    case FILE_TYPE_SLIDES:
        return "slides";
    case FILE_TYPE_TEX:
        return "tex";
    case FILE_TYPE_DVI:
        return "dvi";
    case FILE_TYPE_TAR:
        return "tar";
    case FILE_TYPE_RTF:
        return "rtf";
    case FILE_TYPE_GZIP:
        return "gzip";
    case FILE_TYPE_Z:
        return "z";
    case FILE_TYPE_CODE:
        return "code";
    case FILE_TYPE_GRAPHIC:
        return "graphics";
    case FILE_TYPE_AUDIO:
        return "audio";
    case FILE_TYPE_MOVIE:
        return "movie";
    default:
        throw std::runtime_error("in FileUtil::FileTypeToString: Unknown file type!");
    }
}


size_t GetFileNameList(const std::string &filename_regex, std::vector<std::string> * const matched_filenames,
                       const std::string &directory_to_scan)
{
    if (unlikely(filename_regex.find('/') != std::string::npos))
        throw std::runtime_error("in FileUtil::GetFileNameList: filename regex contained a slash!");
    std::string err_msg;
    std::unique_ptr<RegexMatcher> matcher(RegexMatcher::RegexMatcherFactory(filename_regex, &err_msg));
    if (unlikely(not err_msg.empty()))
        throw std::runtime_error("in FileUtil::GetFileNameList: failed to compile regular expression \"" + filename_regex
                                 + "\"! (" + err_msg + ")");

    DIR *dir_handle(::opendir(directory_to_scan.c_str()));
    if (unlikely(dir_handle == nullptr))
        throw std::runtime_error("in FileUtil::GetFileNameList: failed to opendir(2) \"" + directory_to_scan + "\"! ("
                                 + std::string(::strerror(errno)) + ")");
    struct dirent *entry;
    while ((entry = ::readdir(dir_handle)) != nullptr) {
        if (matcher->matched(entry->d_name))
            matched_filenames->emplace_back(entry->d_name);
    }
    if (unlikely(errno != 0)) {
        CloseDirWhilePreservingErrno(dir_handle);
        throw std::runtime_error("in FileUtil::GetFileNameList: readdir(2) failed: " + std::string(::strerror(errno)));
    }

    if (unlikely(::closedir(dir_handle) != 0))
        throw std::runtime_error("in FileUtil::GetFileNameList: failed to opendir(2) \"" + directory_to_scan + "\"! ("
                                 + std::string(::strerror(errno)) + ")");

    return matched_filenames->size();
}


bool RenameFile(const std::string &old_name, const std::string &new_name, const bool remove_target) {
    struct stat stat_buf;
    if (::stat(new_name.c_str(), &stat_buf) == -1) {
        if (errno != ENOENT)
            throw std::runtime_error("in FileUtil::: stat(2) failed: " + std::string(::strerror(errno)));
    } else { // Target file or directory already exists!
        if (not remove_target) {
            errno = EEXIST;
            return false;
        }

        if (S_ISDIR(stat_buf.st_mode)) {
            if (unlikely(not RemoveDirectory(new_name)))
                return false;
        } else {
            if (unlikely(::unlink(new_name.c_str()) == -1))
                throw std::runtime_error("in FileUtil::: unlink(2) failed: " + std::string(::strerror(errno)));
        }
    }

    return ::rename(old_name.c_str(), new_name.c_str()) == 0;
}


std::unique_ptr<File> OpenInputFileOrDie(const std::string &filename) {
    std::unique_ptr<File> file(new File(filename, "r"));
    if (file->fail())
        Error("can't open \"" + filename + "\" for reading!");

    return file;
}


std::unique_ptr<File> OpenOutputFileOrDie(const std::string &filename) {
    std::unique_ptr<File> file(new File(filename, "w"));
    if (file->fail())
        Error("can't open \"" + filename + "\" for writing!");

    return file;
}


std::unique_ptr<File> OpenForAppeningOrDie(const std::string &filename) {
    std::unique_ptr<File> file(new File(filename, "a"));
    if (file->fail())
        Error("can't open \"" + filename + "\" for appending!");

    return file;
}


bool Copy(File * const from, File * const to, const size_t no_of_bytes) {
    errno = 0;
    std::string buffer;
    buffer.resize(no_of_bytes);
    if (unlikely(from->read((void *)buffer.data(), no_of_bytes) != no_of_bytes))
        return false;
    return to->write((void *)buffer.data(), no_of_bytes) == no_of_bytes;
}


void CopyOrDie(const std::string &from_path, const std::string &to_path) {
    const int from_fd(::open(from_path.c_str(), O_RDONLY));
    if (unlikely(from_fd == -1))
        Error("in FileUtil::CopyOrDie: failed to open \"" + from_path  + "\" for reading! ("
              + std::string(::strerror(errno)) + ")");

    const int to_fd(::open(to_path.c_str(), O_WRONLY));
    if (unlikely(to_fd == -1))
        Error("in FileUtil::CopyOrDie: failed to open \"" + to_path  + "\" for writing! ("
              + std::string(::strerror(errno)) + ")");

    char buf[BUFSIZ];
    for (;;) {
        const ssize_t no_of_bytes(::read(from_fd, &buf[0], sizeof(buf)));
        if (no_of_bytes == 0)
            break;

        if (unlikely(no_of_bytes < 0))
            Error("in FileUtil::CopyOrDie: read(2) failed! (" + std::string(::strerror(errno)) + ")");

        if (unlikely(::write(to_fd, &buf[0], no_of_bytes) != no_of_bytes))
            Error("in FileUtil::CopyOrDie: write(2) failed! (" + std::string(::strerror(errno)) + ")");
    }

    ::close(from_fd);
    ::close(to_fd);
}


bool DeleteFile(const std::string &path) {
    return ::unlink(path.c_str()) == 0;
}


bool DescriptorIsReadyForReading(const int fd, const TimeLimit &time_limit) {
    return SocketUtil::TimedRead(fd, time_limit, reinterpret_cast<void *>(NULL), 0) == 0;
}


bool DescriptorIsReadyForWriting(const int fd, const TimeLimit &time_limit) {
    return SocketUtil::TimedWrite(fd, time_limit, reinterpret_cast<void *>(NULL), 0) == 0;
}


bool GetLine(std::istream &stream, std::string * const line, const char terminator) {
    const std::string::size_type INITIAL_CAPACITY(128);
    line->clear();
    line->reserve(INITIAL_CAPACITY);

    int ch;
    for (ch = stream.get(); ch != EOF and ch != terminator; ch = stream.get()) {
        if (line->size() == line->capacity())
            line->reserve(2 * line->capacity());
        line->push_back(static_cast<char>(ch));
    }

    return ch != EOF;
}


std::string UniqueFileName(const std::string &directory, const std::string &filename_prefix,
			   const std::string &filename_suffix)
{
    static unsigned generation_number(1);

    // Set default for prefix if necessary.
    std::string prefix(filename_prefix);
    if (prefix.empty())
        prefix = ::progname;

    std::string suffix(filename_suffix);
    if (not suffix.empty() and suffix[0] != '.')
        suffix = "." + suffix;

    std::string dir(directory);
    if (dir.empty())
        dir = "/tmp";

    return (dir + "/" + prefix + "." +
            std::to_string(getpid()) + "." +
            std::to_string(generation_number++) + suffix);
}


bool FilesDiffer(const std::string &path1, const std::string &path2) {
    File input1(path1, "r");
    if (not input1)
        throw std::runtime_error("in FileUtil::FilesDiffer: failed to open \"" + path1 + "\" for reading!");

    File input2(path2, "r");
    if (not input2)
        throw std::runtime_error("in FileUtil::FilesDiffer: failed to open \"" + path2 + "\" for reading!");

    for (;;) {
        char buf1[BUFSIZ];
        const size_t read_count1(input1.read(reinterpret_cast<void *>(buf1), BUFSIZ));
        if (unlikely(read_count1 < BUFSIZ) and input1.anErrorOccurred())
            throw std::runtime_error("in FileUtil::FilesDiffer: an error occurred while trying to read \"" + path1
                                     + "\"!");

        char buf2[BUFSIZ];
        const size_t read_count2(input2.read(reinterpret_cast<void *>(buf2), BUFSIZ));
        if (unlikely(read_count2 < BUFSIZ) and input2.anErrorOccurred())
            throw std::runtime_error("in FileUtil::FilesDiffer: an error occurred while trying to read \"" + path2
                                     + "\"!");

        if (read_count1 != read_count2 or std::memcmp(buf1, buf2, BUFSIZ) != 0)
            return true;

        if (read_count1 < BUFSIZ)
            return false;
    }
}


void AppendStringToFile(const std::string &path, const std::string &text) {
    std::unique_ptr<File> file(OpenForAppeningOrDie(path));
    if (unlikely(file->write(text.data(), text.size()) != text.size()))
        Error("in FileUtil::AppendStringToFile: failed to append data to \"" + path + "\"!");
}


} // namespace FileUtil

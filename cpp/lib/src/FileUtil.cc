/** \file   FileUtil.cc
 *  \brief  Implementation of file related utility classes and functions.
 *  \author Dr. Johannes Ruscheinski (johannes.ruscheinski@uni-tuebingen.de)
 *  \author Steven Lolong (steven.lolong@uni-tuebingen.de)
 *
 *  \copyright 2015-2023 Universitätsbibliothek Tübingen.  All rights reserved.
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
#include <iostream>
#include <list>
#include <memory>
#include <regex>
#include <stdexcept>
#include <cassert>
#include <cerrno>
#include <climits>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dirent.h>
#include <grp.h>
#include <linux/limits.h>
#include <pwd.h>
#ifdef HAS_SELINUX_HEADERS
#include <selinux/selinux.h>
#endif
#include <sys/mman.h>
#include <sys/sendfile.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <unistd.h>
#include "Compiler.h"
#include "ExecUtil.h"
#include "FileDescriptor.h"
#include "MiscUtil.h"
#include "RegexMatcher.h"
#include "SocketUtil.h"
#include "StringUtil.h"
#include "TextUtil.h"
#include "TimeUtil.h"
#include "util.h"


namespace FileUtil {


bool ReadLines::const_iterator::operator==(const const_iterator &rhs) const {
    if (file_ == rhs.file_)
        return true;

    if (rhs.file_ == nullptr)
        return file_->eof();

    return rhs.file_->eof();
}


std::string ReadLines::const_iterator::operator*() {
    std::string line;
    file_->getline(&line);

    if (trim_mode_ == TRIM_RIGHT)
        StringUtil::RightTrim(&line);
    else if (trim_mode_ == TRIM_LEFT_AND_RIGHT)
        StringUtil::Trim(&line);

    if (case_mode_ == TO_UPPER)
        TextUtil::UTF8ToUpper(&line);
    else if (case_mode_ == TO_LOWER)
        TextUtil::UTF8ToLower(&line);

    return line;
}


void ReadLines::const_iterator::operator++() {
}


ReadLines::ReadLines(const std::string &path, const TrimMode trim_mode, const CaseMode case_mode)
    : trim_mode_(trim_mode), case_mode_(case_mode) {
    file_ = OpenInputFileOrDie(path).release();
}


std::vector<std::string> ReadLines::ReadOrDie(const std::string &path, const ReadLines::TrimMode trim_mode,
                                              const ReadLines::CaseMode case_mode) {
    ReadLines readlines(path, trim_mode, case_mode);
    std::vector<std::string> lines;
    for (const auto line : readlines) {
        if (not line.empty())
            lines.emplace_back(line);
    }
    return lines;
}


AutoTempFile::AutoTempFile(const std::string &path_prefix, const std::string &path_suffix, bool automatically_remove)
    : automatically_remove_(automatically_remove) {
    std::string path_template(path_prefix + "XXXXXX" + path_suffix);
    const int fd(::mkstemps(const_cast<char *>(path_template.c_str()), path_suffix.length()));
    if (fd == -1)
        LOG_ERROR("mkstemps(3) for path prefix \"" + path_prefix + "\" failed!");

    ::close(fd);
    path_ = path_template;
}


SELinuxFileContext::SELinuxFileContext(const std::string &path) {
#ifndef HAS_SELINUX_HEADERS
    (void)path;
#else
    char *file_context;
    if (::getfilecon(path.c_str(), &file_context) == -1) {
        if (errno == ENODATA or errno == ENOTSUP)
            return;
        throw std::runtime_error("in SELinuxFileContext::SELinuxFileContext: failed to get file context for \"" + path + "\"!");
    }
    if (file_context == nullptr)
        return;

    std::vector<std::string> context_as_vector;
    const unsigned no_of_components(StringUtil::Split(std::string(file_context), ':', &context_as_vector,
                                                      /* suppress_empty_components = */ false));
    if (unlikely(no_of_components != 4))
        throw std::runtime_error("in SELinuxFileContext::SELinuxFileContext: context \"" + std::string(file_context)
                                 + "\"has unexpected no. of components (" + std::to_string(no_of_components) + ")!");

    user_ = context_as_vector[0];
    role_ = context_as_vector[1];
    type_ = context_as_vector[2];
    range_ = context_as_vector[3];

    ::freecon(file_context);
#endif
}


Directory::Entry::Entry(const Directory::Entry &other): dirname_(other.dirname_), name_(other.name_), statbuf_(other.statbuf_) {
}


Directory::const_iterator::const_iterator(const std::string &path, const std::string &regex, const bool end)
    : path_(path), regex_matcher_(nullptr), entry_(path_) {
    if (end)
        dir_handle_ = nullptr;
    else {
        std::string err_msg;
        if ((regex_matcher_ = RegexMatcher::RegexMatcherFactory(regex, &err_msg)) == nullptr)
            throw std::runtime_error("in Directory::const_iterator::const_iterator: bad PCRE \"" + regex + "\"! (" + err_msg + ")");
        if ((dir_handle_ = ::opendir(path.c_str())) == nullptr)
            throw std::runtime_error("in Directory::const_iterator::const_iterator: opendir(3) on \"" + path + "\" failed! ("
                                     + std::string(std::strerror(errno)) + ")");

        advance();
    }
}


Directory::const_iterator::~const_iterator() {
    delete regex_matcher_;
    if (dir_handle_ != nullptr)
        ::closedir(dir_handle_);
}


void Directory::const_iterator::advance() {
    if (dir_handle_ == nullptr)
        return;

    errno = 0;
    while (true) {
        struct dirent *entry_ptr;
        if (unlikely((entry_ptr = ::readdir(dir_handle_)) == nullptr and errno != 0))
            throw std::runtime_error("in Directory::const_iterator::advance: readdir(3) failed!");

        // Reached end-of-directory?
        if (entry_ptr == nullptr) { // Yes!
            ::closedir(dir_handle_);
            dir_handle_ = nullptr;
            return;
        }

        entry_.name_ = std::string(entry_ptr->d_name);
        if (regex_matcher_->matched(entry_.name_)) {
            if (::stat((entry_.dirname_ + "/" + entry_.name_).c_str(), &entry_.statbuf_) == 0)
                return;
            else if (errno == ENOENT)
                errno = 0;
            else
                throw std::runtime_error("in FileUtil::Directory::const_iterator::advance: stat(2) on \"" + entry_.dirname_ + "/"
                                         + entry_.name_ + " \"failed! (" + std::string(std::strerror(errno)) + ")");
        }
    }
}


Directory::Entry Directory::const_iterator::operator*() {
    if (dir_handle_ == nullptr)
        throw std::runtime_error(
            "in Directory::const_iterator::operator*: can't dereference an iterator pointing"
            " to the end!");
    return entry_;
}


void Directory::const_iterator::operator++() {
    advance();
}


bool Directory::const_iterator::operator==(const const_iterator &rhs) const {
    if (rhs.dir_handle_ == nullptr and dir_handle_ == nullptr)
        return true;
    if ((rhs.dir_handle_ == nullptr and dir_handle_ != nullptr) or (rhs.dir_handle_ != nullptr and dir_handle_ == nullptr))
        return false;

    return rhs.entry_.name_ == entry_.name_;
}


off_t GetFileSize(const std::string &path) {
    struct stat stat_buf;
    if (::stat(path.c_str(), &stat_buf) == -1)
        LOG_ERROR("can't stat(2) \"" + path + "\"!");

    return stat_buf.st_size;
}


bool GetLastModificationTimestamp(const std::string &path, timespec * const mtim) {
    struct stat stat_buf;
    if (::stat(path.c_str(), &stat_buf) == -1)
        return false;
    *mtim = stat_buf.st_mtim;
    return true;
}


bool WriteString(const std::string &path, const std::string &data) {
    std::ofstream output(path, std::ios_base::out | std::ios_base::binary | std::ios_base::trunc);
    if (output.fail())
        return false;

    output.write(data.data(), data.size());
    return not output.bad();
}


void WriteStringOrDie(const std::string &path, const std::string &data) {
    if (not FileUtil::WriteString(path, data))
        LOG_ERROR("failed to write data to \"" + path + "\"!");
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


void ReadStringOrDie(const std::string &path, std::string * const data) {
    if (not FileUtil::ReadString(path, data))
        LOG_ERROR("failed to read \"" + path + "\"!");
}


std::string ReadStringOrDie(const std::string &path) {
    std::string data;
    if (not FileUtil::ReadString(path, &data))
        LOG_ERROR("failed to read \"" + path + "\"!");
    return data;
}


bool ReadStringFromPseudoFile(const std::string &path, std::string * const data) {
    data->clear();

    std::ifstream input(path, std::ios_base::in | std::ios_base::binary);
    if (input.fail())
        return false;

    std::string line;
    while (std::getline(input, line))
        *data += line + "\n";

    return input.fail();
}


void ReadStringFromPseudoFileOrDie(const std::string &path, std::string * const data) {
    if (not FileUtil::ReadStringFromPseudoFile(path, data))
        LOG_ERROR("failed to read \"" + path + "\"!");
}


std::string ReadStringFromPseudoFileOrDie(const std::string &path) {
    std::string data;
    if (not FileUtil::ReadStringFromPseudoFile(path, &data))
        LOG_ERROR("failed to read \"" + path + "\"!");
    return data;
}


bool AppendString(const std::string &path, const std::string &data) {
    std::ofstream output(path, std::ios_base::out | std::ios_base::binary | std::ios_base::app);
    if (output.fail())
        return false;

    output.write(data.data(), data.size());
    return not output.bad();
}


void AppendStringOrDie(const std::string &path, const std::string &data) {
    if (not FileUtil::AppendString(path, data))
        LOG_ERROR("failed to append data to \"" + path + "\"!");
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
               " permission is denied to one of the directories in '"
               + pathname + "'";
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


// Returns true if stat(2) succeeded and false otherwise.
static bool Stat(struct stat * const stat_buf, const std::string &path, std::string * const error_message) {
    errno = 0;

    if (::stat(path.c_str(), stat_buf) != 0) {
        if (error_message != nullptr)
            *error_message = "can't stat(2) \"" + path + "\": " + std::string(::strerror(errno));
        errno = 0;
        return false;
    }

    return true;
}


bool Exists(const std::string &path, std::string * const error_message) {
    struct stat stat_buf;
    return Stat(&stat_buf, path, error_message);
}


bool IsReadable(const std::string &path, std::string * const error_message) {
    struct stat stat_buf;
    if (not Stat(&stat_buf, path, error_message))
        return false;

    if ((S_IRUSR & stat_buf.st_mode) == S_IRUSR)
        return true;

    *error_message = "\"" + path + "\" exists but is not readable!";
    return false;
}


std::string GetCurrentWorkingDirectory() {
    char buf[PATH_MAX];
    const char * const current_working_dir(::getcwd(buf, sizeof buf));
    if (unlikely(current_working_dir == nullptr))
        throw std::runtime_error("in FileUtil::GetCurrentWorkingDirectory: getcwd(3) failed (" + std::string(::strerror(errno)) + ")!");
    return current_working_dir;
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
        } else
            canonical_path_list->push_back(directory);
    }
}


} // unnamed namespace


std::string CanonisePath(const std::string &path) {
    std::list<std::string> canonical_path_list;
    MakeCanonicalPathList(path.c_str(), &canonical_path_list);

    std::string canonised_path;
    for (std::list<std::string>::const_iterator path_component(canonical_path_list.begin()); path_component != canonical_path_list.end();
         ++path_component)
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
        if (*component == ".." and (resultant_dirname_components.size() > 1 or resultant_dirname_components.front() != "/"))
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


bool MakeEmpty(const std::string &path) {
    int fd;
    if ((fd = ::open(path.c_str(), O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR)) == -1)
        return false;

    ::close(fd);
    return true;
}


void TouchFileOrDie(const std::string &path) {
    if (Exists(path))
        OpenForAppendingOrDie(path);
    else
        WriteStringOrDie(path, "");
}


void ChangeOwnerOrDie(const std::string &path, const std::string &user, const std::string &group, const bool recursive) {
    if (user.empty() and group.empty())
        LOG_ERROR("Cannot change owner of path \"" + path + "\": Neither user nor group given!");

    std::vector<std::string> args;
    if (recursive)
        args.emplace_back("--recursive");

    std::string user_and_group(user);
    if (not group.empty())
        user_and_group += ":" + group;
    args.emplace_back(user_and_group);

    args.emplace_back(path);
    ExecUtil::ExecOrDie(ExecUtil::LocateOrDie("chown"), args);
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
    if (unlikely(path.empty())) {
        *dirname = *basename = "";
        return;
    }

    const auto last_slash_pos(path.rfind('/'));
    if (last_slash_pos == std::string::npos) {
        *dirname = "";
        *basename = path;
    } else {
        *dirname = path.substr(0, last_slash_pos);
        *basename = path.substr(last_slash_pos + 1);
    }
}


std::string GetBasename(const std::string &path) {
    if (unlikely(path.empty()))
        return "";

    const auto last_slash_pos(path.rfind('/'));
    if (last_slash_pos == std::string::npos)
        return path;
    return path.substr(last_slash_pos + 1);
}


std::string GetDirname(const std::string &path) {
    if (unlikely(path.empty()))
        return "";

    const auto last_slash_pos(path.rfind('/'));
    if (last_slash_pos == std::string::npos)
        return "";
    return path.substr(0, last_slash_pos);
}


std::string GetLastPathComponent(const std::string &path) {
    const auto last_slash_pos(path.rfind('/'));
    return (last_slash_pos == std::string::npos) ? path : path.substr(last_slash_pos + 1);
}


// IsDirectory -- Is the specified file a directory?
//
bool IsDirectory(const std::string &dir_name) {
    struct stat statbuf;
    errno = 0;
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
    StringUtil::Split(path, '/', &path_components, /* suppress_empty_components = */ true);

    std::string path_so_far;
    if (absolute)
        path_so_far += "/";
    for (std::vector<std::string>::const_iterator path_component(path_components.begin()); path_component != path_components.end();
         ++path_component)
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


void MakeDirectoryOrDie(const std::string &path, const bool recursive, const mode_t mode) {
    if (not MakeDirectory(path, recursive, mode))
        LOG_ERROR("failed to create directory \"" + path + "\"!");
}

void MakeParentDirectoryOrDie(const std::string &path, const bool recursive, const mode_t mode) {
    MakeDirectoryOrDie(GetDirname(path), recursive, mode);
}


static void CloseDirWhilePreservingErrno(DIR * const dir_handle) {
    const int old_errno(errno);
    ::closedir(dir_handle);
    errno = old_errno;
}


bool RemoveDirectory(const std::string &dir_name) {
    errno = 0;
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


AutoTempDirectory::AutoTempDirectory(const std::string &path_prefix, const bool cleanup_if_exception_is_active,
                                     const bool remove_when_out_of_scope)
    : cleanup_if_exception_is_active_(cleanup_if_exception_is_active), remove_when_out_of_scope_(remove_when_out_of_scope) {
    std::string path_template(path_prefix + "XXXXXX");
    const char * const path(::mkdtemp(const_cast<char *>(path_template.c_str())));
    if (path == nullptr)
        LOG_ERROR("mkdtemp(3) for path prefix \"" + path_prefix + "\" failed!");
    char resolved_path[PATH_MAX];
    if (unlikely(::realpath(path, resolved_path) == nullptr))
        LOG_ERROR("realpath(3) for path \"" + std::string(path) + "\" failed!");
    path_ = resolved_path;
}


AutoTempDirectory::~AutoTempDirectory() {
    if (not IsDirectory(path_))
        LOG_ERROR("\"" + path_ + "\" doesn't exist anymore!");

    if (remove_when_out_of_scope_ and ((not std::uncaught_exceptions() or cleanup_if_exception_is_active_) and not RemoveDirectory(path_)))
        LOG_ERROR("can't remove \"" + path_ + "\"!");
}


ssize_t RemoveMatchingFiles(const std::string &filename_regex, const bool include_directories, const std::string &directory_to_scan) {
    if (unlikely(filename_regex.find('/') != std::string::npos))
        throw std::runtime_error("in FileUtil::RemoveMatchingFiles: filename regex contained a slash!");

    std::string err_msg;
    std::unique_ptr<RegexMatcher> matcher(RegexMatcher::RegexMatcherFactory(filename_regex, &err_msg));
    if (unlikely(not err_msg.empty()))
        throw std::runtime_error("in FileUtil::RemoveMatchingFiles: failed to compile regular expression \"" + filename_regex + "\"! ("
                                 + err_msg + ")");

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
    TextUtil::UTF8ToLower(&file_extension);
    if (file_extension.find("htm") != std::string::npos) // .phtml, .shtml, .html
        return FILE_TYPE_HTML;

    FileUtil::FileType file_type = FILE_TYPE_UNKNOWN;
    switch (file_extension[0]) {
    case 'c':
        if (file_extension == "c" or file_extension == "cc" or file_extension == "cpp" or file_extension == "cxx")
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
            if (file_extension == "phtml")    // serverside parsed html
                file_type = FILE_TYPE_HTML;
            else if (file_extension == "php") //
                file_type = FILE_TYPE_HTML;
            break;
        case 'l':
            if (file_extension == "pl")
                file_type = FILE_TYPE_HTML; // it might be a source code too!
            break;
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
                       const std::string &directory_to_scan) {
    if (unlikely(filename_regex.find('/') != std::string::npos))
        throw std::runtime_error("in FileUtil::GetFileNameList: filename regex contained a slash!");

    Directory directory(directory_to_scan, filename_regex);
    for (const auto entry : directory)
        matched_filenames->emplace_back(entry.getName());

    return matched_filenames->size();
}


bool RenameFile(const std::string &old_name, const std::string &new_name, const bool remove_target, const bool copy_if_cross_device) {
    struct stat stat_buf;
    if (::stat(new_name.c_str(), &stat_buf) == -1) {
        if (errno != ENOENT)
            LOG_ERROR("stat(2) failed!");
    } else { // Target file or directory already exists!
        if (not remove_target) {
            errno = EEXIST;
            return false;
        }

        if (S_ISDIR(stat_buf.st_mode)) {
            if (unlikely(not RemoveDirectory(new_name)))
                return false;
        } else if (unlikely(::unlink(new_name.c_str()) == -1))
            LOG_ERROR("unlink(2) failed!");
    }

    if (::rename(old_name.c_str(), new_name.c_str()) == 0)
        return true;
    else if (errno == EXDEV and copy_if_cross_device) {
        if (not Copy(old_name, new_name))
            return false;
        return ::unlink(old_name.c_str()) == 0;
    } else
        return false;
}


void RenameFileOrDie(const std::string &old_name, const std::string &new_name, const bool remove_target, const bool copy_if_cross_device) {
    if (not RenameFile(old_name, new_name, remove_target, copy_if_cross_device))
        LOG_ERROR("failed to rename \"" + old_name + "\" to \"" + new_name + "\"!");
}


std::unique_ptr<File> OpenInputFileOrDie(const std::string &filename) {
    std::unique_ptr<File> file(new File(filename, "r"));
    if (file->fail())
        LOG_ERROR("can't open \"" + filename + "\" for reading!");

    return file;
}


std::unique_ptr<File> OpenOutputFileOrDie(const std::string &filename) {
    std::unique_ptr<File> file(new File(filename, "w"));
    if (file->fail())
        LOG_ERROR("can't open \"" + filename + "\" for writing!");

    return file;
}


std::unique_ptr<File> OpenTempFileOrDie(const std::string &path_prefix, const std::string &path_suffix) {
    std::string path_template(path_prefix + "XXXXXX" + path_suffix);
    const int fd(::mkstemps(const_cast<char *>(path_template.c_str()), path_suffix.length()));
    if (fd == -1)
        LOG_ERROR("mkstemps(3) for template \"" + path_template + "\" failed! (" + std::string(::strerror(errno)) + ")");

    std::unique_ptr<File> file(new File(fd));
    return file;
}


std::unique_ptr<File> OpenForAppendingOrDie(const std::string &filename) {
    std::unique_ptr<File> file(new File(filename, "a"));
    if (file->fail())
        LOG_ERROR("can't open \"" + filename + "\" for appending!");

    return file;
}


// Hack for CentOS 7 which only has ::sync_file_range
static loff_t copy_file_range(int fd_in, loff_t *off_in, int fd_out, loff_t *off_out, size_t len, unsigned int flags) {
    return syscall(__NR_copy_file_range, fd_in, off_in, fd_out, off_out, len, flags);
}


static bool ActualCopy(const int &from_fd, const int &to_fd, const size_t no_of_bytes_to_copy, const loff_t offset, const int whence) {
    if (whence != SEEK_SET and whence != SEEK_CUR and whence != SEEK_END)
        LOG_ERROR("whence need to be one of {SEEK_SET, SEEK_CUR, SEEK_END}!");

    loff_t remaining_bytes;
    if (no_of_bytes_to_copy > 0)
        remaining_bytes = no_of_bytes_to_copy;
    else {
        struct stat stat_buf;
        if (::fstat(from_fd, &stat_buf) == -1)
            return false;
        remaining_bytes = stat_buf.st_size;
    }

    if (offset != 0 or whence != SEEK_CUR) {
        if (::lseek(from_fd, offset, whence) == static_cast<loff_t>(-1))
            return false;
    }

    do {
        const loff_t actually_copied(copy_file_range(from_fd, nullptr, to_fd, nullptr, remaining_bytes, 0));
        if (unlikely(actually_copied == -1))
            return false;

        remaining_bytes -= actually_copied;
    } while (remaining_bytes > 0);

    return true;
}


bool Copy(File * const from, File * const to, const size_t no_of_bytes) {
    return ActualCopy(from->getFileDescriptor(), to->getFileDescriptor(), no_of_bytes, 0, SEEK_CUR);
}


bool Copy(const std::string &from_path, const std::string &to_path, const bool truncate_target, const size_t no_of_bytes_to_copy,
          const loff_t offset, const int whence) {
    const int from_fd(::open(from_path.c_str(), O_RDONLY));
    if (unlikely(from_fd == -1))
        return false;

    const int to_fd(::open(to_path.c_str(), O_WRONLY | O_CREAT | (truncate_target ? O_TRUNC : 0), 0600));
    if (unlikely(to_fd == -1)) {
        ::close(from_fd);
        return false;
    }

    const bool retcode(ActualCopy(from_fd, to_fd, no_of_bytes_to_copy, offset, whence));

    ::close(from_fd);
    ::close(to_fd);

    return retcode;
}


void CopyOrDie(const std::string &from_path, const std::string &to_path) {
    if (not Copy(from_path, to_path))
        LOG_ERROR("failed to copy \"" + from_path + "\" to \"" + to_path + "\"!");
}

void CopyOrDieXFs(const std::string fromPath, const std::string toPath) {
    const std::string copy_file_syntax(ExecUtil::Which("cp"));
    const std::vector<std::string> exec_param{ fromPath, toPath };
    ExecUtil::ExecOrDie(copy_file_syntax, exec_param);
}

bool DeleteFile(const std::string &path) {
    return ::unlink(path.c_str()) == 0;
}


bool DescriptorIsReadyForReading(const int fd, const TimeLimit &time_limit) {
    return SocketUtil::TimedRead(fd, time_limit, static_cast<void *>(nullptr), 0) == 0;
}


bool DescriptorIsReadyForWriting(const int fd, const TimeLimit &time_limit) {
    return SocketUtil::TimedWrite(fd, time_limit, nullptr, 0) == 0;
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


std::string UniqueFileName(const std::string &directory, const std::string &filename_prefix, const std::string &filename_suffix) {
    static unsigned generation_number(1);

    // Set default for prefix if necessary.
    std::string prefix(filename_prefix);
    if (prefix.empty())
        prefix = ::program_invocation_name;

    std::string suffix(filename_suffix);
    if (not suffix.empty() and suffix[0] != '.')
        suffix = "." + suffix;

    std::string dir(directory);
    if (dir.empty())
        dir = "/tmp";

    return dir + "/" + prefix + "." + std::to_string(getpid()) + "." + std::to_string(generation_number++) + suffix;
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
            throw std::runtime_error("in FileUtil::FilesDiffer: an error occurred while trying to read \"" + path1 + "\"!");

        char buf2[BUFSIZ];
        const size_t read_count2(input2.read(reinterpret_cast<void *>(buf2), BUFSIZ));
        if (unlikely(read_count2 < BUFSIZ) and input2.anErrorOccurred())
            throw std::runtime_error("in FileUtil::FilesDiffer: an error occurred while trying to read \"" + path2 + "\"!");

        if (read_count1 != read_count2 or std::memcmp(buf1, buf2, BUFSIZ) != 0)
            return true;

        if (read_count1 < BUFSIZ)
            return false;
    }
}


void AppendStringToFile(const std::string &path, const std::string &text) {
    std::unique_ptr<File> file(OpenForAppendingOrDie(path));
    if (unlikely(file->write(text.data(), text.size()) != text.size()))
        LOG_ERROR("failed to append data to \"" + path + "\"!");
}


// Creates a symlink called "link_filename" pointing to "target_filename".
void CreateSymlink(const std::string &target_filename, const std::string &link_filename) {
    if (unlikely(::unlink(link_filename.c_str()) == -1 and errno != ENOENT /* "No such file or directory." */))
        throw std::runtime_error("in FileUtil::CreateSymlink: unlink(2) of \"" + link_filename
                                 + "\" failed: " + std::string(::strerror(errno)));
    if (unlikely(::symlink(target_filename.c_str(), link_filename.c_str()) != 0))
        throw std::runtime_error("in FileUtil::CreateSymlink: failed to create symlink \"" + link_filename + "\" => \"" + target_filename
                                 + "\"! (" + std::string(::strerror(errno)) + ")");
}


size_t ConcatFiles(const std::string &target_path, const std::vector<std::string> &filenames, const mode_t target_mode) {
    if (filenames.empty())
        LOG_ERROR("no files to concatenate!");

    FileDescriptor target_fd(::open(target_path.c_str(), O_WRONLY | O_CREAT | O_LARGEFILE | O_TRUNC, target_mode));
    if (target_fd == -1)
        LOG_ERROR("failed to open or create \"" + target_path + "\"!");

    size_t total_size(0);
    for (const auto &filename : filenames) {
        FileDescriptor source_fd(::open(filename.c_str(), O_RDONLY | O_LARGEFILE));
        if (source_fd == -1)
            LOG_ERROR("failed to open \"" + filename + "\" for reading!");

        struct stat statbuf;
        if (::fstat(source_fd, &statbuf) == -1)
            LOG_ERROR("failed to fstat(2) \"" + filename + "\"!");
        ssize_t count(0);
        // Get around sendfile limitation of transfering only 2,147,479,552 in one bunch
        const size_t sendfile_max_chunk(0x7ffff000);
        ssize_t offset(0);

        while (static_cast<size_t>(offset) != static_cast<size_t>(statbuf.st_size)) {
            off_t offset_arg(offset); // New variable needed since on first call (i.e. offset == 0) offset would be
                                      // modified
            count =
                ::sendfile(target_fd, source_fd, &offset_arg, std::min(static_cast<size_t>(statbuf.st_size - offset), sendfile_max_chunk));
            if (count == -1)
                LOG_ERROR("failed to append \"" + filename + "\" to \"" + target_path + "\"!");
            offset += static_cast<size_t>(count);
            total_size += static_cast<size_t>(count);
        }
    }

    return total_size;
}


bool IsMountPoint(const std::string &path) {
    struct stat statbuf;
    if (::stat(path.c_str(), &statbuf) == -1)
        LOG_ERROR("stat(2) on \"" + path + "\" failed!");

    struct stat parent_statbuf;
    if (::stat((path + "/..").c_str(), &parent_statbuf) == -1)
        LOG_ERROR("stat(2) on \"" + path + "/..\" failed!");

    return statbuf.st_dev != parent_statbuf.st_dev;
}


size_t CountLines(const std::string &filename) {
    if (GetFileSize(filename) == 0)
        return 0;

    std::unique_ptr<File> input(OpenInputFileOrDie(filename));
    size_t line_count(0);
    while (not input->eof()) {
        std::string line;
        input->getline(&line);
        ++line_count;
    }

    return line_count;
}


// Strips all extensions from "filename" and returns what is left after that.
std::string GetFilenameWithoutExtensionOrDie(const std::string &filename) {
    const auto last_dot_pos(filename.rfind('.'));
    if (unlikely(last_dot_pos == std::string::npos))
        LOG_ERROR("\"" + filename + "\" has no extension!");
    return filename.substr(0, last_dot_pos);
}


std::string GetExtension(const std::string &filename, const bool to_lowercase) {
    const std::string::size_type last_dot_pos(filename.rfind('.'));

    if (last_dot_pos == std::string::npos)
        return "";
    else if (not to_lowercase)
        return filename.substr(last_dot_pos + 1);
    else
        return TextUtil::UTF8ToLower(filename.substr(last_dot_pos + 1));
}


std::string StripLastPathComponent(const std::string &path) {
    std::vector<std::string> path_components;
    if (StringUtil::Split(path, '/', &path_components, /* suppress_empty_components = */ true) < 1)
        LOG_ERROR("\"" + path + "\" has no path components");
    path_components.pop_back();
    return (path[0] == '/' ? "/" : "") + StringUtil::Join(path_components, '/');
}


bool IsEmpty(const std::string &path) {
    struct stat buf;
    if (::stat(path.c_str(), &buf) != 0)
        LOG_ERROR("stat(2) on \"" + path + "\" failed!");
    return buf.st_size == 0;
}


bool IsPipeOrFIFO(const std::string &path) {
    struct stat buf;
    if (::stat(path.c_str(), &buf) != 0)
        LOG_ERROR("stat(2) on \"" + path + "\" failed!");
    return S_ISFIFO(buf.st_mode);
}


bool IsSymlink(const std::string &path) {
    struct stat buf;
    if (::stat(path.c_str(), &buf) != 0)
        LOG_ERROR("stat(2) on \"" + path + "\" failed!");
    return S_ISLNK(buf.st_mode);
}


void ChangeDirectoryOrDie(const std::string &directory) {
    if (unlikely(::chdir(directory.c_str()) != 0))
        LOG_ERROR("failed to change directory to \"" + directory + "\"!");
}


std::string GetPathFromFileDescriptor(const int fd) {
    const std::string proc_self_fd_path("/proc/self/fd/" + std::to_string(fd));
    size_t buf_size(0);
    char *buf(nullptr);
    for (;;) {
        buf_size += PATH_MAX;
        buf = reinterpret_cast<char *>(std::realloc(buf, buf_size));
        if (unlikely(buf == nullptr))
            LOG_ERROR("realloc(3) failed!");

        const auto path_size(::readlink(proc_self_fd_path.c_str(), buf, buf_size));
        if (path_size == -1)
            LOG_ERROR("readlink(2) failed!");

        if (static_cast<size_t>(path_size) < buf_size) {
            buf[path_size] = '\0';

            const std::string str_buf(buf);
            std::free(buf);
            return str_buf;
        }
    }
}


std::string ExpandTildePath(const std::string &path) {
    if (path.empty() or path[0] != '~')
        return path;

    const std::string HOME(MiscUtil::SafeGetEnv("HOME"));
    if (unlikely(HOME.empty()))
        LOG_ERROR("HOME has not been set!");

    if (path.length() == 1)
        return HOME;

    if (HOME[HOME.length() - 1] == '/') {
        if (path[1] == '/')
            return HOME + path.substr(2);
        else
            return HOME + path.substr(1);
    } else if (path[1] == '/')
        return HOME + path.substr(1);
    else
        return HOME + "/" + path.substr(1);
}


bool WaitForFile(const std::string &path, const unsigned timeout, const unsigned sleep_increment) {
    TimeLimit time_limit(timeout * 1000);

    for (;;) {
        if (Exists(path))
            return true;

        if (time_limit.limitExceeded())
            return false;

        ::sleep(std::min((time_limit.getRemainingTime() + 500) / 1000, sleep_increment));
    }
}


bool ChangeMode(const std::string &path, const mode_t mode) {
    errno = 0;
    return ::chmod(path.c_str(), mode) == 0;
}


void ChangeModeOrDie(const std::string &path, const mode_t mode) {
    if (not ChangeMode(path, mode))
        LOG_ERROR("failed to set the mode to 0" + StringUtil::ToString(mode, 8) + " for \"" + path + "\"!");
}


bool GetMostRecentlyModifiedFile(const std::string &directory_path, std::string * const filename, timespec * const last_modification_time) {
    if (not IsDirectory(directory_path))
        LOG_ERROR("\"" + directory_path + "\" does not exist or is not a directory!");

    Directory directory(directory_path);
    filename->clear();
    for (const auto &entry : directory) {
        if (entry.getType() != DT_REG)
            continue;

        struct stat statbuf;
        if (unlikely(::stat(entry.getName().c_str(), &statbuf) != 0))
            LOG_ERROR("stat(2) on \"" + entry.getName() + "\" failed!");

        if (filename->empty() or statbuf.st_mtim > *last_modification_time) {
            *filename = entry.getName();
            *last_modification_time = statbuf.st_mtim;
        }
    }

    return not filename->empty();
}


void RemoveLeadingBytes(const std::string &path, const loff_t no_of_bytes) {
    if (no_of_bytes < 0)
        LOG_ERROR("no_of_bytes must not be negative!");

    if (no_of_bytes == 0)
        return;

    const auto original_file_size(GetFileSize(path));
    if (original_file_size <= no_of_bytes) {
        if (::truncate(path.c_str(), 0) != 0)
            LOG_ERROR("failed to truncate(2) \"" + path + "\" to 0!");
        return;
    }

    const int fd(::open(path.c_str(), O_LARGEFILE | O_RDWR));
    if (fd == -1)
        LOG_ERROR("failed to open(2) \"" + path + "\"!");

    void * const map_start(::mmap(nullptr, original_file_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0));
    if (map_start == MAP_FAILED or map_start == nullptr)
        LOG_ERROR("Failed to mmap(2) \"" + path + "\"!");

    ::memmove(map_start, static_cast<void *>(static_cast<char *>(map_start) + no_of_bytes), original_file_size - no_of_bytes);

    if (::munmap(map_start, original_file_size) != 0)
        LOG_ERROR("munmap(2) failed!");

    if (::ftruncate(fd, original_file_size - no_of_bytes))
        LOG_ERROR("Failed to ftruncate(2) \"" + path + "\"!");

    if (::close(fd) != 0)
        LOG_ERROR("failed to close(2) a file descriptor!");
}


void OnlyKeepLastNLines(const std::string &path, const unsigned n) {
    const auto file_size(GetFileSize(path));

    const int fd(::open(path.c_str(), O_LARGEFILE | O_RDONLY));
    if (fd == -1)
        LOG_ERROR("failed to open(2) \"" + path + "\"!");

    const char * const map_start(static_cast<char *>(::mmap(nullptr, file_size, PROT_READ, MAP_PRIVATE, fd, 0)));
    if (map_start == MAP_FAILED or map_start == nullptr)
        LOG_ERROR("Failed to mmap(2) \"" + path + "\"!");

    if (::close(fd) != 0)
        LOG_ERROR("failed to close(2) a file descriptor!");

    const char *cp(static_cast<const char *>(map_start) + file_size);
    unsigned trailing_line_count(0);
    while (trailing_line_count < n and cp >= map_start) {
        if (*cp == '\n')
            ++trailing_line_count;
        --cp;
    }

    if (::munmap((void * const)map_start, file_size) != 0)
        LOG_ERROR("munmap(2) failed!");

    const loff_t no_of_leading_bytes_to_remove(map_start - cp + 1);
    RemoveLeadingBytes(path, no_of_leading_bytes_to_remove);
}


std::string UsernameFromUID(const uid_t uid) {
    struct passwd *pw(::getpwuid(uid));
    return (pw == nullptr) ? "" : pw->pw_name;
}


std::string GroupnameFromGID(const gid_t gid) {
    struct group *grp(::getgrgid(gid));
    return (grp == nullptr) ? "" : grp->gr_name;
}


bool ReadLink(const std::string &path, std::string * const link_target) {
    constexpr size_t INITAL_BUFFER_SIZE(4096);
    errno = 0;
    static char *buffer(reinterpret_cast<char *>(std::malloc(INITAL_BUFFER_SIZE)));
    static size_t buffer_size(INITAL_BUFFER_SIZE);
    for (;;) {
        const ssize_t retval(::readlink(path.c_str(), buffer, buffer_size));
        if (retval == -1)
            return false;
        if (unlikely(retval == static_cast<ssize_t>(buffer_size))) {
            buffer_size *= 2;
            buffer = reinterpret_cast<char *>(std::realloc(buffer, buffer_size));
            if (unlikely(buffer == nullptr))
                LOG_ERROR("realloc(3) failed!");
        } else {
            link_target->assign(buffer);
            return true;
        }
    }
}

} // namespace FileUtil

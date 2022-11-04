/** \file    FileUtil.h
 *  \brief   Declaration of file-related utility functions.
 *  \author  Dr. Gordon W. Paynter
 *  \author  Dr. Johannes Ruscheinski
 *  \author  Artur Kedzierski
 *  \author  Steven Lolong
 */

/*
 *  Copyright 2002-2008 Project iVia.
 *  Copyright 2002-2008 The Regents of The University of California.
 *  Copyright 2015-2021 Universitätsbibliothek Tübingen.  All rights reserved.
 *  Copyright 2022 Universitätsbibliothek Tübingen.
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
#pragma once


#include <memory>
#include <string>
#include <tuple>
#include <vector>
#include <cstdio>
#include <dirent.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include "File.h"
#include "TimeLimit.h"


// Forward declaration:
class RegexMatcher;


namespace FileUtil {


/** \brief   Convenient iterator over lines in a file.
 *  \usage   for (auto line : FileUtil::ReadLines(path)) ...
 *  \warning The implementation is terrible and does not support any other usage than the suggested one!
 */
class ReadLines {
public:
    enum TrimMode { DO_NOT_TRIM, TRIM_RIGHT, TRIM_LEFT_AND_RIGHT };
    enum CaseMode { DO_NOT_CHANGE, TO_UPPER, TO_LOWER };

    class const_iterator {
        friend class ReadLines;
        File * const file_;
        TrimMode trim_mode_;
        CaseMode case_mode_;
        std::string current_line_;

    public:
        std::string operator*();
        void operator++();
        bool operator==(const const_iterator &rhs) const;
        inline bool operator!=(const const_iterator &rhs) const { return not operator==(rhs); }

    private:
        const_iterator(File * const file, const TrimMode trim_mode, const CaseMode case_mode)
            : file_(file), trim_mode_(trim_mode), case_mode_(case_mode) { }
    };

private:
    File *file_;
    TrimMode trim_mode_;
    CaseMode case_mode_;

public:
    explicit ReadLines(const std::string &path, const TrimMode trim_mode = TRIM_LEFT_AND_RIGHT,
                       const ReadLines::CaseMode case_mode = DO_NOT_CHANGE);
    ~ReadLines() { delete file_; }

    const_iterator begin() { return const_iterator(file_, trim_mode_, case_mode_); }
    const_iterator end() { return const_iterator(nullptr, trim_mode_, case_mode_); }

    static std::vector<std::string> ReadOrDie(const std::string &path, const ReadLines::TrimMode trim_mode = TRIM_LEFT_AND_RIGHT,
                                              const ReadLines::CaseMode case_mode = DO_NOT_CHANGE);
};


/** \class AutoDeleteFile
 *  \brief Deletes the file, specified by the path that was given in the constructor, when going out of scope.
 */
class AutoDeleteFile {
    std::string path_;

public:
    explicit AutoDeleteFile(const std::string &path): path_(path) { }
    ~AutoDeleteFile() {
        if (not path_.empty())
            ::unlink(path_.c_str());
    }
};


class SELinuxFileContext {
    std::string user_;
    std::string role_;
    std::string type_;
    std::string range_;

public:
    SELinuxFileContext() = default;
    SELinuxFileContext(const SELinuxFileContext &rhs) = default;
    SELinuxFileContext(SELinuxFileContext &&rhs) = default;
    explicit SELinuxFileContext(const std::string &path);
    inline bool empty() const { return user_.empty() and role_.empty() and type_.empty() and range_.empty(); }
    inline const std::string &getUser() const { return user_; }
    inline const std::string &getRole() const { return role_; }
    inline const std::string &getType() const { return type_; }
    inline const std::string &getRange() const { return range_; }
    std::string toString() const { return user_ + ":" + role_ + ":" + type_ + ":" + range_; }
};


class Directory {
    const std::string path_;
    const std::string regex_;

public:
    const std::string &getDirectoryPath() const { return path_; }
    class const_iterator; // Forward declaration.
    class Entry {
        friend class Directory::const_iterator;
        const std::string &dirname_;
        std::string name_;
        struct stat statbuf_;

    public:
        Entry(const Entry &other);

        /** \return the name of the entry w/o the directory path. */
        inline const std::string &getName() const { return name_; }

        /** \return the name of the entry w/ the directory path. */
        inline const std::string getFullName() const { return dirname_ + "/" + name_; }

        inline SELinuxFileContext getSELinuxFileContext() const { return SELinuxFileContext(dirname_ + "/" + name_); }

        // \return One of DT_BLK(block device), DT_CHR(character device), DT_DIR(directory), DT_FIFO(named pipe),
        //         DT_LNK(symlink), DT_REG(regular file), DT_SOCK(UNIX domain socket), or DT_UNKNOWN(unknown type).
        unsigned char getType() const { return IFTODT(statbuf_.st_mode); /* Convert from st_mode to d_type. */ }

        inline ino_t getInode() const { return statbuf_.st_ino; }
        void getUidAndGid(uid_t * const uid, gid_t * const gid) const { *uid = statbuf_.st_uid, *gid = statbuf_.st_gid; }

        /** \note To get the file type part of the return value, and it with S_IFMT, to get the mode part
         *        and it with with the binary complement of S_IFMT. */
        mode_t getFileTypeAndMode() const { return statbuf_.st_mode; }

    private:
        Entry(const std::string &dirname): dirname_(dirname) { }
    };

public:
    class const_iterator {
        friend class Directory;
        std::string path_;
        RegexMatcher *regex_matcher_;
        DIR *dir_handle_;
        Entry entry_;

    public:
        ~const_iterator();

        Entry operator*();
        void operator++();
        bool operator==(const const_iterator &rhs) const;
        bool operator!=(const const_iterator &rhs) const { return not operator==(rhs); }

    private:
        explicit const_iterator(const std::string &path, const std::string &regex, const bool end = false);
        void advance();
    };

public:
    /** \brief Initialises a new instance of Directory.
     *  \param path   The path to the directory we want to list.
     *  \param regex  Only entries matching this PCRE will be listed.
     */
    explicit Directory(const std::string &path, const std::string &regex = ".*"): path_(path), regex_(regex) { }

    inline const_iterator begin() const { return const_iterator(path_, regex_); }
    inline const_iterator end() const { return const_iterator(path_, regex_, /* end = */ true); }
};


/** \return The size of the file named by "path".
 *  \note   Exits with an error message if "path" does not exist or we don't have the rights to stat it.
 */
off_t GetFileSize(const std::string &path);


/** \return The last modification timestamp of the file named by "path".
 *  \note   Returns false if "path" does not exist or we don't have the rights to stat it.
 */
bool GetLastModificationTimestamp(const std::string &path, timespec * const mtim);


/** \class AutoTempFile
 *  \brief Creates a temp file and removes it when going out of scope.
 */
class AutoTempFile {
    std::string path_;
    bool automatically_remove_;

public:
    explicit AutoTempFile(const std::string &path_prefix = "/tmp/ATF", const std::string &path_suffix = "",
                          bool automatically_remove = true);
    ~AutoTempFile() {
        if (not path_.empty() and automatically_remove_)
            ::unlink(path_.c_str());
    }

    const std::string &getFilePath() const { return path_; }
};


/** \class AutoTempDirectory
 *  \brief Creates a temp directory and removes it when going out of scope.
 */
class AutoTempDirectory {
    std::string path_;
    bool cleanup_if_exception_is_active_;
    bool remove_when_out_of_scope_;

public:
    explicit AutoTempDirectory(const std::string &path_prefix = "/tmp/ATD", const bool cleanup_if_exception_is_active = true,
                               const bool remove_when_out_of_scope = true);
    AutoTempDirectory(const AutoTempDirectory &rhs) = delete;
    ~AutoTempDirectory();

    const std::string &getDirectoryPath() const { return path_; }
};


bool WriteString(const std::string &path, const std::string &data);
void WriteStringOrDie(const std::string &path, const std::string &data);
bool ReadString(const std::string &path, std::string * const data);
void ReadStringOrDie(const std::string &path, std::string * const data);
std::string ReadStringOrDie(const std::string &path);

/* Same as ReadString, but can be used on /proc files (size 0 bytes) */
bool ReadStringFromPseudoFile(const std::string &path, std::string * const data);
void ReadStringFromPseudoFileOrDie(const std::string &path, std::string * const data);
std::string ReadStringFromPseudoFileOrDie(const std::string &path);


/** \brief Append "data" to "path".  If "path" does not exist, it will be created. */
bool AppendString(const std::string &path, const std::string &data);
void AppendStringOrDie(const std::string &path, const std::string &data);


/** \brief  Does the named file (or directory) exist?.
 *  \param  path           The path of the file.
 *  \param  error_message  Where to store an error message if an error occurred.
 */
bool Exists(const std::string &path, std::string * const error_message = nullptr);


/** \brief  Does the named file or directory exist and is it readable?.
 *  \param  path           The path of the file.
 *  \param  error_message  Where to store an error message if an error occurred.
 */
bool IsReadable(const std::string &path, std::string * const error_message = nullptr);


std::string GetCurrentWorkingDirectory();


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
inline std::string MakeAbsolutePath(const std::string &relative_path) {
    return MakeAbsolutePath(GetCurrentWorkingDirectory() + "/", relative_path);
}


inline std::string MakeAbsolutePath(const char * const relative_path) {
    return MakeAbsolutePath(std::string(relative_path));
}


/** \brief Create an empty file or clear an existing file.
 *  \returnTrue upon success, false otherwise.
 *  \note Sets errno upon failure.
 *  \note If the file does not exist it will be created w/ mode (600 & ~umask).
 */
bool MakeEmpty(const std::string &path);


/** \brief Create an empty file or change timestamps (access, modify, change) of an existing file. */
void TouchFileOrDie(const std::string &path);


void ChangeOwnerOrDie(const std::string &path, const std::string &user = "", const std::string &group = "", const bool recursive = true);


/** \brief Attempts to get a filename (there may be multiple) from a file descriptor. */
std::string GetFileName(const int fd);


/** \brief Attempts to get a filename (there may be multiple) from a FILE. */
inline std::string GetFileName(FILE *file) {
    return GetFileName(fileno(file));
}


/** \brief  Attempts to set O_NONBLOCK on a file descriptor.
 *  \param  fd  An open file descriptor.
 *  \return True if we succeeded in setting O_NONBLOCK on "fd", else false.
 *  \note   If this function returns false errno contains an appropriate error code.
 */
bool SetNonblocking(const int fd);


/** \brief  Attempts to clear O_NONBLOCK on a file descriptor.
 *  \param  fd  An open file descriptor.
 *  \return True if we succeeded in clearing O_NONBLOCK on "fd", else false.
 *  \note   If this function returns false errno contains an appropriate error code.
 */
bool SetBlocking(const int fd);


/** \brief  Split a path into a directory name part and filename part.
 *  \param  path      The path to split.
 *  \param  dirname   Will hold the directory name part.
 *  \param  basename  Will hold the filename part.
 */
void DirnameAndBasename(const std::string &path, std::string * const dirname, std::string * const basename);
std::string GetDirname(const std::string &path);
std::string GetBasename(const std::string &path);


/** \return the part of "path" after the last slash or "path" if there is no slash. */
std::string GetLastPathComponent(const std::string &path);


/** \brief  Is the given path the name of a directory?
 *  \param  dir_name  The path to test.
 *  \return True if the path is a directory and can be accessed.
 *
 *  IsDirectory returns false if "dir_name" either doesn't exist, we
 *  don't have sufficient priviledges to stat it or it exists but is
 *  not a directory.
 */
bool IsDirectory(const std::string &dir_name);


/** \brief  Create a directory.
 *  \param  path       The path to create.
 *  \param  recursive  If true, attempt to recursively create parent directories too.
 *  \param  mode       The access permission for the directory/directories that will be created.
 *  \return True if the directory already existed or has been created else false.
 */
bool MakeDirectory(const std::string &path, const bool recursive = false, const mode_t mode = 0755);
void MakeDirectoryOrDie(const std::string &path, const bool recursive = false, const mode_t mode = 0755);
void MakeParentDirectoryOrDie(const std::string &path, const bool recursive = false, const mode_t mode = 0755);


/** \brief  Recursively delete a directory and all the files and subdirectories contained in it.
 *  \param  dir_name  The root of the directory tree that we wish to delete.
 *  \return True if we succeeded in removing the directory tree, else false.
 *  \note   If the function returns false, it sets errno which you can consult to determine the reason
 *          for the failure.
 */
bool RemoveDirectory(const std::string &dir_name);


/** \brief Removes files and possibly directories matching a regular expression pattern.
 *  \param  filename_regex       The pattern for the file and possibly the directory names to be deleted.
 *  \param  include_directories  If true, matching directories will also be deleted, even if non-empty.
 *                               If false, -1 will be returned should a directory match "filename_regex".
 *  \note   The pattern must not include a slash.
 *  \note   If a system call failed, errno will be set to indicate the error.
 *  \return The number of matched files if all files matching "filename_regex" have been successfully deleted, else -1.
 */
ssize_t RemoveMatchingFiles(const std::string &filename_regex, const bool include_directories = true,
                            const std::string &directory_to_scan = ".");


/** Repositions the offset of the open file associated with the file descriptor "fd" to the start of the file.
 *  If this function fails it returns false and sets errno to an appropriate error code.
 */
bool Rewind(const int fd);


/** \enum   FileType
 *  \brief  Possible types for a file.
 */
enum FileType {
    FILE_TYPE_UNKNOWN,
    FILE_TYPE_TEXT,    // .txt
    FILE_TYPE_HTML,    // .htm .html .php
    FILE_TYPE_PDF,     // .pdf
    FILE_TYPE_PS,      // .ps, .eps
    FILE_TYPE_DOC,     // .sxw .doc
    FILE_TYPE_SLIDES,  // .sxi .ppt
    FILE_TYPE_TEX,     // .tex ???
    FILE_TYPE_DVI,     // .dvi
    FILE_TYPE_TAR,     // .tar
    FILE_TYPE_RTF,     // .rtf
    FILE_TYPE_GZIP,    // .tgz, .gz
    FILE_TYPE_Z,       // .Z    COMPRESS
    FILE_TYPE_CODE,    // .c, .cc, .h, .pm, ...
    FILE_TYPE_GRAPHIC, // .gif, .jpg, ...
    FILE_TYPE_AUDIO,   // .ogg, .mp3
    FILE_TYPE_MOVIE    // .mpg, .mpeg, .divx
};


/** \brief  Attempt to guess the file type of "filename".
 *  \param  filename  The filename for which we'd like to determine the file type.
 *  \return The guessed file type.
 */
FileType GuessFileType(const std::string &filename);


/** \brief  Converts an enumerated type 'file_type' to a std::string.
 *  \param  file_type   The type of file that should be converted to a std::string.
 */
std::string FileTypeToString(FileType const file_type);


/** \brief Generates a list of filenames matching a regular expression.
 *  \note  The pattern must not include a slash.
 *  \return The number of matched file names.
 */
size_t GetFileNameList(const std::string &filename_regex, std::vector<std::string> * const matched_filenames,
                       const std::string &directory_to_scan = ".");

/** \brief Rename a file or directory.
 *  \param old_name              The original name.
 *  \param new_name              The target name.
 *  \param remove_target         If "new_name" already exists and this is set to true we will attempt to delete the existing
 *                               renaming target before attempting to rename "old_name".
 *  \param copy_if_cross_device  We will use copying if we're attempting a coss-device rename if this is set to true.
 *  \return True, upon success, else false.
 *  \note Sets errno if there was a failure.
 */
bool RenameFile(const std::string &old_name, const std::string &new_name, const bool remove_target = false,
                const bool copy_if_cross_device = true);
void RenameFileOrDie(const std::string &old_name, const std::string &new_name, const bool remove_target = false,
                     const bool copy_if_cross_device = true);


/** \brief Opens a file for reading or aborts. */
std::unique_ptr<File> OpenInputFileOrDie(const std::string &filename);


/** \brief Opens a file for writing or aborts. */
std::unique_ptr<File> OpenOutputFileOrDie(const std::string &filename);


/** \brief Opens a file for writing or aborts. */
std::unique_ptr<File> OpenTempFileOrDie(const std::string &path_prefix = "/tmp/ATF", const std::string &path_suffix = "");


/** \brief Opens a file for appending or aborts. */
std::unique_ptr<File> OpenForAppendingOrDie(const std::string &filename);


/** \brief Copies "no_of_bytes" from the current file offset of "from" to the current file offset of "to".
 *  \return True if the copying was successful, else false
 *  \note If false was returned, errno has been set.
 */
bool Copy(File * const from, File * const to, const size_t no_of_bytes);


/** \brief Copy parts or all of one file to another.
 *  \param truncate_target      If true and "to_path" already exists we truncate "to_path" before we start copying.
 *  \param no_of_bytes_to_copy  If 0 we copy the entire file from the starting offset o/w we copy "no_of_bytes_to_copy".
 *  \param offset               Where we start copying.  The interpretation depends on the value of "whence".
 *  \param whence               One of {SEEK_SET, SEEK_CUR, SEEK_END}.  See lseek(2) for what these values mean.
 */
bool Copy(const std::string &from_path, const std::string &to_path, const bool truncate_target = true, const size_t no_of_bytes_to_copy = 0,
          const loff_t offset = 0, const int whence = SEEK_CUR);
void CopyOrDie(const std::string &from_path, const std::string &to_path);

/**
 * \brief Copy file that support cross filesystem's type including mounted drive
 * \param fromPath              Source file with absolute or relative path
 * \param toPath                Target file with absolute or relative path
 */
void CopyOrDieXFs(const std::string fromPath, const std::string toPath);

/** \brief   Delete a file.
 *  \param   path  The path of the file to delete.
 *  \return  True if the files was successfully deleted.
 */
bool DeleteFile(const std::string &path);


/** \brief  Tests a file descriptor for readiness for reading.
 *  \param  fd          The file descriptor that we we want to test for read readiness.
 *  \param  time_limit  Up to how long to wait, in milliseconds, for the descriptor to become ready for reading.
 *  \return True if the specified file descriptor is ready for reading, otherwise false.
 *  \note   Please beware that on POSIX based systems it is possible that a file descriptor is ready for reading yet may
 *          still return 0 bytes!  It just means that read(2) will not hang.
 */
bool DescriptorIsReadyForReading(const int fd, const TimeLimit &time_limit = 0);


/** \brief  Tests a file descriptor for readiness for writing.
 *  \param  fd          The file descriptor that we we want to test for write readiness.
 *  \param  time_limit  Up to how long to wait, in milliseconds, for the descriptor to become ready for writing.
 *  \return True if the specified file descriptor is ready for writing, otherwise false.
 *  \note   Please beware that on POSIX based systems it is possible that a file descriptor is ready for writing yet may
 *          still return 0 bytes!  It just means that write(2) will not hang.
 */
bool DescriptorIsReadyForWriting(const int fd, const TimeLimit &time_limit = 0);


/** \brief  Reads an arbitrarily long line.
 *  \param  stream      The input stream to read from.
 *  \param  line        Where to store the read line.
 *  \param  terminator  The character terminating the line (won't be stored in "line").
 *  \return False on EOF, otherwise true.
 */
bool GetLine(std::istream &stream, std::string * const line, const char terminator = '\n');


/** \brief   Generate a unique filename
 *  \param   directory        The directory in which to create the temporary file (default: /tmp).
 *                            If directory is passed to be empty, the default is used again.
 *  \param   filename_prefix  Optional prefix for the filename.
 *  \param   filename_suffix  Optional suffix for the filename.
 *  \return  A new, unique file name.
 */
std::string UniqueFileName(const std::string &directory = "/tmp", const std::string &filename_prefix = "",
                           const std::string &filename_suffix = "");


/** \brief Compares the contents of two files.
 *  \note  Throws a std::runtime_error exception if "path1" or "path2" can't be read.
 */
bool FilesDiffer(const std::string &path1, const std::string &path2);


void AppendStringToFile(const std::string &path, const std::string &text);


void CreateSymlink(const std::string &target_filename, const std::string &link_filename);


// \return The size of the concatenated result.
size_t ConcatFiles(const std::string &target_path, const std::vector<std::string> &filenames,
                   const mode_t target_mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);


bool IsMountPoint(const std::string &path);


size_t CountLines(const std::string &filename);


// Strips all extensions from "filename" and returns what is left after that.
std::string GetFilenameWithoutExtensionOrDie(const std::string &filename);


// If "filename" has at least one period in its name, we return everything after the last period.  O/w we return an empty string.
std::string GetExtension(const std::string &filename, const bool to_lowercase = false);


/** \brief Nomen est omen.
 *  \note Aborts if "path" is no path components.
 *  \return The shortend path.
 */
std::string StripLastPathComponent(const std::string &path);


bool IsEmpty(const std::string &path);
bool IsPipeOrFIFO(const std::string &path);
bool IsSymlink(const std::string &path);
void ChangeDirectoryOrDie(const std::string &directory);
std::string GetPathFromFileDescriptor(const int fd);


/** Replaces leading tildes with the home directory of the current user. */
std::string ExpandTildePath(const std::string &path);


/**  \brief  Wait until a file appears.
 *   \param  sleep_increment  For how long to suspend the current process before we look again. (In seconds.)
 *   \return True if "path" was found before "timeout" seconds have elapsed, o/w/ false.
 */
bool WaitForFile(const std::string &path, const unsigned timeout, const unsigned sleep_increment = 10);


/** \brief Change the acces mode on "path", see chmod(2) for "mode".
 *  \note  If this function returns false you can consult errno for the reason that it failed.
 */
bool ChangeMode(const std::string &path, const mode_t mode);

void ChangeModeOrDie(const std::string &path, const mode_t mode);


/** \brief  Determine the most-recently-changed file in a given directory
 *  \return True if there was at least one file in "directory_path", else false.
 *  \note   This function aborts if "directory_path" does not exist.
 */
bool GetMostRecentlyModifiedFile(const std::string &directory_path, std::string * const filename, timespec * const last_modification_time);


// Removes up to "no_of_bytes" bytes from the beginning of "path" in please, IOW,
// w/o creating any other or temporary files.
void RemoveLeadingBytes(const std::string &path, const loff_t no_of_bytes);


// Removes leading lines from "path" and only keeps the last "n".
// If "n" is greater or equal to the lines in "path" the file will not be modified.
void OnlyKeepLastNLines(const std::string &path, const unsigned n);


/** \warning The following two functions are *not* threadsafe!
 *  \note They both return empty strings if the uid or gid can't be mapped.
 */
std::string UsernameFromUID(const uid_t uid);
std::string GroupnameFromGID(const gid_t gid);


/** \brief    A thin wrapper around the readlink(2) system call.
 *  \note     If this function returned false and you want to know the reason for
 *            the failure you can consult the value of errno.
 *  \warning This function is not reentrant!
 */
bool ReadLink(const std::string &path, std::string * const link_target);


} // namespace FileUtil

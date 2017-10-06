/** \file    FileUtil.h
 *  \brief   Declaration of file-related utility functions.
 *  \author  Dr. Gordon W. Paynter
 *  \author  Dr. Johannes Ruscheinski
 *  \author  Artur Kedzierski
 */

/*
 *  Copyright 2002-2008 Project iVia.
 *  Copyright 2002-2008 The Regents of The University of California.
 *  Copyright 2015-2017 Universitätsbibliothek Tübingen.  All rights reserved.
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


#include <memory>
#include <string>
#include <vector>
#include <cstdio>
#include <dirent.h>
#include <fcntl.h>
#include <sys/types.h>
#include <unistd.h>
#include "File.h"
#include "TimeLimit.h"


// Forward declaration:
class RegexMatcher;


namespace FileUtil {


/** \class AutoDeleteFile
 *  \brief Deletes the file, specified by the path that was given in the constructor, when going out of scope.
 */
class AutoDeleteFile {
    std::string path_;
public:
    explicit AutoDeleteFile(const std::string &path): path_(path) {}
    ~AutoDeleteFile() { if (not path_.empty()) ::unlink(path_.c_str()); }
};


class Directory {
    const std::string path_;
    const std::string regex_;
public:
    class const_iterator; // Forward declaration.
    class Entry {
        friend class Directory::const_iterator;
        const std::string * const dirname_;
        struct dirent entry_;
    public:
        Entry(const Entry &other);
        inline std::string getName() const { return entry_.d_name; }

        // \return One of DT_BLK(block device), DT_CHR(character device), DT_DIR(directory), DT_FIFO(named pipe),
        //         DT_LNK(symlink), DT_REG(regular file), DT_SOCK(UNIX domain socket), or DT_UNKNOWN(unknown type).
        unsigned char getType() const;
        inline ino_t getInode() const { return entry_.d_ino; }
    private:
        Entry(const struct dirent &entry, const std::string * const dirname);
    };
public:
    class const_iterator {
        friend class Directory;
        std::string path_;
        RegexMatcher *regex_matcher_;
        DIR *dir_handle_;
        struct dirent *last_entry_, entry_;
    public:
        ~const_iterator();

        Entry operator*();
        void operator++();
        bool operator==(const const_iterator &rhs);
        bool operator!=(const const_iterator &rhs) { return not operator==(rhs); }
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


/** \class AutoTempFile
 *  \brief Creates a temp file and removes it when going out of scope.
 */
class AutoTempFile {
    std::string path_;
public:
    explicit AutoTempFile(const std::string &path_prefix = "/tmp/AT");
    ~AutoTempFile() { if (not path_.empty()) ::unlink(path_.c_str()); }

    const std::string &getFilePath() const { return path_; }
};


bool WriteString(const std::string &path, const std::string &data);
bool ReadString(const std::string &path, std::string * const data);


/** \brief Append "data" to "path".  If "path" does not exist, it will be created. */
bool AppendString(const std::string &path, const std::string &data);


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


/** \brief Create an empty file or clear an existing file.
 *  \returnTrue upon success, false otherwise.
 *  \note Sets errno upon failure.
 *  \note If the file does not exist it will be created w/ mode (600 & ~umask).
 */
bool MakeEmpty(const std::string &path);


/** \brief Attempts to get a filename (there may be multiple) from a file descriptor. */
std::string GetFileName(const int fd);


/** \brief Attempts to get a filename (there may be multiple) from a FILE. */
inline std::string GetFileName(FILE * file) { return GetFileName(fileno(file)); }


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
 *  \param  recursive  If true, attempt to recursively create parent directoris too.
 *  \param  mode       The access permission for the directory/directories that will be created.
 *  \return True if the directory already existed or has been created else false.
 */
bool MakeDirectory(const std::string &path, const bool recursive = false, const mode_t mode = 0755);


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
        FILE_TYPE_TEXT,      // .txt
        FILE_TYPE_HTML,      // .htm .html .php
        FILE_TYPE_PDF,       // .pdf
        FILE_TYPE_PS,        // .ps, .eps
        FILE_TYPE_DOC,       // .sxw .doc
        FILE_TYPE_SLIDES,    // .sxi .ppt
        FILE_TYPE_TEX,       // .tex ???
        FILE_TYPE_DVI,       // .dvi
        FILE_TYPE_TAR,       // .tar
        FILE_TYPE_RTF,       // .rtf
        FILE_TYPE_GZIP,      // .tgz, .gz
        FILE_TYPE_Z,         // .Z    COMPRESS
        FILE_TYPE_CODE,      // .c, .cc, .h, .pm, ...
        FILE_TYPE_GRAPHIC,   // .gif, .jpg, ...
        FILE_TYPE_AUDIO,     // .ogg, .mp3
        FILE_TYPE_MOVIE      // .mpg, .mpeg, .divx
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
 *  \param old_name       The original name.
 *  \param new_name       The target name.
 *  \param remove_target  If "new_name" already exists and this is set to true we will attempt to delete the existing
 *                        renaming target before attempting to rename "old_name".
 *  \return True, upon success, else false.
 *  \note Sets errno if there was a failure.
 */
bool RenameFile(const std::string &old_name, const std::string &new_name, const bool remove_target = false);


/** \brief Opens a file for reading or aborts. */
std::unique_ptr<File> OpenInputFileOrDie(const std::string &filename);


/** \brief Opens a file for writing or aborts. */
std::unique_ptr<File> OpenOutputFileOrDie(const std::string &filename);


/** \brief Opens a file for appending or aborts. */
std::unique_ptr<File> OpenForAppendingOrDie(const std::string &filename);


/** \brief Copies "no_of_bytes" from the current file offset of "from" to the current file offset of "to".
 *  \return True if the copying was successful, else false
 *  \note If false was returned, errno has been set.
 */
bool Copy(File * const from, File * const to, const size_t no_of_bytes);


void CopyOrDie(const std::string &from_path, const std::string &to_path);


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


// \return The size of the concatenated result.
size_t ConcatFiles(const std::string &target_path, const std::vector<std::string> &filenames,
                   const mode_t target_mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);


bool IsMountPoint(const std::string &path);


} // namespace FileUtil


#endif // ifndef FILE_UTIL_H

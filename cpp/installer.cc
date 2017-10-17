/** \brief A tool for installing IxTheo and KrimDok from scratch on Ubuntu and Centos systems.
 *  \author Dr. Johannes Ruscheinski (johannes.ruscheinski@uni-tuebingen.de)
 *
 *  \note Compile with   g++ -std=gnu++14 -O3 -o installer installer.cc
 *  \note or             clang++ -std=gnu++11 -Wno-vla-extension -Wno-c++1y-extensions -O3 -o installer installer.cc
 *
 *  \copyright 2016,2017 Universitätsbibliothek Tübingen.  All rights reserved.
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
#include <algorithm>
#include <fstream>
#include <iostream>
#include <map>
#include <memory>
#include <set>
#include <sstream>
#include <stack>
#include <vector>
#include <cassert>
#include <climits>
#include <cstdlib>
#include <cstring>
#include <unordered_map>
#include <fcntl.h>
#include <signal.h>
#include <sys/sendfile.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>


/* Somewhere in the middle of the GCC 2.96 development cycle, a mechanism was implemented by which the user can tag likely branch directions and
   expect the blocks to be reordered appropriately.  Define __builtin_expect to nothing for earlier compilers.  */
#if __GNUC__ == 2 && __GNUC_MINOR__ < 96
#       define __builtin_expect(x, expected_value) (x)
#endif


#define likely(x)       __builtin_expect(!!(x), 1)
#define unlikely(x)     __builtin_expect(!!(x), 0)


/** Must be set to point to argv[0] in main(). */
static char *progname;


__attribute__((noreturn)) void Error(const std::string &msg) {
    if (progname == nullptr)
        std::cerr << "You must set \"progname\" in main() with \"progname = argv[0];\" in oder to use Error().\n";
    else
        std::cerr << progname << ": " << msg << '\n';
    std::exit(EXIT_FAILURE);
}


__attribute__((noreturn)) void Usage() {
    std::cerr << "Usage: " << ::progname << " --ub-tools-only|(vufind_system_type [--omit-cronjobs] [--omit-systemctl])\n";
    std::cerr << "       where \"vufind_system_type\" must be either \"krimdok\" or \"ixtheo\".\n\n";
    std::exit(EXIT_FAILURE);
}


// Print a log message to the terminal with a bright green background.
void Echo(const std::string &log_message) {
    std::cout << "\x1B" << "[42m--- " << log_message << "\x1B" << "[0m\n";
}


// Creates a symlink called "link_filename" pointing to "target_filename".
void FileUtil_CreateSymlink(const std::string &target_filename, const std::string &link_filename) {
    if (unlikely(::unlink(link_filename.c_str()) == -1 and errno != ENOENT /* "No such file or directory." */))
        throw std::runtime_error("unlink(2) of \"" + link_filename + "\" failed: " + std::string(::strerror(errno)));
    if (unlikely(::symlink(target_filename.c_str(), link_filename.c_str()) != 0))
        throw std::runtime_error("failed to create symlink \"" + link_filename + "\" => \"" + target_filename + "\"! ("
                           + std::string(::strerror(errno)) + ")");
}


// DirnameAndBasename -- Split a path into a directory name part and filename part.
//
void FileUtil_DirnameAndBasename(const std::string &path, std::string * const dirname, std::string * const basename) {
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


// Exists -- test whether a file exists
bool FileUtil_Exists(const std::string &path) {
    int access_status = ::access(path.c_str(), F_OK);
    return (access_status == 0);
}


std::string FileUtil_GetCurrentWorkingDirectory() {
    char buf[PATH_MAX];
    const char * const current_working_dir(::getcwd(buf, sizeof buf));
    if (unlikely(current_working_dir == nullptr))
        throw std::runtime_error("in FileUtil::GetCurrentWorkingDirectory: getcwd(3) failed ("
                                 + std::string(::strerror(errno)) + ")!");
    return current_working_dir;
}


off_t FileUtil_GetFileSize(const std::string &path) {
    struct stat stat_buf;
    if (::stat(path.c_str(), &stat_buf) == -1)
        Error("in FileUtil::GetFileSize: can't stat(2) \"" + path + "\"!");

    return stat_buf.st_size;
}


bool FileUtil_IsMountPoint(const std::string &path) {
    struct stat statbuf;
    if (::stat(path.c_str(), &statbuf) == -1)
        Error("in FileUtil::IsMountPoint: stat(2) on \"" + path + "\" failed! (" + std::string(::strerror(errno))
              + ")");

    struct stat parent_statbuf;
    if (::stat((path + "/..").c_str(), &parent_statbuf) == -1)
        Error("in FileUtil::IsMountPoint: stat(2) on \"" + path + "/..\" failed! (" + std::string(::strerror(errno))
              + ")");

    return statbuf.st_dev != parent_statbuf.st_dev;
}


bool FileUtil_ReadString(const std::string &path, std::string * const data) {
    std::ifstream input(path, std::ios_base::in | std::ios_base::binary);
    if (input.fail())
        return false;

    const off_t file_size(FileUtil_GetFileSize(path));
    data->resize(file_size);
    input.read(const_cast<char *>(data->data()), file_size);
    return not input.bad();

}


bool FileUtil_Rewind(const int fd) {
    return ::lseek(fd, 0, SEEK_SET) == 0;
}


std::string ReadStringOrDie(const std::string &path) {
    std::string file_contents;
    if (not FileUtil_ReadString(path, &file_contents))
        Error("in ReadStringOrDie: failed to read \"" + path + "\"!");
    return file_contents;
}


static inline bool StringUtil_CaseInsensitiveEqual(const char ch1, const char ch2) {
    return std::toupper(ch1) == std::toupper(ch2);
}


size_t StringUtil_FindCaseInsensitive(const std::string &haystack, const std::string &needle) {
    const auto iter(std::search(haystack.begin(), haystack.end(), needle.begin(), needle.end(),
                                StringUtil_CaseInsensitiveEqual));
    return iter == haystack.end() ? std::string::npos : iter - haystack.begin();
}


enum VuFindSystemType { KRIMDOK, IXTHEO };

std::string VuFindSystemTypeToString(VuFindSystemType vufind_system_type) {
    if (vufind_system_type == KRIMDOK)
        return "krimdok";
    else if (vufind_system_type == IXTHEO)
        return "ixtheo";
    else
        Error("invalid VuFind system type!");
}

enum OSSystemType { UBUNTU, CENTOS };


OSSystemType DetermineOSSystemType() {
    std::string file_contents;
    if (FileUtil_ReadString("/etc/issue", &file_contents)
        and StringUtil_FindCaseInsensitive(file_contents, "ubuntu") != std::string::npos)
        return UBUNTU;
    if (FileUtil_ReadString("/etc/redhat-release", &file_contents)
        and StringUtil_FindCaseInsensitive(file_contents, "centos") != std::string::npos)
        return CENTOS;
    Error("you're probably not on an Ubuntu nor on a CentOS system!");
}


namespace {


// The following variables are set in Execute.
static bool alarm_went_off;
pid_t child_pid;


// SigAlarmHandler -- Used by Execute.
//
void SigAlarmHandler(int /* sig_no */) {
    alarm_went_off = true;
}


bool IsExecutableFile(const std::string &path) {
    struct stat statbuf;
    return ::stat(path.c_str(), &statbuf) == 0 and (statbuf.st_mode & S_IXUSR);
}


enum class ExecMode {
    WAIT,  //< Exec() will wait for the child to exit.
    DETACH //< Exec() will not wait for the child to exit and will return the child's PID.
};


int Exec(const std::string &command, const std::vector<std::string> &args, const std::string &new_stdin,
         const std::string &new_stdout, const std::string &new_stderr, const ExecMode exec_mode,
         unsigned timeout_in_seconds, const int tardy_child_signal)
{
    if (::access(command.c_str(), X_OK) != 0)
        throw std::runtime_error("in ExecUtil::Exec: can't execute \"" + command + "\"!");

    if (exec_mode == ExecMode::DETACH and timeout_in_seconds > 0)
        throw std::runtime_error("in ExecUtil::Exec: non-zero timeout is imcompatible w/ ExecMode::DETACH!");

    const int EXECVE_FAILURE(248);

    const pid_t pid = ::fork();
    if (pid == -1)
        throw std::runtime_error("in Exec: ::fork() failed: " + std::to_string(errno) + "!");

    // The child process:
    else if (pid == 0) {
        // Make us the leader of a new process group:
        if (::setsid() == static_cast<pid_t>(-1))
            Error("in Exec(): child failed to become a new session leader!");

        if (not new_stdin.empty()) {
            const int new_stdin_fd(::open(new_stdin.c_str(), O_RDONLY));
            if (new_stdin_fd == -1)
                ::_exit(-1);
            if (::dup2(new_stdin_fd, STDIN_FILENO) == -1)
                ::_exit(-1);
            ::close(new_stdin_fd);
        }

        if (not new_stdout.empty()) {
            const int new_stdout_fd(::open(new_stdout.c_str(), O_WRONLY | O_CREAT, 0644));
            if (new_stdout_fd == -1)
                ::_exit(-1);
            if (::dup2(new_stdout_fd, STDOUT_FILENO) == -1)
                ::_exit(-1);
            ::close(new_stdout_fd);
        }

        if (not new_stderr.empty()) {
            const int new_stderr_fd(::open(new_stderr.c_str(), O_WRONLY | O_CREAT, 0644));
            if (new_stderr_fd == -1)
                ::_exit(-1);
            if (::dup2(new_stderr_fd, STDERR_FILENO) == -1)
                ::_exit(-1);
            ::close(new_stderr_fd);
        }

        // Build the argument list for execve(2):
        #pragma GCC diagnostic ignored "-Wvla"
        char *argv[1 + args.size() + 1];
        #pragma GCC diagnostic warning "-Wvla"
        unsigned arg_no(0);
        argv[arg_no++] = ::strdup(command.c_str());
        for (const auto &arg : args)
            argv[arg_no++] = ::strdup(arg.c_str());
        argv[arg_no] = nullptr;
        ::execv(command.c_str(), argv);

        ::_exit(EXECVE_FAILURE); // We typically never get here.
    }

    // The parent of the fork:
    else {
        if (exec_mode == ExecMode::DETACH)
            return pid;

        void (*old_alarm_handler)(int) = nullptr;

        if (timeout_in_seconds > 0) {
            // Install new alarm handler...
            alarm_went_off = false;
            child_pid = pid;
            old_alarm_handler = ::signal(SIGALRM, SigAlarmHandler);

            // ...and wind the clock:
            ::alarm(timeout_in_seconds);
        }

        int child_exit_status;
        errno = 0;
        int wait_retval = ::wait4(pid, &child_exit_status, 0, nullptr);
        assert(wait_retval == pid or errno == EINTR);

        if (timeout_in_seconds > 0) {
            // Cancel any outstanding alarm:
            ::alarm(0);

            // Restore the old alarm handler:
            ::signal(SIGALRM, old_alarm_handler);

            // Check to see whether the test timed out or not:
            if (alarm_went_off) {
                // Snuff out all of our offspring.
                ::kill(-pid, tardy_child_signal);
                while (::wait4(-pid, &child_exit_status, 0, nullptr) != -1)
                    /* Intentionally empty! */;

                return -1;
            }
        }

        // Now process the child's various exit status values:
        if (WIFEXITED(child_exit_status)) {
            switch (WEXITSTATUS(child_exit_status)) {
            case EXECVE_FAILURE:
                throw std::runtime_error("in Exec: failed to execve(2) in child!");
            default:
                return WEXITSTATUS(child_exit_status);
            }
        }
        else if (WIFSIGNALED(child_exit_status))
            throw std::runtime_error("in Exec: \"" + command + "\" killed by signal "
                                     + std::to_string(WTERMSIG(child_exit_status)) + "!");
        else // I have no idea how we got here!
            Error("in Exec: dazed and confused!");
    }

    return 0; // Keep the compiler happy!
}


} // unnamed namespace


/** \brief  Run a subcommand to completion.
 *  \param  command             The path to the command that should be executed.
 *  \param  args                The arguments for the command, not including the command itself.
 *  \param  new_stdin           An optional replacement file path for stdin.
 *  \param  new_stdout          An optional replacement file path for stdout.
 *  \param  new_stderr          An optional replacement file path for stderr.
 *  \param  timeout_in_seconds  If not zero, the subprocess will be killed if the timeout expires before
 *                              the process terminates.
 *  \param  tardy_child_signal  The signal to send to our offspring if there was a timeout.
 *  \return The exit code of the subcommand or an error code if there was a failure along the way.
 */
int ExecUtil_Exec(const std::string &command, const std::vector<std::string> &args = {},
                  const std::string &new_stdin = "", const std::string &new_stdout = "",
                  const std::string &new_stderr = "", const unsigned timeout_in_seconds = 0,
                  const int tardy_child_signal = SIGKILL)
{
    return ::Exec(command, args, new_stdin, new_stdout, new_stderr, ExecMode::WAIT, timeout_in_seconds,
                  tardy_child_signal);
}


void ExecOrDie(const std::string &command, const std::vector<std::string> &arguments = {},
               const std::string &new_stdin = "", const std::string &new_stdout = "")
{
    int exit_code;
    if ((exit_code = ExecUtil_Exec(command, arguments, new_stdin, new_stdout)) != 0)
        Error("Failed to execute \"" + command + "\"! (exit code was " + std::to_string(exit_code) + ")");
}


const std::string UB_TOOLS_DIRECTORY("/usr/local/ub_tools");
const std::string VUFIND_DIRECTORY("/usr/local/vufind");

const std::string INSTALLER_DATA_DIRECTORY(UB_TOOLS_DIRECTORY + "/cpp/data/installer");
const std::string INSTALLER_SCRIPTS_DIRECTORY(INSTALLER_DATA_DIRECTORY + "/scripts");


void ChangeDirectoryOrDie(const std::string &new_working_directory) {
    if (::chdir(new_working_directory.c_str()) != 0)
        Error("failed to set the new working directory to \"" + new_working_directory + "\"! ("
              + std::string(::strerror(errno)) + ")");
}


std::string GetPassword(const std::string &prompt) {
    errno = 0;
    const std::string password(::getpass((prompt + " > ").c_str()));
    if (errno != 0)
        Error("failed to read the password from the terminal!");

    return password;
}


class TemporaryChDir {
    std::string old_working_dir_;
public:
    explicit TemporaryChDir(const std::string &new_working_dir);
    ~TemporaryChDir();
};


TemporaryChDir::TemporaryChDir(const std::string &new_working_dir)
    : old_working_dir_(FileUtil_GetCurrentWorkingDirectory())
{
    ChangeDirectoryOrDie(new_working_dir);
}


TemporaryChDir::~TemporaryChDir() {
    ChangeDirectoryOrDie(old_working_dir_);
}


class File {
public:
    template<typename ArgType> struct SingleArgManipulator {
        File &(*func_)(File &file, ArgType arg);
        ArgType arg_;
    public:
        SingleArgManipulator(File &(*func)(File &file, ArgType arg), ArgType arg): func_(func), arg_(arg) { }
    };

    enum ThrowOnOpenBehaviour { THROW_ON_ERROR, DO_NOT_THROW_ON_ERROR };
private:
    enum OpenMode { READING, WRITING, READING_AND_WRITING };
private:
    std::string filename_;
    char buffer_[BUFSIZ];
    char *buffer_ptr_;
    size_t read_count_;
    FILE *file_;
    unsigned pushed_back_count_;
    char pushed_back_chars_[2];
    int precision_;
    OpenMode open_mode_;
public:
    /** \brief  Creates and initalises a File object.
     *  \param  path                      The pathname for the file (see fopen(3) for details).
     *  \param  mode                      The open mode (see fopen(3) for details).  An extension to the fopen modes
     *                                    are either "c" or "u".  "c" meaning "compress" can only be combined with "w"
     *                                    and "u" meaning "uncompress" with "r".  Using either flag makes seeking or
     *                                    rewinding impossible.
     *  \param  throw_on_error_behaviour  If true, any open failure will cause an exception to be thrown.  If not true
     *                                    you must use the fail() member function.
     */
    File(const std::string &filename, const std::string &mode,
         const ThrowOnOpenBehaviour throw_on_error_behaviour = DO_NOT_THROW_ON_ERROR);

    /** \brief  Create a File object from a file descriptor.
     *  \param  fd    A valid (open) file descriptor for the current process.
     *  \param  mode  The open mode (see fopen(3) for details).  Must be compatible, i.e. a subset of the mode for
     *                "fd."  If not specified the mode for "fd" will be used.
     */
    explicit File(const int fd, const std::string &mode = "");

    ~File() { if (file_ != nullptr) std::fclose(file_); }

    /** Closes this File.  If this fails you may consult the global "errno" for the reason. */
    bool close();

    inline int getFileDescriptor() const { return fileno(file_); }

    inline off_t tell() const {
        const off_t file_pos(::ftello(file_));
        if (open_mode_ == WRITING)
            return file_pos;
        return file_pos - read_count_ + (buffer_ptr_ - buffer_) - pushed_back_count_;
    }

    /** \brief  Set the file pointer for the next I/O operation.
     *  \param  offset  Signed offset relative to the reference point specified by "whence".
     *  \param  whence  SEEK_SET, SEEK_END, or SEEK_CUR.
     *  \return True upon success, else false.  If false, you can consult the global "errno" variable for the type
     *          of error encountered.
     */
    bool seek(const off_t offset, const int whence = SEEK_SET);

    inline int get() {
        if (unlikely(pushed_back_count_ > 0)) {
            const char pushed_back_char(pushed_back_chars_[0]);
            --pushed_back_count_;
            for (unsigned i(0); i < pushed_back_count_; ++i)
                pushed_back_chars_[i] = pushed_back_chars_[i + 1];
            return pushed_back_char;
        }

        if (unlikely(buffer_ptr_ == buffer_ + read_count_))
            fillBuffer();
        if (unlikely(read_count_ == 0))
            return EOF;
        return *buffer_ptr_++;
    }

    /** \brief  Read some data from a file.
     *  \param  buf       The data to read.
     *  \param  buf_size  How much data to read.
     *  \return Returns a short count if an error occurred or EOF was encountered, otherwise returns "buf_size".
     *  \note   On returning a short count you need to call either eof() or anErrorOccurred() in order to
     *          determine whether the short count is due to an error condition or EOF.
     */
    size_t read(void * const buf, const size_t buf_size);

    /** \brief  Write some data to a file.
     *  \param  buf       The data to write.
     *  \param  buf_size  How much data to write.
     *  \return Returns a short count if an error occurred, otherwise returns "buf_size".
     */
    size_t write(const void * const buf, const size_t buf_size);

    /** \brief Write a string to a file.
     *  \param s  The string to write.
     *  \return true if we succeeded, else false.
     */
    inline bool write(const std::string &s)
        { return write(reinterpret_cast<const void *>(s.data()), s.size()) == s.size(); }

    /** \brief Write a character. */
    inline int put(const char ch) { return putc(ch, file_); }

    inline void putback(const char ch) {
        if (unlikely(pushed_back_count_ == sizeof(pushed_back_chars_)))
            throw std::runtime_error("in File::putback: can't push back " + std::to_string(sizeof(pushed_back_chars_))
                                     + " characters in a row!");
        pushed_back_chars_[pushed_back_count_] = ch;
        ++pushed_back_count_;
    }

    inline int peek() {
        if (unlikely(pushed_back_count_ > 0))
            return pushed_back_chars_[0];
        const int ch(get());
        if (likely(ch != EOF))
            putback(static_cast<char>(ch));
        return ch;
    }

    /** \brief    Extracts a "line" from an input stream.
     *  \param    line        The extracted "line" after the call.
     *  \param    terminator  The line terminator.  (Will not be included in "line".)
     *  \return   The number of extracted characters not including a possible terminator.
     *  \warning  The caller has to test for EOF separately, for example with the eof() member function!
     */
    size_t getline(std::string * const line, const char terminator = '\n');

    /** \brief    Extracts a "line" from an input stream.
     *  \param    terminator  The line terminator.  (Will not be included in "line".)
     *  \return   The line as an std::string
     *  \warning  The caller has to test for EOF separately, for example with the eof() member function!
     */
    inline std::string getline(const char terminator = '\n') {
        std::string line;
        getline(&line, terminator);
        return line;
    }

    const std::string &getPath() const { return filename_; }

    /** Returns a File's size in bytes. */
    off_t size() const;

    inline bool eof() const { return (buffer_ptr_ == buffer_ + read_count_) and std::feof(file_) != 0; }
    inline bool anErrorOccurred() const { return std::ferror(file_) != 0; }

    /** Will the next I/O operation fail? */
    inline bool fail() const { return file_ == nullptr or eof() or std::ferror(file_) != 0; }

    inline bool operator!() const { return fail(); }

    File &operator<<(const char * const s);
    File &operator<<(const std::string &s) { return operator<<(s.c_str()); }
    File &operator<<(const char ch);
    File &operator<<(const int i);
    File &operator<<(const unsigned u);
    File &operator<<(const long l);
    File &operator<<(const unsigned long ul);
    File &operator<<(const long long ll);
    File &operator<<(const unsigned long long ull);
    File &operator<<(const double d);
    File &operator<<(File &(*f)(File &)) { return f(*this); } // Supports I/O manipulators taking 0 arguments.

    /** Resets the file pointer to the beginning of the file. */
    inline void rewind() {
        if (unlikely(file_ == nullptr))
            throw std::runtime_error("in File::rewind: can't rewind a non-open file!");
        std::rewind(file_);
        if (open_mode_ != WRITING) {
            read_count_ = 0;
            buffer_ptr_ = buffer_;
        }
    }

    /** \brief  Flush all internal I/O buffers.
     *  \return True on success and false on failure.  Sets errno if there is a failure. */
    inline bool flush() const { return std::fflush(file_) == 0; }

    /** Appends the contents of the file corresponding to "fd" to the current File. (Maintains "fd"'s original
        offset.) */
    bool append(const int fd);

    bool append(const File &file);

    /** \brief  Change the length a file.
     *  \param  new_length  The length to which the file will be truncated if it was previously longer, or the length
     *                      to which the file will be extended if it was previously shorter.
     *  \return True if the operation succeeded, else false.  In case of an error, "errno" will be set to the
     *          approriate error code.
     */
    bool truncate(const off_t new_length = 0);

    static File &endl(File &f) { f.put('\n'), f.flush(); return f; }
    static SingleArgManipulator<int> setprecision(int new_precision) {
        return SingleArgManipulator<int>(SetPrecision, new_precision); }
private:
    void fillBuffer();
    static File &SetPrecision(File &f, int new_precision) { f.precision_ = new_precision; return f; }
};


File::File(const std::string &filename, const std::string &mode, const ThrowOnOpenBehaviour throw_on_error_behaviour)
    : filename_(filename), buffer_ptr_(buffer_), read_count_(0), file_(nullptr), pushed_back_count_(0), precision_(6)
{
    if (mode == "w")
        open_mode_ = WRITING;
    else if (mode == "a")
        open_mode_ = WRITING;
    else if (mode == "r")
        open_mode_ = READING;
    else if (mode == "r+")
        open_mode_ = READING_AND_WRITING;
    else {
        if (throw_on_error_behaviour == THROW_ON_ERROR)
            throw std::runtime_error("in File::File: open mode \"" + mode + "\" not supported! (1)");
        return;
    }

    file_ = std::fopen(filename.c_str(), mode.c_str());
    if (file_ == nullptr) {
        if (throw_on_error_behaviour == THROW_ON_ERROR)
            throw std::runtime_error("in File::File: could not open \"" + filename + "\" w/ mode \"" + mode + "\"!");
    }

}


File::File(const int fd, const std::string &mode)
    : filename_("/proc/self/fd/" + std::to_string(fd)), buffer_ptr_(buffer_), read_count_(0), file_(nullptr),
      pushed_back_count_(0), precision_(6)
{
    std::string local_mode;
    if (mode.empty()) {
        // Determine the mode from "fd":
        int flags;
        if (unlikely((flags = ::fcntl(fd, F_GETFL, &flags)) == -1))
            throw std::runtime_error("in File::File: fcntl(2) failed (" + std::string(::strerror(errno)) + ")!");

        flags &= O_ACCMODE;
        if (flags == O_RDONLY) {
            local_mode = "r";
            open_mode_ = READING;
        } else if (flags == O_WRONLY) {
            local_mode = "w";
            open_mode_ = WRITING;
        } else { // We assume flags == O_RDWR.
            local_mode = "r+";
            open_mode_ = READING_AND_WRITING;
        }
    } else {
        if (mode == "r")
            open_mode_ = READING;
        else if (mode == "w")
            open_mode_ = WRITING;
        else if (mode == "r+")
            open_mode_ = READING_AND_WRITING;
        else
            throw std::runtime_error("in File::File: open mode \"" + mode + "\" not supported! (2)");
        local_mode = mode;
    }

    file_ = ::fdopen(fd, local_mode.c_str());
    if (unlikely(file_ == nullptr))
        throw std::runtime_error("in File::File: fdopen(3) on \"" + std::to_string(fd) + "\" with mode \""
                                 + mode + "\" failed (" + std::string(::strerror(errno)) + ") (3)!");
}


bool File::close() {
    if (file_ == nullptr) {
        errno = 0;
        return false;
    }

    const bool retval(std::fclose(file_) == 0);
    file_ = nullptr;
    return retval;
}


void File::fillBuffer() {
    read_count_ = std::fread(reinterpret_cast<void *>(buffer_), 1, BUFSIZ, file_);
    if (unlikely(std::ferror(file_) != 0))
        throw std::runtime_error("in File:fillBuffer: error while reading \"" + filename_ + "\"!");
    buffer_ptr_ = buffer_;
}


off_t File::size() const {
    if (unlikely(file_ == nullptr))
        throw std::runtime_error("in File::size: can't obtain the size of non-open File \"" + filename_
                                 + "\"!");

    struct stat stat_buf;
    if (unlikely(::fstat(fileno(file_), &stat_buf) == -1))
        throw std::runtime_error("in File::size: fstat(2) failed on \"" + filename_ + "\" ("
                                 + std::string(::strerror(errno)) + ")!");

    return stat_buf.st_size;
}


bool File::seek(const off_t offset, const int whence) {
    if (unlikely(file_ == nullptr))
        throw std::runtime_error("in File::seek: can't seek on non-open file \"" + filename_ + "\"!");

    const off_t adjusted_offset(whence != SEEK_CUR ? offset : offset - pushed_back_count_);
    if (::fseeko(file_, adjusted_offset, whence) != 0)
        return false;

    pushed_back_count_ = 0;
    read_count_        = 0;
    buffer_ptr_        = buffer_;

    return true;
}


size_t File::read(void * const buf, const size_t buf_size) {
    if (unlikely(file_ == nullptr))
        throw std::runtime_error("in File::read: can't read from non-open file \"" + filename_ + "\"!");

    return ::fread(buf, 1, buf_size, file_);
}


size_t File::write(const void * const buf, const size_t buf_size) {
    if (unlikely(file_ == nullptr))
        throw std::runtime_error("in File::write: can't write to non-open file \"" + filename_ + "\"!");

    return ::fwrite(buf, 1, buf_size, file_);
}


size_t File::getline(std::string * const line, const char terminator) {
    line->clear();

    size_t count(0);
    for (;;) {
        const int ch(get());
        if (unlikely(ch == terminator or ch == EOF))
            return count;
        *line += static_cast<char>(ch);
        ++count;
    }
}


File &File::operator<<(const char * const s) {
    std::fputs(s, file_);
    return *this;
}


File &File::operator<<(const char ch) {
    std::fputc(ch, file_);
    return *this;
}


File &File::operator<<(const int i) {
    std::fprintf(file_, "%1d", i);
    return *this;
}


File &File::operator<<(const unsigned u) {
    std::fprintf(file_, "%1u", u);
    return *this;
}


File &File::operator<<(const long l) {
    std::fprintf(file_, "%1ld", l);
    return *this;
}


File &File::operator<<(const unsigned long ul) {
    std::fprintf(file_, "%1lu", ul);
    return *this;
}


File &File::operator<<(const long long ll) {
    std::fprintf(file_, "%1lld", ll);
    return *this;
}


File &File::operator<<(const unsigned long long ull) {
    std::fprintf(file_, "%1llu", ull);
    return *this;
}


File &File::operator<<(const double d) {
    std::fprintf(file_, "%.*g", precision_, d);
    return *this;
}


class FileDescriptor {
    int fd_;
public:
    FileDescriptor(): fd_(-1) { }
    explicit FileDescriptor(const int fd): fd_(fd) { }

    /** Creates a duplicate file descriptor using dup(2). */
    FileDescriptor(const FileDescriptor &rhs);

    ~FileDescriptor() { close();}

    void close();

    bool isValid() const { return fd_ != -1; }
    bool operator!() const { return fd_ == -1; }
    operator int() const { return fd_; }

    // Assignment operators:
    const FileDescriptor &operator=(const FileDescriptor &rhs);
    const FileDescriptor &operator=(const int new_fd);

    /** \brief   Reliquishes ownership.
     *  \warning The caller becomes responsible for closing of the returned file descriptor!
     */
    int release();
};


FileDescriptor::FileDescriptor(const FileDescriptor &rhs) {
    if (rhs.fd_ == -1)
        fd_ = -1;
    else {
        fd_ = ::dup(rhs.fd_);
        if (unlikely(fd_ == -1))
            throw std::runtime_error("in FileDescriptor::FileDescriptor: dup(2) failed (" + std::to_string(errno) + ")!");
    }
}


void FileDescriptor::close() {
    if (unlikely(fd_ != -1))
        ::close(fd_);

    fd_ = -1;
}


const FileDescriptor &FileDescriptor::operator=(const FileDescriptor &rhs) {
    // Prevent self-assignment!
    if (likely(&rhs != this)) {
        if (unlikely(fd_ != -1))
            ::close(fd_);

        fd_ = ::dup(rhs.fd_);
        if (unlikely(fd_ == -1))
            throw std::runtime_error("in FileDescriptor::operator=: dup(2) failed (" + std::to_string(errno) + ")!");
    }

    return *this;
}


const FileDescriptor &FileDescriptor::operator=(const int new_fd) {
    if (unlikely(fd_ != -1))
        ::close(fd_);

    fd_ = new_fd;

    return *this;
}


int FileDescriptor::release() {
    const int retval(fd_);
    fd_ = -1;
    return retval;
}


bool File::append(const int fd) {
    const off_t original_offset(::lseek(fd, 0, SEEK_CUR));
    if (unlikely(not FileUtil_Rewind(fd)))
        return false;

    flush();
    const int target_fd(fileno(file_));
    char buf[BUFSIZ];
    ssize_t read_count;
    errno = 0;
    while ((read_count = ::read(fd, buf, sizeof(buf))) > 0) {
        if (unlikely(::write(target_fd, buf, read_count) != read_count)) {
            ::lseek(fd, original_offset, SEEK_SET);
            return false;
        }
    }

    ::lseek(fd, original_offset, SEEK_SET);
    return errno == 0 ? true : false;
}


bool File::append(const File &file) {
    if (unlikely(not file.flush()))
        return false;
    return append(fileno(file.file_));
}


bool File::truncate(const off_t new_length) {
    if (unlikely(file_ == nullptr))
        throw std::runtime_error("in File::setNewSize: can't get non-open file's size \"" + filename_ + "\"!");

    flush();
    return ::ftruncate(fileno(file_), new_length) == 0;
}


std::unique_ptr<File> FileUtil_OpenInputFileOrDie(const std::string &filename) {
    std::unique_ptr<File> file(new File(filename, "r"));
    if (file->fail())
        Error("can't open \"" + filename + "\" for reading!");

    return file;
}


/** \brief   Does the given string start with the suggested prefix?
 *  \param   s            The string to test.
 *  \param   prefix       The prefix to test for.
 *  \param   ignore_case  If true, the match will be case-insensitive.
 *  \return  True if the string "s" equals or starts with the prefix "prefix."
 */
inline bool StringUtil_StartsWith(const std::string &s, const std::string &prefix, const bool ignore_case = false) {
    return prefix.empty() or (s.length() >= prefix.length()
           and (ignore_case ? (::strncasecmp(s.c_str(), prefix.c_str(), prefix.length()) == 0)
                            : (std::strncmp(s.c_str(), prefix.c_str(), prefix.length()) == 0)));
}


bool FileContainsLineStartingWith(const std::string &path, const std::string &prefix) {
    std::unique_ptr<File> input(FileUtil_OpenInputFileOrDie(path));
    while (not input->eof()) {
        std::string line;
        input->getline(&line);
        if (StringUtil_StartsWith(line, prefix))
            return true;
    }

    return false;
}


/** \brief  Split a string around a delimiter string.
 *  \param  source                     The string to split.
 *  \param  delimiter_string           The string to split around.
 *  \param  container                  A list to return the resulting fields in.
 *  \param  suppress_empty_components  If true we will not return empty fields.
 *
 *  Splits "source" around the characters in "field_separators" and return the resulting list of fields in "fields."
 *  Empty fields are returned in the list.
 *
 *  \return The number of extracted "fields".
 */
template<typename InsertableContainer> unsigned StringUtil_Split(const std::string &source,
                                                                 const std::string &delimiter_string,
                                                                 InsertableContainer * const container,
                                                                 const bool suppress_empty_components = true)
{
        if (unlikely(delimiter_string.empty()))
                throw std::runtime_error("in StringUtil::Split: empty delimited string!");

        container->clear();
        if (source.empty())
              return 0;

        std::string::size_type start = 0;
        std::string::size_type next_delimiter = 0;
        unsigned count = 0;

        while (next_delimiter != std::string::npos) {
                // Search for first occurence of delimiter that appears after start
                next_delimiter = source.find(delimiter_string, start);

                // Add the field starting at start and ending at next_delimiter
                if (next_delimiter == std::string::npos) {
                        if (not suppress_empty_components or start < source.length())
                                container->insert(container->end(), source.substr(start));
                        ++count;
                }
                else if (next_delimiter > start) {
                        if (not suppress_empty_components or start < next_delimiter)
                                container->insert(container->end(), source.substr(start, next_delimiter - start));
                        ++count;
                }

                // Move the start pointer along the array
                if (next_delimiter != std::string::npos)
                        start = next_delimiter + delimiter_string.length();
                if (start >= source.length())
                        next_delimiter = std::string::npos;
        }

        return count;
}


/** \brief  Split a string around a delimiter.
 *  \param  source                     The string to split.
 *  \param  delimiter                  The character to split around.
 *  \param  container                  A list to return the resulting fields in.
 *  \param  suppress_empty_components  If true we will not return empty fields.
 *  \return The number of extracted "fields".
 *
 *  Splits "source" around the character in "delimiter" and return the resulting list of fields in "fields."
 *  Empty fields are returned in the list.
 */
template<typename InsertableContainer> unsigned StringUtil_Split(const std::string &source, const char delimiter,
                                                                 InsertableContainer * const container,
                                                                 const bool suppress_empty_components = true)
{
        container->clear();
        if (source.empty())
              return 0;

        std::string::size_type start = 0;
        std::string::size_type next_delimiter = 0;
        unsigned count = 0;

        while (next_delimiter != std::string::npos) {
                // Search for first occurence of delimiter that appears after start:
                next_delimiter = source.find(delimiter, start);

                // Add the field starting at start and ending at next_delimiter:
                if (next_delimiter == std::string::npos) {
                        if (not suppress_empty_components or start < source.length())
                                container->insert(container->end(), source.substr(start));
                        ++count;
                }
                else if (next_delimiter > start) {
                        if (not suppress_empty_components or start < next_delimiter)
                                container->insert(container->end(), source.substr(start, next_delimiter - start));
                        ++count;
                }

                // Move the start pointer along the string:
                if (next_delimiter != std::string::npos)
                        start = next_delimiter + 1;
                if (start >= source.length())
                        next_delimiter = std::string::npos;
        }

        return count;
}


// IsDirectory -- Is the specified file a directory?
//
bool FileUtil_IsDirectory(const std::string &dir_name) {
    struct stat statbuf;
    if (::stat(dir_name.c_str(), &statbuf) != 0)
        return false;

    return S_ISDIR(statbuf.st_mode);
}


/** \brief  Create a directory.
 *  \param  path       The path to create.
 *  \param  recursive  If true, attempt to recursively create parent directoris too.
 *  \param  mode       The access permission for the directory/directories that will be created.
 *  \return True if the directory already existed or has been created else false.
 */
bool FileUtil_MakeDirectory(const std::string &path, const bool recursive = false, const mode_t mode = 0755) {
    const bool absolute(path[0] == '/' ? true : false);
    // In NON-recursive mode we make a single attempt to create the directory:
    if (not recursive) {
        errno = 0;
        if (::mkdir(path.c_str(), mode) == 0)
            return true;
        const bool dir_exists(errno == EEXIST and FileUtil_IsDirectory(path));
        if (dir_exists)
            errno = 0;
        return dir_exists;
    }

    std::vector<std::string> path_components;
    StringUtil_Split(path, '/', &path_components);

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
        if (errno == EEXIST and not FileUtil_IsDirectory(path_so_far))
            return false;
    }

    return true;
}


bool FileUtil_WriteString(const std::string &path, const std::string &data) {
    std::ofstream output(path, std::ios_base::out | std::ios_base::binary | std::ios_base::trunc);
    if (output.fail())
        return false;

    output.write(data.data(), data.size());
    return not output.bad();
}


std::unique_ptr<File> FileUtil_OpenForAppendingOrDie(const std::string &filename) {
    std::unique_ptr<File> file(new File(filename, "a"));
    if (file->fail())
        Error("can't open \"" + filename + "\" for appending!");

    return file;
}


std::unique_ptr<File> FileUtil_OpenOutputFileOrDie(const std::string &filename) {
    std::unique_ptr<File> file(new File(filename, "w"));
    if (file->fail())
        Error("can't open \"" + filename + "\" for writing!");

    return file;
}


void FileUtil_AppendStringToFile(const std::string &path, const std::string &text) {
    std::unique_ptr<File> file(FileUtil_OpenForAppendingOrDie(path));
    if (unlikely(file->write(text.data(), text.size()) != text.size()))
        Error("in FileUtil::AppendStringToFile: failed to append data to \"" + path + "\"!");
}

void FileUtil_AppendFileToFile(const std::string &path_source, const std::string &path_target) {
    std::string string_source = "";
    if (unlikely(!FileUtil_ReadString(path_source, &string_source)))
        Error("in FileUtil::AppendFileToFile: failed to read file: \"" + path_source + "\"!");

    FileUtil_AppendStringToFile(path_target, string_source);
}


size_t FileUtil_ConcatFiles(const std::string &target_path, const std::vector<std::string> &filenames,
                   const mode_t target_mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH)
{
    if (filenames.empty())
        Error("in FileUtil::ConcatFiles: no files to concatenate!");

    FileDescriptor target_fd(::open(target_path.c_str(), O_WRONLY | O_CREAT | O_LARGEFILE | O_TRUNC, target_mode));
    if (target_fd == -1)
        Error("in FileUtil::ConcatFiles: failed to open or create \"" + target_path + "\"!");

    size_t total_size(0);
    for (const auto &filename : filenames) {
        FileDescriptor source_fd(::open(filename.c_str(), O_RDONLY | O_LARGEFILE));
        if (source_fd == -1)
            Error("in FileUtil::ConcatFiles: failed to open \"" + filename + "\" for reading!");

        struct stat statbuf;
        if (::fstat(source_fd, &statbuf) == -1)
            Error("in FileUtil::ConcatFiles: failed to fstat(2) \"" + filename + "\"! ("
                  + std::string(::strerror(errno)) + ")");
        const ssize_t count(::sendfile(target_fd, source_fd, nullptr, statbuf.st_size));
        if (count == -1)
            Error("in FileUtil::ConcatFiles: failed to append \"" + filename + "\" to \"" + target_path
                  + "\"! (" + std::string(::strerror(errno)) + ")");
        total_size += static_cast<size_t>(count);
    }

    return total_size;
}


void MountDeptDriveOrDie(const VuFindSystemType vufind_system_type) {
    const std::string MOUNT_POINT("/mnt/ZE020150/");
    if (not FileUtil_MakeDirectory(MOUNT_POINT))
        Error("failed to create mount point \"" + MOUNT_POINT + "\"!");

    if (FileUtil_IsMountPoint(MOUNT_POINT) or FileUtil_IsDirectory(MOUNT_POINT + "/FID-Entwicklung"))
        Echo("Department drive already mounted");
    else {
        const std::string role_account(vufind_system_type == KRIMDOK ? "qubob15" : "qubob16");
        const std::string password(GetPassword("Enter password for " + role_account));
        const std::string credentials_file("/root/.smbcredentials");
        if (unlikely(not FileUtil_WriteString(credentials_file, "username=" + role_account + "\npassword=" + password
                                              + "\n")))
            Error("failed to write " + credentials_file + "!");
        if (not FileContainsLineStartingWith("/etc/fstab", "//sn00.zdv.uni-tuebingen.de/ZE020150"))
            FileUtil_AppendStringToFile("/etc/fstab",
                                        "//sn00.zdv.uni-tuebingen.de/ZE020150 " + MOUNT_POINT + " cifs "
                                        "credentials=/root/.smbcredentials,workgroup=uni-tuebingen.de,uid=root,"
                                        "gid=root,auto 0 0");
        ExecOrDie("/bin/mount", { MOUNT_POINT });
        Echo("Successfully mounted the department drive.");
    }
}


std::unordered_map<std::string, std::string> which_cache;


std::string ExecUtil_Which(const std::string &executable_candidate) {
    auto which_cache_entry = which_cache.find(executable_candidate);
    if (which_cache_entry != which_cache.end())
        return which_cache[executable_candidate];

    std::string executable;

    const size_t last_slash_pos(executable_candidate.find_last_of('/'));
    if (last_slash_pos != std::string::npos) {
        if (not IsExecutableFile(executable_candidate))
            return "";
        executable = executable_candidate;
    }

    if (executable.empty()) {
        const char * const PATH(::secure_getenv("PATH"));
        if (PATH == nullptr)
            return "";

        const std::string path_str(PATH);
        std::vector<std::string> path_components;
        StringUtil_Split(path_str, ':', &path_components);
        for (const auto &path_component : path_components) {
            const std::string full_path(path_component + "/" + executable_candidate);
            if (IsExecutableFile(full_path)) {
                executable = full_path;
                break;
            }
        }
    }

    if (executable.empty())
        return "";
    else {
        which_cache[executable_candidate] = executable;
        return executable;
    }
}


void InstallSoftwareDependencies(const OSSystemType os_system_type) {
    if (os_system_type == UBUNTU)
        ExecOrDie(INSTALLER_SCRIPTS_DIRECTORY + "/install_ubuntu_packages.sh");
    else
        ExecOrDie(INSTALLER_SCRIPTS_DIRECTORY + "/install_centos_packages.sh");
}


void InstallUBTools(const OSSystemType os_system_type, const bool make_install) {
    InstallSoftwareDependencies(os_system_type);

    // First install iViaCore-mkdep...
    ChangeDirectoryOrDie(UB_TOOLS_DIRECTORY + "/cpp/lib/mkdep");
    ExecOrDie(ExecUtil_Which("make"), { "install" });

    // ...and then install the rest of ub_tools:
    ChangeDirectoryOrDie(UB_TOOLS_DIRECTORY);
    if (make_install)
        ExecOrDie(ExecUtil_Which("make"), { "install" });
    else
        ExecOrDie(ExecUtil_Which("make"));

    Echo("Installed ub_tools.");
}


std::string StringUtil_RightTrim(const std::string &trim_set, std::string * const s) {
    size_t original_length(s->length());
    if (original_length == 0)
        return *s;

    size_t trimmed_length(original_length);
    const char * const set(trim_set.c_str());
    while (trimmed_length > 0 and ::strchr(set, (*s)[trimmed_length-1]) != nullptr)
        --trimmed_length;

    if (trimmed_length < original_length)
        *s = s->substr(0, trimmed_length);

    return *s;
}


std::string StringUtil_LeftTrim(const std::string &trim_set, std::string * const s) {
    size_t original_length(s->length());
    if (original_length == 0)
        return *s;

    size_t no_of_leading_trim_chars(0);
    const char * const set(trim_set.c_str());
    while (no_of_leading_trim_chars < original_length - 1
           and std::strchr(set, (*s)[no_of_leading_trim_chars]) != nullptr)
        ++no_of_leading_trim_chars;

    if (no_of_leading_trim_chars > 0)
        *s = s->substr(no_of_leading_trim_chars);

    return *s;
}


/** \brief   Remove all occurences of a set of characters from either end of a string.
 *  \param   s         The string to trim.
 *  \param   trim_set  The set of characters to remove.
 *  \return  The trimmed string.
 */
std::string StringUtil_Trim(const std::string &trim_set, std::string * const s) {
    StringUtil_RightTrim(trim_set, s);
    return StringUtil_LeftTrim(trim_set, s);
}


const std::string WHITE_SPACE(" \t\n\v\r\f\xA0");


/** \brief   Remove all occurences of whitespace characters from either end of a string.
 *  \param   s         The string to trim.
 *  \return  The trimmed string.
 */
inline std::string StringUtil_TrimWhite(std::string * const s) {
    return StringUtil_Trim(WHITE_SPACE, s);
}


/** \brief   Remove all occurences of whitespace characters from either end of a string.
 *  \param   s         The string to trim.
 *  \return  The trimmed string.
 */
inline std::string StringUtil_TrimWhite(const std::string &s) {
    std::string temp_s(s);
    return StringUtil_TrimWhite(&temp_s);
}


std::string GetStringFromTerminal(const std::string &prompt) {
    std::cout << prompt << " >";
    std::string input;
    std::getline(std::cin, input);
    return StringUtil_TrimWhite(&input);
}


/** \class FileUtil_AutoTempFile
 *  \brief Creates a temp file and removes it when going out of scope.
 */
class FileUtil_AutoTempFile {
    std::string path_;
public:
    explicit FileUtil_AutoTempFile(const std::string &path_prefix = "/tmp/AT");
    ~FileUtil_AutoTempFile() { if (not path_.empty()) ::unlink(path_.c_str()); }

    const std::string &getFilePath() const { return path_; }
};


FileUtil_AutoTempFile::FileUtil_AutoTempFile(const std::string &path_prefix) {
    std::string path_template(path_prefix + "XXXXXX");
    const int fd(::mkstemp(const_cast<char *>(path_template.c_str())));
    if (fd == -1)
        throw std::runtime_error("in AutoTempFile::AutoTempFile: mkstemp(3) for path prefix \"" + path_prefix
                                 + "\" failed! (" + std::string(::strerror(errno)) + ")");
    ::close(fd);
    path_ = path_template;
}


/** \brief  Join a "list" of words to form a single string.
 *  \param  source     The container of strings that are to be joined.
 *  \param  separator  The text to insert between the "source" elements.
 *  \param  dest       A string in which to return the combined result.
 *  \return The number of strings that were joined.
 *
 *  Joins a list of strings from "source" into a single long string, "dest".  The source can be pretty much any container
 *  of strings, and will typically be std::list<std::string> or std::vector<std::string>.  The string "separator" will be
 *  inserted between the elements.
 */
template<typename StringContainer> unsigned StringUtil_Join(const StringContainer &source,
                                                            const std::string &separator, std::string * const dest)
{
    dest->clear();
    unsigned word_count(0);
    const unsigned SOURCE_SIZE(static_cast<unsigned>(source.size()));
    for (typename StringContainer::const_iterator i(source.begin()); i !=  source.end(); ++i) {
        *dest += *i;
        ++word_count;
        if (word_count < SOURCE_SIZE)
            *dest += separator;
    }

    return SOURCE_SIZE;
}


/** \brief  Join a list of words to form a single string and return that string
 *  \param  source         A container of strings.
 *  \param  separator      The text to insert between the list elements.
 *
 *  Joins a list of strings from "source" into a single long string and returns it.
 */
template<typename StringContainer> inline std::string StringUtil_Join(const StringContainer &source,
                                                                      const std::string &separator)
{
    std::string dest;
    StringUtil_Join(source, separator, &dest);

    return dest;
}


namespace {


class TemplateScanner {
    std::string last_variable_name_, last_string_constant_, last_error_message_;
    unsigned line_no_;
    std::istream &input_;
    std::ostream &output_;
    std::string::const_iterator ch_;
    const std::string::const_iterator end_;
    bool in_syntax_;
public:
    enum TokenType { END_OF_INPUT, IF, ELSE, ENDIF, DEFINED, LOOP, ENDLOOP, VARIABLE_NAME, OPEN_PAREN, CLOSE_PAREN,
                     COMMA, EQUALS, NOT_EQUALS, STRING_CONSTANT, AND, OR, END_OF_SYNTAX, ERROR };
public:
    TemplateScanner(std::istream &input, std::ostream &output)
        : line_no_(1), input_(input), output_(output), in_syntax_(false) { }

    /** If "emit_output" non-syntax elements of the input will be copied to the output. */
    TokenType getToken(const bool emit_output);

    /** Only call this immediately after getToken() has returned VARIABLE_NAME. */
    const std::string &getLastVariableName() const { return last_variable_name_; }

    /** Only call this immediately after getToken() has returned STRING_CONSTANT. */
    const std::string &getLastStringConstant() const { return last_string_constant_; }

    /** Only call this immediately after getToken() has returned ERROR. */
    const std::string &getLastErrorMessage() const { return last_error_message_; }

    unsigned getLineNo() const { return line_no_; }

    /** \brief Repositions the input stream and sets the appropriate line number for that position. */
    void seek(const std::istream::streampos stream_position, const unsigned line_no);

    /** Skips over blank characters in the input stream w/o emitting anything to the output stream. */
    void skipWhitespace();

    std::istream::streampos getInputStreamPos() const { return input_.tellg(); }

    /** \return A string representation of "token". */
    static std::string TokenTypeToString(const TokenType token);
private:
    std::string extractKeywordCandidate();
    std::string extractVariableName();
    void extractStringConstant();

    /** \return Either a keyword token type or ERROR if we failed to recognise the keyword. */
    static TokenType MapStringToKeywordToken(const std::string &keyword_candidate);
};


TemplateScanner::TokenType TemplateScanner::getToken(const bool emit_output) {
    if (unlikely(input_.eof()))
        return END_OF_INPUT;
    if (unlikely(not last_error_message_.empty()))
        Error("in TemplateScanner::getToken: attempting to continue scanning after an error occurred!");

    for (;;) {
        int ch(input_.get());
        if (unlikely(ch == EOF)) {
            if (unlikely(in_syntax_))
                throw std::runtime_error("in MiscUtil::TemplateScanner::getToken: unexpected EOF on line "
                                         + std::to_string(line_no_) + "!");
            return END_OF_INPUT;
        }
        if (ch == '\n')
            ++line_no_;

        if (in_syntax_) {
            if (ch == '(')
                return OPEN_PAREN;
            if (ch == ')')
                return CLOSE_PAREN;
            if (ch == ',')
                return COMMA;
            if (ch == '}') {
                in_syntax_ = false;
                return END_OF_SYNTAX;
            }
            if (ch == '=') {
                ch = input_.get();
                if (likely(ch == '='))
                    return EQUALS;
                else
                    throw std::runtime_error("in MiscUtil::TemplateScanner::getToken: expected '=' after '=' on line "
                                             + std::to_string(line_no_) + "!");
            }
            if (ch == '!') {
                ch = input_.get();
                if (likely(ch == '='))
                    return NOT_EQUALS;
                else
                    throw std::runtime_error("in MiscUtil::TemplateScanner::getToken: expected '=' after '!' on line "
                                             + std::to_string(line_no_) + "!");
            }

            if (ch >= 'A' and ch <= 'Z') {
                input_.unget();
                const std::string keyword_candidate(extractKeywordCandidate());
                const TokenType token(MapStringToKeywordToken(keyword_candidate));
                if (unlikely(token == ERROR))
                    last_error_message_ = "unknown keyword " + keyword_candidate + " on line "
                                          + std::to_string(line_no_);
                return token;
            } else if (ch >= 'a' and ch <= 'z') {
                input_.unget();
                last_variable_name_ = extractVariableName();
                return VARIABLE_NAME;
            } else if (ch == '"') {
                extractStringConstant();
                return STRING_CONSTANT;
            }
        } else if (ch == '{') {
            if (input_.peek() == '{') {
                input_.get();
                output_.put('{');
            } else
                in_syntax_ = true;
        } else if (emit_output)
            output_.put(static_cast<char>(ch));
    }
}


std::string TemplateScanner::extractKeywordCandidate() {
    std::string keyword_candidate;
    int ch;
    while (ch = input_.get(), ch >= 'A' and ch <= 'Z')
        keyword_candidate += static_cast<char>(ch);
    input_.unget();

    return keyword_candidate;
}


void TemplateScanner::seek(const std::istream::streampos stream_position, const unsigned line_no) {
    input_.seekg(stream_position);
    if (unlikely(not input_))
        Error("in TemplateScanner::seek: this should never happen!");
    line_no_ = line_no;
}


void TemplateScanner::skipWhitespace() {
    for (int ch(input_.get()); ch != EOF and isspace(ch); ch = input_.get())
        /* Intentionally empty! */;
    input_.unget();
}


std::string TemplateScanner::TokenTypeToString(const TemplateScanner::TokenType token) {
    switch (token) {
    case END_OF_INPUT:
        return "END_OF_INPUT";
    case IF:
        return "IF";
    case ELSE:
        return "ELSE";
    case ENDIF:
        return "ENDIF";
    case DEFINED:
        return "DEFINED";
    case LOOP:
        return "LOOP";
    case ENDLOOP:
        return "ENDLOOP";
    case VARIABLE_NAME:
        return "VARIABLE_NAME";
    case OPEN_PAREN:
        return "OPEN_PAREN";
    case CLOSE_PAREN:
        return "CLOSE_PAREN";
    case COMMA:
        return "COMMA";
    case EQUALS:
        return "EQUALS";
    case NOT_EQUALS:
        return "NOT_EQUALS";
    case STRING_CONSTANT:
        return "STRING_CONSTANT";
    case AND:
        return "AND";
    case OR:
        return "OR";
    case END_OF_SYNTAX:
        return "END_OF_SYNTAX";
    case ERROR:
        return "ERROR";
    }

    Error("in TemplateScanner::TokenTypeToString: we should *never* get here!");
}


std::string TemplateScanner::extractVariableName() {
    std::string variable_name;
    int ch;
    while (ch = input_.get(), (ch >= 'a' and ch <= 'z') or ch == '_' or (ch >= '0' and ch <= '9'))
        variable_name += static_cast<char>(ch);
    input_.unget();

    return variable_name;
}


void TemplateScanner::extractStringConstant() {
    last_string_constant_.clear();

    int ch;
    while ((ch = input_.get()) != '"') {
        switch (ch) {
        case EOF:
            throw std::runtime_error("in TemplateScanner::extractStringConstant: unexpected EOF while parsing a "
                                     "string constant on line " + std::to_string(line_no_) + "!");
        case '\\': {
                ch = input_.get();
                if (unlikely(ch == EOF))
                    throw std::runtime_error("in TemplateScanner::extractStringConstant: unexpected EOF while "
                                             "parsing a string constant on line " + std::to_string(line_no_) + "!");
                if (ch == '\\')
                    last_string_constant_ += '\\';
                else if (ch == 'n')
                    last_string_constant_ += '\n';
                else if (ch == '"')
                    last_string_constant_ += '"';
                else
                    throw std::runtime_error("in TemplateScanner::extractStringConstant: illegal character after "
                                             "backslash in a string constant on line " + std::to_string(line_no_)
                                             + "!");
                break;
        } default:
            last_string_constant_ += static_cast<char>(ch);
        }
    }
}


TemplateScanner::TokenType TemplateScanner::MapStringToKeywordToken(const std::string &keyword_candidate) {
    static const std::map<std::string, TokenType> keywords_to_tokens_map{
        { "IF",      IF      },
        { "ELSE",    ELSE    },
        { "ENDIF",   ENDIF   },
        { "DEFINED", DEFINED },
        { "LOOP",    LOOP    },
        { "ENDLOOP", ENDLOOP },
        { "AND",     AND     },
        { "OR",      OR      },
    };

    const auto key_and_value(keywords_to_tokens_map.find(keyword_candidate));
    return key_and_value == keywords_to_tokens_map.cend() ? ERROR : key_and_value->second;
}


class Scope {
public:
    enum Type { TOP_LEVEL, IF, LOOP };
private:
    Type type_;
    unsigned start_line_number_, iteration_count_, loop_count_;
    std::istream::streampos start_stream_pos_;
    std::set<std::string> loop_vars_;
public:
    Type getType() const { return type_; }

    /** \return where an IF or a LOOP started. */
    unsigned getStartLineNumber() const { return start_line_number_; }

    bool isLoopVariable(const std::string &variable_name) const;
    unsigned getCurrentIterationCount() const;
    unsigned getLoopCount() const;
    void incIterationCount();
    std::istream::streampos getStartStreamPos() const;

    static Scope MakeTopLevelScope() { return Scope(TOP_LEVEL); }
    static Scope MakeIfScope(const unsigned start_line_number) { return Scope(IF, start_line_number); }
    static Scope MakeLoopScope(const unsigned start_line_number, const std::istream::streampos start_stream_pos,
                               const std::set<std::string> &loop_vars, const unsigned loop_count)
    { return Scope(LOOP, start_line_number, start_stream_pos, loop_vars, loop_count); }
private:
    explicit Scope(const Type type): type_(type) { }
    explicit Scope(const Type type, const unsigned start_line_number)
        : type_(type), start_line_number_(start_line_number) { }
    explicit Scope(const Type type, const unsigned start_line_number, const std::istream::streampos start_stream_pos,
                   const std::set<std::string> &loop_vars, const unsigned loop_count)
        : type_(type), start_line_number_(start_line_number), iteration_count_(0), loop_count_(loop_count),
          start_stream_pos_(start_stream_pos), loop_vars_(loop_vars) { }
    static std::string TypeToString(const Type type);
};


bool Scope::isLoopVariable(const std::string &variable_name) const {
    if (unlikely(type_ != LOOP))
        Error("in MiscUtil::Scope::isLoopVariable: this should never happen! (type is "
              + TypeToString(type_) + ", variable is \"" + variable_name + "\")");

    return loop_vars_.find(variable_name) != loop_vars_.cend();
}


unsigned Scope::getCurrentIterationCount() const {
    if (unlikely(type_ != LOOP))
        Error("in MiscUtil::Scope::getCurrentIterationCount: this should never happen! (type is "
              + TypeToString(type_) + ")");

    return iteration_count_;
}


unsigned Scope::getLoopCount() const {
    if (unlikely(type_ != LOOP))
        Error("in MiscUtil::Scope::getLoopCount: this should never happen!");

    return loop_count_;
}


void Scope::incIterationCount() {
    if (unlikely(type_ != LOOP))
        Error("in MiscUtil::Scope::incIterationCount: this should never happen!");

    ++iteration_count_;
}


std::istream::streampos Scope::getStartStreamPos() const {
    if (unlikely(type_ != LOOP))
        Error("in MiscUtil::Scope::getStartStreamPos: this should never happen!");

        return start_stream_pos_;
}


std::string Scope::TypeToString(const Type type) {
    switch (type) {
    case TOP_LEVEL:
        return "TOP_LEVEL";
    case IF:
        return "IF";
    case LOOP:
        return "LOOP";
    }

    Error("in Scope::TypeToString: we should *never* get here!");
}


// Returns true, if "variable_name" exists and can be accessed as a scalar based on the current scope.
bool GetScalarValue(const std::string &variable_name,
                    const std::map<std::string, std::vector<std::string>> &names_to_values_map,
                    const std::vector<Scope> &active_scopes, std::string * const value)
{
    const auto &name_and_values(names_to_values_map.find(variable_name));
    if (name_and_values == names_to_values_map.cend())
        return false;

    if (name_and_values->second.size() == 1) { // We can always access this variable in a scalar context!
        *value = name_and_values->second[0];
        return true;
    }

    // Now deal w/ multivalued variables:
    for (auto scope(active_scopes.crbegin()); scope != active_scopes.crend(); ++scope) {
        if (scope->isLoopVariable(variable_name)) {
            *value = name_and_values->second[scope->getCurrentIterationCount()];
            return true;
        }
    }

    return false;
}


bool ParseIfCondition(TemplateScanner * const scanner,
                      const std::map<std::string, std::vector<std::string>> &names_to_values_map,
                      const std::vector<Scope> &active_scopes, const bool parse_only = false)
{
    scanner->skipWhitespace();
    TemplateScanner::TokenType token(scanner->getToken(/* emit_output = */false));
    if (unlikely(token != TemplateScanner::DEFINED and token != TemplateScanner::VARIABLE_NAME))
        throw std::runtime_error("in MiscUtil::ParseIfCondition: error on line "
                                 + std::to_string(scanner->getLineNo())
                                 + " DEFINED or variable name expected but found "
                                 + TemplateScanner::TokenTypeToString(token) + " instead!");

    bool expression_value;
    if (token == TemplateScanner::DEFINED) {
        token = scanner->getToken(/* emit_output = */false);
        if (unlikely(token != TemplateScanner::OPEN_PAREN))
            throw std::runtime_error("in MiscUtil::ParseIfCondition: error on line "
                                     + std::to_string(scanner->getLineNo()) + " '(' expected but found "
                                     + TemplateScanner::TokenTypeToString(token) + " instead!");

        token = scanner->getToken(/* emit_output = */false);
        if (unlikely(token != TemplateScanner::VARIABLE_NAME))
            throw std::runtime_error("in MiscUtil::ParseIfCondition: error on line "
                                     + std::to_string(scanner->getLineNo()) + " variable name expected but found "
                                     + TemplateScanner::TokenTypeToString(token) + " instead!");
        expression_value = names_to_values_map.find(scanner->getLastVariableName())
                           != names_to_values_map.cend();

        token = scanner->getToken(/* emit_output = */false);
        if (unlikely(token != TemplateScanner::CLOSE_PAREN))
            throw std::runtime_error("in MiscUtil::ParseIfCondition: error on line "
                                     + std::to_string(scanner->getLineNo()) + " '(' expected but found "
                                     + TemplateScanner::TokenTypeToString(token) + " instead!");
    } else { // Comparison.
        std::string variable_name(scanner->getLastVariableName());
        std::string lhs;
        if (not parse_only and unlikely(not GetScalarValue(variable_name, names_to_values_map, active_scopes, &lhs)))
            throw std::runtime_error("in MiscUtil::ParseIfCondition: error on line "
                                     + std::to_string(scanner->getLineNo())
                                     + " unknown or non-scalar variable name \"" + variable_name + "\"!");
        scanner->skipWhitespace();
        const TemplateScanner::TokenType operator_token(scanner->getToken(/* emit_output = */false));
        if (unlikely(operator_token != TemplateScanner::EQUALS and operator_token != TemplateScanner::NOT_EQUALS))
            throw std::runtime_error("in MiscUtil::ParseIfCondition: error on line "
                                     + std::to_string(scanner->getLineNo())
                                     + " \"==\" or \"!=\" expected after variable name!");

        scanner->skipWhitespace();
        token = scanner->getToken(/* emit_output = */false);
        if (unlikely(token != TemplateScanner::VARIABLE_NAME and token != TemplateScanner::STRING_CONSTANT))
            throw std::runtime_error("in MiscUtil::ParseIfCondition: error on line "
                                     + std::to_string(scanner->getLineNo())
                                     + " variable name or string constant expected after comparison operator! ("
                                     "Found " + TemplateScanner::TokenTypeToString(token) + " instead.)");
        std::string rhs;
        if (token == TemplateScanner::STRING_CONSTANT)
            rhs = scanner->getLastStringConstant();
        else {
            variable_name = scanner->getLastVariableName();
            if (unlikely(not GetScalarValue(variable_name, names_to_values_map, active_scopes, &rhs)))
                throw std::runtime_error("in MiscUtil::ParseIfCondition: error on line "
                                         + std::to_string(scanner->getLineNo())
                                         + " unknown or non-scalar variable name \"" + variable_name + "\"!");
        }

        expression_value = (rhs == lhs);
        if (operator_token == TemplateScanner::NOT_EQUALS)
            expression_value = !expression_value;
    }

    return expression_value;
}


// \return The value of the conditional expression.
bool ParseIf(TemplateScanner * const scanner,
             const std::map<std::string, std::vector<std::string>> &names_to_values_map,
             const std::vector<Scope> &active_scopes)
{
    const bool condition1(ParseIfCondition(scanner, names_to_values_map, active_scopes));

    scanner->skipWhitespace();
    const TemplateScanner::TokenType token(scanner->getToken(/* emit_output = */false));
    if (unlikely(token == TemplateScanner::END_OF_SYNTAX))
        return condition1;

    if (unlikely(token != TemplateScanner::AND and token != TemplateScanner::OR))
        throw std::runtime_error("in MiscUtil::ParseIf: error on line " + std::to_string(scanner->getLineNo())
                                 + " '}' expected but found " + TemplateScanner::TokenTypeToString(token)
                                 + " instead!");

    // Syntax check only:
    const auto start_of_condition(scanner->getInputStreamPos());
    const unsigned start_of_condition_lineno(scanner->getLineNo());
    ParseIfCondition(scanner, names_to_values_map, active_scopes, /*parse_only =*/true);

    if (token == TemplateScanner::AND) {
        if (not condition1)
            return false;
    } else { // token == TemplateScanner::OR
        if (condition1)
            return true;
    }
    scanner->seek(start_of_condition, start_of_condition_lineno);
    return ParseIfCondition(scanner, names_to_values_map, active_scopes);
}


/** \return False if "variable_name_candidate" is an unknown variable name, else true. */
bool GetVariableCardinality(const std::string variable_name_candidate,
                            const std::map<std::string, std::vector<std::string>> &names_to_values_map,
                            unsigned * const cardinality)
{
    const auto name_and_values(names_to_values_map.find(variable_name_candidate));
    if (unlikely(name_and_values == names_to_values_map.cend()))
        return false;
    *cardinality = name_and_values->second.size();
    return true;
}


void ParseLoop(TemplateScanner * const scanner, std::set<std::string> * const loop_vars, unsigned * const loop_count,
               const std::map<std::string, std::vector<std::string>> &names_to_values_map)
{
    scanner->skipWhitespace();

    TemplateScanner::TokenType token(scanner->getToken(/* emit_output = */false));
    if (unlikely(token != TemplateScanner::VARIABLE_NAME))
        throw std::runtime_error("error on line " + std::to_string(scanner->getLineNo())
                                 + ": variable name expected, found " + TemplateScanner::TokenTypeToString(token)
                                 + " instead!");
    std::string variable_name_candidate(scanner->getLastVariableName());
    if (unlikely(not GetVariableCardinality(variable_name_candidate, names_to_values_map, loop_count)))
        throw std::runtime_error("error on line " + std::to_string(scanner->getLineNo())
                                 + ": undefined loop variable \"" + variable_name_candidate + "\"!");
    loop_vars->insert(variable_name_candidate);

    while ((token = scanner->getToken(/* emit_output = */false)) == TemplateScanner::COMMA) {
        token = scanner->getToken(/* emit_output = */false);
        if (unlikely(token != TemplateScanner::VARIABLE_NAME))
            throw std::runtime_error("error on line "
                                     + std::to_string(scanner->getLineNo())
                                     + ": variable name expected after comma, found "
                                     + TemplateScanner::TokenTypeToString(token) + " instead!");
        variable_name_candidate = scanner->getLastVariableName();
        unsigned cardinality;
        if (unlikely(not GetVariableCardinality(variable_name_candidate, names_to_values_map, &cardinality)))
            throw std::runtime_error("error on line " + std::to_string(scanner->getLineNo())
                                     + ": undefined loop variable \"" + variable_name_candidate + "\"!"
                                     + " (Possible loop variables are: " + StringUtil_Join(*loop_vars, ", ") + ")");
        if (unlikely(cardinality != *loop_count))
            throw std::runtime_error("error on line " + std::to_string(scanner->getLineNo())
                                     + " loop variables do not all have the same cardinality!");
        loop_vars->insert(variable_name_candidate);
    }

    if (unlikely(token != TemplateScanner::END_OF_SYNTAX))
        throw std::runtime_error("error on line " + std::to_string(scanner->getLineNo())
                                 + " expected '}' at end of LOOP construct but found "
                                 + TemplateScanner::TokenTypeToString(token) + " instead!");
}


void ProcessEndOfSyntax(const std::string &name_of_syntactic_construct, TemplateScanner * const scanner) {
    const TemplateScanner::TokenType token(scanner->getToken(/* emit_output = */false));
    if (unlikely(token != TemplateScanner::END_OF_SYNTAX))
        throw std::runtime_error("in MiscUtil::ProcessEndOfSyntax: error on line "
                                 + std::to_string(scanner->getLineNo()) + " expected '}' after "
                                 + name_of_syntactic_construct + " but found "
                                 + TemplateScanner::TokenTypeToString(token) + "!");
}


void SkipToToken(TemplateScanner * const scanner, TemplateScanner::TokenType target_token) {
    TemplateScanner::TokenType token;
    while ((token = scanner->getToken(false)) != target_token and token != TemplateScanner::END_OF_INPUT)
    { /* do nothing */ }
    if (token == TemplateScanner::END_OF_INPUT)
        throw std::runtime_error("in MiscUtil::SkipToToken: error on line "
                                 + std::to_string(scanner->getLineNo())
                                 + " expected '" + TemplateScanner::TokenTypeToString(token)
                                 + "' but none was found.");
}


} // unnamed namespace


void MiscUtil_ExpandTemplate(std::istream &input, std::ostream &output,
                             const std::map<std::string, std::vector<std::string>> &names_to_values_map)
{
    TemplateScanner scanner(input, output);
    std::vector<Scope> scopes;
    scopes.push_back(Scope::MakeTopLevelScope());

    std::stack<bool> skipping;
    TemplateScanner::TokenType token;
    while ((token = scanner.getToken(skipping.empty() or not skipping.top())) != TemplateScanner::END_OF_INPUT) {
        if (unlikely(token == TemplateScanner::ERROR))
            throw std::runtime_error("in MiscUtil::ExpandTemplate: error on line "
                                     + std::to_string(scanner.getLineNo()) + ": " + scanner.getLastErrorMessage());
        if (token == TemplateScanner::IF) {
            const unsigned start_line_no(scanner.getLineNo());
            skipping.push(not ParseIf(&scanner, names_to_values_map, scopes));
            scopes.push_back(Scope::MakeIfScope(start_line_no));
        } else if (token == TemplateScanner::ELSE) {
            if (unlikely(scopes.back().getType() != Scope::IF))
                throw std::runtime_error("in MiscUtil::ExpandTemplate: error on line "
                                         + std::to_string(scanner.getLineNo())
                                         + ": ELSE found w/o corresponding earlier IF!");
            skipping.top() = not skipping.top();
            ProcessEndOfSyntax("ELSE", &scanner);
        } else if (token == TemplateScanner::ENDIF) {
            if (unlikely(scopes.back().getType() != Scope::IF))
                throw std::runtime_error("in MiscUtil::ExpandTemplate: error on line "
                                         + std::to_string(scanner.getLineNo())
                                         + ": ENDIF found w/o corresponding earlier IF!");
            scopes.pop_back();
            skipping.pop();
            ProcessEndOfSyntax("ENDIF", &scanner);
        } else if (token == TemplateScanner::LOOP) {
            std::set<std::string> loop_vars;
            unsigned loop_count;
            try {
                ParseLoop(&scanner, &loop_vars, &loop_count, names_to_values_map);
            } catch (const std::exception &x) {
                throw std::runtime_error("in MiscUtil::ExpandTemplate: " + std::string(x.what()));
            }
            const unsigned start_line_no(scanner.getLineNo());
            if (likely(loop_count > 0))
                scopes.push_back(Scope::MakeLoopScope(start_line_no, scanner.getInputStreamPos(), loop_vars,
                                                      loop_count));
            else
                SkipToToken(&scanner, TemplateScanner::ENDLOOP);

        } else if (token == TemplateScanner::ENDLOOP) {
            Scope &current_scope(scopes.back());
            if (unlikely(current_scope.getType() != Scope::LOOP))
                throw std::runtime_error("in MiscUtil::ExpandTemplate: error on line "
                                         + std::to_string(scanner.getLineNo())
                                         + ": ENDLOOP found w/o corresponding earlier LOOP!");
            ProcessEndOfSyntax("ENDLOOP", &scanner);

            current_scope.incIterationCount();
            if (current_scope.getCurrentIterationCount() >= current_scope.getLoopCount())
                scopes.pop_back();
            else
                scanner.seek(current_scope.getStartStreamPos(), current_scope.getStartLineNumber());
        } else if (token == TemplateScanner::VARIABLE_NAME) {
            const std::string &last_variable_name(scanner.getLastVariableName());
            if (skipping.empty() or not skipping.top()) {
                std::string variable_value;
                if (not GetScalarValue(last_variable_name, names_to_values_map, scopes, &variable_value))
                    throw std::runtime_error("in MiscUtil::ExpandTemplate: error on line "
                                             + std::to_string(scanner.getLineNo()) + ": found unexpected variable \""
                                             + last_variable_name + "\"!");
                output << variable_value;
            }
            ProcessEndOfSyntax("variable expansion", &scanner);
        }
    }

    const Scope &scope(scopes.back());
    switch (scope.getType()) {
    case Scope::TOP_LEVEL:
        return;
    case Scope::IF:
        throw std::runtime_error("in MiscUtil::ExpandTemplate: error on line "
                                 + std::to_string(scanner.getLineNo()) + ": IF started on line "
                                 + std::to_string(scope.getStartLineNumber()) + " was never closed!");
    case Scope::LOOP:
        throw std::runtime_error("in MiscUtil::ExpandTemplate: error on line "
                                 + std::to_string(scanner.getLineNo()) + ": LOOP started on line "
                                 + std::to_string(scope.getStartLineNumber()) + " was never closed!");
    }
}


std::string MiscUtil_ExpandTemplate(const std::string &template_string,
                                    const std::map<std::string, std::vector<std::string>> &names_to_values_map)
{
    std::istringstream input(template_string);
    std::ostringstream expanded_template;
    MiscUtil_ExpandTemplate(input, expanded_template, names_to_values_map);
    return expanded_template.str();
}


void InstallCronjobs(const VuFindSystemType vufind_system_type) {
    std::map<std::string, std::vector<std::string>> names_to_values_map;
    if (vufind_system_type == IXTHEO) {
        names_to_values_map.insert(
            std::make_pair<std::string, std::vector<std::string>>(
                "ixtheo_host", { GetStringFromTerminal("IxTheo Hostname") }));
        names_to_values_map.insert(
            std::make_pair<std::string, std::vector<std::string>>(
                "relbib_host", { GetStringFromTerminal("RelBib Hostname") }));
    }

    FileUtil_AutoTempFile crontab_temp_file_old;
    // crontab -l returns error code if crontab is empty, so dont use ExecOrDie!!!
    ExecUtil_Exec(ExecUtil_Which("crontab"), { "-l" }, "", crontab_temp_file_old.getFilePath());
    FileUtil_AutoTempFile crontab_temp_file_custom;
    const std::string crontab_block_start = "# START VUFIND AUTOGENERATED";
    const std::string crontab_block_end = "# END VUFIND AUTOGENERATED";
    ExecOrDie(ExecUtil_Which("sed"),
              { "-e", "/" + crontab_block_start + "/,/" + crontab_block_end + "/d",
                crontab_temp_file_old.getFilePath() }, "", crontab_temp_file_custom.getFilePath());
    const std::string cronjobs_custom = ReadStringOrDie(crontab_temp_file_custom.getFilePath());

    std::string cronjobs_generated(crontab_block_start + "\n");
    if (vufind_system_type == KRIMDOK)
        cronjobs_generated += ReadStringOrDie(INSTALLER_DATA_DIRECTORY + "/krimdok.cronjobs");
    else
        cronjobs_generated += MiscUtil_ExpandTemplate(ReadStringOrDie(INSTALLER_DATA_DIRECTORY + "/ixtheo.cronjobs"), names_to_values_map);
    cronjobs_generated += crontab_block_end + "\n";

    FileUtil_AutoTempFile crontab_temp_file_new;
    FileUtil_AppendStringToFile(crontab_temp_file_new.getFilePath(), cronjobs_generated);
    FileUtil_AppendStringToFile(crontab_temp_file_new.getFilePath(), cronjobs_custom);

    ExecOrDie(ExecUtil_Which("crontab"), { crontab_temp_file_new.getFilePath() });
    Echo("Installed cronjobs.");
}


// Note: this will also create a group with the same name
void CreateUserIfNotExists(const std::string &username) {
    const int user_exists(ExecUtil_Exec(ExecUtil_Which("id"), { "-u", username }));
    if (user_exists == 1) {
        Echo("Creating user " + username + "...");
        ExecOrDie(ExecUtil_Which("adduser"), { "--system", "--no-create-home", username });
    } else if (user_exists > 1) {
        Error("Failed to check if user exists: " + username);
    }
}


void GenerateXml(const std::string &filename_source, const std::string &filename_target) {
    std::string dirname_source, basename_source;
    FileUtil_DirnameAndBasename(filename_source, &dirname_source, &basename_source);

    Echo("Generating " + filename_target + " from " + basename_source);
    ExecUtil_Exec(ExecUtil_Which("xmllint"), { "--xinclude", "--format", filename_source }, "", filename_target);
}


void GitAssumeUnchanged(const std::string &filename) {
    std::string dirname, basename;
    FileUtil_DirnameAndBasename(filename, &dirname, &basename);
    TemporaryChDir tmp(dirname);
    ExecOrDie(ExecUtil_Which("git"), { "update-index", "--assume-unchanged", filename });
}

void GitCheckout(const std::string &filename) {
    std::string dirname, basename;
    FileUtil_DirnameAndBasename(filename, &dirname, &basename);
    TemporaryChDir tmp(dirname);
    ExecOrDie(ExecUtil_Which("git"), { "checkout", filename });
}

void UseCustomFileIfExists(std::string filename_custom, std::string filename_default) {
    if (FileUtil_Exists(filename_custom)) {
        GitAssumeUnchanged(filename_default);
        FileUtil_CreateSymlink(filename_custom, filename_default);
    } else {
        GitCheckout(filename_default);
    }
}


void DownloadVuFind() {
    if (FileUtil_IsDirectory(VUFIND_DIRECTORY)) {
        Echo("VuFind directory already exists, skipping download");
    } else {
        Echo("Downloading TuFind git repository");
        const std::string git_url("https://github.com/ubtue/tufind.git");
        ExecOrDie(ExecUtil_Which("git"), { "clone", git_url, VUFIND_DIRECTORY });

        TemporaryChDir tmp(VUFIND_DIRECTORY);
        ExecOrDie(ExecUtil_Which("composer"), { "install" });
        ExecOrDie(ExecUtil_Which("php"), { "util/cssBuilder.php" });
    }
}


/**
 * Configure Apache User
 * - Create user "vufind" as system user if not exists
 * - Grant permissions on relevant directories
 */
void ConfigureApacheUser(const std::string &vufind_system_type_string, const OSSystemType os_system_type) {
    const std::string username("vufind");
    CreateUserIfNotExists(username);

    // systemd will start apache as root
    // but apache will start children as configured in /etc
    if (os_system_type == UBUNTU) {
        const std::string config_filename("/etc/apache2/envvars");
        ExecOrDie(ExecUtil_Which("sed"),
            { "-i", "s/export APACHE_RUN_USER=www-data/export APACHE_RUN_USER=" + username + "/",
              config_filename });

        ExecOrDie(ExecUtil_Which("sed"),
            { "-i", "s/export APACHE_RUN_GROUP=www-data/export APACHE_RUN_GROUP=" + username + "/",
              config_filename });
    } else if (os_system_type == CENTOS) {
        const std::string config_filename("/etc/httpd/conf/httpd.conf");
        ExecOrDie(ExecUtil_Which("sed"),
            { "-i", "s/User apache/User " + username + "/", config_filename });

        ExecOrDie(ExecUtil_Which("sed"),
            { "-i", "s/Group apache/Group " + username + "/", config_filename });
    }

    ExecOrDie(ExecUtil_Which("find"), {"/usr/local/vufind/local", "-name", "cache", "-exec", "chown", "-R", username + ":" + username, "{}", "+"});
    ExecOrDie(ExecUtil_Which("chown"), { "-R", username + ":" + username, "/var/log/" + vufind_system_type_string});
}


/**
 * Configure Solr User
 * - Create user "solr" as system user if not exists
 * - Grant permissions on relevant directories
 * - register solr service in systemctl
 */
void ConfigureSolrUserAndService(const bool install_systemctl) {
    // note: if you wanna change username, dont do it only here, also check vufind.service!
    const std::string username("solr");
    const std::string servicename("vufind");

    CreateUserIfNotExists(username);

    Echo("Setting directory permissions for solr user...");
    ExecOrDie(ExecUtil_Which("chown"), { "-R", username + ":" + username, VUFIND_DIRECTORY + "/solr" });
    ExecOrDie(ExecUtil_Which("chown"), { "-R", username + ":" + username, VUFIND_DIRECTORY + "/import" });

    // systemctl: we do enable as well as daemon-reload and restart
    // to achieve an indepotent installation
    if (install_systemctl) {
        Echo("Activating solr service...");
        const std::string SYSTEMD_SERVICE_DIRECTORY("/usr/local/lib/systemd/system/");
        ExecOrDie(ExecUtil_Which("mkdir"), { "-p", SYSTEMD_SERVICE_DIRECTORY });
        ExecOrDie(ExecUtil_Which("cp"), { INSTALLER_DATA_DIRECTORY + "/" + servicename + ".service", SYSTEMD_SERVICE_DIRECTORY + "/" + servicename + ".service" });
        ExecOrDie(ExecUtil_Which("systemctl"), { "enable", servicename });
        ExecOrDie(ExecUtil_Which("systemctl"), { "daemon-reload" });
        ExecOrDie(ExecUtil_Which("systemctl"), { "restart", servicename });
    }
}


void SetEnvironmentVariables(const std::string &vufind_system_type_string) {
    std::string variables;
    variables += "export VUFIND_HOME=" + VUFIND_DIRECTORY + "\n";
    variables += "export VUFIND_LOCAL_DIR=" + VUFIND_DIRECTORY + "/local/tufind/instances/" + vufind_system_type_string + "\n";
    FileUtil_WriteString("/etc/profile.d/vufind.sh", variables);
}


/**
 * Configure VuFind system
 * - Solr Configuration
 * - Schema Fields & Types
 * - solrmarc settings (including VUFIND_LOCAL_DIR)
 * - alphabetical browse
 * - cronjobs
 * - create directories in /var/log/<vufind_system_type> and /usr/local/var/lib/tuelib
 *
 * Writes a file into vufind directory to save configured system type
 */
void ConfigureVuFind(const VuFindSystemType vufind_system_type, const OSSystemType os_system_type, const bool install_cronjobs, const bool install_systemctl) {
    const std::string vufind_system_type_string = VuFindSystemTypeToString(vufind_system_type);
    Echo("Starting configuration for " + vufind_system_type_string);
    const std::string dirname_solr_conf = VUFIND_DIRECTORY + "/solr/vufind/biblio/conf";

    Echo("SOLR Configuration (solrconfig.xml)");
    const std::string filename_solr_conf_local = dirname_solr_conf + "/solrconfig.xml";
    GitAssumeUnchanged(filename_solr_conf_local);
    FileUtil_CreateSymlink(dirname_solr_conf + "/solrconfig_" + vufind_system_type_string + ".xml", filename_solr_conf_local);

    Echo("SOLR Schema (schema_local_*.xml)");
    Echo("  (note: if you get XInclude errors, these may be ignored => fallback IS defined and working!!!)");
    GenerateXml(dirname_solr_conf + "/schema_" + vufind_system_type_string + "_types.xml", dirname_solr_conf + "/schema_local_types.xml");
    GenerateXml(dirname_solr_conf + "/schema_" + vufind_system_type_string + "_fields.xml", dirname_solr_conf + "/schema_local_fields.xml");

    Echo("solrmarc (marc_local.properties)");
    const std::string dirname_solrmarc_conf = VUFIND_DIRECTORY + "/import";
    const std::string filename_solrmarc_conf_local = dirname_solrmarc_conf + "/marc_local.properties";
    GitAssumeUnchanged(filename_solrmarc_conf_local);
    const std::vector<std::string> filenames_solrmarc_conf_custom{
        dirname_solrmarc_conf + "/marc_tufind.properties",
        dirname_solrmarc_conf + "/marc_" + vufind_system_type_string + ".properties"
    };
    FileUtil_ConcatFiles(filename_solrmarc_conf_local, filenames_solrmarc_conf_custom);
    SetEnvironmentVariables(vufind_system_type_string);

    Echo("alphabetical browse");
    UseCustomFileIfExists(VUFIND_DIRECTORY + "/index-alphabetic-browse_" + vufind_system_type_string + ".sh", VUFIND_DIRECTORY + "/index-alphabetic-browse.sh");
    UseCustomFileIfExists(VUFIND_DIRECTORY + "/import/browse-indexing_" + vufind_system_type_string + ".jar", VUFIND_DIRECTORY + "/import/browse-indexing.jar");
    UseCustomFileIfExists(VUFIND_DIRECTORY + "/solr/vufind/jars/browse-handler_" + vufind_system_type_string + ".jar", VUFIND_DIRECTORY + "/solr/vufind/jars/browse-handler.jar");

    if (install_cronjobs) {
        Echo("cronjobs");
        InstallCronjobs(vufind_system_type);
    }

    Echo("creating directories");
    ExecOrDie(ExecUtil_Which("mkdir"), { "-p", "/usr/local/var/lib/tuelib" });
    ExecOrDie(ExecUtil_Which("mkdir"), { "-p", "/var/log/" + vufind_system_type_string });

    ConfigureSolrUserAndService(install_systemctl);
    ConfigureApacheUser(vufind_system_type_string, os_system_type);

    // write configured instance type to file
    FileUtil_WriteString(VUFIND_DIRECTORY + "/tufind.instance", vufind_system_type_string);
    Echo(vufind_system_type_string + " configuration completed!");
}


int main(int argc, char **argv) {
    ::progname = argv[0];

    bool ub_tools_only(false);
    VuFindSystemType vufind_system_type;
    bool omit_cronjobs(false);
    bool omit_systemctl(false);

    if (argc < 2 || argc > 4)
        Usage();

    if (std::strcmp("--ub-tools-only", argv[1]) == 0) {
        ub_tools_only = true;
        if (argc > 2)
            Usage();
    } else {
        if (::strcasecmp(argv[1], "krimdok") == 0)
            vufind_system_type = KRIMDOK;
        else if (::strcasecmp(argv[1], "ixtheo") == 0)
            vufind_system_type = IXTHEO;
        else
            Usage();

        if (argc >= 3) {
            for (int i=2;i<=3;i++) {
                if (i < argc) {
                    if (std::strcmp("--omit-cronjobs", argv[i]) == 0) {
                        omit_cronjobs = true;
                    } else if (std::strcmp("--omit-systemctl", argv[i]) == 0) {
                        omit_systemctl = true;
                    } else {
                        Usage();
                    }
                }
            }
        }
    }

    if (::geteuid() != 0)
        Error("you must execute this program as root!");

    const OSSystemType os_system_type(DetermineOSSystemType());

    try {
        if (not ub_tools_only) {
            MountDeptDriveOrDie(vufind_system_type);
            DownloadVuFind();
            ConfigureVuFind(vufind_system_type, os_system_type, not omit_cronjobs, not omit_systemctl);
        }
        InstallUBTools(os_system_type, /* make_install = */ not ub_tools_only);
    } catch (const std::exception &x) {
        Error("caught exception: " + std::string(x.what()));
    }
}

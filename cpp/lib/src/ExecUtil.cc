#include "ExecUtil.h"
#include <stdexcept>
#include <cassert>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <unordered_map>
#include "StringUtil.h"
#include "util.h"


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
        } else if (WIFSIGNALED(child_exit_status))
            throw std::runtime_error("in Exec: \"" + command + "\" killed by signal "
                                     + std::to_string(WTERMSIG(child_exit_status)) + "!");
        else // I have no idea how we got here!
            Error("in Exec: dazed and confused!");
    }

    return 0; // Keep the compiler happy!
}


} // unnamed namespace


namespace ExecUtil {


SignalBlocker::SignalBlocker(const int signal_to_block) {
    sigset_t new_set;
    ::sigemptyset(&new_set);
    ::sigaddset(&new_set, signal_to_block);
    if (::sigprocmask(SIG_BLOCK, &new_set, &saved_set_) != 0)
        Error("in ExecUtil::SignalBlocker::SignalBlocker: call to sigprocmask(2) failed!");
}


SignalBlocker::~SignalBlocker() {
    if (::sigprocmask(SIG_SETMASK, &saved_set_, nullptr) != 0)
        Error("in ExecUtil::SignalBlocker::~SignalBlocker: call to sigprocmask(2) failed!");
}


int Exec(const std::string &command, const std::vector<std::string> &args, const std::string &new_stdin,
         const std::string &new_stdout, const std::string &new_stderr, const unsigned timeout_in_seconds,
         const int tardy_child_signal)
{
    return ::Exec(command, args, new_stdin, new_stdout, new_stderr, ExecMode::WAIT, timeout_in_seconds,
                  tardy_child_signal);
}


int Spawn(const std::string &command, const std::vector<std::string> &args, const std::string &new_stdin,
          const std::string &new_stdout , const std::string &new_stderr)
{
    return ::Exec(command, args, new_stdin, new_stdout, new_stderr, ExecMode::DETACH, 0,
                  SIGKILL /* Not used because the timeout is 0. */);
}


std::unordered_map<std::string, std::string> which_cache;


std::string Which(const std::string &executable_candidate) {
    auto which_cache_entry = which_cache.find(executable_candidate);
    if (which_cache_entry != which_cache.cend())
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
        std::vector<std::string> path_compoments;
        StringUtil::Split(path_str, ':', &path_compoments);
        for (const auto &path_compoment : path_compoments) {
            const std::string full_path(path_compoment + "/" + executable_candidate);
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


std::string LocateOrDie(const std::string &executable_candidate) {
    const std::string path(ExecUtil::Which(executable_candidate));
    if (path.empty())
        Error("in ExecUtil::LocateOrDie: can't find \"" + executable_candidate + "\" in our PATH environment!");
    return path;
}



bool ExecSubcommandAndCaptureStdout(const std::string &command, std::string * const stdout_output) {
    stdout_output->clear();

    FILE * const subcommand_stdout(::popen(command.c_str(), "r"));
    if (subcommand_stdout == nullptr)
        return false;

    int ch;
    while ((ch = std::getc(subcommand_stdout)) != EOF)
        *stdout_output += static_cast<char>(ch);

    const int ret_code(::pclose(subcommand_stdout));
    if (ret_code == -1)
        Error("pclose(3) failed: " + std::string(::strerror(errno)));

    return WEXITSTATUS(ret_code) == 0;
}


} // namespace ExecUtil

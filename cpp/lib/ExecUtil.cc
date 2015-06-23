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
#include "util.h"


namespace {


// The following variables are set in Execute.
static bool alarm_went_off;
pid_t child_pid;


// SigAlarmHandler -- Used by Execute.
//
void SigAlarmHandler(int /* sig_no */) {
    alarm_went_off = true;
    ::kill(-child_pid, SIGTERM);
    ::sleep(2);
    ::kill(-child_pid, SIGKILL);
}


} // unnamed namespace


int Exec(const std::string &command, const std::vector<std::string> &args, const std::string &new_stdout,
	 unsigned timeout_in_seconds)
{
    if (::access(command.c_str(), X_OK) != 0)
	throw std::runtime_error("in Exec: can't execute \"" + command + "\"!");

    const int EXECVE_FAILURE(248);

    const pid_t pid = ::fork();
    if (pid == -1)
	throw std::runtime_error("in Exec: ::fork() failed: " + std::to_string(errno) + "!");

    // The child process:
    else if (pid == 0) {
	// Make us the leader of a new process group:
	if (::setsid() == static_cast<pid_t>(-1))
	    Error("in Exec(): child failed to become a new session leader!");

	if (not new_stdout.empty()) {
	    const int new_stdout_fd(::open(new_stdout.c_str(), O_WRONLY | O_CREAT, 0644));
	    if (new_stdout_fd == -1)
		::_exit(-1);
	    if (::dup2(new_stdout_fd, STDOUT_FILENO) == -1)
		::_exit(-1);
	    ::close(new_stdout_fd);
	}

	// Build the argument list for execve(2):
	char *argv[1 + args.size() + 1];
	unsigned arg_no(0);
	argv[arg_no++] = ::strdup(command.c_str());
	for (const auto &arg : args)
	    argv[arg_no++] = ::strdup(arg.c_str());
	argv[arg_no] = NULL;
	::execv(command.c_str(), argv);

	::_exit(EXECVE_FAILURE); // We typically never get here.
    }

    // The parent of the fork:
    else {
	void (*old_alarm_handler)(int) = NULL;

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
	int wait_retval = ::wait4(pid, &child_exit_status, 0, NULL);
	assert(wait_retval == pid or errno == EINTR);

	if (timeout_in_seconds > 0) {
	    // Cancel any outstanding alarm:
	    ::alarm(0);

	    // Restore the old alarm handler:
	    ::signal(SIGALRM, old_alarm_handler);

	    // Check to see whether the test timed out or not:
	    if (alarm_went_off) {
		// Snuff out all of our offspring.
		::kill(-pid, SIGKILL);
		while (::wait4(-pid, &child_exit_status, 0, NULL) != -1)
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

#include "Downloader.h"
#include <stdexcept>
#include <cassert>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>
#include "util.h"


const std::string WGET("/usr/bin/wget");


int Download(const std::string &url, const std::string &output_filename, const unsigned timeout)
{
    if (::access(WGET.c_str(), X_OK) != 0)
	throw std::runtime_error("in Downloader: can't execute \"" + WGET + "\"!");

    const int EXECVE_FAILURE(248);

    const pid_t pid = ::fork();
    if (pid == -1)
	throw std::runtime_error("in Downloader: ::fork() failed: " + std::to_string(errno) + "!");

    // The child process:
    else if (pid == 0) {
	// We're in the child.

	// Build the argument list for execve(2):
	char *argv[8];
	argv[0] = ::strdup(WGET.c_str());
	argv[1] = ::strdup("--quiet");
	argv[2] = ::strdup(url.c_str());
	argv[3] = ::strdup("-O");
	argv[4] = ::strdup(output_filename.c_str());
	argv[5] = ::strdup("--tries=1");
	if (timeout == 0)
	    argv[6] = NULL;
	else {
	    argv[6] = ::strdup(("--timeout=" + std::to_string(timeout)).c_str());
	    argv[7] = NULL;
	}
	::execv(WGET.c_str(), argv);

	::_exit(EXECVE_FAILURE); // We typically never get here.
    }

    // The parent of the fork:
    else {
	int child_exit_status;
	errno = 0;
	int wait_retval = ::wait4(pid, &child_exit_status, 0, NULL);
	assert(wait_retval == pid or errno == EINTR);

	// Now process the child's various exit status values:
	if (WIFEXITED(child_exit_status)) {
	    switch (WEXITSTATUS(child_exit_status)) {
	    case EXECVE_FAILURE:
		throw std::runtime_error("in Downloader: failed to execve(2) in child!");
	    default:
		return WEXITSTATUS(child_exit_status);
	    }
	}
	else if (WIFSIGNALED(child_exit_status))
	    throw std::runtime_error("in Downloader: \"" + WGET + "\" killed by signal "
				     + std::to_string(WTERMSIG(child_exit_status)) + "!");
	else // I have no idea how we got here!
	    Error("in Downloader: dazed and confused!");
    }

    return 0; // Keep the compiler happy!
}


int Download(const std::string &url, const unsigned timeout, std::string * const output) {
    char filename_template[] = "/tmp/DownloadXXXXXX";
    const std::string output_filename(::mktemp(filename_template));
    const int retval = Download(url, output_filename, timeout);
    if (retval == 0) {
	if (not ReadFile(output_filename, output)) {
	    ::unlink(output_filename.c_str());
	    return -1;
	}
    }
    ::unlink(output_filename.c_str());

    return retval;
}

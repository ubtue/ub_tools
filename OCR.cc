#include "OCR.h"
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


namespace {


// The following variables are set in Execute.
static bool alarm_went_off;
pid_t child_pid;


// SigAlarmHandler -- Used by Execute.
//
void SigAlarmHandler(int /* sig_no */)
{
	alarm_went_off = true;
	::kill(-child_pid, SIGKILL);
}


} // unnamed namespace


const std::string TESSERACT("/usr/bin/tesseract");
const int TIMEOUT(100);


int OCR(const std::string &input_document_path, const std::string &output_document_path,
	const std::string &language_codes)
{
    if (::access(TESSERACT.c_str(), X_OK) != 0)
	throw std::runtime_error("in OCR: can't execute \"" + TESSERACT + "\"!");

    if (language_codes.length() < 3)
	throw std::runtime_error("in OCR: missing or incorrect language code \"" + language_codes + "\"!");

    const int EXECVE_FAILURE(248);

    const pid_t pid = ::fork();
    if (pid == -1)
	throw std::runtime_error("in OCR: ::fork() failed: " + std::to_string(errno) + "!");

    // The child process:
    else if (pid == 0) {
	// We're in the child.

	// Build the argument list for execve(2):
	char *argv[6];
	argv[0] = ::strdup(TESSERACT.c_str());
	argv[1] = ::strdup("-1");
	argv[2] = ::strdup(language_codes.c_str());
	argv[3] = ::strdup(input_document_path.c_str());
	argv[4] = ::strdup(output_document_path.c_str());
	argv[5] = NULL;
	::execv(TESSERACT.c_str(), argv);

	::_exit(EXECVE_FAILURE); // We typically never get here.
    }

    // The parent of the fork:
    else {
	void (*old_alarm_handler)(int) = NULL;

	// Install new alarm handler...
	alarm_went_off = false;
	child_pid = pid;
	old_alarm_handler = ::signal(SIGALRM, SigAlarmHandler);

	// ...and wind the clock:
	::alarm(TIMEOUT);

	int child_exit_status;
	errno = 0;
	int wait_retval = ::wait4(pid, &child_exit_status, 0, NULL);
	assert(wait_retval == pid or errno == EINTR);

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

	    throw std::runtime_error("in OCR: \"" + TESSERACT + "\" timed out!");
	}

	// Now process the child's various exit status values:
	if (WIFEXITED(child_exit_status)) {
	    switch (WEXITSTATUS(child_exit_status)) {
	    case EXECVE_FAILURE:
		throw std::runtime_error("in OCR: failed to execve(2) in child!");
	    default:
		return WEXITSTATUS(child_exit_status);
	    }
	}
	else if (WIFSIGNALED(child_exit_status))
	    throw std::runtime_error("in OCR: \"" + TESSERACT + "\" killed by signal "
				     + std::to_string(WTERMSIG(child_exit_status)) + "!");
	else // I have no idea how we got here!
	    Error("in OCR: dazed and confused!");
    }

    return 0; // Keep the compiler happy!
}


int OCR(const std::string &input_document_path, const std::string &language_codes, std::string * const output) {
    char filename_template[] = "/tmp/ORCXXXXXX";
    const std::string output_filename(::mktemp(filename_template));
    const int retval = OCR(input_document_path, output_filename, language_codes);
    if (retval == 0) {
	if (not ReadFile(output_filename, output)) {
	    ::unlink(output_filename.c_str());
	    return -1;
	}
    }
    ::unlink(output_filename.c_str());

    return retval;
}

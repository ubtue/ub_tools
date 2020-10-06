// Test harness for ProcessUtil::GetProcessIdsForPath.
#include "FileUtil.h"
#include "ProcessUtil.h"
#include "util.h"


int Main(int, char **) {
    const std::string TEMPFILE_NAME("/tmp/GetProcessIdsForPath_test.temp");
    const auto open_file(FileUtil::OpenOutputFileOrDie(TEMPFILE_NAME));
    auto pids(ProcessUtil::GetProcessIdsForPath(TEMPFILE_NAME));
    if (pids.size() != 1)
        LOG_ERROR("we found " + std::to_string(pids.size()) + " PID's even though we expected 1!");

    const auto our_pid(::getpid());
    if (*(pids.cbegin()) != our_pid)
        LOG_ERROR("PID returned (" + std::to_string(*(pids.cbegin())) + ") does not match ours (" + std::to_string(our_pid) + ")!");

    open_file->close();
    pids = ProcessUtil::GetProcessIdsForPath(TEMPFILE_NAME);
    if (pids.empty())
        LOG_ERROR("we found " + std::to_string(pids.size()) + " PID's even though we expected 0!");

    ::unlink(TEMPFILE_NAME.c_str());

    return EXIT_SUCCESS;
}

#include "Downloader.h"
#include <unistd.h>
#include "ExecUtil.h"
#include "util.h"


const std::string WGET("/usr/bin/wget");


int Download(const std::string &url, const std::string &output_filename, const unsigned timeout) {
    std::vector<std::string> args;
    args.push_back("--quiet");
    args.push_back(url);
    args.push_back("-O");
    args.push_back(output_filename);
    args.push_back("--tries=1");
    if (timeout != 0)
	args.push_back("--timeout=" + std::to_string(timeout));

    return Exec(WGET, args);
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

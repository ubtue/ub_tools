#include "Downloader.h"
#include "ExecUtil.h"
#include "FileUtil.h"
#include "util.h"


const std::string WGET("/usr/bin/wget");


int Download(const std::string &url, const std::string &output_filename, const unsigned timeout,
             const std::string &cookie_file)
{
    std::vector<std::string> args;
    args.push_back("--quiet");
    args.push_back(url);
    args.push_back("-O");
    args.push_back(output_filename);
    args.push_back("--tries=1");
    if (timeout != 0)
        args.push_back("--timeout=" + std::to_string(timeout));
    if (not cookie_file.empty()) {
        args.push_back("--load-cookies");
        args.push_back(cookie_file);
        args.push_back("--save-cookies");
        args.push_back(cookie_file);
        args.push_back("--keep-session-cookies");
    }

    return Exec(WGET, args);
}


int Download(const std::string &url, const unsigned timeout, std::string * const output,
             const std::string &cookie_file)
{
    const FileUtil::AutoTempFile auto_temp_file;
    const std::string &output_filename(auto_temp_file.getFilePath());
    const int retval = Download(url, output_filename, timeout, cookie_file);
    if (retval == 0) {
        if (not ReadFile(output_filename, output))
            return -1;
    }

    return retval;
}

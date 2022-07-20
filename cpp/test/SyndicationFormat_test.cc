#include <fstream>
#include <iostream>
#include <cstdlib>
#include "Downloader.h"
#include "FileUtil.h"
#include "SyndicationFormat.h"
#include "util.h"


[[noreturn]] void Usage() {
    ::Usage(
        "[rss_url]\n"
        "\t- rss_url: link to RSS data\n");
}

std::string DownloadRSSData(const std::string &url) {
    const std::string DOWNLOAD_URL(url);

    Downloader downloader(DOWNLOAD_URL, Downloader::Params(), 15 * 1000);
    if (downloader.anErrorOccurred()) {
        LOG_WARNING("Error while downloading data for RSS URL " + url + ": " + downloader.getLastErrorMessage());
    }

    return downloader.getMessageBody();
}

int Main(int argc, char *argv[]) {
    if (argc != 2)
        Usage();

    const std::string RSSURL(argv[1]);

    const std::string xml(DownloadRSSData(RSSURL));
    const SyndicationFormat::AugmentParams augmentParams;
    std::string errorMessage;
    const auto syndicationFormat(SyndicationFormat::Factory(xml, augmentParams, &errorMessage));
    if (not errorMessage.empty())
        LOG_WARNING(errorMessage);

    LOG_INFO("\nStatistics:\n"
              "\tURL: " + RSSURL + "\n"
              "\tTitle: " + syndicationFormat->getTitle() + "\n"
              "\tFormat: " + syndicationFormat->getFormatName() + "\n"
              "\tDescription: " + syndicationFormat->getDescription() + "\n"
              "\tLink: " + syndicationFormat->getLink() + "\n");

    return EXIT_SUCCESS;
}

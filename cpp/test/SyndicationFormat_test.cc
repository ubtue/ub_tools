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

bool DownloadRSSData(const std::string &url, std::ofstream &newDataFile) {
    const std::string DOWNLOAD_URL(url);

    Downloader downloader(DOWNLOAD_URL, Downloader::Params(), 15 * 1000);
    if (downloader.anErrorOccurred()) {
        LOG_WARNING("Error while downloading data for RSS URL " + url + ": " + downloader.getLastErrorMessage());
        return false;
    }

    const std::string &parseDocument(downloader.getMessageBody());

    newDataFile << parseDocument;

    return true;
}

int Main(int argc, char *argv[]) {
    if (argc != 2)
        Usage();

    std::string fileName("/tmp/rssData");
    std::ofstream newDataFile(fileName);
    std::string RSSURL(argv[1]);

    DownloadRSSData(RSSURL, newDataFile);

    const std::string xml(FileUtil::ReadStringOrDie(fileName));
    SyndicationFormat::AugmentParams augmentParams;
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

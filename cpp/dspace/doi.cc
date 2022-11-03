#include <iostream>
#include <regex>
#include "Downloader.h"
#include "HttpHeader.h"
#include "JSON.h"

#include <set>
#include <unordered_map>
#include <cstdlib>
#include "DbConnection.h"
#include "DbResultSet.h"
#include "DbRow.h"
#include "StlHelpers.h"
#include "StringUtil.h"
#include "VuFind.h"
#include "util.h"

using namespace std;

const std::string DspaceServerURL("https://bibliographie.uni-tuebingen.de/xmlui/handle/");

void replaceHTML(std::string& s) {
    std::size_t posFirst = s.find("<td>");
    if (posFirst == std::string::npos) return;
    s.replace(posFirst, 4, "");

    std::size_t posLast = s.find("</td>");
    if (posLast == std::string::npos) return;
    s.replace(posLast, 5, "");
}

void UpdateItem(DbConnection * const db_writer, const std::string doi_link, const std::string publication_id) {
    db_writer->queryOrDie("UPDATE tuefind_publications SET doi_link = " +db_writer->escapeAndQuoteString(doi_link)+ " WHERE id= " +publication_id + "");
}

bool DownloadAndUpdate(DbConnection * const db_writer, const std::string external_document_id, const std::string publicationID) {
    const std::string DOWNLOAD_URL(DspaceServerURL+external_document_id+"?show=full");

    Downloader downloader(DOWNLOAD_URL, Downloader::Params(), 15 * 1000);
    if (downloader.anErrorOccurred()) {
        LOG_WARNING("Error while downloading data for id : " + downloader.getLastErrorMessage());
        return false;
    }

    const HttpHeader http_header(downloader.getMessageHeader());
    if (http_header.getStatusCode() != 200) {
        LOG_WARNING("DOI returned HTTP status code " + std::to_string(http_header.getStatusCode()) + "! for item id: "+publicationID);
        return false;
    }

    const std::string sitebody(downloader.getMessageBody());
    std::regex rex{ "<td>(.*?)http://dx.doi.org(.*?)</td>" };
    std::sregex_iterator beg{ sitebody.cbegin(), sitebody.cend(), rex }; 
    std::sregex_iterator end{};

    for (auto i = beg; i != end; ++i)
    {
        std::string DOILink(i->str());
        replaceHTML(DOILink);
        UpdateItem(db_writer,DOILink,publicationID);
    }  

    return true;

}


int main()
{   
    auto db_reader((DbConnection::VuFindMySQLFactory()));

    if (not VuFind::GetTueFindFlavour().empty()) {
        auto db_writer(DbConnection::VuFindMySQLFactory());
        db_reader.queryOrDie("SELECT * FROM tuefind_publications WHERE doi_link IS NULL OR doi_link=''");
        auto result_set(db_reader.getLastResultSet());
        DbTransaction transaction(&db_writer);
        while (const auto row = result_set.getNextRow()) {
            DownloadAndUpdate(&db_writer,row["external_document_id"],row["id"]);
        }
    }
    
    return 0;
}
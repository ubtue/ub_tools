/** \file    CachedPageFetcher.cc
 *  \brief   Implementation of the CachedPageFetcher class.
 *  \author  Dr. Johannes Ruscheinski
 *  \author  Dr. Gordon W. Paynter
 *  \author  Artur Kedzierski
 *  \author  Wagner Truppel
 */

/*
 *  \copyright 2002-2009 Project iVia.
 *  \copyright 2002-2009 The Regents of The University of California.
 *  \copyright 2017 Universitätsbibliothek Tübingen.
 *
 *  This file is part of the libiViaCore package.
 *
 *  The libiViaCore package is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public License as published
 *  by the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  libiViaCore is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public License
 *  along with libiViaCore; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "CachedPageFetcher.h"
#include <stdexcept>
#include <string>
#include <cerrno>
#include "DbRow.h"
#include "DnsUtil.h"
#include "FileUtil.h"
#include "GzStream.h"
#include "HttpHeader.h"
#include "IniFile.h"
#include "Logger.h"
#include "MediaTypeUtil.h"
#include "MiscUtil.h"
#include "PageFetcher.h"
#include "SqlUtil.h"
#include "StringUtil.h"
#include "SystemAndUserTimer.h"
#include "Url.h"
#include "util.h"
#include "WebUtil.h"


// Define static variables:
const std::string CachedPageFetcher::DENIED_BY_ROBOTS_DOT_TXT_ERROR_MSG("access_denied_by_robots_dot_txt");
const std::string CachedPageFetcher::DEFAULT_ACCEPTABLE_LANGUAGES("en,eng,english,de,deutsch,ger,german");
CachedPageFetcher::TimeoutOverrides CachedPageFetcher::timeout_overrides_;
std::string CachedPageFetcher::server_host_name_;
uint16_t CachedPageFetcher::server_port_(5432);
std::string CachedPageFetcher::server_user_name_;
std::string CachedPageFetcher::server_password_;
std::string CachedPageFetcher::page_cache_database_name_("iVia");
std::string CachedPageFetcher::page_cache_schema_name_("page_cache");
unsigned CachedPageFetcher::default_expiration_time_(720 * 3600); //30 days, in seconds
unsigned CachedPageFetcher::minimum_expiration_time_(24 * 3600); // 1 day, in seconds
unsigned CachedPageFetcher::maximum_document_size_(2147483648U); // 2 Gi, in bytes
std::string CachedPageFetcher::http_proxy_;
std::string CachedPageFetcher::default_user_agent_package_("ub_utils/5.5.0");
std::string CachedPageFetcher::default_user_agent_url_("http://ivia.ucr.edu/useragent.shtml");
unsigned CachedPageFetcher::no_of_new_connections_;
unsigned CachedPageFetcher::no_of_queries_;
unsigned CachedPageFetcher::verbosity_;
Logger * CachedPageFetcher::logger_(nullptr);


namespace {


const std::string HEADER_SEPARATOR("\r\n\r\n");


void ThrowIfUrlIsNotAcceptable(const std::string &url, const std::string &function_name) {
    if (url.length() > 5 and url.substr(0, 5) == "file:")
        return;

    Url test_url(url);
    if (not test_url.isValid())
        throw std::runtime_error("in " + function_name + ": URL \"" + url + "\" is not valid!");
    if (not test_url.isAbsolute())
        throw std::runtime_error("in " + function_name + ": URL \"" + url + "\" is not absolute!");
    if (test_url.getScheme() != "http" and test_url.getScheme() != "https")
        throw std::runtime_error("in " + function_name + ": URL \"" + url
                                 + "\" must be an \"http\" or \"https\" or \"file\" URL!");
}


// TrimFragment -- take the fragment part off of the end of a URL.
//
std::string TrimFragment(const std::string &url) {
    const std::string::size_type pos(url.find("#"));
    return pos != std::string::npos ? url.substr(0, pos) : url;
}


// UrlToCacheKey -- Make sure the URL is safe to use as a key in the cache table.
//                  This means it has to be SQL escaped and < 256 chars in length.
//
std::string UrlToCacheKey(const std::string &url) {
    std::string escaped_url(TrimFragment(url));
    SqlUtil::EscapeBlob(&escaped_url);
    if (escaped_url.length() > 255)
        escaped_url = escaped_url.substr(0, 255);

    // We can't end in a backslash:
    while (unlikely(escaped_url[escaped_url.length() - 1] == '\\'))
        escaped_url = escaped_url.substr(0, escaped_url.length() - 1);

    return escaped_url;
}


} // unnamed namespace


std::string CachedPageFetcher::TimeoutOverrides::getTimeoutForError(const std::string &error_message,
                                                                    const long default_timeout_override) const
{
    unsigned default_timeout(default_timeout_override != -1 ? static_cast<unsigned>(default_timeout_override)
                                                            : default_timeout_);
    for (const_iterator timeout_override(begin()); timeout_override != end(); ++timeout_override) {
        if (timeout_override->reg_exp_.match(error_message)) {
            if (default_timeout == 0 or default_timeout > timeout_override->timeout_)
                return SqlUtil::GetDatetime(timeout_override->timeout_);
            else
                return SqlUtil::GetDatetime(default_timeout);
        }
    }

    return SqlUtil::GetDatetime(default_timeout);
}


void CachedPageFetcher::ReadIniFile() {
    // The configuration file variables are static, so we only need to read the file once:
    if (not server_host_name_.empty())
        return;

    // Read configuration file:
    IniFile ini_file(ETC_DIR "/CachedPageFetcher.conf");
    server_host_name_ = ini_file.getString("Database", "server_host_name");
    server_port_ = ini_file.getUnsigned("Database", "server_port");
    server_user_name_ = ini_file.getString("Database", "server_user_name");
    page_cache_database_name_ = ini_file.getString("Database", "page_cache_database_name");
    default_expiration_time_ = ini_file.getUnsigned("Page cache", "default_expiration_time_in_hours");
    if (default_expiration_time_ > UINT_MAX / 3600)
        throw std::runtime_error("in CachedPageFetcher::ReadIniFile: \"default_expiration_time_in_hours\" "
                                 "exceeded possible maximum of " + StringUtil::ToString(UINT_MAX / 3600) + "!");

    default_expiration_time_ *= 3600;

    minimum_expiration_time_ = ini_file.getUnsigned("Page cache", "minimum_expiration_time_in_hours");
    if (minimum_expiration_time_ > UINT_MAX / 3600)
        throw std::runtime_error("in CachedPageFetcher::ReadIniFile: \"minimum_expiration_time_in_hours\" exceeded "
                                 "possible maximum of " + std::to_string(UINT_MAX / 3600) + "!");
    minimum_expiration_time_ *= 3600;

    maximum_document_size_ = ini_file.getUnsigned("Page cache", "maximum_document_size");

    http_proxy_ = ini_file.getString("Connection", "http_proxy", "");

    // User agent information:
    default_user_agent_package_ = ini_file.getString("User Agent", "default_package");
    default_user_agent_url_     = ini_file.getString("User Agent", "default_url");

    const std::list<std::string> entrys(ini_file.getSectionEntryNames("TimeoutOverrides"));
    for (std::list<std::string>::const_iterator entry(entrys.begin()); entry != entrys.end(); ++entry) {
        if (std::strncmp("error_msg_pattern", entry->c_str(), 17) == 0) {
            const unsigned timeout(static_cast<unsigned>(ini_file.getDouble("TimeoutOverrides", "timeout"
                                                                            + entry->substr(17))));
            timeout_overrides_.push_back(TimeoutOverride(ini_file.getString("TimeoutOverrides", *entry), timeout));
        }
    }

    if (ini_file.sectionIsDefined("Logging")) {
        verbosity_ = ini_file.getUnsigned("Logging", "verbosity");
        if (verbosity_ > 0) {
            const std::string log_filename = ini_file.getString("Logging", "log_filename");
            logger_ = new Logger(log_filename);
        }
    } else
        verbosity_ = 0;
}


CachedPageFetcher::CachedPageFetcher(const std::string &url, const TimeLimit &time_limit, const Params &params)
    : params_(params), last_url_(url), last_error_code_(0), robots_dot_txt_("")
{
    ThrowIfUrlIsNotAcceptable(url, "CachedPageFetcher::CachedPageFetcher");
    ReadIniFile();
    retrieveDocument(url, time_limit, params_.max_redirects_);
}


CachedPageFetcher::CachedPageFetcher(const Params &params)
    : params_(params), last_url_(""), last_error_code_(0), robots_dot_txt_("")
{
    ReadIniFile();
}


void CachedPageFetcher::newUrl(const std::string &url, const TimeLimit &time_limit) {
    ThrowIfUrlIsNotAcceptable(url, "CachedPageFetcher::newUrl");

    last_url_ = url;
    last_error_message_.clear();
    last_error_code_ = 0;
    message_body_.clear();
    message_headers_.clear();

    retrieveDocument(url, time_limit, params_.max_redirects_);
}


void CachedPageFetcher::newUrl(const std::string &url, const Params &params, const TimeLimit &time_limit) {
    ThrowIfUrlIsNotAcceptable(url, "CachedPageFetcher::newUrl");

    params_   = params;
    last_url_ = url;
    last_error_message_.clear();
    last_error_code_ = 0;
    message_body_.clear();
    message_headers_.clear();

    retrieveDocument(url, time_limit, params_.max_redirects_);
}


// DefaultUserAgentString -- Generate a useful default user agent for this program.
//
std::string CachedPageFetcher::DefaultUserAgent() {
    return default_user_agent_package_ + " " + ::progname + " (" + default_user_agent_url_ + ")";
}


std::string CachedPageFetcher::RobotsDotTxtOptionToString(const RobotsDotTxtOption robots_dot_txt_option) {
    switch (robots_dot_txt_option) {
    case CONSULT_ROBOTS_DOT_TXT:
        return "CONSULT_ROBOTS_DOT_TXT";
    case IGNORE_ROBOTS_DOT_TXT:
        return "IGNORE_ROBOTS_DOT_TXT";
    default:
        throw std::runtime_error("in CachedPageFetcher::RobotsDotTxtOptionToString: unknown option: "
                                 + StringUtil::ToString(robots_dot_txt_option) + "!");
    }
}


std::string CachedPageFetcher::RobotsDotTxtOptionToBool(const RobotsDotTxtOption robots_dot_txt_option) {
    switch (robots_dot_txt_option) {
    case CONSULT_ROBOTS_DOT_TXT:
        return "t";
    case IGNORE_ROBOTS_DOT_TXT:
        return "f";
    default:
        throw std::runtime_error("in CachedPageFetcher::RobotsDotTxtOptionToBool: unknown option: "
                                 + StringUtil::ToString(robots_dot_txt_option) + "!");
    }
}


CachedPageFetcher::RobotsDotTxtOption CachedPageFetcher::StringToRobotsDotTxtOption(
    const std::string &robots_dot_txt_option_as_string)
{
    if (robots_dot_txt_option_as_string == "CONSULT_ROBOTS_DOT_TXT")
        return CONSULT_ROBOTS_DOT_TXT;
    if (robots_dot_txt_option_as_string == "IGNORE_ROBOTS_DOT_TXT")
        return IGNORE_ROBOTS_DOT_TXT;

    throw std::runtime_error("in CachedPageFetcher::StringToRobotsDotTxtOption: unknown option: \""
                             + robots_dot_txt_option_as_string + "\"!");
}


CachedPageFetcher::RobotsDotTxtOption CachedPageFetcher::BoolToRobotsDotTxtOption(
    const std::string &robots_dot_txt_option_as_bool)
{
    if (::strcasecmp(robots_dot_txt_option_as_bool.c_str(), "t") == 0
        or ::strcasecmp(robots_dot_txt_option_as_bool.c_str(), "true") == 0)
        return CONSULT_ROBOTS_DOT_TXT;
    if (::strcasecmp(robots_dot_txt_option_as_bool.c_str(), "f") == 0
        or ::strcasecmp(robots_dot_txt_option_as_bool.c_str(), "false") == 0)
        return IGNORE_ROBOTS_DOT_TXT;

    throw std::runtime_error("in CachedPageFetcher::BoolToRobotsDotTxtOption: unknown option: \""
                             + robots_dot_txt_option_as_bool + "\"!");
}


namespace {


inline std::string CreateCacheTableName(const std::string &schema) {
    return schema + ".cache";
}


inline std::string CreateRedirectTableName(const std::string &schema) {
    return schema + ".redirect";
}


} // unnamed namespace


bool CachedPageFetcher::CreatePageCacheDatabaseTable(const std::string &server_host_name, const uint16_t server_port,
                                                     const std::string &server_user_name,
                                                     const std::string &server_password,
						     const std::string &page_cache_database_name,
                                                     const std::string &page_cache_schema_name,
						     std::string * const error_message)
{
    error_message->clear();

    try {
        DbConnection db_connection(page_cache_database_name, server_user_name, server_password, server_host_name,
                                   server_port);
        db_connection.queryOrDie("SELECT make_schema('" + page_cache_schema_name +"')");

        server_host_name_         = server_host_name;
        server_port_              = server_port;
        server_user_name_         = server_user_name;
        server_password_          = server_password;
        page_cache_database_name_ = page_cache_database_name;
        page_cache_schema_name_   = page_cache_schema_name;
    } catch (const std::exception &x) {
        *error_message = x.what();
        return false;
    }

    return true;
}


bool CachedPageFetcher::SetPageCacheDatabaseTable(const std::string &server_host_name, const uint16_t server_port,
                                                  const std::string &server_user_name,
                                                  const std::string &server_password,
						  const std::string &page_cache_database_name,
                                                  const std::string &page_cache_schema_name,
						  std::string * const error_message)
{
    error_message->clear();

    try {

        DbConnection db_connection(/* database_name = */ "", server_user_name, server_password,
                                   server_host_name, server_port);
        db_connection.queryOrDie("SELECT make_schema('" + page_cache_schema_name +"')");

        server_host_name_         = server_host_name;
        server_port_              = server_port;
        server_user_name_         = server_user_name;
        server_password_          = server_password;
        page_cache_database_name_ = page_cache_database_name;
        page_cache_schema_name_   = page_cache_schema_name;
    } catch (const std::exception &x) {
        *error_message = x.what();
        return false;
    }

    return true;
}


// getMessageBodyHash -- Make a fast hash from the message body content.
//
unsigned CachedPageFetcher::getMessageBodyHash() const {
    if (last_url_.empty())
        throw std::runtime_error("in CachedPageFetcher::getMessageBodyHash: no URL has previously been specified!");

    std::string escaped_url(UrlToCacheKey(last_url_));
    std::string body;
    last_error_message_.clear();

    // Retrieve body data from the database
    requireDbConnection();
    std::string select_stmt("SELECT status, LENGTH(compressed_document_source) AS compressed_document_source_size "
                            "FROM ");
    select_stmt += CreateCacheTableName(page_cache_schema_name_) + " WHERE url=E'" + escaped_url + "'";

    db_connection_->queryOrDie(select_stmt);
    DbResultSet result_set(db_connection_->getLastResultSet());
    ++CachedPageFetcher::no_of_queries_;
    if (unlikely(result_set.empty()))
        Error("in CachedPageFetcher::getMessageBodyHash: could not retrieve data from cache table with: "
              + select_stmt);

    const DbRow row(result_set.getNextRow());

    // Translate the body data into a string (if possible)
    if (row[0] != "ok") {
        last_error_message_ = row[0];
        return 0;
    }

    return StringUtil::ToUnsigned(row[1]);
}


unsigned CachedPageFetcher::getRedirectCount() const {
    if (last_url_.empty())
        throw std::runtime_error("in CachedPageFetcher::getRedirectCount: no URL has previously been "
                                 "specified!");

    std::string escaped_url(UrlToCacheKey(last_url_));

    requireDbConnection();
    std::string select_stmt("SELECT count(*) FROM " + CreateRedirectTableName(page_cache_schema_name_) +
                            " WHERE cache_id = (SELECT url_id FROM "
                            + CreateCacheTableName(page_cache_schema_name_) + "WHERE url = E'" + escaped_url
                            + "')");
    db_connection_->queryOrDie(select_stmt);
    DbResultSet result_set(db_connection_->getLastResultSet());

    ++CachedPageFetcher::no_of_queries_;
    if (unlikely(result_set.empty()))
        Error("in CachedPageFetcher::getRedirectCount: query \"" + select_stmt + "\" returned an empty result set!");

    DbRow row(result_set.getNextRow());
    return StringUtil::ToUnsigned(row[0]);
}


std::string CachedPageFetcher::getRedirectedUrl() const {
    if (last_url_.empty())
        throw std::runtime_error("in CachedPageFetcher::getRedirectedUrl: no URL has previously been specified!");

    std::string escaped_url(UrlToCacheKey(last_url_));

    requireDbConnection();
    const std::string SELECT_STMT("SELECT status,redirected_url FROM " + getCacheTableName() + " WHERE url=E'"
                                  + escaped_url + "'");
    db_connection_->queryOrDie(SELECT_STMT);
    ++CachedPageFetcher::no_of_queries_;
    DbResultSet result_set(db_connection_->getLastResultSet());
    if (unlikely(result_set.empty()))
        Error("in CachedPageFetcher::getRedirectedUrl: unexpected empty result set for query: " + SELECT_STMT);

    DbRow row(result_set.getNextRow());
    std::string redirected_url;
    if (row[0] != "ok")
        last_error_message_ = row[0];
    else if (row.size() > 1)
        redirected_url = row[1];

    return redirected_url;
}


// CachedPageFetcher::getRobotsDotTxtForUrl -- generate a RobotsDotTxt object based on a Url object.
//
void CachedPageFetcher::getRobotsDotTxtForUrl(const Url &url, const TimeLimit &time_limit, const long max_redirects) {
    const std::string robots_txt_url(url.getRobotsDotTxtUrl());

    // Save the current state variables and set temporary ones:
    const RobotsDotTxtOption saved_robots_dot_txt_option(params_.robots_dot_txt_option_);
    const std::string saved_last_url(last_url_);
    params_.robots_dot_txt_option_ = IGNORE_ROBOTS_DOT_TXT; // Prevent infinite recursion!
    last_url_                      = robots_txt_url;

    // Download the robots.txt file.
    if (not robots_txt_url.empty()) {
        retrieveDocument(robots_txt_url, time_limit, max_redirects);
        if (not anErrorOccurred()) {
            robots_dot_txt_.reinitialize(getMessageBody());

            // Restore the saved state:
            params_.robots_dot_txt_option_ = saved_robots_dot_txt_option;
            last_url_                      = saved_last_url;

            return;
        }
    }

    // Restore the saved state:
    last_error_message_.clear();
    params_.robots_dot_txt_option_ = saved_robots_dot_txt_option;
    last_url_                      = saved_last_url;

    robots_dot_txt_.clear();
}


bool CachedPageFetcher::accessAllowed(const std::string &url, const TimeLimit &time_limit, const long max_redirects) {
    if (params_.robots_dot_txt_option_ == IGNORE_ROBOTS_DOT_TXT)
        return true;

    const Url test_url(url);
    if (not test_url.isValid())
        Error("in CachedPageFetcher::accessAllowed: URL: \"" + url + "\" is invalid!");

    // Access is always allowed for non-HTTP protocols, and for the robots.txt file.
    if (not test_url.isValidWebUrl() or ::strcasecmp("/robots.txt", test_url.getPath().c_str()) == 0)
        return true;

    // Can we use the "cached" robots.txt object?
    static std::string last_domain;
    std::string new_domain(test_url.getAuthority());
    if (last_domain == new_domain)
        return robots_dot_txt_.accessAllowed(params_.user_agent_, test_url.getPath());

    // Create a robots.txt file for the current domain:
    getRobotsDotTxtForUrl(test_url, time_limit, max_redirects);
    if (time_limit.limitExceeded())
        return true;
    last_domain = new_domain;
    return robots_dot_txt_.accessAllowed(params_.user_agent_, test_url.getPath());
}


void CachedPageFetcher::retrieveDocument(const std::string &url, const TimeLimit &time_limit,
                                         const long max_redirects)
{
    if (url.substr(0, 5) == "file:")
        readDocumentFromFileSystem(url);
    else {
        unsigned http_equiv_redirects(0);
        std::string current_url(url);
        for (;;) {
            if (unlikely(not Url(current_url).isValid()))
                return;
            if (not downloadPage(current_url, time_limit, max_redirects))
                return;
            last_url_ = current_url;

            if (http_equiv_redirects >= params_.max_http_equiv_redirects_)
                return;

            // Only look for redirects in Web pages:
            if (media_type_ != "text/html" and media_type_ != "text/xhtml")
                return;

            // Look for HTTP-EQUIV "Refresh" meta tags:
            std::list< std::pair<std::string, std::string> > refresh_meta_tags;
            HttpEquivExtractor http_equiv_extractor(message_body_, "refresh", &refresh_meta_tags);
            http_equiv_extractor.parse();
            if (refresh_meta_tags.empty())
                return;

            std::string delay, redirect_url;
            if (not StringUtil::SplitOnStringThenTrimWhite(refresh_meta_tags.front().second, ";", &delay,
                                                           &redirect_url))
                return;

            // Attempt to find a URL in "redirect_url":
            static const PerlCompatRegExp perl_compat_reg_exp("(https?://.+)");
            std::string actual_redirected_url;
            if (not perl_compat_reg_exp.match(redirect_url)
                or not perl_compat_reg_exp.getMatchedSubstring(1, &actual_redirected_url)
                or actual_redirected_url == current_url)
                return;

            current_url = Url(actual_redirected_url, current_url);
            ++http_equiv_redirects;
        }
    }
}


void CachedPageFetcher::readDocumentFromFileSystem(const std::string &url) {
    message_headers_.clear();
    std::string escaped_url(UrlToCacheKey(url));
    std::string retrieval_datetime(SqlUtil::GetDatetime());

    const std::string filename(url.substr(5)); // Strip off a leading "file:".
    std::string document;
    if (not FileUtil::ReadString(filename, &document)) {
        last_error_code_    = UINT_MAX;
        last_error_message_ = "can't read file";
        if (useCache())
            actualStoreInCache(escaped_url, retrieval_datetime, retrieval_datetime, last_error_message_);
    } else {
        // Make sure the page fits in the cache:
        if (document.size() > maximum_document_size_) {
            media_type_ = MediaTypeUtil::GetMediaType(document);
            if (StringUtil::StartsWith(media_type_, "text")) {
                // Text documents can be truncated:
                document = document.substr(0, maximum_document_size_);
            } else {
                // Other documents cannot be truncated:
                last_error_message_ = "document too large to cache (" + StringUtil::ToString(document.size())
                    + " bytes, max. is " + StringUtil::ToString(maximum_document_size_) + ", "
                    + (media_type_.empty() ? "unknown type" : media_type_) + ")";
                last_error_code_    = UINT_MAX;
                if (useCache())
                    actualStoreInCache(escaped_url, retrieval_datetime, retrieval_datetime, last_error_message_);
                return;
            }
        }

        // Store the document in the cache if requested:
        if (useCache())
            actualStoreInCache(escaped_url, retrieval_datetime, retrieval_datetime, "ok", 0 /* redirect_count */, "",
                               document);
        message_body_ = document;
        last_error_code_ = 0;
        last_error_message_.clear();
    }
}


bool CachedPageFetcher::downloadPage(const std::string &url, const TimeLimit &time_limit, const long max_redirects) {
    // Make sure that the maximum number of redirects is within reasonable limits:
    if (unlikely(max_redirects < 0 or max_redirects > 255))
        Error("in CachedPageFetcher::downloadPage: max_redirects " + std::to_string(max_redirects)
              + "must be in [0, 255]!");

    if (verbosity_ >= 5 and logger_ != nullptr)
        logger_->log("%sStoring: '%s', %u ms", useCache() ? "":"Not ", url.c_str(),  time_limit.getRemainingTime());

    std::string escaped_url(UrlToCacheKey(url));
    std::string retrieval_datetime(SqlUtil::GetDatetime());
    timeout_overrides_.setDefaultTimeout(default_expiration_time_);

    // If robots.txt forbids the download, store that fact:
    if (not accessAllowed(url, time_limit, max_redirects)) {
        last_error_message_ = CachedPageFetcher::DENIED_BY_ROBOTS_DOT_TXT_ERROR_MSG;
        if (useCache())
            actualStoreInCache(escaped_url, retrieval_datetime,
                               timeout_overrides_.getTimeoutForError(last_error_message_),
                               last_error_message_);
        return false;
    }

    if (params_.connection_option_ == CLOSE_DB_CONNECTION_WHILE_FETCHING)
        db_connection_.reset();

    // If the user has specified an HTTP Proxy, parse it:
    std::string proxy_host;
    unsigned short proxy_port(0);
    if (not http_proxy_.empty()) {
        std::string::size_type colon_pos = http_proxy_.find(':');
        if (colon_pos == std::string::npos)
            throw std::runtime_error("in CachedPageFetcher::downloadPage: missing colon in hostname:port in "
                                     "configuration file (section \"Connection\", variable \"http_proxy\".");
        proxy_host = http_proxy_.substr(0, colon_pos);
        if (not DnsUtil::IsValidHostName(proxy_host)
            and not DnsUtil::IsDottedQuad(proxy_host))
            throw std::runtime_error("in CachedPageFetcher::downloadPage: invalid hostname in configuration file"
                                     " (section \"Connection\", variable \"http_proxy\".");
        char *endptr;
        errno = 0;
        unsigned long port = std::strtoul(http_proxy_.c_str() + colon_pos + 1, &endptr, 10);
        if (errno != 0 or *endptr != '\0' or port > 65535)
            throw std::runtime_error("in CachedPageFetcher::downloadPage: invalid port in configuration file"
                                     " (section \"Connection\", variable \"http_proxy\".");
        proxy_port = static_cast<unsigned short>(port);
    }

    if (useCache())
        requireDbConnection();

    // Are we out of time yet?
    if (time_limit.limitExceeded()) {
        last_error_message_ = "timeout: timed out before actually attempting a page download";
        if (useCache())
            actualStoreInCache(escaped_url, retrieval_datetime,
                               timeout_overrides_.getTimeoutForError(last_error_message_),
                               last_error_message_);
        return false;
    }

    // Download the page:

    // Make sure we honour robots.txt on each URL even when we follow redirects:
    Url redirected_url(url);
    std::string previous_url;
    std::string current_header, all_headers;
    int redirect_count = 0;

    while (redirect_count <= max_redirects) {
        if (verbosity_ >= 4 and logger_ != nullptr)
            logger_->log("Fetch no. %u, URL: '%s', remaining time: %u ms", redirect_count,
                         redirected_url.toString().c_str(), time_limit.getRemainingTime());

        unsigned timeout;
        bool found_in_cache(foundInCache(redirected_url, time_limit, &timeout));
        if (found_in_cache) {
            if (timeout > 0) // Found a timed out entry in the cache.
                return false;

            if (not retrieveFromCache(redirected_url, &current_header, &message_body_))
                return false;
        } else { // Attempt to actually download the requested page.
            // Get any cookies we might have for the following request:
            std::string additional_headers;
            cookie_jar_.getCookieHeaders(redirected_url.getAuthority(), redirected_url.getPath(),
                                         &additional_headers);

            // If we have a referrer add it to our HTTP headers:
            if (not previous_url.empty())
                additional_headers += "Referer: " + previous_url + "\r\n";

            // Download the page with the PageFetcher:
            PageFetcher page_fetcher(redirected_url, additional_headers, proxy_host, proxy_port,
                                     time_limit.getRemainingTime(), 0 /* max_redirects */,
                                     /* ignore_redirect_errors = */ true, /* transparently_unzip = */ true ,
                                     params_.user_agent_, params_.acceptable_languages_);
            previous_url = redirected_url;

            // Handle page fetcher errors:
            if (page_fetcher.anErrorOccurred()) {
                last_error_code_ = page_fetcher.getLastErrorCode();
                if (page_fetcher.getErrorMsg() == page_fetcher.getTimeoutErrorMsg())
                    last_error_message_ = "timeout: PageFetcher timed out.";
                else if (page_fetcher.getErrorMsg() == page_fetcher.getNoSuchDomainErrorMsg()) {
                    // Evil hack: try again with a prepended "www." if we don't already have that:
                    if (not StringUtil::IsPrefixOf("www.", redirected_url.getAuthority())) {
                        redirected_url.setAuthority("www." + redirected_url.getAuthority());
                        continue;
                    }
                }
                else if (page_fetcher.getErrorMsg().empty())
                    last_error_message_ = "unspecified error.";
                else
                    last_error_message_ = page_fetcher.getErrorMsg();

                if (useCache())
                    actualStoreInCache(escaped_url, retrieval_datetime,
                                       timeout_overrides_.getTimeoutForError(last_error_message_),
                                       last_error_message_, redirect_count,
                                       (redirect_count > 0 ? redirected_url.toString() : ""));
                return false;
            }

            PageFetcher::SplitHttpHeadersFromBody(page_fetcher.getData(), &current_header, &message_body_);
            last_error_code_ = page_fetcher.getLastErrorCode();
        }

        StringUtil::Trim(" \t\r\n", &current_header);
        current_header += "\r\n\r\n";
        message_headers_.push_back(current_header);

        // Ensure the HTTP Header is usable:
        const HttpHeader http_header(current_header);
        if (not http_header.isValid()) {
            last_error_message_ = "invalid HTTP Header downloading \"" + redirected_url.toString() + "\"!";
            if (not found_in_cache and useCache())
                actualStoreInCache(escaped_url, retrieval_datetime,
                                   timeout_overrides_.getTimeoutForError(last_error_message_),
                                   last_error_message_, redirect_count,
                                   (redirect_count > 0 ? redirected_url.toString() : ""));
            return false;
        } else if (not http_header.hasAcceptableLanguage(params_.acceptable_languages_)) {
            last_error_message_ = "HTTP header content language(s) (\"" + http_header.getContentLanguages()
                + "\") did not match at least one of the the requested languages (\"" + params_.acceptable_languages_
                + "\")!";
            if (not found_in_cache and useCache())
                actualStoreInCache(escaped_url, retrieval_datetime,
                                   timeout_overrides_.getTimeoutForError(last_error_message_), last_error_message_,
                                   redirect_count, (redirect_count > 0 ? redirected_url.toString() : ""));
            return false;
        }
        cookie_jar_.addCookies(http_header, Url(redirected_url).getAuthority());

        // Update the combined header data:
        all_headers += current_header;

        // If we have finished following redirections, break the loop and continue processing:
        last_error_code_ = http_header.getStatusCode();
        if (last_error_code_ < 300 or last_error_code_ > 399)
            break; // No more redirections!

        // Make sure we don't exceed the max. allowed number of redirects:
        ++redirect_count;
        if (redirect_count > max_redirects) {
            last_error_message_ = "too many redirects (> " + StringUtil::ToString(max_redirects) + ")!";
            last_error_code_    = 0;
            if (not found_in_cache and useCache())
                actualStoreInCache(escaped_url, retrieval_datetime,
                                   timeout_overrides_.getTimeoutForError(last_error_message_), last_error_message_,
                                   redirect_count, redirected_url, message_body_, current_header,
                                   http_header.getETag());
            return false;
        }

        // Make sure there is a redirection location:
        if (http_header.getLocation().empty() and http_header.getUri().empty()) {
            last_error_message_ = "can't extract redirection URI for URL \"" + redirected_url.toString() + "\"!";
            if (not found_in_cache and useCache())
                actualStoreInCache(escaped_url, retrieval_datetime,
                                   timeout_overrides_.getTimeoutForError(last_error_message_), last_error_message_,
                                   redirect_count, (redirect_count > 0 ? redirected_url.toString() : ""),
                                   message_body_, current_header, http_header.getETag());
            return false;
        }

        // Make sure the redirection location is valid:
        Url new_url(http_header.getLocation().empty() ? http_header.getUri() : http_header.getLocation(),
                    redirected_url);
        std::string url_error_message;
        if (new_url.anErrorOccurred())
            url_error_message = "URL error during redirection: " + new_url.getErrorMsg();
        else if (not new_url.makeAbsolute())
            url_error_message = "the redirected URL \"" + new_url.toString() + "\" is not absolute!";
        else if (not new_url.isValidWebUrl())
            url_error_message = "the URL \"" + url + "\" was redirected to the invalid URL \"" + new_url.toString()
                                + "\"!";
        if (not url_error_message.empty()) {
            last_error_message_ = url_error_message;
            last_error_code_    = UINT_MAX;
            if (not found_in_cache and useCache())
                actualStoreInCache(escaped_url, retrieval_datetime,
                                   timeout_overrides_.getTimeoutForError(last_error_message_), last_error_message_,
                                   redirect_count, (redirect_count > 0 ? redirected_url.toString() : ""),
                                   message_body_, current_header);
            return false;
        }

        // Accept the redirection:
        redirected_url = new_url;

        // Make sure we didn't time out during this download:
        if (time_limit.limitExceeded()) {
            last_error_message_ = "timeout: timed out while following redirects!";
            last_error_code_    = UINT_MAX;
            if (not found_in_cache and useCache())
                actualStoreInCache(escaped_url, retrieval_datetime,
                                   timeout_overrides_.getTimeoutForError(last_error_message_), last_error_message_,
                                   redirect_count, (redirect_count > 0 ? redirected_url.toString() : ""),
                                   message_body_, current_header);
            return false;
        }

        // Make sure that intermediate redirected URLs are accessible:
        if (not accessAllowed(redirected_url, time_limit, max_redirects)) {
            last_error_message_ = CachedPageFetcher::DENIED_BY_ROBOTS_DOT_TXT_ERROR_MSG;
            last_error_code_    = UINT_MAX;
            if (not found_in_cache and useCache())
                actualStoreInCache(escaped_url, retrieval_datetime,
                                   timeout_overrides_.getTimeoutForError(last_error_message_), last_error_message_,
                                   redirect_count, (redirect_count > 0 ? redirected_url.toString() : ""),
                                   message_body_, current_header);
            return false;
        }

        // Finally: store the page in the cache if requested:
        if (not found_in_cache and useCache())
            actualStoreInCache(escaped_url, retrieval_datetime,
                               timeout_overrides_.getTimeoutForError(last_error_message_), "ok",
                               redirect_count, (redirect_count > 0 ? redirected_url.toString() : ""), message_body_,
                               current_header);
    }

    long expiration_time(-1);

    // Get the header's expire time if possible:
    HttpHeader http_header(current_header);
    if (http_header.expiresIsValid()) {
        const time_t now(std::time(nullptr));
        if (now < http_header.getExpires())
            expiration_time = http_header.getExpires() - now;

        // Make sure we keep the page for at least the specified minimum time
        int minimum_expiration(minimum_expiration_time_);
        if (expiration_time < minimum_expiration)
            expiration_time = minimum_expiration;
    }

    // If the final header resulted in a 404 error, cache that result.
    if (http_header.getStatusCode() == 404) {
        last_error_message_ = "page not found";
        last_error_code_    = 404;
        if (useCache())
            actualStoreInCache(escaped_url, retrieval_datetime,
                               timeout_overrides_.getTimeoutForError(last_error_message_, expiration_time),
                               last_error_message_, redirect_count,
                               (redirect_count > 0 ? redirected_url.toString() : ""),
                               message_body_, current_header, http_header.getETag());
        return false;
    }

    if (params_.text_translation_mode_ == MAP_TO_LATIN9)
        message_body_ = WebUtil::ConvertToLatin9(http_header, message_body_);

    // Make sure the page fits in the cache:
    media_type_ = MediaTypeUtil::GetMediaType(current_header, message_body_);
    if (message_body_.size() > maximum_document_size_) {
        if (StringUtil::StartsWith(media_type_, "text")) {
            // Text documents can be truncated:
            message_body_ = message_body_.substr(0, maximum_document_size_);
        } else {
            // Other documents cannot be truncated:
            last_error_message_ = "document too large to cache (" + StringUtil::ToString(message_body_.size())
                + " bytes, max. is " + StringUtil::ToString(maximum_document_size_) + ", "
                + (media_type_.empty() ? "unknown type" : media_type_) + ")";
            last_error_code_    = UINT_MAX;
            if (useCache())
                actualStoreInCache(escaped_url, retrieval_datetime,
                                   timeout_overrides_.getTimeoutForError(last_error_message_, expiration_time),
                                   last_error_message_, redirect_count,
                                   (redirect_count > 0 ? redirected_url.toString() : ""), message_body_,
                                   current_header, http_header.getETag());
            return false;
        }
    }

    // Finally: store the page in the cache if requested:
    if (useCache())
        actualStoreInCache(escaped_url, retrieval_datetime,
                           timeout_overrides_.getTimeoutForError("ok", expiration_time), "ok", redirect_count,
                           (redirect_count > 0 ? redirected_url.toString() : ""), message_body_, current_header,
                           http_header.getETag());

    last_error_message_.clear();

    return true;
}


// CachedPageFetcher::foundInCache -- we return true if the entry is in the cache and it doesn't indicate a timeout
//                                    error of less than "timeout" and has the same robots.txt option, otherwise we
//                                    return false.
//
bool CachedPageFetcher::foundInCache(const std::string &url, const TimeLimit &time_limit, unsigned * const timeout)
    const
{
    *timeout = 0;

    // Always claim that we didn't find a page in the cache if the client requested no cache lookups:
    if (not useCache())
        return false;

    requireDbConnection();

    if (verbosity_ >= 4 and logger_ != nullptr)
        logger_->log("Finding: '%s', %u ms", url.c_str(), time_limit.getRemainingTime());

    const bool is_cached(CachedPageFetcher::IsCached(url, params_.robots_dot_txt_option_, &last_error_message_,
                                                     db_connection_.get()));

    if (is_cached and StringUtil::StartsWith(last_error_message_, "timeout:")) {
        // The error message indicates a timeout of some sort.  These are either simple (i.e. numbers) or timeouts
        // with other errors.
        if (StringUtil::IsUnsignedNumber(last_error_message_.substr(8))) { // A simple timeout:
            *timeout = StringUtil::ToUnsigned(last_error_message_.substr(8));
            if (*timeout >= time_limit.getRemainingTime())
                return true;
            else {
                last_error_message_.clear();
                return false;
            }
        } else { // A timeout for more complex reasons.  We're going to claim the page is not in the cache to force
                 // a reload.
            last_error_message_.clear();
            return false;
        }
    }

    return is_cached;
}


bool CachedPageFetcher::retrieveFromCache(const std::string &url, std::string * const message_header,
                                          std::string * const message_body) const
{
    last_error_message_.clear();
    message_body->clear();
    message_header->clear();

    // Retrieve the header(s) from the database
    requireDbConnection();
    const std::string escaped_url(UrlToCacheKey(url));
    const std::string SELECT_STMT("SELECT status, http_header, compressed_document_source, "
                                  "LENGTH(compressed_document_source) AS compressed_document_source_size "
                                  "FROM " + CreateCacheTableName(page_cache_schema_name_) + " WHERE url='"
                                  + escaped_url + "'");
    db_connection_->queryOrDie(SELECT_STMT);
    ++CachedPageFetcher::no_of_queries_;
    DbResultSet result_set(db_connection_->getLastResultSet());
    if (unlikely(result_set.empty()))
        Error("in CachedPageFetcher::retrieveFromCache: unexpected empty result set for query: " + SELECT_STMT);

    DbRow row(result_set.getNextRow());
    if (row[0] !="ok") {
        last_error_message_ = row[0];
        return false;
    }

    if (row.size() < 2)
        return false;

    *message_header = row[1];

    if (row.size() > 2) {
        const long compressed_document_source_size(StringUtil::ToNumber(row[3]));
        if (compressed_document_source_size > 1) {
            std::string compressed_document_source(row[2]);
            *message_body = GzStream::DecompressString(SqlUtil::Unescape(&compressed_document_source));
        }
    }

    return true;
}


// IsCached -- Static member that *actually* checks the database for a URL.
//
bool CachedPageFetcher::IsCached(const std::string &url,
                                 const CachedPageFetcher::RobotsDotTxtOption robots_dot_txt_option,
				 std::string * const error_message, DbConnection * const db_connection)
{
    ReadIniFile();

    std::string escaped_url(UrlToCacheKey(url));
    db_connection->queryOrDie("SELECT NOW(), expiration_datetime, status, honor_robots_dot_txt FROM " +
                              CreateCacheTableName(page_cache_schema_name_) + " WHERE url='" + escaped_url + "'");
    ++CachedPageFetcher::no_of_queries_;

    DbResultSet result_set(db_connection->getLastResultSet());
    if (result_set.empty()) // If we don't have an entry, the URL's data are definitely not cached.
        return false;
    DbRow row(result_set.getNextRow());

    // Only consider records to be cached if they have not expired!
    const std::string current_database_datetime(row[0]);
    const std::string expiration_datetime(row[1]);
    bool is_cached(current_database_datetime < expiration_datetime);

    if (is_cached) {
        // Only consider records to be cached if they have the same robots.txt option as the requested one:
        const RobotsDotTxtOption cached_robots_dot_txt_option(BoolToRobotsDotTxtOption(row[3]));
        if (cached_robots_dot_txt_option != robots_dot_txt_option)
            is_cached = false;
    }

    // If the record is expired, delete it!
    if (not is_cached)
        db_connection->queryOrDie("DELETE FROM " + CreateCacheTableName(page_cache_schema_name_) + " WHERE url='"
                                  + escaped_url + "'");
    else if (error_message != nullptr) {
        const std::string status(row[2]);
        if (status != "ok")
            *error_message = status;
    }

    return is_cached;
}


bool CachedPageFetcher::IsCached(const std::string &url, const RobotsDotTxtOption robots_dot_txt_option,
                                 std::string * const error_message, const ConnectionOption connection_option)
{
    ReadIniFile();

    if (connection_option == KEEP_DB_CONNECTION_OPEN) {
        static DbConnection *static_db_connection(nullptr);
        if (static_db_connection == nullptr) {
            ++CachedPageFetcher::no_of_new_connections_;
            static_db_connection = new DbConnection(page_cache_database_name_, server_user_name_, server_password_,
                                                    server_host_name_, server_port_);
        }

        return CachedPageFetcher::IsCached(url, robots_dot_txt_option, error_message, static_db_connection);
    } else {
        // Create an independent DbConnection because this is a static member function.
        ++CachedPageFetcher::no_of_new_connections_;
        DbConnection stack_db_connection(page_cache_database_name_, server_user_name_, server_password_,
                                         server_host_name_, server_port_);
        return CachedPageFetcher::IsCached(url, robots_dot_txt_option, error_message, &stack_db_connection);
    }
}


void CachedPageFetcher::StoreInCache(const std::string &url, const std::string &http_header,
                                     const std::string &http_body, const time_t retrieval_datetime,
				     const RobotsDotTxtOption robots_dot_txt_option,
                                     const ConnectionOption connection_option)
{
    if (verbosity_ >= 5 and logger_ != nullptr)
        logger_->log("Storing: '%s', %zu bytes", url.c_str(), http_body.size());

    // Read config file?
    if (unlikely(server_host_name_.empty()))
        CachedPageFetcher::ReadIniFile();

    const std::string sql_retrieval_datetime(SqlUtil::TimeTToDatetime(retrieval_datetime));
    const HttpHeader header(http_header);

    // Get the header's expire time if possible:
    std::string sql_expiration_datetime;
    time_t expiration_time(-1);
    if (header.expiresIsValid()) {
        const time_t now(std::time(nullptr));
        if (now < header.getExpires())
            expiration_time = header.getExpires() - now;

        // Make sure we keep the page for at least the specified minimum time:
        if (expiration_time < static_cast<time_t>(minimum_expiration_time_))
            expiration_time = minimum_expiration_time_;

        sql_expiration_datetime = SqlUtil::TimeTToDatetime(header.getExpires());
    }

    const std::string status("ok");
    const std::string etag(header.getETag());

    if (connection_option == KEEP_DB_CONNECTION_OPEN) {
        static DbConnection *static_db_connection = nullptr;
        if (static_db_connection == nullptr) {
            ++CachedPageFetcher::no_of_new_connections_;
            static_db_connection = new DbConnection(page_cache_database_name_, server_user_name_, server_password_,
                                                    server_host_name_, server_port_);
        }

        CachedPageFetcher::ActualStoreInCache(UrlToCacheKey(url), sql_retrieval_datetime, sql_expiration_datetime,
                                              status, robots_dot_txt_option, static_db_connection, 0, "", http_body,
                                              http_header, etag);
    } else {
        // Create an independent DbConnection because this is a static member function.
        ++CachedPageFetcher::no_of_new_connections_;
        DbConnection stack_db_connection(page_cache_database_name_, server_user_name_, server_password_,
                                         server_host_name_, server_port_);
        CachedPageFetcher::ActualStoreInCache(UrlToCacheKey(url), sql_retrieval_datetime, sql_expiration_datetime,
                                              status, robots_dot_txt_option, &stack_db_connection, 0, "", http_body,
                                              http_header, etag);
    }
}


// requireDbConnection -- ensure we have a valid database connection
//
void CachedPageFetcher::requireDbConnection() const {
    if (db_connection_.get() == nullptr) {
        db_connection_.reset(new DbConnection(page_cache_database_name_, server_user_name_, server_password_,
                                              server_host_name_, server_port_));
        ++CachedPageFetcher::no_of_new_connections_;
    }
}


void CachedPageFetcher::actualStoreInCache(const std::string &escaped_url, const std::string &retrieval_datetime,
                                           const std::string &expiration_datetime, const std::string &status,
                                           const unsigned redirect_count, const std::string &redirected_url,
					   const std::string &document_source, const std::string &http_header,
                                           const std::string &etag)
{
    // Make sure we never end up here if we don't want to store in the cache:
    if (unlikely(not useCache()))
        Error("in CachedPageFetcher::actualStoreInCache: called but useCache() returned false!");

    requireDbConnection();

    CachedPageFetcher::ActualStoreInCache(escaped_url, retrieval_datetime, expiration_datetime, status,
                                          params_.robots_dot_txt_option_, db_connection_.get(), redirect_count,
                                          redirected_url, document_source, http_header, etag);

    ++CachedPageFetcher::no_of_queries_;
}


std::string CachedPageFetcher::getCacheIdByUrl(const std::string &url) {
    std::string escaped_url("E'" + url + "'");
    db_connection_->queryOrDie("SELECT url_id FROM " + getCacheTableName() + " WHERE url="+ escaped_url);
    DbResultSet result_set(db_connection_->getLastResultSet());
    if (result_set.empty())
        return "-1";
    DbRow row(result_set.getNextRow());
    return row[0];
}


void CachedPageFetcher::ActualStoreInCache(const std::string &escaped_url, const std::string &retrieval_datetime,
                                           const std::string &expiration_datetime, const std::string &status,
                                           const RobotsDotTxtOption robots_dot_txt_option,
                                           DbConnection * const db_connection, const unsigned redirect_count,
                                           const std::string &redirected_url, const std::string &document_source,
					   const std::string &http_header, const std::string &etag)
{
    if (redirect_count > 0 and redirected_url.empty())
        throw std::runtime_error("in CachedPageFetcher::ActualStoreInCache: redirect count > 0 but no redirected "
                                 "URL provided!");

    std::string escaped_redirected_url(redirected_url);
    SqlUtil::EscapeBlob(&escaped_redirected_url);

    // Compress the document source:
    std::string compressed_document_source;
    if (not document_source.empty()) {
        if (unlikely(document_source.size() > maximum_document_size_))
            throw std::runtime_error("in CachedPageFetcher::ActualStoreInCache: uncompressed document size "
                                     + std::to_string(document_source.size())
                                     + " exceeds maximum allowed size of " + std::to_string(maximum_document_size_)
                                     + "!");

        compressed_document_source = GzStream::CompressString(document_source);
    }


    const std::string cache_table_name(CreateCacheTableName(page_cache_schema_name_));
    const std::string redirect_table_name(CreateRedirectTableName(page_cache_schema_name_));
    const std::string query_by_url("SELECT cache_id FROM " + redirect_table_name + " WHERE url=E'" + escaped_url
                                   + "'");

    std::string insert_stmt("INSERT INTO " + cache_table_name
                            + "(url, retrieval_datetime, expiration_datetime, honor_robots_dot_txt, etag, "
                            "http_header, status, compressed_document_source, uncompressed_document_source_size) "
                            "VALUES (");
    insert_stmt += "E'" + (escaped_redirected_url.empty() ? escaped_url : escaped_redirected_url) + "', ";
    insert_stmt += " '" + retrieval_datetime + "', ";
    insert_stmt += " '" + expiration_datetime + "', ";
    insert_stmt += " '" + RobotsDotTxtOptionToBool(robots_dot_txt_option) + "', ";
    insert_stmt += " '" + db_connection->escapeString(etag) + "', ";
    insert_stmt += " '" + db_connection->escapeString(http_header) + ", ";

    // Insert the status message (max length = 255 chars):
    insert_stmt += " '" + db_connection->escapeString(status.length() < 255 ? status : status.substr(0, 250) + "...")
                   + "', ";

    insert_stmt += " " + SqlUtil::EscapeBlob(&compressed_document_source) + ", "
                   + std::to_string(document_source.size()) +")";
    insert_stmt += " RETURNING url_id";

    std::string update_stmt("UPDATE " + cache_table_name + " SET ");
    update_stmt += "retrieval_datetime='" + retrieval_datetime + "', ";
    update_stmt += "expiration_datetime='" + expiration_datetime + "', ";
    update_stmt += "honor_robots_dot_txt='" + RobotsDotTxtOptionToBool(robots_dot_txt_option) +"', ";
    update_stmt += "etag='" + db_connection->escapeString(etag) + "', ";
    update_stmt += "http_header='" + db_connection->escapeString(http_header) + "', ";

    // Update the status message (max length = 255 chars):
    update_stmt += "status='"
                   + db_connection->escapeString(status.length() < 255 ? status : status.substr(0, 250) + "...")
                   + "', ";

    update_stmt += "compressed_document_source='" + compressed_document_source + "', ";
    update_stmt += "uncompressed_document_source_size=" + std::to_string(document_source.size());

    SqlUtil::TransactionGuard transaction_guard(db_connection);
    db_connection->queryOrDie(query_by_url);
    DbResultSet result_set(db_connection->getLastResultSet());
    std::string url_id;
    if (result_set.empty()) {
        if (logger_ != nullptr)
            logger_->log(insert_stmt);

        db_connection->queryOrDie(insert_stmt);
        DbResultSet result_set2(db_connection->getLastResultSet());
        DbRow current_index(result_set2.getNextRow());
        url_id = current_index[0];
        AddUrlToRedirectTable(db_connection, escaped_url, url_id);
    } else { // Need to replace the database row.
        DbRow row(result_set.getNextRow());
        url_id = row[0];
        update_stmt += " WHERE url_id=" + url_id;
        if (logger_ != nullptr)
            logger_->log(update_stmt);
        db_connection->queryOrDie(update_stmt);
    }

    if (not redirected_url.empty())
        AddUrlToRedirectTable(db_connection, escaped_redirected_url, url_id);
}


void CachedPageFetcher::AddUrlToRedirectTable(DbConnection * const db_connection, const std::string &escaped_url,
                                              const std::string &url_id)
{
    const std::string escaped_and_quoted_url("E'" + escaped_url + "'");
    const std::string redirect_table_name(CreateRedirectTableName(page_cache_schema_name_));
    const std::string cache_id_query("SELECT cache_id FROM " + redirect_table_name + " WHERE url="
                                     + escaped_and_quoted_url);
    SqlUtil::TransactionGuard guard(db_connection);
    try {
        db_connection->queryOrDie(cache_id_query);
        DbResultSet result_set(db_connection->getLastResultSet());
        if (not result_set.empty()) {
            DbRow row(result_set.getNextRow());
            const std::string cache_id(row[0]);
            if (unlikely(cache_id != url_id)) {
                throw std::runtime_error("in CachedPageFetcher::AddUrlToRedirectTable:1: found a URL \""
                                         + escaped_url + "\" whose cache_id \"" + cache_id
                                         + "\" does not match our current url_id \"" + url_id + "\"!");
            } else
                return; // We already have this URL with a matching cache ID.
        } else {
            // Check to see if it was just created
            db_connection->queryOrDie(cache_id_query);
            DbResultSet result_set2(db_connection->getLastResultSet());
            if (not result_set2.empty()) {
                DbRow row(result_set2.getNextRow());
                const std::string cache_id(row[0]);
                if (unlikely(cache_id != url_id)) {
                    throw std::runtime_error("in CachedPageFetcher::AddUrlToRedirectTable:2: found a URL \""
                                             + escaped_url + "\" whose cache_id \"" + cache_id
                                             + "\" does not match our current url_id \"" + url_id + "\"!");
                }
                else
                    return; // We already have this URL with a matching cache ID.
            }

            // Now add the new (url,url_id) pair to the redirect table:
            const std::string redirect_stmt("INSERT INTO "+ CreateRedirectTableName(page_cache_schema_name_)
                                            + " (url, cache_id) VALUES(E'" + escaped_url + "', " + url_id + ")");
            if (logger_ != nullptr)
                logger_->log(redirect_stmt);
            db_connection->queryOrDie(redirect_stmt);
        }
    } catch (std::runtime_error &e) {
        if (logger_ != nullptr){
            logger_->log(std::string(e.what()));
            logger_->log("ROLLBACK");
        }
        guard.rollback();
    }
}

/** \file    CachedPageFetcher.h
 *  \brief   Declaration of class CachedPageFetcher.
 *  \author  Dr. Johannes Ruscheinski
 *  \author  Dr. Gordon W. Paynter
 */

/*
 *  \copyright 2002-2009 Project iVia.
 *  \copyright 2002-2009 The Regents of The University of California. // 
 *  \copyright 2017 Universitätsbibliothek Tübingen
 *
 *  This file is part of the libiViaCore package.
 *
 *  The libiViaCore package is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public License as
 *  published by the Free Software Foundation; either version 2 of the License,
 *  or (at your option) any later version.
 *
 *  libiViaCore is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public License
 *  along with libiViaCore; if not, write to the Free Software Foundation, Inc.,
 *  59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef CACHED_PAGE_FETCHER_H
#define CACHED_PAGE_FETCHER_H


#include <memory>
#include <string>
#include <cinttypes>
#include "CookieJar.h"
#include "DbConnection.h"
#include "PerlCompatRegExp.h"
#include "RobotsDotTxt.h"
#include "SList.h"
#include "TimeLimit.h"


/** \class  CachedPageFetcher
 *  \brief  Download and transparently cache Web pages.
 *
 *  CachedPageFetcher is a class for downloading Web pages (and other documents) from the Internet with HTTP.  It
 *  transparently caches the Web pages in an SQL database if requested.  It also supports the "file:" URI scheme.
 *
 *  \par
 *  If a page cannot be downloaded, an error is reported.  These errors include both network errors (such as not being
 *  able to connect to a host) and errors reported by the HTTP server (such as the good old "404 Not Found" message).
 *  This is slightly different from the behaviour of the PageFetcher class.
 *
 *  \par The page cache
 *  The CachedPageFetcher is stored in a database as defined in the [Page cache] section of the CachedPageFetcher.conf
 *  configuration file if requested.  The page_cache is a MySQL database.
 *
 *  \par Timeout Overrides
 *  Additionally there can be an optional configuration file section called [TimeoutOverrides].  If it exists it
 *  should contain pairs of entries of the form "error_msg_patternXXX=..." and "timeoutXXX=..." where the XXX has to
 *  be unique for each pair.  This allows you to specify timouts in seconds that apply when error messages match the
 *  PCRE following "error_msg_patternXXX" entries.  The algorithm for determining the applicable timeout is to take
 *  the smallest specified timeout.  The set of possible timeouts includes the default timeout specified in
 *  CachedPageFetcher.conf, a timeout provided in an HTTP header and possibly a timeout override due to a matching
 *  error message pattern.
 *
 *  \par Logging
 *  Logging can be specified by providing a non-zero verbosity setting (no larger than 5) and specifying a log
 *  filename.  This can be done by specifying a [Logging] section in the CachedPageFetcher.conf file.  If the
 *  [Logging] section has been provided in the configuration file it must contain "verbosity" and "log_filename"
 *  entries.
 */
class CachedPageFetcher {
public:
    /** \enum   RobotsDotTxtOption
     *  \brief  Controls whether the robots.txt file applies to this fetcher.
     */
    enum RobotsDotTxtOption { CONSULT_ROBOTS_DOT_TXT, IGNORE_ROBOTS_DOT_TXT };

    /** \enum   ConnectionOption
     *  \brief  Controls whether a PostgreSQL database connection may be reserved for this fetcher.
     *
     *  This option cntrols whether the instance maintains a connection to the page_cache for its entire
     *  lifetime (KEEP_DB_CONNECTION_OPEN) or whether it only opens a connection to the database when it needs one
     *  (CLOSE_DB_CONNECTION_WHILE_FETCHING).  In most cases KEEP_DB_CONNECTION_OPEN is correct, but
     *  CLOSE_DB_CONNECTION_WHILE_FETCHING should be used by highly parallelised applications.
     *
     *  \par
     *  When a new URL is requested in KEEP_DB_CONNECTION_OPEN mode, CachedPageFetcher opens a connection to the SQL
     *  database to check whether the page is already cached.  If the URL is not cached, it attempts to download the
     *  Web page.  If the Web page is unavailable, then the HTTP connection and the database connection will be held
     *  open for "timeout" millieconds.  This causes problems in highly parallelised applications if the number of
     *  available database connections is limited. Using the CLOSE_DB_CONNECTION_WHILE_FETCHING closes the database
     *  connection whenever possible, reducing the number of database connections active, but making the page fetcher
     *  slower because closing and opening database connections takes time. By default the PostgreSQL server has a
     *  maximum of 100 simultaneous connections, though this can be changed in the server configuration files.
     */

    enum ConnectionOption {
        KEEP_DB_CONNECTION_OPEN,           //< PostgreSQL connection persists for lifetime of object.
        CLOSE_DB_CONNECTION_WHILE_FETCHING //< PostgreSQL connection is only opened when it is required.
    };

    enum TextTranslationMode {
        TRANSPARENT   = 0, //< If set, perform no character set translations.
        MAP_TO_LATIN9 = 1  //< If set, attempt to convert from whatever to Latin-9.
    };

    // Default configuration options.  These should be in the configuration file.
    static const unsigned    DEFAULT_TIMEOUT       = 10000; // milliseconds
    static const long        DEFAULT_MAX_REDIRECTS = 10;
    static const std::string DEFAULT_ACCEPTABLE_LANGUAGES;

    /** The exact error message that is returned when a download is blocked by robots.txt. */
    static const std::string DENIED_BY_ROBOTS_DOT_TXT_ERROR_MSG;

    /** \struct Params
     *  \brief  The combined parameters of the CachedPageFetcher class.
     */
    struct Params {
        std::string user_agent_;
        long max_redirects_;
        std::string acceptable_languages_;
        RobotsDotTxtOption robots_dot_txt_option_;
        ConnectionOption connection_option_;
        bool use_cache_;
        unsigned max_http_equiv_redirects_;
        TextTranslationMode text_translation_mode_;
    public:
        /** \brief  Construct a set of parameters for a CachedPageFetcher object.
         *  \param  user_agent                 The user agent to report to the Web server.
         *  \param  max_redirects              The maximum permitted redrects to follow.
         *  \param  acceptable_languages       A comma-separated list of acceptable languages (as per the Acceptable
         *                                     -Language HTTP header).
         *  \param  robots_dot_txt_option      Should the robots.txt file be consulted?
         *  \param  connection_option          Should the database connection to the page cache be held open?
         *  \param  use_cache                  Controls page cache usage before and after downloading a Web page.
         *  \param  max_http_equiv_redirects   The max number of times to follow an http-equiv redirect.
         */
        explicit Params(const std::string user_agent = DefaultUserAgent(),
                        const long max_redirects = DEFAULT_MAX_REDIRECTS,
                        const std::string acceptable_languages = DEFAULT_ACCEPTABLE_LANGUAGES,
                        const RobotsDotTxtOption robots_dot_txt_option = CONSULT_ROBOTS_DOT_TXT,
                        const ConnectionOption connection_option = KEEP_DB_CONNECTION_OPEN,
                        const bool use_cache = true,
                        const unsigned max_http_equiv_redirects = 1,
                        const TextTranslationMode text_translation_mode = TRANSPARENT)
        : user_agent_(user_agent), max_redirects_(max_redirects), acceptable_languages_(acceptable_languages),
          robots_dot_txt_option_(robots_dot_txt_option), connection_option_(connection_option), use_cache_(use_cache),
          max_http_equiv_redirects_(max_http_equiv_redirects), text_translation_mode_(text_translation_mode) { }
    };
private:
    /** The parameters of this cached page fetcher instance. */
    Params params_;

    /** A connection to the page cache database. */
    mutable std::unique_ptr<DbConnection> db_connection_;

    std::string last_url_;
    mutable std::string last_error_message_;
    unsigned last_error_code_;

    /** The robots.txt file for the site we are accessing (or last accessed). */
    RobotsDotTxt robots_dot_txt_;

    std::string media_type_;
    std::string message_body_;
    std::vector<std::string> message_headers_;

    CookieJar cookie_jar_;

    // Configurable parameters read from the configuration file:

    struct TimeoutOverride {
        PerlCompatRegExp reg_exp_;
        unsigned timeout_; // In seconds.
    public:
        TimeoutOverride(const std::string &reg_exp, const unsigned timeout): reg_exp_(reg_exp), timeout_(timeout) { }
    };

    class TimeoutOverrides: public SList<TimeoutOverride> {
        unsigned default_timeout_; // In seconds.
    public:
        void setDefaultTimeout(const unsigned default_timeout) { default_timeout_ = default_timeout; }

        /** \brief  Calculates a timeout based on the type of error message.
         *  \param  error_message             The message for which we want to determine a timeout.
         *  \param  default_timeout_override  If non-zero, the timeout to use when none of the patterns match
         *                                    (in seconds).  If zero we'll use the timeout set with the last call
         *                                    to setDefaultTimeout().
         *  \return An SQL Datetime.
         *  \note   This function will never return a value that is greater then the default timeout unless it is
         *          -1, meaning not set.
         */
        std::string getTimeoutForError(const std::string &error_message, const long default_timeout_override = -1)
            const;
    };

    /** The list of special page cache timeouts for certain types of errors. */
    static TimeoutOverrides timeout_overrides_;

    /** The SQL server where the page_cache is hosted. */
    static std::string server_host_name_;

    /** The TCP port of the SQL server where the page_cache is hosted. */
    static uint16_t server_port_;

    /** The username to use to connect to the SQL server. */
    static std::string server_user_name_;

    /** The username to use to connect to the SQL server. */
    static std::string server_password_;

    /** The name of the database containing the page cache schema.
     *  If it has an '=' in it, then it is treated as a connection string by SQL.
     */
    static std::string page_cache_database_name_;

    /** The name of the schema containing the page cache tables. */
    static std::string page_cache_schema_name_;

    /** The page cache expiration time to use if none is specified (in seconds). */
    static unsigned default_expiration_time_;

    /** The minimum page cache expiration time (in seconds). */
    static unsigned minimum_expiration_time_;

    /** The maximum size of document that may be saved in the cache. Longer documents are truncated. */
    static unsigned maximum_document_size_;

    /** The HTTP proxy, or empty if none is in use. */
    static std::string http_proxy_;

    /** The package name for use in the default 'User Agent' string. */
    static std::string default_user_agent_package_;

    /** The URL for use in the default 'User Agent' string. */
    static std::string default_user_agent_url_;

    /** Count the number of connections and queries made. */
    static unsigned no_of_new_connections_, no_of_queries_;

    /** The verbosity setting for logging purposes (should be between 0 and 5 inclusive). */
    static unsigned verbosity_;
public:
    /** \brief  Create a CachedPageFetcher object to download a URL.
     *  \param  url          The URL of the page to download.
     *  \param  params       The combined CachedPageFetcher parameters.
     *  \param  time_limit   A TimeLimit object that defines a timeout
     */
    explicit CachedPageFetcher(const std::string &url, const TimeLimit &time_limit = DEFAULT_TIMEOUT,
                               const Params &params = Params());

    /** \brief  Create a CachedPageFetcher object for use with newUrl.
     *  \param  params       The combined CachedPageFetcher parameters.
     */
    explicit CachedPageFetcher(const Params &params = Params());

    /** \brief  Download a new URL using an exising CachedPageFetcher object.
     *  \param  url          The new URL to download.
     *  \param  time_limit   A TimeLimit object
     */
    void newUrl(const std::string &url, const TimeLimit &time_limit = DEFAULT_TIMEOUT);

    /** \brief  Download a new URL using an existing CachedPageFetcher object with new parameters.
     *  \param  url         The new URL to download.
     *  \param  params      The new combined CachedPageFetcher parameters.
     *  \param  time_limit  A TimeLimit object
     */
    void newUrl(const std::string &url, const Params &params, const TimeLimit &time_limit = DEFAULT_TIMEOUT);

    void setUseCache(const bool new_use_cache) { params_.use_cache_ = new_use_cache; }
    bool useCache() const { return params_.use_cache_; }

    /** \brief  Get the HTTP message headers of the Web page.
     *  \param  headers  A list that will hold the headers.
     *  \note   There will be more than one header when redirections are encountered.
     */
    void getMessageHeaders(std::vector<std::string> * const headers) const { *headers = message_headers_; }

    /** \brief   Get the HTTP Message Header that directly relates to the web page.
     *  \return  The header as a string
     *  \note    The header directly relating to the string is returned, not the redirecting header.
     */
    std::string getMessageHeader() const { return message_headers_.empty() ? "" : message_headers_.back(); }

    /** \brief  Get the HTTP Message Body of the Web page. */
    std::string getMessageBody() const { return message_body_; }

    /** \brief  Get the number of HTTP redirects followed to reach the page. */
    unsigned getRedirectCount() const;

    /** \brief   Get the URL of the page after redirection.
     *  \return  The redirected URL (or empty string if no redirection was performed).
     *
     *  This function returns the "redirected" URL for any page we attempted to download where our request was
     *  redirected by the Web server.  If this function is called on a URL that is not redirected, then it returns an
     *  empty string.  If this function is called with a URL that could not be downloaded (i.e. it's status is not
     *  'ok') then it returns nothing and sets the error message.
     */
    std::string getRedirectedUrl() const;

    /** \brief  Returns the last url that was fetched. */
    std::string getCurrentUrl() const { return getRedirectedUrl().empty() ? last_url_ : getRedirectedUrl(); }

    /** \brief   Get a (probably) unique hash of the compressed document source.
     *  \return  A number with a high probability of being unique to the message body, or zero if there is no message
     *           body or the status is not "ok".
     *  \note    Implementation detail: this is currently the compressed message body size.
     */
    unsigned getMessageBodyHash() const;

    /** \brief   Find out if an error was reported during the last download.
     *  \return  True if an error occurred, false if the page was downloaded.
     */
    bool anErrorOccurred() const { return not last_error_message_.empty(); }

    /** \brief   Get the error message reported by the last download.
     *  \return  The error message, or an empty string if there was no error.
     */
    std::string getErrorMsg() const { return last_error_message_; }

    unsigned getLastErrorCode() const { return last_error_code_; }

    /** \brief  Get the User-Agent we are reporting to the Web server when downloading. */
    std::string getUserAgent() const { return params_.user_agent_; }

    /** \brief   Get the current setting of Accept-Language.
     *  \return  The current value for the HTTP \"Accept-language\" header.
     */
    std::string getAcceptableLanguages() const { return params_.acceptable_languages_; }

    /** \brief  Is the Web page associated with the URL currently in the cache?
     *  \param  url		           The URL we're inquiring about.
     *  \param  robots_dot_txt_option  Enum flag for toggling honoring robots.txt files.
     *  \param  error_message          The error message associated with the URL, if previously cached.  "Error
     *                                 messages" equaling "ok" are suppressed.
     *  \param  connection_option      Should the database connection always be kept open?
     *  \return True if the URL is cached, false otherwise.
     */

    static bool IsCached(const std::string &url, const RobotsDotTxtOption robots_dot_txt_option,
                         std::string * const error_message = nullptr,
                         const ConnectionOption connection_option = KEEP_DB_CONNECTION_OPEN);

    /** \brief  Get a cached url_id for a URL.
     *  \param  url The URL we're inquiring about.
     *  \return The url_id if it exists, otherwise -1 on failure.
     */
    std::string getCacheIdByUrl(const std::string &url);

    std::string getCacheTableName() const { return page_cache_schema_name_ + ".cache"; }

    std::string getRedirectTableName() const { return page_cache_schema_name_ + ".redirect"; }

    std::string getAnchorsTableName() const { return page_cache_schema_name_ + ".anchors"; }


    /** \brief  Store a Web page in the cache?
     *  \param  url                   The URL for the page we're storing.
     *  \param  http_header           The HTTP header for the page we're storing.
     *  \param  http_body             The actual page contents to store.
     *  \param  retrieval_datetime    When this page was retrieved.
     *  \param  robots_dot_txt_option Enum for toggling honoring robots.txt files.
     *  \param  connection_option     Should the database connection always be kept open?
     */
    static void StoreInCache(const std::string &url, const std::string &http_header, const std::string &http_body,
                             const time_t retrieval_datetime, const RobotsDotTxtOption robots_dot_txt_option,
                             const ConnectionOption connection_option = KEEP_DB_CONNECTION_OPEN);

    /** \brief   Generate a default "User-Agent" string for a program.
     *  \return  The User-Agent string.
     *  \note    This function is intended for test programs and other unimportant programs; you should supply your
     *           own user agent string wherever possible.
     */
    static std::string DefaultUserAgent();

    /** \brief   Get/Set the "User-Agent" string for a program.
     *  \return  The User-Agent string.
     */
    static std::string GetUserAgentUrl() { return default_user_agent_url_; }

    static void SetUserAgentUrl(const std::string &url) { default_user_agent_url_ = url; }

    /** \brief   Get/Set the "User-Agent" package string for a program.
     *  \return  The User-Agent package string.
     */
    static std::string GetUserAgentPackage() { return default_user_agent_package_; }

    static void SetUserAgentPackage(const std::string &package_name) { default_user_agent_package_ = package_name; }

    /** \brief   Get the maximum size of document that can be cached.
     *  \return  The maximum document size.
     */

    static unsigned GetMaximumDocumentSize() { return maximum_document_size_; }

    static void  SetMaximumDocumentSize(const unsigned document_size) { maximum_document_size_ = document_size; }

    /** \brief   Get the minimum expiration time for a document in the cache.
     *  \return  The minimum expiration time in hours.
     */
    static unsigned GetMinimumExpirationTime() { return minimum_expiration_time_ / 3600; }

    static void  SetMinimumExpirationTime(const unsigned hours) { minimum_expiration_time_ = hours * 3600; }

    /** \brief   Get the default expiration time for a document in the cache.
     *  \return  The default expiration time in hours.
     */
    static unsigned GetDefaultExpirationTime() { return default_expiration_time_ / 3600; }

    static void  SetDefaultExpirationTime(const unsigned hours) { default_expiration_time_ = hours * 3600; }

    static std::string RobotsDotTxtOptionToString(const RobotsDotTxtOption robots_dot_txt_option);
    static std::string RobotsDotTxtOptionToBool(const RobotsDotTxtOption robots_dot_txt_option);

    static RobotsDotTxtOption StringToRobotsDotTxtOption(const std::string &robots_dot_txt_option_as_string);
    static RobotsDotTxtOption BoolToRobotsDotTxtOption(const std::string &robots_dot_txt_option_as_bool);

    /** \brief  Overrides the defaults for the page cache database "page_cache" and the page cache schema .
     *  \param  server_host_name          The database server's hostname.
     *  \param  server_port               The database server's TCP port number.
     *  \param  server_user_name          The user name for the database server.
     *  \param  server_password           The password for the "server_user_name".
     *  \param  page_cache_database_name  The name for the database that will contain the cache.  If this database
     *                                    does not exist, an attempt will be made to create it.
     *  \param  page_cache_schema_name    The name for the schema that will contain the cache and redirect tables.
     *                                    If this table does exist, an attempt will be made to first drop it.
     *  \param  error_message             If this function returns false a descriptive text explaining the reason for
     *                                    the failure will be written here.
     *  \return True, if the optional creation of a new page cache database and the creation of a page cache table
     *          succeeded, else false.
     */
    static bool CreatePageCacheDatabaseTable(const std::string &server_host_name, const uint16_t server_port,
                                             const std::string &server_user_name,
                                             const std::string &server_password,
                                             const std::string &page_cache_database_name,
                                             const std::string &page_cache_schema_name,
                                             std::string * const error_message);

    /** \brief  Overrides the defaults for the page cache database "page_cache" and the page cache table "cache".
     *  \param  server_host_name          The database server's hostname.
     *  \param  server_port               The database server's TCP port number.
     *  \param  server_user_name          The user name for the database server.
     *  \param  server_password           The password for the "server_user_name" user.
     *  \param  page_cache_database_name  The name for the database that will contain the cache.  If this database
     *                                    does not exist, an attempt will be made to create it.
     *  \param  page_cache_schema_name    The name for the schema that will contain the cache.  If this table does
     *                                    not exist, an attempt will be made to create it.
     *  \param  error_message             If this function returns false a descriptive text explaining the reason for
     *                                    the failure will be written here.
     *  \return True, if the optional creation of a new page cache database and the creation of a page cache table
     *          succeeded, else false.
     */
    static bool SetPageCacheDatabaseTable(const std::string &server_host_name, const uint16_t server_port,
                                          const std::string &server_user_name,
                                          const std::string &server_password,
                                          const std::string &page_cache_database_name,
                                          const std::string &page_cache_schema_name,
                                          std::string * const error_message);
private:
    CachedPageFetcher(const CachedPageFetcher &rhs);           // Disallow copy constructor invocations!
    CachedPageFetcher operator=(const CachedPageFetcher &rhs); // Disallow assignment operator invocations!

    /** \brief  Initialise various parameters from the CachedPageFetcher.conf file.
     *  \note   Must be called by all constructors and almost all static member functions.
     */
    static void ReadIniFile();

    void getRobotsDotTxtForUrl(const class Url &url, const TimeLimit &time_limit, const long max_redirects);

    bool accessAllowed(const std::string &url, const TimeLimit &time_limit, const long max_redirects);
    void retrieveDocument(const std::string &url, const TimeLimit &time_limit, const long max_redirects);
    void readDocumentFromFileSystem(const std::string &url);
    void processProxy(std::string * const proxy_host, unsigned short * const proxy_port);
    bool httpHeaderChecksOut(const HttpHeader &http_header, const std::string &original_url,
                             const Url &redirected_url, const std::string &retrieval_datetime,
                             const unsigned redirect_count, const bool found_in_cache);
    bool downloadPage(const std::string &url, const TimeLimit &time_limit, const long max_redirects);

    /** \brief  Check whether the message body and header for a given URL are in the cache.
     *  \param  url         The URL the existence of whose associated data we'd like to check for.
     *  \param  time_limit  Only used if the cache contains an entry indication that we could not retrieve a message
     *                      header and body within a given amount of time.  In this case, we only consider the entry
     *                      in the cache to be valid if the original (and therefore cached) timeout value is >=
     *                      time_limit.
     *  \param  timeout     If we accepted a cache entry and it indicated a timeout, "timeout" will be the amount of
     *                      the original timeout value.  This can be used to differentiate the existence of a cached
     *                      header and body from that of a cached timeout failure.  If this function returns true,
     *                      but "*timout" is not zero, we have a cached timeout failure.
     */
    bool foundInCache(const std::string &url, const TimeLimit &time_limit, unsigned * const timeout) const;

    /** \brief  Attempts to retrieve a message body and header from the cache.
     *  \param  url             The URL key for which we'd like to retrieve the HTTP header and body.
     *  \param  message_body    The cached message body.
     *  \param  message_header  The cached message header.
     *  \note   Should preferably only be called if foundInCache() returned true and did not indicate a cached timeout
     *          failure via its "timeout" parameter.
     */
    bool retrieveFromCache(const std::string &url, std::string * const message_header,
                           std::string * const message_body) const;

    void requireDbConnection() const;

    /** Wrapper for the actual store operation to the cache database. */
    void actualStoreInCache(const std::string &escaped_url, const std::string &retrieval_datetime,
                            const std::string &expiration_datetime, const std::string &status,
                            const unsigned redirect_count = 0, const std::string &redirected_url = "",
                            const std::string &document_source = "", const std::string &http_header = "",
                            const std::string &etag = "");

    static void AddUrlToRedirectTable(DbConnection * const db_connection, const std::string &escaped_url,
                                      const std::string &url_id);

    /** Performs the actual store operation to the cache database. */
    static void ActualStoreInCache(const std::string &escaped_url, const std::string &retrieval_datetime,
                                   const std::string &expiration_datetime, const std::string &status,
                                   const RobotsDotTxtOption robots_dot_txt_option, DbConnection * const db_connection,
                                   const unsigned redirect_count = 0, const std::string &redirected_url = "",
                                   const std::string &document_source = "", const std::string &http_header = "",
                                   const std::string &etag = "");

    static bool IsCached(const std::string &url, const RobotsDotTxtOption robots_dot_txt_option,
                         std::string * const error_message, DbConnection * const db_connection);
};


#endif // ifndef CACHED_PAGE_FETCHER_H

/** \file    Url.h
 *  \brief   Declaration of class Url, the swiss-army knife of URL wrappers.
 *  \author  Dr. Johannes Ruscheinski
 *  \author  Jiangtao Hu
 */

/*
 *  Copyright 2003-2008 Project iVia.
 *  Copyright 2003-2008 The Regents of The University of California.
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

#ifndef URL_H
#define URL_H


#include <fstream>
#include <list>
#include <stdexcept>
#include <string>
#include <cassert>


/** \class  Url
 *  \brief  A class representing a URL (or more accurately: a URL reference).
 *
 *  This class provides facilities for manipulating a URL, e.g. fixing it if it is invalid, making it absolute if it is
 *  relative and a base URL has been provided and for extracting various components of the URL.
 *
 *  Urls can be tested for "validity" and "made valid".  This tests whether they comply with the URL syntax standards
 *  (with a few practical modifications).  Url's can also be "made clean" or "cleaned up", which means that a series of
 *  harmless transformations are applied to the URL to improve it's appearance.
 *
 *  Finally, Urls have a "canonical" form; a Url can be "canonized" or "made canonical" which means it is transformed
 *  into an equivalent form that is stylistically consistent accross the database.  For example, trailing slashes are
 *  removed, and hostanmes are prefixed with "www".  However, these canonizations occur *iff* the resulting URL links to
 *  the same content as the original URL.  This is tested by looking up both URLs over the Internet, which can be time-
 *  consuming, so in some situations it is better to use a URL which is merely "clean" or "valid" instead. It can also
 *  result in Url errors if your network connection goes down.
 *
 *  Note that when a URL is canonized, it is necessarily also cleaned.
 */
class Url {
    friend bool operator==(const Url &lhs, const Url &rhs);
    friend bool operator==(const std::string &lhs, const Url &rhs);
    friend bool operator==(const Url &lhs, const std::string &rhs);

    /** The principle form of the URL. */
    std::string url_;
    /** A reference URL which, if non-empty, can be used to make relative URLs absolute. */
    std::string default_base_url_;
public:
    /** To canonize a URL, mulitple versions may be downloaded.
     *  We can either consult or ignore robots.txt access control files.*/
    enum RobotsDotTxtOption { CONSULT_ROBOTS_DOT_TXT, IGNORE_ROBOTS_DOT_TXT };

    /** The default timeout_ value in milliseconds. */
    enum { DEFAULT_TIMEOUT = 3000 };
private:
    /** Will the robots.txt file be consulted or ignored? */
    RobotsDotTxtOption robots_dot_txt_option_;

    /** The time allowed for any network access operation. */
    unsigned timeout_;

    /** The user agent string to be used by the Downloader when we attempt to make a URL canonical. */
    std::string user_agent_;

    // URL component variables.  These are only initialised if state_ has the HAS_BEEN_PARSED bit set.
    mutable std::string scheme_, username_password_, authority_, port_, path_, params_, query_, fragment_;

    /** \brief  The content of a relative URL.
     *  If the URL is relative it is stored here, and the scheme_,
     *  username_password_, and other component variables muct be
     *  empty; if any of the other components are set then this is
     *  not a relative URL and this field must be empty.
     */
    mutable std::string relative_url_;

    enum StateFlags { UNINITIALISED = 0, IS_VALID = 1, VALIDITY_HAS_BEEN_CHECKED = 2, HAS_BEEN_PARSED = 4, MADE_VALID_ATTEMPTED = 8,
                      CANONIZATION_ATTEMPTED = 16, HAS_BEEN_MADE_ABSOLUTE = 32, HAS_BEEN_CLEANED_UP = 64, IS_CANONICAL = 128,
                      AN_ERROR_OCCURRED = 256 };

    /** \enum   StateFlags
     *  \brief  The possible states of the object, represented by bit flags.
     *  \note   These constants must be distinct powers of two and fit
     *          into an unsigned.  (Caution: UNINITIALISED is not really a bit flag!)
     */

    /** The current state of the object, represented by ORed StateFlags. */
    mutable unsigned state_;

    /** The error message (if any) reported by the last operation.
     *  Only meaningful if the AN_ERROR_OCCURED bit is set in state_.
     */
    mutable std::string error_message_;

    bool throw_exceptions_; // Enable error reporting by throwing exceptions if "true."

    static std::string default_user_agent_;
public:
    /** Constructor flag for performing no automatic operations. */
    enum { NO_AUTO_OPERATIONS     = 1u << 0u };

    /** Constructor flag for attempting to automatically make an invalid URL valid. */
    enum { AUTO_MAKE_VALID        = 1u << 1u };

    /** Constructor flag for automatically performing a safe, fast cleanup on the URL. */
    enum { AUTO_CLEAN_UP          = 1u << 2u };

    /** Constructor flag for automatically converting a URL to canonical form. */
    enum { AUTO_CANONIZE          = 1u << 3u };

    /** Constructor flag for automatically making a relative URL absolute when required. */
    enum { AUTO_MAKE_ABSOLUTE     = 1u << 4u };

    /** Constructor flag for disabling exception throwing. */
    enum { THROW_EXCEPTIONS       = 1u << 5u };

    /** Constructor flag to force interpreting a URL as absolute, an attempt is made to turn the URL into an absolute HTTP URL. */
    enum { FORCE_ABSOLUTE_HTTP_URL = 1u << 6u };

    /** Constructor flag to remove the fragment part of a URL if present. */
    enum { REMOVE_FRAGMENT         = 1u << 7u };
public:
    /** \brief  Construct am empty Url object. */
    Url();

    /** \brief  Construct a URL from a reference URL and relative path.
     *  \param  url                    The initial form of the URL.
     *  \param  default_base_url       A reference URL, only used when "url" is relative.
     *  \param  creation_flags         A set of operations to perform on "url" during construction.
     *  \param  robots_dot_txt_option  Whether to consult or ignore robots.txt files.
     *  \param  timeout                The total time allowed for any network access operation.
     *  \param  user_agent             The user agent string to be used by the Downloader when we attempt to make a URL canonical.
     *
     *  \note   If you pass in the AUTO_CANONIZE flag you must ensure that you've either previously set a default user agent string with
     *          SetDefaultUserAgentString() or you pass in a non-empty "user_agent" parameter value.
     */
    Url(const std::string &url, const std::string &default_base_url, const unsigned creation_flags = AUTO_MAKE_VALID | AUTO_MAKE_ABSOLUTE,
        const RobotsDotTxtOption robots_dot_txt_option = CONSULT_ROBOTS_DOT_TXT, const unsigned timeout = DEFAULT_TIMEOUT,
        const std::string &user_agent = "");

    /** \brief  Construct a Url object from a URL.
     *  \param  url                    The initial form of the URL.
     *  \param  creation_flags         A set of operations to perform on "url" during construction.
     *  \param  robots_dot_txt_option  Whether to consult or ignore robots.txt files.
     *  \param  timeout                The total time allowed for any network access operation.
     *  \param  user_agent             The user agent string to be used by the Downloader when we attempt to make a URL canonical.
     *
     *  \note   If you pass in the AUTO_CANONIZE flag you must ensure that you've either previously set a default user agent string with
     *          SetDefaultUserAgentString() or you pass in a non-empty "user_agent" parameter value.
     */
    explicit Url(const std::string &url, const unsigned creation_flags = AUTO_MAKE_VALID,
                 const RobotsDotTxtOption robots_dot_txt_option = CONSULT_ROBOTS_DOT_TXT, const unsigned timeout = DEFAULT_TIMEOUT,
                 const std::string &user_agent = "");

    /** \brief  Construct a URL from its component parts.
     *  \param  scheme                 The URL's scheme component.
     *  \param  username_password      The URL's password component.
     *  \param  authority              The URL's authority component.
     *  \param  port                   The URL's port component.
     *  \param  path                   The URL's path component.
     *  \param  params                 The URL's params component.
     *  \param  query                  The URL's query component.
     *  \param  fragment               The URL's fragment component.
     *  \param  creation_flags         A set of operations to perform on "url" during construction.
     *  \param  robots_dot_txt_option  Whether to consult or ignore robots.txt files.
     *  \param  timeout                The total time allowed for any network access operation in milliseconds.
     *  \param  user_agent             The user agent string to be used by the Downloader when we attempt to make a URL canonical.
     *
     *  \note   If you pass in the AUTO_CANONIZE flag you must ensure that you've either previously set a default
     *          user agent string with SetDefaultUserAgentString() or you pass in a non-empty "user_agent" parameter
     *          value.
     */
    Url(const std::string &scheme, const std::string &username_password, const std::string &authority, const std::string &port, const std::string path,
        const std::string &params = "", const std::string query = "", const std::string &fragment = "",
        const unsigned creation_flags = AUTO_MAKE_VALID, const RobotsDotTxtOption robots_dot_txt_option = CONSULT_ROBOTS_DOT_TXT,
        const unsigned timeout = DEFAULT_TIMEOUT, const std::string &user_agent = "");

    /** \brief  Factory method for constructing a Url in canonical form.
     *
     *  \note   You must ensure that you've either previously set a default user agent string with
     *          SetDefaultUserAgentString() or you pass in a non-empty "user_agent" parameter value.
     */
    static Url CreateCanonicalUrl(const std::string &url, const std::string &base_url = "",
                                  const RobotsDotTxtOption robots_dot_txt_option = CONSULT_ROBOTS_DOT_TXT,
                                  const unsigned timeout = DEFAULT_TIMEOUT, const std::string &user_agent = "")
    {
        return Url(url, base_url, AUTO_MAKE_VALID | AUTO_CANONIZE, robots_dot_txt_option, timeout, user_agent);
    }

    /** \brief  Factory method for constructing a Url in canonical form.
     *
     *  \note   You must ensure that you've either previously set a default user agent string with
     *          SetDefaultUserAgentString() or you pass in a non-empty "user_agent" parameter value.
     */
    static Url CreateCanonicalUrl(const std::string &url, const RobotsDotTxtOption robots_dot_txt_option = CONSULT_ROBOTS_DOT_TXT,
                                  const unsigned timeout = DEFAULT_TIMEOUT, const std::string &user_agent = "")
    { return Url(url, AUTO_MAKE_VALID | AUTO_CANONIZE, robots_dot_txt_option, timeout, user_agent); }

    /** Allow automatic type conversion to "std::string". */
    operator std::string() const { return url_; }

    /** Allow forced type conversion to "std::string". */
    const std::string &toString() const { return url_; }

    /** Allow forced type conversion to C-style strings. */
    const char *c_str() const { return url_.c_str(); }

    enum HashBehaviour { IGNORE_LEADING_WWW, DO_NOT_IGNORE_LEADING_WWW };

    /** \brief  Generate a hash value from a URL.  Ignores any part of a URL starting at a hash mark. */
    uint64_t getHash(const HashBehaviour hash_behaviour = IGNORE_LEADING_WWW) const;

    /** \brief  Generate a hash value from a URL Authority(Domain name). */
    uint64_t getAuthorityHash() const;

    /** \brief  Returns the number of non-empty components of a URL's path. */
    unsigned getPathLength() const;

    /** \brief  Returns true if an error occurred since the object was created. */
    bool anErrorOccurred() const { return state_ & AN_ERROR_OCCURRED; }

    bool empty() const { return url_.empty(); }

    /** Usually it makes sense to call this only after anErrorOccurred() returned true. */
    std::string getErrorMsg() const { return error_message_; }

    /** \brief  Test to see if the URL is valid. */
    bool isValid() const
    {
        if (state_ & AN_ERROR_OCCURRED)
            return false;
        if (not (state_ & VALIDITY_HAS_BEEN_CHECKED))
            internalIsValid();
        return state_ & IS_VALID;
    }

    /** \brief  Test to see if the URL is a valid HTTP or HTTPS URL. */
    bool isValidWebUrl() const;

    /** \brief  Test to see if the URL is canonical. */
    bool isCanonical()
    {
        if (state_ & AN_ERROR_OCCURRED)
            return false;
        if (not (state_ & CANONIZATION_ATTEMPTED))
            makeCanonical();
        return state_ & IS_CANONICAL;
    }

    /** \brief   Quickly find a simple "cleaned up" version of a URL.
     *  \return  True if the URL has been successfully cleaned up, otherwise false.
     *
     *  This function quickly performs a set of safe, fast URL transformations.  It does NOT require Internet lookups.
     *  Cleanup guarantees that the resulting URL remains valid iff the original was valid.
     */
    bool cleanUp();

    /** \brief  Converts all URL components to lowercase. */
    void toLower();

    /** \brief   Make the URL valid.
     *  \return  True if the URL is now a valid URL, otherwise false.
     */
    bool makeValid();

    /** \brief   Make the URL canonical.
     *  \param   override_timeout  If non-zero, override the default download timeout variable (in milliseconds).
     *  \return  True if the URL is now a canonical URL, otherwise false.
     */
    bool makeCanonical(const unsigned override_timeout = 0 /* in milliseconds */);

    /** \brief  Test to see if the URL is an absolute URL. */
    bool isAbsolute() const
    {
        if (state_ & AN_ERROR_OCCURRED)
            return false;
        if (not (state_ & HAS_BEEN_PARSED))
            parseUrl(url_);
        return (not scheme_.empty() and scheme_ != "http" and scheme_ != "https") or relative_url_.empty();
    }

    /** \brief  Test to see if the URL is a relative URL. */
    bool isRelative() const
    {
        if (state_ & AN_ERROR_OCCURRED)
            return false;
        if (not (state_ & HAS_BEEN_PARSED))
            parseUrl(url_);
        return not relative_url_.empty();
    }

    /** \brief   Make the URL absolute relative to its base URL.
     *  \param   override_base_url  If non-empty use this as the reference URL, otherwise use the
     *                              default base URL that was passed into a constructor.
     *  \return  True if the URL is now an absolute URL, otherwise false.
     */
    bool makeAbsolute(const std::string &override_base_url = "");

    /** \brief  Generate a RobotsDotTxt URL based on this Url object. */
    std::string getRobotsDotTxtUrl() const;

    /** \brief  Is this a robots.txt url? */
    bool isRobotsDotTxtUrl() const { return path_ == "/robots.txt"; }

    /** \brief   Is the URL blacklisted by the local configuration?
     *  \return  True if the URL is invalid or has been locally blacklisted, otherwise false.
     *
     *  This function is used to see if a valid HTTP URL has been blacklisted by the local configuration.  Note that if the
     *  URL is not valid, or is not an "http" or "https" URL, then this function returns true (i.e. invalid URLs are
     *  blacklisted by default).
     *
     *  \note  URL blacklisting is controlled by the UrlBlacklist.conf configuration file in the iViaCore etc
     *         directory.  These contain regular expressions for identifying blacklisted (and non-blacklisted) URLs and are
     *         compared against the current URL.
     */
    bool isBlacklisted() const;

    /** Get the path of the URL Blacklisting configuration file. */
    static std::string UrlBlacklistConfFile();

    /** \brief   Transform a URL into a preferred format based on local configuration.
     *  \return  True if the URL was changed.
     *
     *  This function is to transform valid HTTP URLs into
     *  preferred forms.  It was originally intended for
     *  transforming URLs that refer to a mirror site into URLs
     *  that point to the original, though its uses are more
     *  general.
     *
     *  \note  After a URL is transformed, there is o guarentee that it is still valid.
     *
     *  \note This function calls Url::TransformUrl().  See its documentation for more information.
     */
    bool transform();

    /** \brief   Test whether a URL is a child or sibling of this Url.
     *  \param   test_url       The potential child/sibling being compared.
     *  \return  True if both Urls are valid, and "test_url" is a child or sibling of the current Url.
     */
    bool isChildOrSiblingUrl(const Url &test_url) const;

    /** \brief   Test whether a URL is a child or sibling of this Url.
     *  \param   test_url       The potential child/sibling being compared.
     *  \return  True if both Urls are valid, and "test_url" is a child or sibling of the current Url.
     */
    bool isChildOrSiblingUrl(const std::string &test_url) const { return isChildOrSiblingUrl(Url(test_url)); }


    /** Get the URL's scheme component as a string. */
    std::string getScheme() const
    {
        if (state_ & AN_ERROR_OCCURRED)
            return "";
        if (not (state_ & HAS_BEEN_PARSED))
            parseUrl(url_);

        return scheme_;
    }

    /** Get the URL's username/password component as a string. */
    std::string getUsernamePassword() const
    {
        if (state_ & AN_ERROR_OCCURRED)
            return "";
        if (not (state_ & HAS_BEEN_PARSED))
            parseUrl(url_);

        return username_password_;
    }

    /** \brief  Get the URL's authority (domain name) component as a string.
     *
     *  This is not strictly what RFC2396 calls the "authority".
     *  That definition includes the optional username/password
     *  component as well.
     */
    std::string getAuthority() const
    {
        if (state_ & AN_ERROR_OCCURRED)
            return "";
        if (not (state_ & HAS_BEEN_PARSED))
            parseUrl(url_);

        return authority_;
    }

    /** \brief   Get the URL's port component as an unsigned integer.
     *  \return  The port number
     *  \note    If the port is undefined, and there is a default
     *           port for this scheme, then the default port wil be
     *           returned.
     */
    unsigned short getPort() const;

    /** \brief   Get the URL's port component as a string.
     *  \param   get_default_when_port_not_set  If the port was not specified, use the default port for this scheme (if known).
     *  \return  The port number
     */
    std::string getPortAsString(const bool get_default_when_port_not_set = false) const;

    /** Get the URL's path component as a string. */
    std::string getPath() const
    {
        if (state_ & AN_ERROR_OCCURRED)
            return "";
        if (not (state_ & HAS_BEEN_PARSED))
            parseUrl(url_);

        return path_;
    }

    /** Get the URL's pamaters component as a string. */
    std::string getParams() const
    {
        if (not (state_ & HAS_BEEN_PARSED))
            parseUrl(url_);

        return params_;
    }

    /** Get the URL's query component as a string. */
    std::string getQuery() const
    {
        if (state_ & AN_ERROR_OCCURRED)
            return "";
        if (not (state_ & HAS_BEEN_PARSED))
            parseUrl(url_);

        return query_;
    }

    /** Get the URL's fragment component as a string. */
    std::string getFragment() const
    {
        if (state_ & AN_ERROR_OCCURRED)
            return "";
        if (not (state_ & HAS_BEEN_PARSED))
            parseUrl(url_);

        return fragment_;
    }

    /** \brief   Returns the `site' part of the URL.  Assumes that this Url is absolute.
     *  \return  The `site' part of this Url or the empty string if no `site' could be
     *           identified.
     *
     *  \note    `Site' is a loosely defined inofficial concept based on a set of heuristics.
     *           It is recommended you only call this on "clean" URLs.
     */
    std::string getSite() const;

    /** \brief   Set the URL's scheme component as a string.
     *  \param   scheme  The new scheme component.
     *  \return  True if the scheme was successfully updated.
     */
    bool setScheme(const std::string &scheme);

    /** \brief   Set the URL's username_password component as a string.
     *  \param   username_password  The new username_password component.
     *  \return  True if the username_password was successfully updated.
     */
    bool setUsernamePassword(const std::string &username_password);

    /** \brief   Set the URL's authority component as a string.
     *  \param   authority  The new authority component.
     *  \return  True if the authority was successfully updated.
     */
    bool setAuthority(const std::string &authority);

    /** \brief   Set the URL's port component as a string.
     *  \param   port  The new port component.
     *  \return  True if the port was successfully updated.
     */
    bool setPort(const std::string &port);

    /** \brief   Set the URL's port component as a string.
     *  \param   port  The new port component.
     *  \return  True if the port was successfully updated.
     */
    bool setPort(const unsigned short port);

    /** \brief   Set the URL's path component as a string.
     *  \param   path  The new path component.
     *  \return  True if the path was successfully updated.
     */
    bool setPath(const std::string &path);

    /** \brief   Set the URL's params component as a string.
     *  \param   params  The new params component.
     *  \return  True if the params was successfully updated.
     */
    bool setParams(const std::string &params);

    /** \brief   Set the URL's query component as a string.
     *  \param   query  The new query component.
     *  \return  True if the query was successfully updated.
     */
    bool setQuery(const std::string &query);

    /** \brief   Set the URL's fragment component as a string.
     *  \param   fragment  The new fragment component.
     *  \return  True if the fragment was successfully updated.
     */
    bool setFragment(const std::string &fragment);

    /** \brief   Assemble a URL given the components.
     *  \param   scheme             The protocol.
     *  \param   username_password  If nonempty something like this: username\@password.
     *  \param   authority          Typically a hostname, e.g. www.cyber-dyne.com.
     *  \param   port               An optional numeric port.
     *  \param   path               An optional path.
     *  \param   params             Optional parameters.
     *  \param   query              An optional query part.
     *  \param   fragment           An optional HTTP/HTTPS fragment.
     *  \return  The URL that has been assembled from the individual components.
     */
    static std::string MakeUrl(const std::string &scheme, const std::string &username_password,
                               const std::string &authority, const std::string &port,
                               const std::string &path, const std::string &params,
                               const std::string &query, const std::string &fragment);

    /** \brief   Transform a URL from one form to another based local configuration.
     *  \param   url  The URL to transform.
     *  \return  The mapped URL if there was a match, otherwise the original URL.
     *
     *  This function uses the mappings stored in UrlTransforms.conf to
     *  transform urls.  The configuration file conatins an ordered list
     *  of substitution patterns in perl-style regular experssion format.
     *
     *  When TransformUrl is called with a particular URL, the URL is compared
     *  to each substituion pattern in in order.  Each subsititution
     *  pattern is applied to each URL until one of the subsitution
     *  patterns causes the URL to change, or until the end of the file is
     *  reached.
     */
    static std::string TransformUrl(const std::string &url);

    /** \brief   Suggest possible canonical forms of a URL.
     *  \param   original_url        A valid URL that may not be canonical.
     *  \param   potential_urls      Will hold a list of possible canonical forms, in order of decreasing preference.
     *  \param   resolve_ip_address  If true, IP addresses will be converted to host names.
     *  \return  False if the original URL was not valid or could not be made valid, otherwise true.
     *
     *  This function suggests "better" forms of a URL, for use in canonization.  The "most canonical" suggestion will
     *  be made first, the "least canonical" last; the "least canonical" URL will always be the original URL (or a
     *  version of the original in which only safe transformations have been made).
     *
     *  No HTTP connections will be made, but a DNS lookup will be performed if the hostname is an IP address and
     *  "resolve_ip_addresses" is true.
     */
    static bool SuggestPotentialCanonicalUrls(const std::string &original_url,
                                              std::list<std::string> * const potential_urls,
                                              const bool resolve_ip_address = false);

    /** \brief   Suggest simple, common variant forms of a URL
     *  \param   url           The URL to check.
     *  \param   url_list      Will hold the variant URLs.
     *  \param   canonize_url  If true "url" will first be canonized before additional processing takes place.
     *  \return  The number of variants.
     *
     *  This function returns some likely simple alternate forms of a Web URL; if any of these variants is in the
     *  database already, we can assume the URL is a duplicate.  The URL itself is always returned.
     */
    static unsigned SuggestLikelyAlternateFormsForWebUrl(const std::string &url, std::list<std::string> * const url_list,
                                                         const bool canonize_url = true);

    /** Sets the default user agent string for Url::makeCanonical(). */
    static void SetDefaultUserAgentString(const std::string &new_default_user_agent_string)
    { default_user_agent_ = new_default_user_agent_string; }
private:
    /** \brief   Force "url_" to be an absolute HTTP URL if it is not relative.
     *  \return  True if we came up with something sensible and false if the relative "url_" did not start out
     *           with a valid hostname.
     */
    bool forceAbsoluteHttp();

    bool internalIsValid() const;
    bool parseUrl(const std::string &url) const;

    /** \brief    Helper function to set the url_ data member from the component parts.
     *  \param    new_state  The new state_ value, which will be ORed with HAS_BEEN_PARSED.
     *  \note     This function assumes that url_ has been parsed.
     *  \warning  Be very careful about setting the state explicitly!
     *            We want to keep whatever state we know to be
     *            correct, but claiming too much state can force the object
     *            into an inconsistent state.
     */
    void setUrlFromComponents(const unsigned new_state = UNINITIALISED) {
        assert(state_ & HAS_BEEN_PARSED);
        url_ = Url::MakeUrl(scheme_, username_password_, authority_, port_,
                            path_, params_, query_, fragment_);
        state_ = new_state | HAS_BEEN_PARSED;
    }

    void error(const std::string &error_message) const {
        error_message_ = error_message;
        state_ |= AN_ERROR_OCCURRED;

        if (throw_exceptions_)
            throw std::runtime_error(error_message);
    }
};


extern bool operator==(const Url &lhs, const Url &rhs);
inline bool operator==(const std::string &lhs, const Url &rhs) { return operator==(Url(lhs), rhs); }
inline bool operator==(const Url &lhs, const std::string &rhs) { return operator==(lhs, Url(rhs)); }


inline bool operator!=(const Url &lhs, const Url &rhs) {
    return not operator==(lhs, rhs);
}


inline bool operator!=(const std::string &lhs, const Url &rhs) {
    return not operator==(lhs, rhs);
}


inline bool operator!=(const Url &lhs, const std::string &rhs) {
    return not operator==(lhs, rhs);
}


inline std::ostream &operator<<(std::ostream &stream, const Url &url) {
    stream << url.toString();
    return stream;
}


#endif // ifndef URL_H

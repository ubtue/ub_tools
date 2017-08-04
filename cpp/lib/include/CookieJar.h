/** \file    CookieJar.h
 *  \brief   Declaration of class CookieJar.
 *  \author  Dr. Johannes Ruscheinski
 */

/*
 *  \copyright 2007 Project iVia.
 *  \copyright 2007 The Regents of The University of California.
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

#ifndef COOKIE_JAR_H
#define COOKIE_JAR_H


#include <string>
#include <unordered_map>
#include <ctime>
#include "StringUtil.h"


// Forward declartion:
class HttpHeader;


/** \class  CookieJar
 *  \brief  Implements a class representing HTTP cookies.
 */
class CookieJar {
public:
    struct Cookie {
        std::string name_;
        std::string value_;
        std::string comment_;
        std::string comment_url_;
        std::string version_;
        std::string domain_;
        std::string request_host_;
        std::string port_;
        std::string path_;
        std::string cookies_supported_;
        bool secure_, discard_, http_only_;
        time_t expiration_time_;
    public:
        explicit Cookie(const std::string &name = "", const std::string &value = "")
            : name_(name), value_(value), path_(""), secure_(false), discard_(false), http_only_(false)
            { expiration_time_ = std::time(nullptr) + 86400; /* One day from now. */ }
        Cookie(const std::string &name, const std::string &value, const std::string &version,
               const std::string &domain, const std::string &path, const time_t expiration_time)
            : name_(name), value_(value), version_(version), domain_(domain), path_(path), secure_(false),
              discard_(false), expiration_time_(expiration_time) { }
        bool empty() const { return name_.empty(); }
        std::string getCookieHeader() const;
        std::string getKey() const { return StringUtil::ToLower(name_) + " " + domain_ + " " + path_; }
        std::string toString() const;
        bool setDomain(const std::string &domain, const std::string &request_host);
    };

    typedef std::unordered_map<std::string, Cookie>::iterator iterator;
    typedef std::unordered_map<std::string, Cookie>::const_iterator const_iterator;
private:
    mutable std::unordered_map<std::string, Cookie> cookies_;
public:
    CookieJar() { }
    CookieJar(const HttpHeader &http_header, const std::string &default_domain)
        { addCookies(http_header, default_domain); }

    bool empty() const { return cookies_.empty();}
    size_t size() const { return cookies_.size(); }

    void addCookie(const std::string &name, const std::string &value, const std::string &version,
                   const std::string &domain, const std::string &path, const time_t expiration_time)
        { addCookie(Cookie(name, value, version, domain, path.empty() ? "/" : path, expiration_time)); }
    void addCookies(const HttpHeader &http_header, const std::string &default_domain);

    /** \brief  Generates the "Cookie:" headers for a given domain name and path.
     *  \param  domain_name     The FQDN for which we'd like all relevant cookies.
     *  \param  path            The path on "domain_name" for which we'd like all relevant cookies.
     *  \param  cookie_headers  After the call, this will point to the generated cookie headers.  May be empty if
     *                          no matches were found.
     */
    void getCookieHeaders(const std::string &domain_name, const std::string &path, std::string * const cookie_headers)
        const;

    const_iterator begin() const { return cookies_.begin(); }
    const_iterator end() const { return cookies_.end(); }
private:
    void parseCookie(const std::string &raw_cookie, const std::string &default_domain = "");
    void addCookie(const Cookie &cookie) { cookies_.insert(std::make_pair(cookie.getKey(), cookie)); }

    /** Comparison function for std::sort(). */
    static bool PathCompare(const Cookie &cookie1, const Cookie &cookie2) {
        // More specific paths must come first:
        return cookie1.path_.length() > cookie2.path_.length();
    }
};


inline std::ostream &operator<<(std::ostream &output, const CookieJar::Cookie &cookie) {
    output << cookie.toString();
    return output;
}


#endif // ifndef COOKIE_JAR_H

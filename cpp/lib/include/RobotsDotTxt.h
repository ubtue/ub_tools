/** \file    RobotsDotTxt.h
 *  \brief   Declaration of the RobotsDotTxt class.
 *  \author  Dr. Johannes Ruscheinski
 */

/*
 *  Copyright 2002-2009 Project iVia.
 *  Copyright 2002-2009 The Regents of The University of California.
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

#ifndef ROBOTS_DOT_TXT_H
#define ROBOTS_DOT_TXT_H


#include <mutex>
#include <unordered_map>
#include <cinttypes>
#include <string>
#include <ctime>
#include <HtmlParser.h>
#include <SList.h>
#include <ThreadUtil.h>
#include <Url.h>


// Forward declaration(s):
//class DbConnection;


/** \class  RobotsDotTxt
 *  \brief  Attempts to implement the behaviour as specified by http://www.robotstxt.org/wc/norobots-rfc.html.
 */
class RobotsDotTxt {
    bool initialized_;
    enum RuleType { ALLOW, DISALLOW };

    class Rule {
        RuleType rule_type_;
        std::string path_prefix_;
    public:
        Rule(const RuleType rule_type, const std::string &path_prefix);
        bool match(const std::string &path) const;
        RuleType getRuleType() const { return rule_type_; }
        std::string toString() const { return (rule_type_ == ALLOW ? "Allow: " : "Disallow: ") + path_prefix_; }
    };

    class UserAgentDescriptor {
        SList<std::string> user_agent_patterns_;
        SList<Rule> rules_;
        unsigned crawl_delay_;
    public:
        UserAgentDescriptor(): crawl_delay_(0) { }
        void addUserAgent(const std::string &user_agent_pattern)
        { user_agent_patterns_.push_back(user_agent_pattern); }
        void addRule(const RuleType rule_type, const std::string &value);
        void setCrawlDelay(const unsigned new_crawl_delay) { crawl_delay_ = new_crawl_delay; }
        bool match(const std::string &user_agent_string) const;
        const SList<Rule> &getRules() const { return rules_; }
        unsigned getCrawlDelay() const { return crawl_delay_; }
        void copyRules(const UserAgentDescriptor &from);
        size_t getNoOfUserAgentPatterns() const { return user_agent_patterns_.size(); }
        void clear() { user_agent_patterns_.clear();  rules_.clear();  crawl_delay_ = 0; }
        std::string toString() const;
    };
    SList<UserAgentDescriptor> user_agent_descriptors_;
public:
    /** \brief  Constructs a RobotsDotTxt object.
     *  \note   If you use this constructor you must call "reinitialize" at some later point.
     */
    RobotsDotTxt(): initialized_(false) { }

    /** \brief  Constructs a RobotsDotTxt object.
     *  \param  robots_dot_txt  The contents of a Web server's "robots.txt" file.
     */
    explicit RobotsDotTxt(const std::string &robots_dot_txt) { reinitialize(robots_dot_txt); }

    /** Empty all rules.  (After this access to every URL will be allowed.) */
    void clear() { user_agent_descriptors_.clear(); }

    void swap(RobotsDotTxt &other);

    /** \brief   Checks access rights for a given user-agent and path.
     *  \param   user_agent  A string used by the caller to identify itself to the Web server whose
     *                       robots.txt file the current object represents.
     *  \param   path        The path we'd like to access.
     *  \return  True if "user_agent" is allowed to access "path", else false.
     *  \note    The pattern matching for the user agent is case insensitive!
     */
    bool accessAllowed(const std::string &user_agent, const std::string &path) const;

    /** \brief   Checks access rights for a given user-agent and URL.
     *  \param   user_agent  A string used by the caller to identify itself to the Web server whose
     *                       robots.txt file the current object represents.
     *  \param   url         A URL on the same site as this robots.txt file.
     *  \return  True if "user_agent" is allowed to access the URL, else false.
     *  \note    The pattern matching for the user agent is case insensitive!
     */
    bool accessAllowed(const std::string &user_agent, const Url &url) const {
        if (not url.isAbsolute())
            throw std::runtime_error("in RobotsDotTxt::accessAllowed: cannot check acces for a relative URL!");
        return accessAllowed(user_agent, url.getPath());
    }

    /** \brief  Returns the crawl delay specified in a robots.txt file or 0 for no specified crawl delay. */
    unsigned getCrawlDelay(const std::string &user_agent) const;

    /** \brief  Resets the access rules based on a new robots.txt document.
     *  \param  robots_dot_txt  The contents of a Web server's "robots.txt" file.
     */
    void reinitialize(const std::string &robots_dot_txt);

    std::string toString() const;

    /** The RobotsDotTxt object returned by this factory method will allways return false from accessAllowed(). */
    static RobotsDotTxt BlockEverythingFactory() { return RobotsDotTxt("User-agent: *\nDisallow: /\n"); }
};


/** \class  RobotsMetaTagExtractor
 *  \brief  Looks for "robots" meta tags and extracts index/noindex, follow/nofollow, and archive/noarchive information.
 */
class RobotsMetaTagExtractor: private HtmlParser {
    bool index_, follow_, archive_;
public:
    explicit RobotsMetaTagExtractor(const std::string &html_document);
    bool index() const { return index_; }
    bool follow() const { return follow_; }
    bool archive() const { return archive_; }
private:
    virtual void notify(const Chunk &chunk);
};


/** \class  RobotsDotTxtCache
 *  \brief  Implements a cache of robots.txt objects as a threadsafe singleton.
 */
class RobotsDotTxtCache {
    unsigned max_cache_size_;
    std::unordered_map<std::string, RobotsDotTxt *> hostname_to_robots_dot_txt_map_;
    static std::mutex mutex_;
    static RobotsDotTxtCache *the_singleton_;
public:
    void clear();

    /** \brief  Associates a robots.txt entry with a hostname.
     *  \param  new_hostname        The hostname (key) to associate with "new_robots_dot_txt."
     *  \param  new_robots_dot_txt  The contents of a robots.txt file.
     */
    void insert(const std::string &new_hostname, const std::string &new_robots_dot_txt);

    /** Adds a robots.txt reference for the robots.txt associated with "original_hostname".  Throws an
        exception if no robots.txt entry can be found for "original_hostname." */
    void addAlias(const std::string &original_hostname, const std::string &new_hostname);

    bool hasHostname(const std::string &hostname) const;

    unsigned getMaxCacheSize() const { return max_cache_size_; }
    void setMaxCacheSize(const unsigned new_max_cache_size);

    /** Returns the RobotsDotTxt for "hostname" or nullptr if "hostname" has not been found. */
    const RobotsDotTxt *getRobotsDotTxt(const std::string &hostname) const;

    static RobotsDotTxtCache &GetInstance();
private:
    RobotsDotTxtCache(): max_cache_size_(10000) { }
    ~RobotsDotTxtCache() { clear(); }

    RobotsDotTxtCache(const RobotsDotTxtCache &rhs) = delete;
    const RobotsDotTxtCache &operator=(const RobotsDotTxtCache &rhs) = delete;

    void nonThreadSafeClear();
};


#endif // ifndef ROBOTS_DOT_TXT_H

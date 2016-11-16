/** \file    RobotsDotTxt.cc
 *  \brief   Implementation of class RobotsDotTxt.
 *  \author  Dr. Johannes Ruscheinski
 */

/*
 *  Copyright 2002-2009 Project iVia.
 *  Copyright 2002-2009 The Regents of The University of California.
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

#include "RobotsDotTxt.h"
#include <mutex>
#include <sstream>
#include <unordered_set>
#include <cstring>
#include <Compiler.h>
#include <DbConnection.h>
#include <DbRow.h>
#include <StringUtil.h>


namespace {


std::string CanonizePath(const std::string &non_canonical_path) {
    std::string canonical_path;
    canonical_path.reserve(non_canonical_path.length());

    for (std::string::const_iterator ch(non_canonical_path.begin()); ch != non_canonical_path.end(); ++ch) {
        if (unlikely(*ch == '%')) {
            ++ch;
            if (unlikely(ch == non_canonical_path.end())) {
                canonical_path += '%';
                break;
            }

            char first_hex_char(*ch);
            if (unlikely(not isxdigit(first_hex_char))) {
                canonical_path += '%';
                canonical_path += first_hex_char;
                continue;
            }

            ++ch;
            if (unlikely(ch == non_canonical_path.end())) {
                canonical_path += '%';
                canonical_path += first_hex_char;
                break;
            }

            char second_hex_char(*ch);
            if (unlikely(not isxdigit(second_hex_char))) {
                canonical_path += '%';
                canonical_path += first_hex_char;
                canonical_path += second_hex_char;
                continue;
            }

            first_hex_char  = static_cast<char>(toupper(first_hex_char));
            second_hex_char = static_cast<char>(toupper(second_hex_char));

            // Don't replace the escaped slash with an actual slash!
            if (unlikely(first_hex_char == '2' and first_hex_char == 'F'))
                canonical_path += "%2F";
            else
                canonical_path += static_cast<char>((StringUtil::FromHex(first_hex_char) << 4)
                                  | StringUtil::FromHex(second_hex_char));
        } else
            canonical_path += *ch;
    }

    return canonical_path;
}


} // unnamed namespace


RobotsDotTxt::Rule::Rule(const RuleType rule_type, const std::string &path_prefix)
    : rule_type_(rule_type), path_prefix_(CanonizePath(path_prefix)) { }


bool RobotsDotTxt::Rule::match(const std::string &path) const {
    const std::string canonical_path(CanonizePath(path));

    if (path_prefix_.empty())
        return true;

    if (::strncasecmp(path_prefix_.c_str(), canonical_path.c_str(), path_prefix_.length()) == 0)
        return true;

    return false;
}


void RobotsDotTxt::UserAgentDescriptor::addRule(const RuleType rule_type, const std::string &value) {
    if (unlikely(value.empty())) {
        switch (rule_type) {
        case ALLOW:
            rules_.push_back(Rule(DISALLOW, "/"));
            return;
        case DISALLOW:
            rules_.push_back(Rule(ALLOW, "/"));
            return;
        default:
            throw std::runtime_error("in RobotsDotTxt::UserAgentDescriptor::addRule: don't know how to handle rule type ("
                            + StringUtil::ToString(rule_type) + ")!");
        }
    }

    rules_.push_back(Rule(rule_type, value));
}


bool RobotsDotTxt::UserAgentDescriptor::match(const std::string &user_agent_string) const {
    for (const auto &pattern : user_agent_patterns_) {
        if (pattern == "*" or ::strncasecmp(pattern.c_str(), user_agent_string.c_str(), pattern.length()) == 0)
            return true;
    }

    return false;
}


void RobotsDotTxt::UserAgentDescriptor::copyRules(const UserAgentDescriptor &from) {
    for (SList<Rule>::const_iterator rule(from.rules_.begin()); rule != from.rules_.end(); ++rule)
        rules_.push_back(*rule);
}


std::string RobotsDotTxt::UserAgentDescriptor::toString() const {
    std::string user_agent_descriptor_as_string;

    // User-agent:
    for (const auto &user_agent_pattern : user_agent_patterns_)
        user_agent_descriptor_as_string += "User-agent: " + user_agent_pattern + "\n";

    // Crawl-delay:
    if (crawl_delay_ != 0)
        user_agent_descriptor_as_string += "Crawl-delay: " + StringUtil::ToString(crawl_delay_) + "\n";

    // Rules:
    for (SList<Rule>::const_iterator rule(rules_.begin()); rule != rules_.end(); ++rule)
        user_agent_descriptor_as_string += rule->toString() + "\n";

    return user_agent_descriptor_as_string;
}


void RobotsDotTxt::swap(RobotsDotTxt &other) {
    if (likely(this != &other)) {
        std::swap(initialized_, other.initialized_);
        user_agent_descriptors_.swap(other.user_agent_descriptors_);
    }
}


bool RobotsDotTxt::accessAllowed(const std::string &user_agent, const std::string &path) const {
    // Always allow access to the robots.txt file:
    if (::strcasecmp("/robots.txt", path.c_str()) == 0)
        return true;

    for (SList<UserAgentDescriptor>::const_iterator user_agent_descriptor(user_agent_descriptors_.begin());
         user_agent_descriptor != user_agent_descriptors_.end(); ++user_agent_descriptor)
    {
        if (user_agent_descriptor->match(user_agent)) {
            for (SList<Rule>::const_iterator rule(user_agent_descriptor->getRules().begin());
                 rule != user_agent_descriptor->getRules().end(); ++rule)
                {
                    if (rule->match(path))
                        return rule->getRuleType() == ALLOW;
                }

            // If we make it here it means we had a match on the user-agent string and must bail out!
            return true;
        }
    }

    // If we made it here we didn't match the wild card user agent descriptor.
    return true;
}


unsigned RobotsDotTxt::getCrawlDelay(const std::string &user_agent) const {
    for (SList<UserAgentDescriptor>::const_iterator user_agent_descriptor(user_agent_descriptors_.begin());
         user_agent_descriptor != user_agent_descriptors_.end(); ++user_agent_descriptor)
    {
        if (user_agent_descriptor->match(user_agent))
            return user_agent_descriptor->getCrawlDelay();
    }

    return 0; // Default: no specified delay found.
}


namespace {


enum LineType { BLANK, COMMENT, GARBAGE, USER_AGENT, RULE, CRAWL_DELAY };


// ParseLine -- break a line from a robots.txt file into its components.  Please note that "rule_type" will only refer
//              to something meaningful if this function returns RULE!  "value" will only have meaning if USER_AGENT or
//              RULE has been returned!
//
LineType ParseLine(std::string line, std::string * const rule_type, std::string * const value) {
    // Remove any comments:
    bool comment_found;
    std::string::size_type hash_pos = line.find('#');
    if (hash_pos == std::string::npos)
        comment_found = false;
    else {
        comment_found = true;
        line = line.substr(0, hash_pos);
    }

    StringUtil::TrimWhite(&line);
    if (line.empty())
        return comment_found ? COMMENT : BLANK;

    const std::string::size_type colon_pos = line.find(':');
    if (colon_pos == std::string::npos or colon_pos == 0)
        return GARBAGE;

    LineType line_type(RULE);
    if (::strcasecmp(line.substr(0, colon_pos).c_str(), "User-agent") == 0)
        line_type = USER_AGENT;
    if (::strcasecmp(line.substr(0, colon_pos).c_str(), "Crawl-delay") == 0)
        line_type = CRAWL_DELAY;

    std::string remainder(colon_pos == line.length() - 1 ? "" : line.substr(colon_pos + 1));
    StringUtil::Trim(&remainder);
    if (remainder.empty() and (line_type == USER_AGENT or line_type == CRAWL_DELAY))
        return GARBAGE;

    *value = remainder;

    if (line_type == RULE)
        *rule_type = line.substr(0, colon_pos);

    return line_type;
}


} // unnamed namespace


void RobotsDotTxt::reinitialize(const std::string &robots_dot_txt) {
    initialized_ = true;

    user_agent_descriptors_.clear();

    std::string rules(robots_dot_txt);

    // Translate carriage returns to newlines and tabs to spaces:
    StringUtil::Map(&rules, "\r\t", "\n ");

    StringUtil::Collapse(&rules, '\n');
    StringUtil::Collapse(&rules, ' ');

    UserAgentDescriptor wild_card_user_agent;
    wild_card_user_agent.addUserAgent("*");
    bool wild_card_seen(false);

    UserAgentDescriptor temp_descriptor;
    enum State { LOOKING_FOR_USER_AGENT, PARSING_RULES } state = LOOKING_FOR_USER_AGENT;

    // Now we process a line at a time:
    std::istringstream lines(rules);
    while (lines) {
        std::string line;
        std::getline(lines, line);

        std::string rule_type, value;
        const LineType line_type = ParseLine(line, &rule_type, &value);
        if (line_type == GARBAGE or line_type == COMMENT)
            continue;

        if (state == LOOKING_FOR_USER_AGENT) {
            if (line_type == USER_AGENT) {
                if (value == "*")
                    wild_card_seen = true;
                else
                    temp_descriptor.addUserAgent(value);
            } else if (line_type == RULE) {
                if (::strcasecmp("Disallow", rule_type.c_str()) == 0)
                    temp_descriptor.addRule(DISALLOW, value);
                else if (::strcasecmp("Allow", rule_type.c_str()) == 0)
                    temp_descriptor.addRule(ALLOW, value);
                state = PARSING_RULES;
            } else if (line_type == CRAWL_DELAY) {
                unsigned crawl_delay;
                if (StringUtil::ToUnsigned(value, &crawl_delay))
                    temp_descriptor.setCrawlDelay(crawl_delay);
                state = PARSING_RULES;
            } else if (line_type == BLANK) { // I believe this should never happen!
                wild_card_seen = false;
                temp_descriptor.clear();
            }
        } else if (state == PARSING_RULES) {
            if (line_type == RULE) {
                state = PARSING_RULES;
                if (::strcasecmp("Disallow", rule_type.c_str()) == 0)
                    temp_descriptor.addRule(DISALLOW, value);
                else if (::strcasecmp("Allow", rule_type.c_str()) == 0)
                    temp_descriptor.addRule(ALLOW, value);
            } else if (line_type == CRAWL_DELAY) {
                unsigned crawl_delay;
                if (StringUtil::ToUnsigned(value, &crawl_delay))
                    temp_descriptor.setCrawlDelay(crawl_delay);
            } else {
                if (temp_descriptor.getNoOfUserAgentPatterns() > 0)
                    user_agent_descriptors_.push_back(temp_descriptor);
                if (wild_card_seen)
                    wild_card_user_agent.copyRules(temp_descriptor);
                wild_card_seen = false;
                temp_descriptor.clear();

                // This should never happen because we should see a BLANK line_type first, but we
                // want to be tolerant.
                if (line_type == USER_AGENT) {
                    if (value == "*")
                        wild_card_seen = true;
                    else
                        temp_descriptor.addUserAgent(value);
                }

                state = LOOKING_FOR_USER_AGENT;
            }
        }
    }

    if (temp_descriptor.getNoOfUserAgentPatterns() > 0)
        user_agent_descriptors_.push_back(temp_descriptor);
    if (wild_card_seen)
        wild_card_user_agent.copyRules(temp_descriptor);

    user_agent_descriptors_.push_back(wild_card_user_agent);
}


std::string RobotsDotTxt::toString() const {
    std::string robots_dot_txt_as_string;
    for (SList<UserAgentDescriptor>::const_iterator user_agent_descriptor(user_agent_descriptors_.begin());
         user_agent_descriptor != user_agent_descriptors_.end(); ++user_agent_descriptor)
    {
        if (user_agent_descriptor != user_agent_descriptors_.begin())
            robots_dot_txt_as_string += "\n\n";
        robots_dot_txt_as_string += user_agent_descriptor->toString();
    }

    return robots_dot_txt_as_string;
}


RobotsMetaTagExtractor::RobotsMetaTagExtractor(const std::string &html_document)
        : HtmlParser(html_document, HtmlParser::OPENING_TAG, true /* = header_only */)
{
        parse();
}


void RobotsMetaTagExtractor::notify(const Chunk &chunk) {
    if (chunk.text_ == "meta") {
        AttributeMap::const_iterator iter = chunk.attribute_map_->find("name");
        if (iter != chunk.attribute_map_->end() and ::strcasecmp(iter->second.c_str(), "robots") == 0) {
            iter = chunk.attribute_map_->find("content");
            if (iter != chunk.attribute_map_->end()) {
                SList<std::string> values;
                const std::string lowercase_content(StringUtil::ToLower(iter->second));
                StringUtil::SplitThenTrim(lowercase_content, ",", " \t\r\f", &values);
                for (SList<std::string>::const_iterator value(values.begin()); value != values.end(); ++value) {
                    if (*value == "index")
                        index_ = true;
                    else if (*value == "noindex")
                        index_ = false;
                    else if (*value == "follow")
                        follow_ = true;
                    else if (*value == "nofollow")
                        follow_ = false;
                    else if (*value == "archive")
                        archive_ = true;
                    else if (*value == "noarchive")
                        archive_ = false;
                }
            }
        }
    }
}


std::mutex RobotsDotTxtCache::mutex_;
RobotsDotTxtCache *RobotsDotTxtCache::the_singleton_(nullptr);


void RobotsDotTxtCache::clear() {
    std::lock_guard<std::mutex> mutex_locker(mutex_);
    nonThreadSafeClear();
}


void RobotsDotTxtCache::insert(const std::string &new_hostname, const std::string &new_robots_dot_txt) {
    std::lock_guard<std::mutex> mutex_locker(mutex_);

    if (unlikely(hostname_to_robots_dot_txt_map_.size() == max_cache_size_))
        nonThreadSafeClear();

    hostname_to_robots_dot_txt_map_.insert(std::make_pair<std::string, RobotsDotTxt *>(StringUtil::ToLower(new_hostname),
                                                                                       new RobotsDotTxt(new_robots_dot_txt)));
}


void RobotsDotTxtCache::addAlias(const std::string &original_hostname, const std::string &new_hostname) {
    std::lock_guard<std::mutex>mutex_locker(mutex_);

    std::unordered_map<std::string, RobotsDotTxt *>::const_iterator entry(
        hostname_to_robots_dot_txt_map_.find(StringUtil::ToLower(original_hostname)));
    if (unlikely(entry == hostname_to_robots_dot_txt_map_.end()))
        throw std::runtime_error("in RobotsDotTxtCache::addAlias: can't add an additional hostname reference for a "
                                 "non-existent entry!");
    hostname_to_robots_dot_txt_map_.insert(std::make_pair(StringUtil::ToLower(new_hostname), entry->second));
}


bool RobotsDotTxtCache::hasHostname(const std::string &hostname) const {
    std::lock_guard<std::mutex> mutex_locker(mutex_);

    return hostname_to_robots_dot_txt_map_.find(hostname) != hostname_to_robots_dot_txt_map_.end();
}


void RobotsDotTxtCache::setMaxCacheSize(const unsigned new_max_cache_size) {
    if (unlikely(new_max_cache_size == 0))
        throw std::runtime_error("in RobotsDotTxtCache::setMaxCacheSize: new_max_cache_size must be greater than zero!");

    std::lock_guard<std::mutex> mutex_locker(mutex_);

    max_cache_size_ = new_max_cache_size;
    if (hostname_to_robots_dot_txt_map_.size() > new_max_cache_size)
        nonThreadSafeClear();
}


const RobotsDotTxt *RobotsDotTxtCache::getRobotsDotTxt(const std::string &hostname) const {
    std::lock_guard<std::mutex> mutex_locker(mutex_);

    std::unordered_map<std::string, RobotsDotTxt *>::const_iterator entry(
        hostname_to_robots_dot_txt_map_.find(StringUtil::ToLower(hostname)));
    return entry == hostname_to_robots_dot_txt_map_.end() ? nullptr : entry->second;
}


RobotsDotTxtCache &RobotsDotTxtCache::GetInstance() {
    std::lock_guard<std::mutex> mutex_locker(mutex_);

    if (the_singleton_ == nullptr)
        the_singleton_ = new RobotsDotTxtCache;

    return *the_singleton_;
}


void RobotsDotTxtCache::nonThreadSafeClear() {
    std::unordered_set<RobotsDotTxt *> already_deleted;
    for (const auto &entry : hostname_to_robots_dot_txt_map_) {
        // Have we deleted the current RobotsDotTxt object yet?
        if (already_deleted.find(entry.second) == already_deleted.end()) {
            already_deleted.insert(entry.second); // Never again!
            delete entry.second;
        }
    }

    hostname_to_robots_dot_txt_map_.clear();
}

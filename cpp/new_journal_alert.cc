/** \file    new_journal_alert.cc
 *  \brief   Detects new journal issues for subscribed users.
 *  \note    Additional documentation can be found at
 * https://github.com/ubtue/ub_tools/wiki/Abonnementservice-f%C3%BCr-Zeitschriftenartikel-in-IxTheo-und-RelBib \author  Dr. Johannes
 * Ruscheinski
 */

/*
    Copyright (C) 2016-2021 Library of the University of Tübingen

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU Affero General Public License as
    published by the Free Software Foundation, either version 3 of the
    License, or (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Affero General Public License for more details.

    You should have received a copy of the GNU Affero General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <iostream>
#include <memory>
#include <sstream>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include "BinaryIO.h"
#include "Compiler.h"
#include "DbConnection.h"
#include "EmailSender.h"
#include "FileUtil.h"
#include "HtmlUtil.h"
#include "IniFile.h"
#include "JSON.h"
#include "KeyValueDB.h"
#include "Solr.h"
#include "StringUtil.h"
#include "Template.h"
#include "TimeUtil.h"
#include "UBTools.h"
#include "VuFind.h"
#include "util.h"


namespace {


[[noreturn]] void Usage() {
    ::Usage("[--debug] [solr_host_and_port] user_type hostname sender_email email_subject\n"
            "  Sends out notification emails for journal subscribers.\n"
            "  Should \"solr_host_and_port\" be missing \"" + Solr::DEFAULT_HOST + ":" + std::to_string(Solr::DEFAULT_PORT) + "\" will be used.\n"
            "  \"user_type\" must be \"ixtheo\", \"relbib\", \"bibstudies\" or \"churchlaw\"."
            "  \"hostname\" should be the symbolic hostname which will be used in constructing\n"
            "  URL's that a user might see.\n"
            "  If \"--debug\" is given, emails will not be sent and database will not be updated.\n");
}


typedef enum journal_subscription_format { MEISTERTASK } journal_subscription_format;


const std::map<journal_subscription_format, std::string> JOURNAL_SUBSCRIPTION_FORMAT_TO_STRING_MAP{ { MEISTERTASK, "meistertask" } };


const std::map<std::string, journal_subscription_format> JOURNAL_SUBSCRIPTION_FORMAT_ENUM_MAP{ { "meistertask", MEISTERTASK } };


struct SerialControlNumberAndMaxLastModificationTime {
    std::string serial_control_number_;
    std::string last_modification_time_;
    bool changed_;

public:
    SerialControlNumberAndMaxLastModificationTime(const std::string &serial_control_number, const std::string &last_modification_time)
        : serial_control_number_(serial_control_number), last_modification_time_(last_modification_time), changed_(false) { }
    inline void setMaxLastModificationTime(const std::string &new_last_modification_time) {
        last_modification_time_ = new_last_modification_time;
        changed_ = true;
    }
    inline bool changed() const { return changed_; }
};


struct NewIssueInfo {
    std::string control_number_;
    std::string series_control_number_;
    std::string series_title_;
    std::string issue_title_;
    std::string volume_, year_, issue_, start_page_;
    std::vector<std::string> authors_;

public:
    NewIssueInfo(const std::string &control_number, const std::string &series_control_number, const std::string &series_title,
                 const std::string &issue_title, const std::string &volume, const std::string &year, const std::string &issue,
                 const std::string &start_page, const std::vector<std::string> &authors)
        : control_number_(control_number), series_control_number_(series_control_number), series_title_(series_title),
          issue_title_(issue_title), volume_(volume), year_(year), issue_(issue), start_page_(start_page), authors_(authors) { }
    bool operator<(const NewIssueInfo &rhs) const;
    bool operator==(const NewIssueInfo &rhs) const;
    bool operator%(const NewIssueInfo &rhs) const; // Has same year, volume and issue
    friend std::ostream &operator<<(std::ostream &output, const NewIssueInfo &new_issue_info);
};


std::ostream &operator<<(std::ostream &output, const NewIssueInfo &new_issue_info) {
    output << new_issue_info.control_number_
           << (new_issue_info.series_control_number_.empty() ? "Missing series PPN*" : new_issue_info.series_control_number_) << ' '
           << (new_issue_info.series_title_.empty() ? "*Missing Series Title*" : new_issue_info.series_title_) << ' '
           << (new_issue_info.issue_title_.empty() ? "*Missing Issue Title*" : new_issue_info.issue_title_) << ' '
           << (new_issue_info.volume_.empty() ? "*Missing Volume*" : new_issue_info.volume_) << ' '
           << (new_issue_info.year_.empty() ? "*Missing Year*" : new_issue_info.year_) << ' '
           << (new_issue_info.issue_.empty() ? "*Missing Issue*" : new_issue_info.issue_) << ' '
           << (new_issue_info.start_page_.empty() ? "*Missing Start Page*" : new_issue_info.start_page_) << ' ' << "Authors: ";
    for (const auto &author : new_issue_info.authors_)
        output << author << ' ';

    return output;
}


bool NewIssueInfo::operator<(const NewIssueInfo &rhs) const {
    if (series_title_ != rhs.series_title_)
        return series_title_ < rhs.series_title_;

    if (year_ != rhs.year_)
        return year_ < rhs.year_;

    if (volume_ != rhs.volume_) {
        if (StringUtil::ConsistsOfAllASCIIDigits(volume_) and StringUtil::ConsistsOfAllASCIIDigits(rhs.volume_))
            return StringUtil::ToUnsigned(volume_) < StringUtil::ToUnsigned(rhs.volume_);
        else
            return volume_ < rhs.volume_;
    }

    if (issue_ != rhs.issue_) {
        if (StringUtil::ConsistsOfAllASCIIDigits(issue_) and StringUtil::ConsistsOfAllASCIIDigits(rhs.issue_))
            return StringUtil::ToUnsigned(issue_) < StringUtil::ToUnsigned(rhs.issue_);
        else
            return issue_ < rhs.issue_;
    }

    if (start_page_ != rhs.start_page_) {
        if (StringUtil::ConsistsOfAllASCIIDigits(start_page_) and StringUtil::ConsistsOfAllASCIIDigits(rhs.start_page_))
            return StringUtil::ToUnsigned(start_page_) < StringUtil::ToUnsigned(rhs.start_page_);
        else
            return start_page_ < rhs.start_page_;
    }

    return false;
}


bool NewIssueInfo::operator==(const NewIssueInfo &rhs) const {
    return control_number_ == rhs.control_number_ and series_title_ == rhs.series_title_ and issue_title_ == rhs.issue_title_
           and volume_ == rhs.volume_ and issue_ == rhs.issue_ and start_page_ == rhs.start_page_ and authors_ == rhs.authors_;
}

// Derive equality only in a certain respect
bool NewIssueInfo::operator%(const NewIssueInfo &rhs) const {
    if (year_ < rhs.year_)
        return true;
    if (rhs.year_ < year_)
        return false;
    if (volume_ < rhs.volume_)
        return true;
    if (rhs.volume_ < volume_)
        return false;
    if (issue_ < rhs.issue_)
        return true;
    if (rhs.issue_ < issue_)
        return false;
    return false;
}


} // namespace

namespace std {
template <>
struct hash<NewIssueInfo> {
    size_t operator()(const NewIssueInfo &i) const {
        // hash method here
        return hash<string>()(i.control_number_);
    }
};
} // namespace std


namespace {


class GenerateEmailContents {
public:
    ~GenerateEmailContents(){};
    virtual std::string generateContent(const std::string &user_type, const std::string &name_of_user, const std::string &language,
                                        const std::string &vufind_host, const std::vector<NewIssueInfo> &new_issue_infos) const = 0;
    virtual std::string generateContent(const std::string &vufind_host, const NewIssueInfo &new_issue_infos) const = 0;
};


class GenerateDefaultEmailContents : public GenerateEmailContents {
public:
    virtual std::string generateContent(const std::string &user_type, const std::string &name_of_user, const std::string &language,
                                        const std::string &vufind_host, const std::vector<NewIssueInfo> &new_issue_infos) const {
        // Generate HTML journal list for email
        std::string list("<ul>\n"); // start journal list
        std::string last_series_title, last_volume_year_and_issue;
        for (const auto &new_issue_info : new_issue_infos) {
            const bool new_serial(new_issue_info.series_title_ != last_series_title);
            if (new_serial) {
                if (not last_series_title.empty()) { // Not first iteration!
                    list += "    </ul>\n";           // end items
                    list += "  </ul>\n";             // end volume/year/issue list
                }
                last_series_title = new_issue_info.series_title_;
                list += "  <li>" + HtmlUtil::HtmlEscape(last_series_title) + "</li>\n";
                list += "  <ul>\n"; // start volume/year/issue list
                last_volume_year_and_issue.clear();
            }

            // Generate "volume_year_and_issue":
            std::string volume_year_and_issue;
            if (not new_issue_info.volume_.empty())
                volume_year_and_issue += new_issue_info.volume_;
            if (not new_issue_info.year_.empty()) {
                if (not volume_year_and_issue.empty())
                    volume_year_and_issue += " ";
                volume_year_and_issue += "(" + new_issue_info.year_ + ")";
            }
            if (not new_issue_info.issue_.empty()) {
                if (not volume_year_and_issue.empty())
                    volume_year_and_issue += ", ";
                volume_year_and_issue += new_issue_info.issue_;
            }

            if (volume_year_and_issue != last_volume_year_and_issue) {
                if (not new_serial)
                    list += "    </ul>\n"; // end items
                list += "    <li>" + HtmlUtil::HtmlEscape(volume_year_and_issue) + "</li>\n";
                last_volume_year_and_issue = volume_year_and_issue;
                list += "    <ul>\n"; // start items
            }

            const std::string URL("https://" + vufind_host + "/Record/" + new_issue_info.control_number_);
            std::string authors;
            for (const auto &author : new_issue_info.authors_)
                authors += "&nbsp;&nbsp;&nbsp;" + HtmlUtil::HtmlEscape(author);
            list += "      <li><a href=\"" + URL + "\">" + HtmlUtil::HtmlEscape(new_issue_info.issue_title_) + "</a>" + authors + "</li>\n";
        }
        list += "    </ul>\n"; // end items
        list += "  </ul>\n";   // end volume/year/issue list
        list += "</ul>\n";     // end journal list


        // Fill mail template
        const auto template_filename_prefix(UBTools::GetTuelibPath() + "new_journal_alert_email.template");
        std::string template_filename(template_filename_prefix + "." + language);
        if (not FileUtil::Exists(template_filename))
            template_filename = template_filename_prefix + ".en";

        const std::string email_template(FileUtil::ReadStringOrDie(template_filename));
        const std::string email_template_prepared = StringUtil::ReplaceString("\n", "<br>\n", email_template);

        Template::Map names_to_values_map;
        names_to_values_map.insertScalar("user_name", name_of_user);
        names_to_values_map.insertScalar("list", list);
        names_to_values_map.insertScalar("system", VuFind::CapitalizedUserType(user_type));
        names_to_values_map.insertScalar("email_reply_to", user_type + "@ub.uni-tuebingen.de");
        const auto email_body(Template::ExpandTemplate(email_template_prepared, names_to_values_map));

        return email_body;
    }


    virtual std::string generateContent(__attribute__((unused)) const std::string &vufind_host,
                                        __attribute__((unused)) const NewIssueInfo &new_issue_infos) const {
        LOG_ERROR(std::string(__FUNCTION__) + " currently not implemented");
    }
};


class GenerateMeistertaskEmailContents : public GenerateEmailContents {
public:
    virtual std::string generateContent(const std::string &vufind_host, const NewIssueInfo &new_issue_info) const {
        std::string email_contents(HtmlUtil::HtmlEscape(new_issue_info.series_title_) + " ");
        // Generate "volume_year_and_issue":
        std::string volume_year_and_issue;
        if (not new_issue_info.volume_.empty())
            volume_year_and_issue += new_issue_info.volume_;
        if (not new_issue_info.year_.empty()) {
            if (not volume_year_and_issue.empty())
                volume_year_and_issue += " ";
            volume_year_and_issue += "(" + new_issue_info.year_ + ")";
        }
        if (not new_issue_info.issue_.empty()) {
            if (not volume_year_and_issue.empty())
                volume_year_and_issue += ", ";
            volume_year_and_issue += new_issue_info.issue_;
        }

        email_contents += HtmlUtil::HtmlEscape(volume_year_and_issue) + "<br/>\n";

        const std::string URL("https://" + vufind_host + "/Record/" + new_issue_info.series_control_number_);
        email_contents += "<a href=\"" + URL + "#details\">" + URL + "</a><br/>\n";
        return email_contents;
    }


    virtual std::string generateContent(__attribute__((unused)) const std::string &user_type,
                                        __attribute__((unused)) const std::string &name_of_user,
                                        __attribute__((unused)) const std::string &language,
                                        __attribute__((unused)) const std::string &vufind_host,
                                        __attribute__((unused)) const std::vector<NewIssueInfo> &new_issue_infos) const {
        LOG_ERROR(std::string(__FUNCTION__) + " currently not implemented");
    }
};


class SendNotificationEmail {
public:
    ~SendNotificationEmail(){};
    virtual void send(const bool debug, const GenerateEmailContents &mail_contents_generator, const std::string &name_of_user,
                      const std::string &language, const std::string &recipient_email, const std::string &vufind_host,
                      const std::string &sender_email, const std::string &email_subject, const std::vector<NewIssueInfo> &new_issue_infos,
                      const std::string &user_type) const = 0;
    virtual void send(const bool debug, const GenerateEmailContents &mail_contents_generator, const std::string &recipient_email,
                      const std::string &vufind_host, const std::string &sender_email,
                      const std::vector<NewIssueInfo> &new_issue_infos) const = 0;
};


class SendDefaultNotificationEmail : public SendNotificationEmail {
public:
    virtual void send(const bool debug, const GenerateEmailContents &mail_contents_generator, const std::string &name_of_user,
                      const std::string &language, const std::string &recipient_email, const std::string &vufind_host,
                      const std::string &sender_email, const std::string &email_subject, const std::vector<NewIssueInfo> &new_issue_infos,
                      const std::string &user_type) const {
        const std::string email_contents(
            mail_contents_generator.generateContent(user_type, name_of_user, language, vufind_host, new_issue_infos));
        if (debug)
            LOG_DEBUG("Debug mode, email address is " + sender_email + ", template expanded to: \"" + email_contents + "\"");
        else {
            const auto response_code(EmailSender::SimplerSendEmail(sender_email, { recipient_email }, email_subject, email_contents,
                                                                   EmailSender::DO_NOT_SET_PRIORITY, EmailSender::HTML));

            if (response_code >= 300) {
                if (response_code == 550)
                    LOG_WARNING("failed to send a notification email to \"" + recipient_email + "\", recipient may not exist!");
                else
                    LOG_ERROR("failed to send a notification email to \"" + recipient_email
                              + "\"! (response code was: " + std::to_string(response_code) + ")");
            }
        }
    }

    virtual void send(__attribute__((unused)) const bool debug,
                      __attribute__((unused)) const GenerateEmailContents &mail_contents_generator,
                      __attribute__((unused)) const std::string &recipient_email, __attribute__((unused)) const std::string &vufind_host,
                      __attribute__((unused)) const std::string &sender_email,
                      __attribute__((unused)) const std::vector<NewIssueInfo> &new_issue_infos) const {
        LOG_ERROR(std::string(__FUNCTION__) + " currently not implemented");
    }
};


class SendMeistertaskNotificationEmails : public SendNotificationEmail {
public:
    virtual void send(const bool debug, const GenerateEmailContents &mail_contents_generator, const std::string &recipient_email,
                      const std::string &vufind_host, const std::string &sender_email,
                      const std::vector<NewIssueInfo> &new_issue_infos) const {
        std::vector<NewIssueInfo> unique_issues_infos;
        deduplicateIdenticalIssues(new_issue_infos, &unique_issues_infos);
        for (const auto &unique_issue_info : unique_issues_infos) {
            const std::string email_contents(mail_contents_generator.generateContent(vufind_host, unique_issue_info));
            if (debug)
                LOG_DEBUG("Debug mode, email address is " + sender_email + ", template expanded to: \"" + email_contents + "\"");
            else {
                const auto response_code(EmailSender::SimplerSendEmail(sender_email, { recipient_email }, unique_issue_info.series_title_,
                                                                       email_contents, EmailSender::DO_NOT_SET_PRIORITY,
                                                                       EmailSender::HTML));

                if (response_code >= 300) {
                    if (response_code == 550)
                        LOG_WARNING("failed to send a notification email to \"" + recipient_email + "\", recipient may not exist!");
                    else
                        LOG_ERROR("failed to send a notification email to \"" + recipient_email
                                  + "\"! (response code was: " + std::to_string(response_code) + ")");
                }
            }
        }
    }


    virtual void send(__attribute__((unused)) const bool debug,
                      __attribute__((unused)) const GenerateEmailContents &mail_contents_generator,
                      __attribute__((unused)) const std::string &name_of_user, __attribute__((unused)) const std::string &language,
                      __attribute__((unused)) const std::string &recipient_email, __attribute__((unused)) const std::string &vufind_host,
                      __attribute__((unused)) const std::string &sender_email, __attribute__((unused)) const std::string &email_subject,
                      __attribute__((unused)) const std::vector<NewIssueInfo> &new_issue_infos,
                      __attribute__((unused)) const std::string &user_type) const {
        LOG_ERROR(std::string(__FUNCTION__) + " currently not implemented");
    }


    void deduplicateIdenticalIssues(const std::vector<NewIssueInfo> new_issue_infos,
                                    std::vector<NewIssueInfo> * const deduplicated_new_issue_infos) const {
        const auto one_per_issue_comparator = [](const NewIssueInfo &lhs, const NewIssueInfo &rhs) { return lhs.operator%(rhs); };
        std::set<NewIssueInfo, decltype(one_per_issue_comparator)> new_issue_infos_one_per_issue(one_per_issue_comparator);
        new_issue_infos_one_per_issue.insert(new_issue_infos.begin(), new_issue_infos.end());
        deduplicated_new_issue_infos->assign(new_issue_infos_one_per_issue.begin(), new_issue_infos_one_per_issue.end());
    }
};


// Makes "date" look like an ISO-8601 date ("2017-01-01 00:00:00" => "2017-01-01T00:00:00Z")
std::string ConvertDateToZuluDate(std::string date) {
    if (unlikely(date.length() != 19 or date[10] != ' '))
        LOG_ERROR("unexpected datetime in " + std::string(__FUNCTION__) + ": \"" + date + "\"!");
    date[10] = 'T';
    return date + 'Z';
}


// Converts ISO-8601 date back to mysql-like date format ("2017-01-01T00:00:00Z" => "2017-01-01 00:00:00")
std::string ConvertDateFromZuluDate(std::string date) {
    if (unlikely(date.length() != 20 or date[10] != 'T' or date[19] != 'Z'))
        LOG_ERROR("unexpected datetime in " + std::string(__FUNCTION__) + ": \"" + date + "\"!");
    date[10] = ' ';
    return date.substr(0, 19);
}


std::string GetIssueId(const std::shared_ptr<const JSON::ObjectNode> &doc_obj) {
    const std::string id(JSON::LookupString("/id", doc_obj, /* default_value = */ ""));
    if (unlikely(id.empty()))
        LOG_ERROR("Did not find 'id' node in JSON tree!");

    return id;
}


const std::string NO_AVAILABLE_TITLE("*No available title*");


std::string GetIssueTitle(const std::string &id, const std::shared_ptr<const JSON::ObjectNode> &doc_obj) {
    const auto issue_title(JSON::LookupString("/title", doc_obj, /* default_value = */ NO_AVAILABLE_TITLE));
    if (unlikely(issue_title == NO_AVAILABLE_TITLE))
        LOG_WARNING("No title found for ID " + id + "!");

    return issue_title;
}


inline std::string GetIssueSubtitle(const std::shared_ptr<const JSON::ObjectNode> &doc_obj) {
    return JSON::LookupString("/title_sub", doc_obj, /* default_value = */ "");
}


std::string GetLastModificationTime(const std::shared_ptr<const JSON::ObjectNode> &doc_obj) {
    const std::string last_modification_time(JSON::LookupString("/last_modification_time", doc_obj, /* default_value = */ ""));
    if (unlikely(last_modification_time.empty()))
        LOG_ERROR("Did not find 'last_modification_time' node in JSON tree!");

    return last_modification_time;
}


std::string GetSeriesTitle(const std::shared_ptr<const JSON::ObjectNode> &doc_obj) {
    const std::string NO_SERIES_TITLE("*No Series Title*");
    const std::shared_ptr<const JSON::JSONNode> container_ids_and_titles(doc_obj->getNode("container_ids_and_titles"));
    if (container_ids_and_titles == nullptr) {
        LOG_WARNING("\"container_ids_and_titles\" is null");
        return NO_SERIES_TITLE;
    }

    const std::shared_ptr<const JSON::ArrayNode> container_ids_and_titles_array(
        JSON::JSONNode::CastToArrayNodeOrDie("container_ids_and_titles", container_ids_and_titles));
    if (unlikely(container_ids_and_titles_array == nullptr))
        LOG_ERROR("container_ids_and_titles_array is not a JSON array!");
    if (container_ids_and_titles_array->empty()) {
        LOG_WARNING("\"container_ids_and_titles\" is empty");
        return NO_SERIES_TITLE;
    }

    std::string first_id_and_title_string_value(container_ids_and_titles_array->getStringNode(0)->getValue());
    StringUtil::ReplaceString("#31;", "\x1F", &first_id_and_title_string_value);
    std::vector<std::string> parts;
    StringUtil::Split(first_id_and_title_string_value, '\x1F', &parts, /* suppress_empty_components = */ true);
    if (unlikely(parts.size() < 2))
        LOG_ERROR("strange id and title value \"" + first_id_and_title_string_value + "\"!");

    return parts[1];
}


std::vector<std::string> GetAuthors(const std::shared_ptr<const JSON::ObjectNode> &doc_obj) {
    const std::shared_ptr<const JSON::JSONNode> author(doc_obj->getNode("author"));
    if (author == nullptr) {
        LOG_DEBUG("\"author\" JSON node is missing!");
        return std::vector<std::string>();
    }

    const std::shared_ptr<const JSON::ArrayNode> author_array(JSON::JSONNode::CastToArrayNodeOrDie("author", author));
    if (author_array->empty()) {
        LOG_WARNING("\"author\" JSON array is empty!");
        return std::vector<std::string>();
    }

    std::vector<std::string> authors;
    for (const auto &array_entry : *author_array) {
        const std::shared_ptr<const JSON::StringNode> author_string(JSON::JSONNode::CastToStringNodeOrDie("author string", array_entry));
        authors.emplace_back(author_string->getValue());
    }

    return authors;
}


/** \return True if new issues were found, false o/w. */
bool ExtractNewIssueInfos(const std::unique_ptr<KeyValueDB> &notified_db, const std::string &serial_control_number,
                          std::unordered_set<std::string> * const new_notification_ids, const std::string &json_document,
                          std::vector<NewIssueInfo> * const new_issue_infos, std::string * const max_last_modification_time) {
    bool found_at_least_one_new_issue(false);

    JSON::Parser parser(json_document);
    std::shared_ptr<JSON::JSONNode> tree;
    if (not parser.parse(&tree))
        LOG_ERROR("JSON parser failed: " + parser.getErrorMessage());

    const std::shared_ptr<const JSON::ObjectNode> tree_obj(JSON::JSONNode::CastToObjectNodeOrDie("top level JSON entity", tree));
    const std::shared_ptr<const JSON::ObjectNode> response(tree_obj->getObjectNode("response"));
    const std::shared_ptr<const JSON::ArrayNode> docs(response->getArrayNode("docs"));

    for (const auto &doc : *docs) {
        const std::shared_ptr<const JSON::ObjectNode> doc_obj(JSON::JSONNode::CastToObjectNodeOrDie("document object", doc));

        const std::string id(GetIssueId(doc_obj));
        if (notified_db->keyIsPresent(id))
            continue; // We already sent a notification for this issue.
        new_notification_ids->insert(id);

        std::string issue_title(GetIssueTitle(id, doc_obj));
        if (issue_title != NO_AVAILABLE_TITLE) {
            const auto subtitle(GetIssueSubtitle(doc_obj));
            if (not subtitle.empty())
                issue_title += " : " + subtitle;
        }

        const std::string series_title(GetSeriesTitle(doc_obj));
        const std::vector<std::string> authors(GetAuthors(doc_obj));
        const std::string volume(JSON::LookupString("/volume", doc_obj, /* default_value = */ ""));
        const std::string year(JSON::LookupString("/year", doc_obj, /* default_value = */ ""));
        const std::string issue(JSON::LookupString("/issue", doc_obj, /* default_value = */ ""));
        const std::string start_page(JSON::LookupString("/start_page", doc_obj, /* default_value = */ ""));

        new_issue_infos->emplace_back(id, serial_control_number, series_title, issue_title, volume, year, issue, start_page, authors);

        const std::string last_modification_time(GetLastModificationTime(doc_obj));
        if (last_modification_time > *max_last_modification_time) {
            *max_last_modification_time = last_modification_time;
            found_at_least_one_new_issue = true;
        }
    }

    return found_at_least_one_new_issue;
}


bool GetNewIssues(const std::unique_ptr<KeyValueDB> &notified_db, std::unordered_set<std::string> * const new_notification_ids,
                  const std::string &solr_host_and_port, const std::string &serial_control_number, std::string last_modification_time,
                  std::vector<NewIssueInfo> * const new_issue_infos, std::string * const max_last_modification_time) {
    const unsigned year_current(StringUtil::ToUnsigned(TimeUtil::GetCurrentYear()));
    const unsigned year_min(year_current - 2);
    const std::string QUERY("superior_ppn:" + serial_control_number + " AND last_modification_time:{" + last_modification_time + " TO *}"
                            + " AND year:[" + std::to_string(year_min) + " TO " + std::to_string(year_current) + "]");

    std::string json_result, err_msg;
    if (unlikely(not Solr::Query(QUERY,
                                 "id,title,title_sub,author,last_modification_time,container_ids_and_titles,volume,year,"
                                 "issue,start_page",
                                 &json_result, &err_msg, solr_host_and_port, /* timeout = */ 5, Solr::JSON)))
        LOG_ERROR("Solr query failed or timed-out: \"" + QUERY + "\". (" + err_msg + ")");

    return ExtractNewIssueInfos(notified_db, serial_control_number, new_notification_ids, json_result, new_issue_infos,
                                max_last_modification_time);
}


void LoadBundleControlNumbers(const IniFile &bundles_config, const std::string &bundle_name,
                              std::vector<std::string> * const control_numbers) {
    const auto section(bundles_config.getSection(bundle_name));
    if (section == bundles_config.end()) {
        LOG_WARNING("can't find bundle \"" + bundle_name + "\" in \"" + bundles_config.getFilename() + "\"!");
        return;
    }

    const std::string bundle_ppns_string(bundles_config.getString(bundle_name, "ppns", ""));
    std::vector<std::string> bundle_ppns;
    StringUtil::SplitThenTrim(bundle_ppns_string, ",", " \t", &bundle_ppns);
    for (const auto &bundle_ppn : bundle_ppns)
        control_numbers->emplace_back(bundle_ppn);
}


inline std::string SingleQuote(const std::string &to_quote) {
    return "'" + to_quote + "'";
}


void LoadBundleMaxLastModificationTimes(DbConnection * const db_connection, const std::string &bundle_name,
                                        std::vector<std::string> bundle_control_numbers,
                                        std::map<std::string, std::string> * const bundle_journals_max_last_modification_times) {
    bundle_journals_max_last_modification_times->clear();
    std::vector<std::string> quoted_bundle_control_numbers;
    std::transform(bundle_control_numbers.begin(), bundle_control_numbers.end(), std::back_inserter(quoted_bundle_control_numbers),
                   SingleQuote);
    db_connection->queryOrDie("SELECT journal_control_number, max_last_modification_time FROM ixtheo_journal_bundles WHERE bundle_name='"
                              + bundle_name + "' AND journal_control_number IN (" + StringUtil::Join(quoted_bundle_control_numbers, ',')
                              + ')');
    DbResultSet result_set(db_connection->getLastResultSet());
    while (const auto row = result_set.getNextRow())
        bundle_journals_max_last_modification_times->emplace(
            std::make_pair(row["journal_control_number"], ConvertDateToZuluDate(row["max_last_modification_time"])));
}


bool IsBundle(const std::string &serial_control_number) {
    if (serial_control_number.empty())
        return false;
    return not std::isdigit(serial_control_number[0]);
}


bool MinModificationTimeLessThan(const std::pair<std::string, std::string> &elem1, const std::pair<std::string, std::string> &elem2) {
    return TimeUtil::Iso8601StringToTimeT(elem1.second, TimeUtil::UTC /* required by function */)
           < TimeUtil::Iso8601StringToTimeT(elem2.second, TimeUtil::UTC /* required by function */);
}


std::string GetMinLastModificationTime(const std::map<std::string, std::string> &control_number_and_max_last_modification_times) {
    const auto min(std::min_element(control_number_and_max_last_modification_times.begin(),
                                    control_number_and_max_last_modification_times.end(), MinModificationTimeLessThan));
    if (min == control_number_and_max_last_modification_times.end())
        return "1970-01-01T00:00:00Z";
    return min->second;
}


std::string GetI18NSubject(const std::string &default_subject, const std::string &language) {
    std::string result = default_subject;
    std::string i18n_subscriptions;
    const std::string i18n_subscriptions_de = "Abonnements";
    const std::string i18n_subscriptions_en = "Subscriptions";

    if (language == "de")
        i18n_subscriptions = i18n_subscriptions_de;
    else if (language == "en")
        i18n_subscriptions = i18n_subscriptions_en;
    else
        return default_subject;

    result = StringUtil::ReplaceString(i18n_subscriptions_de, "", result);
    result = StringUtil::ReplaceString(i18n_subscriptions_en, "", result);
    StringUtil::TrimWhite(&result);
    result += " " + i18n_subscriptions;
    return result;
}


void ProcessSingleUser(
    const bool debug, DbConnection * const db_connection, const std::unique_ptr<KeyValueDB> &notified_db, const IniFile &bundles_config,
    std::unordered_set<std::string> * const new_notification_ids,
    std::unordered_map<std::string, unsigned> * const journal_ppns_to_counts_map, const std::string &user_id,
    const std::string &solr_host_and_port, const std::string &hostname, const std::string &sender_email,
    const std::string &email_default_subject,
    std::vector<SerialControlNumberAndMaxLastModificationTime> &control_numbers_or_bundle_names_and_last_modification_times,
    std::map<std::string, std::map<std::string, std::string>> * const bundle_journal_last_modification_times) {
    db_connection->queryOrDie("SELECT * FROM user WHERE user.id=" + user_id);
    DbResultSet result_set(db_connection->getLastResultSet());

    if (result_set.empty())
        LOG_ERROR("found no user attributes in table \"user\" for ID \"" + user_id + "\"!");
    if (result_set.size() > 1)
        LOG_ERROR("found multiple user attribute sets in table \"user\" for ID \"" + user_id + "\"!");

    const DbRow row(result_set.getNextRow());
    const std::string username(row["username"]);

    LOG_INFO("Found " + std::to_string(control_numbers_or_bundle_names_and_last_modification_times.size()) + " subscriptions for \""
             + username + "\".");

    const std::string firstname(row["firstname"]);
    const std::string lastname(row["lastname"]);
    const std::string email(row["email"]);
    const std::string language(row["last_language"]);
    const std::string user_type(row["ixtheo_user_type"]);
    const auto name_of_user(MiscUtil::GenerateSubscriptionRecipientName(firstname, lastname, language));

    // Collect the dates for new issues.
    std::vector<NewIssueInfo> new_issue_infos;
    for (auto &control_number_or_bundle_name_and_last_modification_time : control_numbers_or_bundle_names_and_last_modification_times) {
        std::string max_last_modification_time(control_number_or_bundle_name_and_last_modification_time.last_modification_time_);
        if (IsBundle(control_number_or_bundle_name_and_last_modification_time.serial_control_number_)) {
            const std::string bundle_name(control_number_or_bundle_name_and_last_modification_time.serial_control_number_);
            std::vector<std::string> bundle_control_numbers;
            LoadBundleControlNumbers(bundles_config, bundle_name, &bundle_control_numbers);
            std::map<std::string, std::string> bundles_journal_control_number_and_last_modification_times;
            LoadBundleMaxLastModificationTimes(db_connection, bundle_name, bundle_control_numbers,
                                               &bundles_journal_control_number_and_last_modification_times);
            for (const auto &bundle_control_number : bundle_control_numbers) {
                const std::string bundle_journal_last_modification_time(
                    bundles_journal_control_number_and_last_modification_times.count(bundle_control_number)
                            and (TimeUtil::Iso8601StringToTimeT(
                                     bundles_journal_control_number_and_last_modification_times[bundle_control_number], TimeUtil::UTC)
                                 >= TimeUtil::Iso8601StringToTimeT(max_last_modification_time, TimeUtil::UTC))
                        ? bundles_journal_control_number_and_last_modification_times[bundle_control_number]
                        : max_last_modification_time);
                if (GetNewIssues(notified_db, new_notification_ids, solr_host_and_port, bundle_control_number,
                                 bundle_journal_last_modification_time, &new_issue_infos, &max_last_modification_time))
                    bundles_journal_control_number_and_last_modification_times[bundle_control_number] = max_last_modification_time;
            }
            (*bundle_journal_last_modification_times)[bundle_name] = bundles_journal_control_number_and_last_modification_times;
            // Get the minimum of all candidates - if they were already sent the notified_db will come in
            control_number_or_bundle_name_and_last_modification_time.setMaxLastModificationTime(
                GetMinLastModificationTime(bundles_journal_control_number_and_last_modification_times));
        } else {
            if (GetNewIssues(notified_db, new_notification_ids, solr_host_and_port,
                             control_number_or_bundle_name_and_last_modification_time.serial_control_number_,
                             control_number_or_bundle_name_and_last_modification_time.last_modification_time_, &new_issue_infos,
                             &max_last_modification_time))
                control_number_or_bundle_name_and_last_modification_time.setMaxLastModificationTime(max_last_modification_time);
        }
    }

    // Deduplicate and sort:
    const std::unordered_set<NewIssueInfo> new_issue_infos_set(new_issue_infos.begin(), new_issue_infos.end());
    new_issue_infos = std::vector<NewIssueInfo>(new_issue_infos_set.begin(), new_issue_infos_set.end());
    std::sort(new_issue_infos.begin(), new_issue_infos.end());

    LOG_INFO("Found " + std::to_string(new_issue_infos.size()) + " new issues for " + "\"" + username + "\".");

    if (not new_issue_infos.empty()) {
        db_connection->queryOrDie("SELECT ixtheo_journal_subscription_format FROM vufind.user WHERE id=" + user_id);
        DbResultSet journal_subscription_format_result_set(db_connection->getLastResultSet());
        const std::string journal_subscription_format(
            journal_subscription_format_result_set.empty()
                ? ""
                : journal_subscription_format_result_set.getNextRow()["ixtheo_journal_subscription_format"]);
        if (journal_subscription_format == JOURNAL_SUBSCRIPTION_FORMAT_TO_STRING_MAP.at(MEISTERTASK))
            SendMeistertaskNotificationEmails().send(debug, GenerateMeistertaskEmailContents(), email, hostname, sender_email,
                                                     new_issue_infos);
        else
            SendDefaultNotificationEmail().send(debug, GenerateDefaultEmailContents(), name_of_user, language, email, hostname,
                                                sender_email, GetI18NSubject(email_default_subject, language), new_issue_infos, user_type);
    }

    // Update the database with the new last issue dates
    // skip in DEBUG mode
    if (debug) {
        std::cerr << "\n\nNew issues for " << username << ":\n";
        for (const auto &new_issue_info : new_issue_infos)
            std::cerr << new_issue_info << '\n';
        return;
    } else {
        for (const auto &new_issue_info : new_issue_infos) {
            auto journal_ppn_and_count(journal_ppns_to_counts_map->find(new_issue_info.series_control_number_));
            if (journal_ppn_and_count == journal_ppns_to_counts_map->end())
                (*journal_ppns_to_counts_map)[new_issue_info.series_control_number_] = 1;
            else
                ++(journal_ppn_and_count->second);
        }
    }

    for (const auto &control_number_or_bundle_name_and_last_modification_time : control_numbers_or_bundle_names_and_last_modification_times)
    {
        if (not control_number_or_bundle_name_and_last_modification_time.changed())
            continue;

        db_connection->queryOrDie(
            "UPDATE ixtheo_journal_subscriptions SET max_last_modification_time='"
            + ConvertDateFromZuluDate(control_number_or_bundle_name_and_last_modification_time.last_modification_time_)
            + "' WHERE user_id=" + user_id + " AND journal_control_number_or_bundle_name='"
            + control_number_or_bundle_name_and_last_modification_time.serial_control_number_ + "'");
    }
}


void StoreBundleJournalsMaxModificationTimes(
    DbConnection * const db_connection,
    const std::map<std::string, std::map<std::string, std::string>> bundle_journals_last_modification_times) {
    for (auto const &[bundle_name, journal_control_number_and_max_last_modification_times] : bundle_journals_last_modification_times) {
        db_connection->queryOrDie("DELETE FROM ixtheo_journal_bundles WHERE bundle_name=\"" + bundle_name + "\"");
        for (const auto &journal_control_number_and_max_last_modification_time : journal_control_number_and_max_last_modification_times)
            db_connection->queryOrDie("INSERT INTO ixtheo_journal_bundles VALUES('" + bundle_name + "','"
                                      + journal_control_number_and_max_last_modification_time.first + "','"
                                      + ConvertDateFromZuluDate(journal_control_number_and_max_last_modification_time.second) + "')");
    }
}


void ProcessSubscriptions(const bool debug, DbConnection * const db_connection, const std::unique_ptr<KeyValueDB> &notified_db,
                          const IniFile &bundles_config, std::unordered_set<std::string> * const new_notification_ids,
                          std::unordered_map<std::string, unsigned> * const journal_ppns_to_counts_map,
                          const std::string &solr_host_and_port, const std::string &user_type, const std::string &hostname,
                          const std::string &sender_email, const std::string &email_default_subject) {
    db_connection->queryOrDie(
        "SELECT DISTINCT user_id FROM ixtheo_journal_subscriptions WHERE user_id IN (SELECT id FROM "
        "user WHERE ixtheo_user_type = '"
        + user_type + "')");

    unsigned subscription_count(0);
    DbResultSet id_result_set(db_connection->getLastResultSet());
    const unsigned user_count(id_result_set.size());
    std::map<std::string, std::map<std::string, std::string>> bundle_journals_last_modification_times;
    while (const DbRow id_row = id_result_set.getNextRow()) {
        const std::string user_id(id_row["user_id"]);

        db_connection->queryOrDie(
            "SELECT journal_control_number_or_bundle_name,max_last_modification_time FROM "
            "ixtheo_journal_subscriptions WHERE user_id="
            + user_id);
        DbResultSet result_set(db_connection->getLastResultSet());
        std::vector<SerialControlNumberAndMaxLastModificationTime> control_numbers_or_bundle_names_and_last_modification_times;
        while (const DbRow row = result_set.getNextRow()) {
            control_numbers_or_bundle_names_and_last_modification_times.emplace_back(SerialControlNumberAndMaxLastModificationTime(
                row["journal_control_number_or_bundle_name"], ConvertDateToZuluDate(row["max_last_modification_time"])));
            ++subscription_count;
        }
        ProcessSingleUser(debug, db_connection, notified_db, bundles_config, new_notification_ids, journal_ppns_to_counts_map, user_id,
                          solr_host_and_port, hostname, sender_email, email_default_subject,
                          control_numbers_or_bundle_names_and_last_modification_times, &bundle_journals_last_modification_times);
    }

    StoreBundleJournalsMaxModificationTimes(db_connection, bundle_journals_last_modification_times);

    LOG_INFO("Processed " + std::to_string(user_count) + " users and " + std::to_string(subscription_count) + " subscriptions.\n");
}


void RecordNewlyNotifiedIds(const std::unique_ptr<KeyValueDB> &notified_db, const std::unordered_set<std::string> &new_notification_ids) {
    const std::string now(TimeUtil::GetCurrentDateAndTime());
    for (const auto &id : new_notification_ids)
        notified_db->addOrReplace(id, now);
}


std::unique_ptr<KeyValueDB> CreateOrOpenKeyValueDB(const std::string &user_type) {
    const std::string DB_FILENAME(UBTools::GetTuelibPath() + user_type + "_notified.db");
    if (not FileUtil::Exists(DB_FILENAME))
        KeyValueDB::Create(DB_FILENAME);

    return std::unique_ptr<KeyValueDB>(new KeyValueDB(DB_FILENAME));
}


void RecordStats(const std::string &user_type, std::unordered_map<std::string, unsigned> &journal_ppns_to_counts_map) {
    const auto USAGE_STATS_PATH(UBTools::GetTuelibPath() + "new_journal_alert.stats");
    const auto usage_stats_file(FileUtil::OpenForAppendingOrDie(USAGE_STATS_PATH));

    const auto JULIAN_DAY_NUMBER(TimeUtil::GetJulianDayNumber());
    for (const auto &[journal_ppn, count] : journal_ppns_to_counts_map) {
        // NOTE: The data written here has to match what will be read by generate_new_journal_alert_stats!
        BinaryIO::WriteOrDie(*usage_stats_file, JULIAN_DAY_NUMBER);
        BinaryIO::WriteOrDie(*usage_stats_file, user_type);
        BinaryIO::WriteOrDie(*usage_stats_file, journal_ppn);
        BinaryIO::WriteOrDie(*usage_stats_file, count);
    }

    LOG_INFO("Appended " + std::to_string(journal_ppns_to_counts_map.size()) + " entries to " + USAGE_STATS_PATH + ".");
}


} // unnamed namespace


// gets user subscriptions for superior works from MySQL
// uses a KeyValueDB instance to prevent entries from being sent multiple times to same user
int Main(int argc, char **argv) {
    if (argc < 5)
        Usage();

    bool debug(false);
    if (std::strcmp("--debug", argv[1]) == 0) {
        if (argc < 6)
            Usage();
        debug = true;
        --argc, ++argv;
    }

    std::string solr_host_and_port;
    if (argc == 5)
        solr_host_and_port = Solr::DEFAULT_HOST + ":" + std::to_string(Solr::DEFAULT_PORT);
    else if (argc == 6) {
        solr_host_and_port = argv[1];
        --argc, ++argv;
    } else
        Usage();

    const std::string user_type(argv[1]);
    if (user_type != "ixtheo" and user_type != "relbib" and user_type != "bibstudies" and user_type != "churchlaw")
        LOG_ERROR("user_type parameter must be either \"ixtheo\", \"relbib\", \"bibstudies\" or \"churchlaw\"!");

    const std::string hostname(argv[2]);
    const std::string sender_email(argv[3]);
    const std::string email_default_subject(argv[4]);

    std::unique_ptr<KeyValueDB> notified_db(CreateOrOpenKeyValueDB(user_type));

    DbConnection db_connection(DbConnection::VuFindMySQLFactory());

    const IniFile bundles_config(UBTools::GetTuelibPath() + "journal_alert_bundles.conf");

    std::unordered_set<std::string> new_notification_ids;
    std::unordered_map<std::string, unsigned> journal_ppns_to_counts_map;
    ProcessSubscriptions(debug, &db_connection, notified_db, bundles_config, &new_notification_ids, &journal_ppns_to_counts_map,
                         solr_host_and_port, user_type, hostname, sender_email, email_default_subject);

    if (not debug) {
        RecordNewlyNotifiedIds(notified_db, new_notification_ids);
        RecordStats(user_type, journal_ppns_to_counts_map);
    }

    return EXIT_SUCCESS;
}

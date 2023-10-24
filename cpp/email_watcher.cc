/** \file    email_watcher.cc
 *  \brief   Checks an mbox file for overdue and urgent messages.
 *  \author  Dr. Johannes Ruscheinski
 */

/*
    Copyright (C) 2020-2021 Library of the University of Tübingen

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
#include <map>
#include <memory>
#include <set>
#include <unordered_map>
#include <cstdlib>
#include <strings.h>
#include "DbConnection.h"
#include "EmailSender.h"
#include "FileUtil.h"
#include "IniFile.h"
#include "MBox.h"
#include "RegexMatcher.h"
#include "StringUtil.h"
#include "TextUtil.h"
#include "TimeUtil.h"
#include "UBTools.h"
#include "util.h"


namespace {


struct EmailDescription {
    std::string from_host_;
    RegexMatcher *subject_matcher_;
    RegexMatcher *positive_body_matcher_;
    RegexMatcher *negative_body_matcher_;
    unsigned overdue_time_window_; // in hours
public:
    EmailDescription() = default;
    explicit EmailDescription(const IniFile::Section &ini_file_section);

    bool subjectAndBodyMatched(const MBox::Message &email_message) const;
};


EmailDescription::EmailDescription(const IniFile::Section &ini_file_section) {
    if (not ini_file_section.hasEntry("from_host"))
        LOG_ERROR("ini file section \"" + ini_file_section.getSectionName() + "\" is missing a \"from_host\" entry!");
    from_host_ = ini_file_section.getString("from_host");

    if (not ini_file_section.hasEntry("subject_pattern"))
        LOG_ERROR("ini file section \"" + ini_file_section.getSectionName() + "\" is missing a \"subject_pattern\" entry!");
    const auto subject_pattern_regex(ini_file_section.getString("subject_pattern"));
    std::string error_message;
    subject_matcher_ = RegexMatcher::RegexMatcherFactory(subject_pattern_regex, &error_message);
    if (subject_matcher_ == nullptr)
        LOG_ERROR("bad regex \"" + subject_pattern_regex + "\" in \"" + ini_file_section.getSectionName() + "\"!");

    const auto body_positive_pattern_regex(ini_file_section.getString("body_positive_pattern", ""));
    if (body_positive_pattern_regex.empty())
        positive_body_matcher_ = nullptr;
    else {
        positive_body_matcher_ = RegexMatcher::RegexMatcherFactory(body_positive_pattern_regex, &error_message);
        if (positive_body_matcher_ == nullptr)
            LOG_ERROR("bad regex \"" + body_positive_pattern_regex + "\" in \"" + ini_file_section.getSectionName() + "\"!");
    }

    const auto body_negative_pattern_regex(ini_file_section.getString("body_negative_pattern", ""));
    if (body_negative_pattern_regex.empty())
        negative_body_matcher_ = nullptr;
    else {
        negative_body_matcher_ = RegexMatcher::RegexMatcherFactory(body_negative_pattern_regex, &error_message);
        if (negative_body_matcher_ == nullptr)
            LOG_ERROR("bad regex \"" + body_negative_pattern_regex + "\" in \"" + ini_file_section.getSectionName() + "\"!");
    }

    if (positive_body_matcher_ == nullptr and negative_body_matcher_ == nullptr)
        LOG_ERROR("section \"" + ini_file_section.getSectionName() + "\" is missing both body_positive_pattern and body_negative_pattern!");

    if (not ini_file_section.hasEntry("overdue_time_window"))
        LOG_ERROR("ini file section \"" + ini_file_section.getSectionName() + "\" is missing an \"overdue_time_window\" entry!");
    overdue_time_window_ = ini_file_section.getUnsigned("overdue_time_window");
}


std::vector<std::string> SplitIntoLines(const std::string &s) {
    std::vector<std::string> lines;

    std::string line;
    for (auto ch(s.cbegin()); ch != s.cend(); ++ch) {
        if (*ch != '\n')
            line += *ch;
        else {
            lines.emplace_back(line);
            line.clear();
        }
    }

    if (not line.empty())
        lines.emplace_back(line);

    return lines;
}


std::string DecodeBodyPart(const MBox::BodyPart &body_part) {
    std::string charset("utf-8");
    bool is_base64_encoded(false);

    for (const auto &header : body_part.getMIMEHeaders()) {
        if (header.first == "content-type") {
            const auto charset_start_pos(header.second.find("charset="));
            if (charset_start_pos != std::string::npos) {
                charset = header.second.substr(charset_start_pos + __builtin_strlen("charset="));
                StringUtil::Trim(&charset);
                StringUtil::Trim(&charset, '"');
            }
        } else if (header.first == "content-transfer-encoding" and ::strcasecmp(header.second.c_str(), "base64") == 0)
            is_base64_encoded = true;
    }

    std::string body;
    for (const auto &body_line : SplitIntoLines(body_part.getBody())) {
        body += body_line;
        if (not is_base64_encoded)
            body += '\n';
    }

    if (::strcasecmp(charset.c_str(), "utf-8") != 0) {
        std::string error_message;
        const auto encoding_converter(
            TextUtil::EncodingConverter::Factory(charset, TextUtil::EncodingConverter::CANONICAL_UTF8_NAME, &error_message));
        if (unlikely(not error_message.empty()))
            LOG_ERROR("failed to create an encoding converter from \"" + charset + "\" to UTF-8!");

        std::string utf8_body;
        if (unlikely(not encoding_converter->convert(body, &utf8_body)))
            LOG_ERROR("couldn't convert the body from \"" + charset + " UTF-8!");
        utf8_body.swap(body);
    }

    return is_base64_encoded ? TextUtil::Base64Decode(body) : body;
}


bool EmailDescription::subjectAndBodyMatched(const MBox::Message &email_message) const {
    if (not subject_matcher_->matched(email_message.getSubject()))
        return false;

    if (positive_body_matcher_ == nullptr and negative_body_matcher_ == nullptr)
        return true;

    if (email_message.isMultipartMessage()) {
        for (const auto &body_part : email_message) {
            const auto decoded_body(DecodeBodyPart(body_part));
            if (positive_body_matcher_ != nullptr and not positive_body_matcher_->matched(decoded_body))
                return false;
            if (negative_body_matcher_ != nullptr and negative_body_matcher_->matched(decoded_body))
                return false;
        }
    } else {
        if (positive_body_matcher_ != nullptr and not positive_body_matcher_->matched(email_message.getMessageBody()))
            return false;
        if (negative_body_matcher_ != nullptr and negative_body_matcher_->matched(email_message.getMessageBody()))
            return false;
    }

    return true;
}


std::unordered_map<std::string, EmailDescription> LoadEmailDescriptions(const IniFile &ini_file) {
    std::unordered_map<std::string, EmailDescription> email_descriptions;

    for (const auto &section : ini_file) {
        const auto &section_name(section.getSectionName());
        if (not section_name.empty())
            email_descriptions[section_name] = EmailDescription(section);
    }

    return email_descriptions;
}


void ProcessMBox(const std::string &mbox_filename, const long forward_priority,
                 const std::vector<std::string> &notification_email_addresses,
                 const std::unordered_map<std::string, EmailDescription> &email_descriptions,
                 std::vector<std::string> * const unmatched_emails, std::set<std::string> * const matched_section_names) {
    const MBox mbox(mbox_filename);
    unsigned email_message_count(0);
    for (const auto &email_message : mbox) {
        ++email_message_count;
        if (email_message.getPriority() >= forward_priority) {
            LOG_DEBUG("Forwarding email w/ subject \"" + email_message.getSubject() + "\" from host \"" + email_message.getOriginalHost()
                      + "\" and sender \"" + email_message.getSender() + "\".");
            EmailSender::SimplerSendEmail("no-reply@ub.uni-tuebingen.de", notification_email_addresses, email_message.getSubject(),
                                          "High priority (" + std::to_string(email_message.getPriority()) + ") email from original host "
                                              + email_message.getOriginalHost() + " and sender " + email_message.getSender() + ".\n\n"
                                              + email_message.getMessageBody());
        }

        bool matched_a_section(false);
        for (const auto &[section_name, email_description] : email_descriptions) {
            if (email_description.subjectAndBodyMatched(email_message)) {
                LOG_DEBUG("Email w/ subject \"" + email_message.getSubject() + "\" from host \"" + email_message.getOriginalHost()
                          + "\" and sender \"" + email_message.getSender() + "\" matched section \"" + section_name + "\".");
                matched_section_names->emplace(section_name);
                matched_a_section = true;
            }
        }

        if (not matched_a_section) {
            if (email_message.isMultipartMessage()) {
                std::string as_string(email_message.headerToString());
                as_string += "\n----\n";
                for (const auto &body_part : email_message) {
                    as_string += DecodeBodyPart(body_part);
                    if (as_string.back() != '\n')
                        as_string += '\n';
                    as_string += "----\n";
                }
                unmatched_emails->emplace_back(as_string);
            } else
                unmatched_emails->emplace_back(email_message.toString());
        }
    }
    LOG_INFO("Processed " + std::to_string(email_message_count) + " email message(s) and found " + std::to_string(unmatched_emails->size())
             + " unmatched message(s).");
}


const std::string MAPFILE_PATH(UBTools::GetTuelibPath() + "email_watcher_last_seen_times.map");


std::map<std::string, time_t> LoadSectionNamesToLastSeenTimeWindowsMap() {
    std::map<std::string, time_t> section_names_to_last_seen_time_map;
    if (FileUtil::Exists(MAPFILE_PATH)) {
        const auto input(FileUtil::OpenInputFileOrDie(MAPFILE_PATH));
        unsigned current_line_number(0);
        while (not input->eof()) {
            ++current_line_number;
            const auto line(input->getline());
            const auto first_equal_pos(line.find('='));
            if (unlikely(first_equal_pos == std::string::npos or first_equal_pos == 0))
                LOG_ERROR("missing or leading equal-sign in " + MAPFILE_PATH + " on line #" + std::to_string(current_line_number) + "!");
            unsigned last_seen_time;
            if (unlikely(not StringUtil::ToNumber(line.substr(first_equal_pos + 1), &last_seen_time)))
                LOG_ERROR("can't convert \"" + line.substr(first_equal_pos + 1) + "\" to an unsigned number! (line #"
                          + std::to_string(current_line_number) + " in " + MAPFILE_PATH + ")");
            section_names_to_last_seen_time_map[line.substr(0, first_equal_pos)] = static_cast<time_t>(last_seen_time);
        }
    }

    LOG_INFO("Loaded " + std::to_string(section_names_to_last_seen_time_map.size()) + " mappings from " + MAPFILE_PATH + ".");

    return section_names_to_last_seen_time_map;
}


void SaveSectionNamesToLastSeenTimeWindowsMap(const std::map<std::string, time_t> &section_names_to_last_seen_time_map) {
    const auto output(FileUtil::OpenOutputFileOrDie(MAPFILE_PATH));
    for (const auto &[section_name, last_seen_time] : section_names_to_last_seen_time_map)
        (*output) << section_name << '=' << last_seen_time << '\n';
    LOG_INFO("Wrote " + std::to_string(section_names_to_last_seen_time_map.size()) + " entries/entry to " + MAPFILE_PATH + ".");
}


void SendNotificationsForOverdueEmails(const IniFile &ini_file, std::set<std::string> * const matched_section_names,
                                       const std::vector<std::string> &notification_email_addresses, const time_t NOW,
                                       const std::map<std::string, time_t> &section_names_to_last_seen_time_map) {
    std::string overdue_list;
    for (const auto &section : ini_file) {
        const auto &section_name(section.getSectionName());
        if (section_name.empty()) // Global section
            continue;

        LOG_DEBUG("Processing section " + section_name + ".");

        if (matched_section_names->find(section_name) != matched_section_names->end()) {
            LOG_DEBUG("\tWe have new mail for " + section_name + " and therefore can't be overdue!");
            continue; // We're definitely *not* overdue!
        }

        const auto section_name_and_last_seen_time(section_names_to_last_seen_time_map.find(section_name));
        if (section_name_and_last_seen_time == section_names_to_last_seen_time_map.end()) {
            LOG_DEBUG("\tSection " + section_name + " not found in section_names_to_last_seen_time_map!");
            matched_section_names->emplace(section_name);
            continue;
        }
        LOG_DEBUG("\tLast seen " + TimeUtil::TimeTToString(section_name_and_last_seen_time->second) + ".");

        const unsigned overdue_time_window(section.getUnsigned("overdue_time_window")); // in hours
        if ((NOW - section_name_and_last_seen_time->second) > overdue_time_window * 3600u) {
            overdue_list += "No email from " + section.getString("from_host") + " with subject pattern \""
                            + section.getString("subject_pattern") + "\" within the last " + section.getString("overdue_time_window")
                            + "hours!\n";
            matched_section_names->emplace(section_name); // So we don't gripe again too soon!
        }
    }
    if (not overdue_list.empty()
        and EmailSender::SimplerSendEmail("no-reply@ub.uni-tuebingen.de", notification_email_addresses, "Overdue Report", overdue_list,
                                          EmailSender::VERY_HIGH)
                > 299)
        LOG_ERROR("Failed to send the \"Overdue Report\" email!");
}


} // unnamed namespace


int Main(int argc, char *argv[]) {
    if (argc != 3)
        ::Usage("ini_file_path mbox_path");

    const IniFile ini_file(argv[1]);

    std::vector<std::string> notification_email_addresses;
    if (StringUtil::SplitThenTrimWhite(ini_file.getString("", "notify"), ',', &notification_email_addresses) == 0)
        LOG_ERROR("we need at least one notification email address in the \"notify\" entry in \"" + ini_file.getFilename() + "\"!");

    const long forward_priority(ini_file.getInteger("", "forward_priority"));
    const std::string backup_dir_path(ini_file.getString("", "backup_dir_path") + "/");
    const auto email_descriptions(LoadEmailDescriptions(ini_file));

    const std::string MBOX_FILENAME(argv[2]);
    if (not FileUtil::Exists(MBOX_FILENAME))
        return EXIT_SUCCESS; // Nothing to be done!

    std::vector<std::string> unmatched_emails;
    std::set<std::string> matched_section_names;
    ProcessMBox(MBOX_FILENAME, forward_priority, notification_email_addresses, email_descriptions, &unmatched_emails,
                &matched_section_names);

    if (not unmatched_emails.empty()) {
        if (EmailSender::SendEmailWithInlineAttachments("no-reply@ub.uni-tuebingen.de", notification_email_addresses, "Unmatched Email(s)!",
                                                        "The emails in the attachments did not match any of our patterns!\n"
                                                        "Please either fix the problems that led to the generation of the\n"
                                                        "original emails or add new patterns so that the currently unmatched\n"
                                                        "emails will be matched in the future!",
                                                        unmatched_emails, EmailSender::VERY_HIGH)
            > 299)
            LOG_ERROR("Failed to send the \"Unmatched Email(s)!\" email!");
    }

    std::map<std::string, time_t> section_names_to_last_seen_time_map(LoadSectionNamesToLastSeenTimeWindowsMap());
    const time_t NOW(::time(nullptr));
    SendNotificationsForOverdueEmails(ini_file, &matched_section_names, notification_email_addresses, NOW,
                                      section_names_to_last_seen_time_map);

    for (const auto &matched_section_name : matched_section_names) {
        auto section_name_and_last_seen_time(section_names_to_last_seen_time_map.find(matched_section_name));
        if (section_name_and_last_seen_time == section_names_to_last_seen_time_map.end())
            section_names_to_last_seen_time_map[matched_section_name] = NOW;
        else
            section_name_and_last_seen_time->second = NOW;
    }

    SaveSectionNamesToLastSeenTimeWindowsMap(section_names_to_last_seen_time_map);

    FileUtil::RenameFileOrDie(MBOX_FILENAME,
                              backup_dir_path + FileUtil::GetBasename(MBOX_FILENAME) + "-" + TimeUtil::GetCurrentDateAndTime());

    return EXIT_SUCCESS;
}

/** \file   BSZUtil.cc
 *  \brief  Various utility functions related to data etc. having to do w/ the BSZ.
 *  \author Dr. Johannes Ruscheinski (johannes.ruscheinski@uni-tuebingen.de)
 *  \author Oliver Obenland (oliver.obenland@uni-tuebingen.de)
 *
 *  \copyright 2017-2020 Universitätsbibliothek Tübingen.  All rights reserved.
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU Affero General Public License as
 *  published by the Free Software Foundation, either version 3 of the
 *  License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Affero General Public License for more details.
 *
 *  You should have received a copy of the GNU Affero General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "BSZUtil.h"
#include <map>
#include <memory>
#include <mutex>
#include <thread>
#include "Archive.h"
#include "Downloader.h"
#include "FileUtil.h"
#include "RegexMatcher.h"
#include "StringUtil.h"
#include "UrlUtil.h"
#include "util.h"


namespace BSZUtil {


// Use the following indicators to select whether to fully delete a record or remove its local data
// For a description of indicators
// c.f. https://wiki.bsz-bw.de/doku.php?id=v-team:daten:datendienste:sekkor (20160426)
const char FULL_RECORD_DELETE_INDICATORS[] = { 'A', 'B', 'C', 'D', 'E' };
const char LOCAL_DATA_DELETE_INDICATORS[] = { '3', '4', '5', '9' };

/*
 * The PPN length was increased from 9 to 10 in 2018.
 * The 10th character can optionally be a space
 */
const size_t MAX_LINE_LENGTH_OLD_WITH_ILN(25);
const size_t MAX_LINE_LENGTH_OLD_NO_ILN(21);
const size_t MAX_LINE_LENGTH_NEW_WITH_ILN(26);
const size_t MAX_LINE_LENGTH_NEW_NO_ILN(22);

const size_t PPN_START_INDEX(12);
const size_t SEPARATOR_INDEX(PPN_START_INDEX - 1);


void ExtractDeletionIds(File * const deletion_list, std::unordered_set<std::string> * const delete_full_record_ids,
                        std::unordered_set<std::string> * const local_deletion_ids) {
    unsigned line_no(0);
top_loop:
    while (not deletion_list->eof()) {
        const std::string line(StringUtil::TrimWhite(deletion_list->getline()));
        ++line_no;
        if (unlikely(line.empty())) // Ignore empty lines.
            continue;

        const size_t line_len(line.length());
        size_t ppn_len(0);

        if (line_len == MAX_LINE_LENGTH_OLD_WITH_ILN or line_len == MAX_LINE_LENGTH_OLD_NO_ILN)
            ppn_len = PPN_LENGTH_OLD;
        else if (line_len == MAX_LINE_LENGTH_NEW_WITH_ILN or line_len == MAX_LINE_LENGTH_NEW_NO_ILN)
            ppn_len = PPN_LENGTH_NEW;
        else {
            LOG_ERROR("unexpected line length " + std::to_string(line_len) + " for entry on line " + std::to_string(line_no)
                      + " in deletion list file \"" + deletion_list->getPath() + "\"!");
            ppn_len = PPN_LENGTH_OLD; // fallback to the more conservative of the two lengths
        }

        for (const char indicator : FULL_RECORD_DELETE_INDICATORS) {
            if (line[SEPARATOR_INDEX] == indicator) {
                delete_full_record_ids->insert(StringUtil::Trim(line.substr(PPN_START_INDEX, ppn_len)));
                goto top_loop;
            }
        }
        for (const char indicator : LOCAL_DATA_DELETE_INDICATORS) {
            if (line[SEPARATOR_INDEX] == indicator) {
                local_deletion_ids->insert(StringUtil::Trim(line.substr(PPN_START_INDEX, ppn_len)));
                goto top_loop;
            }
        }
        LOG_WARNING("in \"" + deletion_list->getPath() + " \" on line #" + std::to_string(line_no) + " unknown indicator: '"
                    + line.substr(SEPARATOR_INDEX, 1) + "'!");
    }
}


std::string ExtractDateFromFilenameOrDie(const std::string &filename) {
    static const std::string DATE_EXTRACTION_REGEX("(\\d\\d[01]\\d[0123]\\d)");
    static RegexMatcher *matcher;
    if (matcher == nullptr) {
        std::string err_msg;
        matcher = RegexMatcher::RegexMatcherFactory(DATE_EXTRACTION_REGEX, &err_msg);
        if (unlikely(not err_msg.empty()))
            LOG_ERROR("in ExtractDateFromFilenameOrDie: failed to compile regex: \"" + DATE_EXTRACTION_REGEX + "\".");
    }

    if (unlikely(not matcher->matched(filename)))
        LOG_ERROR("in ExtractDateFromFilenameOrDie: \"" + filename + "\" failed to match the regex \"" + DATE_EXTRACTION_REGEX + "\"!");

    return (*matcher)[1];
}


ArchiveType GetArchiveType(const std::string &member_name) {
    if (StringUtil::EndsWith(member_name, "aut.mrc"))
        return AUTHORITY_RECORDS;
    if (StringUtil::EndsWith(member_name, "tit.mrc"))
        return TITLE_RECORDS;
    if (StringUtil::EndsWith(member_name, "lok.mrc"))
        return LOCAL_RECORDS;
    static auto matcher(RegexMatcher::RegexMatcherFactoryOrDie("([abc])\\d\\d\\d.raw"));
    if (not matcher->matched(member_name))
        LOG_ERROR("bad member type for archive member \"" + member_name + "\"!");

    const auto type_char((*matcher)[1][0]);
    switch (type_char) {
    case 'a':
        return TITLE_RECORDS;
    case 'b':
        return SUPERIOR_TITLES;
    case 'c':
        return AUTHORITY_RECORDS;
    default:
        LOG_ERROR("Unknown type character '" + std::string(1, type_char) + "'!");
    }
}


std::string GetAuthorGNDNumber(const std::string &author, const std::string &author_lookup_base_url) {
    static std::mutex fetch_author_gnd_url_to_gnd_cache_mutex;
    static std::unordered_map<std::string, std::string> fetch_author_gnd_url_to_gnd_cache;
    static const ThreadSafeRegexMatcher AUTHOR_GND_MATCHER("Link zu diesem Datensatz in der GND:\\s*<[^>]+><a[^>]*>http(?:s)?://d-nb.info/gnd/([0-9X]+)</a>");

    // "author" must be in the lastname,firstname format
    const std::string lookup_url(author_lookup_base_url + UrlUtil::UrlEncode(author));
    {
        std::lock_guard<std::mutex> lock(fetch_author_gnd_url_to_gnd_cache_mutex);
        const auto match(fetch_author_gnd_url_to_gnd_cache.find(lookup_url));
        if (match != fetch_author_gnd_url_to_gnd_cache.end())
            return match->second;
    }

    Downloader downloader(lookup_url);
    if (downloader.anErrorOccurred()) {
        LOG_WARNING("couldn't download author GND results! downloader error: " + downloader.getLastErrorMessage());
        return "";
    }

    const auto match(AUTHOR_GND_MATCHER.match(downloader.getMessageBody()));
    if (match) {
        std::lock_guard<std::mutex> lock(fetch_author_gnd_url_to_gnd_cache_mutex);
        fetch_author_gnd_url_to_gnd_cache.emplace(lookup_url, match[1]);
        return match[1];
    }

    return "";
}


// Maps ".*[abc]???.raw" to ".*[abc]001.raw"
static inline std::string GenerateOutputMemberName(std::string member_name) {
    if (member_name == "sekkor-aut.mrc")
        return "sekkor-c001.raw";
    if (member_name == "sekkor-tit.mrc")
        return "sekkor-a001.raw";
    if (unlikely(member_name.length() < 8))
        LOG_ERROR("short archive member name \"" + member_name + "\"!");
    member_name[member_name.length() - 7] = '0';
    member_name[member_name.length() - 6] = '0';
    member_name[member_name.length() - 5] = '1';
    return member_name;
}


void ExtractArchiveMembers(const std::string &archive_name, std::vector<std::string> * const archive_members,
                           const std::string &optional_suffix) {
    static auto member_matcher(RegexMatcher::RegexMatcherFactoryOrDie("([abc]\\d\\d\\d\\.raw|sekkor-...\\.mrc)$"));

    std::map<ArchiveType, std::shared_ptr<File>> member_type_to_file_map;
    Archive::Reader reader(archive_name);
    Archive::Reader::EntryInfo file_info;
    while (reader.getNext(&file_info)) {
        if (file_info.empty())
            continue;

        const std::string member_name(file_info.getFilename());
        if (unlikely(not file_info.isRegularFile()))
            LOG_ERROR("unexpectedly, the entry \"" + member_name + "\" in \"" + archive_name + "\" is not a regular file!");
        if (unlikely(not member_matcher->matched(member_name)))
            LOG_ERROR("unexpected entry name \"" + member_name + "\"!");

        const auto member_type(GetArchiveType(member_name));
        auto type_and_file(member_type_to_file_map.find(member_type));
        if (type_and_file == member_type_to_file_map.end()) {
            const auto output_filename(GenerateOutputMemberName(member_name) + optional_suffix);
            std::shared_ptr<File> file(FileUtil::OpenOutputFileOrDie(output_filename));
            member_type_to_file_map.emplace(member_type, file);
            type_and_file = member_type_to_file_map.find(member_type);
            archive_members->emplace_back(output_filename);
        }

        char buf[8192];
        size_t read_count;
        while ((read_count = reader.read(buf, sizeof buf)) > 0) {
            if (unlikely(type_and_file->second->write(buf, read_count) != read_count))
                LOG_ERROR("failed to write data to \"" + type_and_file->second->getPath() + "\"! (No room?)");
        }
    }
}


void ExtractYearVolumeIssue(const MARC::Record &record, std::string * const year, std::string * const volume, std::string * const issue) {
    const auto field_008(record.findTag("008"));
    if (field_008 != record.end())
        *year = field_008->getContents().substr(7, 4);

    const auto field_936(record.findTag("936"));
    if (field_936 == record.end())
        return;

    *volume = field_936->getFirstSubfieldWithCode('d');
    *issue = field_936->getFirstSubfieldWithCode('e');
}


std::string GetK10PlusPPNFromSubfield(const MARC::Record::Field &field, const char subfield_code) {
    for (const auto &subfield_code_and_value : field.getSubfields()) {
        if (subfield_code_and_value.code_ == subfield_code and StringUtil::StartsWith(subfield_code_and_value.value_, "(DE-627)"))
            return subfield_code_and_value.value_.substr(__builtin_strlen("(DE-627)"));
    }
    return "";
}


} // namespace BSZUtil

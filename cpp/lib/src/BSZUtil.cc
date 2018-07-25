/** \file   BSZUtil.h
 *  \brief  Various utility functions related to data etc. having to do w/ the BSZ.
 *  \author Dr. Johannes Ruscheinski (johannes.ruscheinski@uni-tuebingen.de)
 *  \author Oliver Obenland (oliver.obenland@uni-tuebingen.de)
 *
 *  \copyright 2017,2018 Universitätsbibliothek Tübingen.  All rights reserved.
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
#include "Archive.h"
#include "FileUtil.h"
#include "StringUtil.h"
#include "util.h"
#include "RegexMatcher.h"


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

const size_t PPN_LENGTH_OLD(9);
const size_t PPN_LENGTH_NEW(10);
const size_t PPN_START_INDEX(12);
const size_t SEPARATOR_INDEX(PPN_START_INDEX - 1);


void ExtractDeletionIds(File * const deletion_list, std::unordered_set <std::string> * const delete_full_record_ids,
                        std::unordered_set <std::string> * const local_deletion_ids)
{
    unsigned line_no(0);
top_loop:
    while (not deletion_list->eof()) {
        const std::string line(StringUtil::Trim(deletion_list->getline()));
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
            LOG_ERROR("unexpected line length " + std::to_string(line_len)
                       + " for entry on line " + std::to_string(line_no)
                       + " in deletion list file \"" + deletion_list->getPath() + "\"!");
            ppn_len = PPN_LENGTH_OLD;       // fallback to the more conservative of the two lengths
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
        LOG_WARNING("in \"" + deletion_list->getPath() + " \" on line #" + std::to_string(line_no)
                   + " unknown indicator: '" + line.substr(SEPARATOR_INDEX, 1) + "'!");
    }
}


std::string ExtractDateFromFilenameOrDie(const std::string &filename) {
    static const std::string DATE_EXTRACTION_REGEX("(\\d\\d[01]\\d[0123]\\d)");
    static RegexMatcher *matcher;
    if (matcher == nullptr) {
        std::string err_msg;
        matcher = RegexMatcher::RegexMatcherFactory(DATE_EXTRACTION_REGEX, &err_msg);
        if (unlikely(not err_msg.empty()))
            LOG_ERROR("in ExtractDateFromFilenameOrDie: failed to compile regex: \"" + DATE_EXTRACTION_REGEX
                     + "\".");
    }

    if (unlikely(not matcher->matched(filename)))
        LOG_ERROR("in ExtractDateFromFilenameOrDie: \"" + filename + "\" failed to match the regex \""
                 + DATE_EXTRACTION_REGEX + "\"!");

    return (*matcher)[1];
}


// Hopefully returns 'a', 'b' or 'c'.
char GetTypeCharOrDie(const std::string &member_name) {
    static auto matcher(RegexMatcher::RegexMatcherFactoryOrDie("([abc])\\d\\d\\d.raw"));
    if (not matcher->matched(member_name))
        LOG_ERROR("bad member type for archive member \"" + member_name + "\"!");

    return (*matcher)[1][0];
}


// Maps ".*[abc]???.raw" to ".*[abc]001.raw"
static inline std::string GenerateOutputMemberName(std::string member_name) {
    if (unlikely(member_name.length() < 8))
        LOG_ERROR("short archive member name \"" + member_name + "\"!");
    member_name[member_name.length() - 7] = '0';
    member_name[member_name.length() - 6] = '0';
    member_name[member_name.length() - 5] = '1';
    return member_name;
}


void ExtractArchiveMembers(const std::string &archive_name, std::vector<std::string> * const archive_members,
                           const std::string &optional_suffix)
{
    static auto member_matcher(RegexMatcher::RegexMatcherFactoryOrDie("[abc]\\d\\d\\d.raw$"));

    std::map<char, std::shared_ptr<File>> member_type_to_file_map;
    ArchiveReader reader(archive_name);
    ArchiveReader::EntryInfo file_info;
    while (reader.getNext(&file_info)) {
        const std::string member_name(file_info.getFilename());
        if (unlikely(not file_info.isRegularFile()))
            LOG_ERROR("unexpectedly, the entry \"" + member_name + "\" in \"" + archive_name + "\" is not a regular file!");
        if (unlikely(not member_matcher->matched(member_name)))
            LOG_ERROR("unexpected entry name \"" + member_name + "\"!");

        const char member_type(GetTypeCharOrDie(member_name));
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


} // namespace BSZUtil

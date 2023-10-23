/** \file   BSZUtil.h
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
#pragma once


#include <string>
#include <unordered_set>
#include <vector>
#include "File.h"
#include "MARC.h"


namespace BSZUtil {


constexpr size_t PPN_LENGTH_OLD(9);
constexpr size_t PPN_LENGTH_NEW(10);


/** \brief Extracts PPNs (ID's) from a LOEKXP file as provided by the BSZ.
 *  \note The sets will not be cleared so that multiple calls to this function will accumulate the ID's.
 */
void ExtractDeletionIds(File * const deletion_list, std::unordered_set<std::string> * const delete_full_record_ids,
                        std::unordered_set<std::string> * const local_deletion_ids);


/** Extracts a date in the form of YYMMDD from "filename". */
std::string ExtractDateFromFilenameOrDie(const std::string &filename);


enum ArchiveType { TITLE_RECORDS, SUPERIOR_TITLES, AUTHORITY_RECORDS, LOCAL_RECORDS };


// Assumes that "member_name" matches "([abc])\\d\\d\\d.raw" or "sekkor-(aut|tit|lok).mrc".
ArchiveType GetArchiveType(const std::string &member_name);


// Try to resolve the GND number for an author using the BSZ (SWB / OGND) website.
// The base URL can e.g. be taken from zotero_harvester.conf.
// This funciton is thread-safe, since it is used in zotero harvester.
std::string GetAuthorGNDNumber(const std::string &author, const std::string &author_lookup_base_url);


// Extracts members from "archive_name" combining those of the same type, e.g. members ending in "a001.raw" and "a002.raw" would
// be extracted as a single concatenated file whose name ends in "a001.raw".  If "optional_suffix" is not empty it will be appended
// to each filename.
// An enforced precondition is that all members must end in "[abc]\\d\\d\\d.raw$".
void ExtractArchiveMembers(const std::string &archive_name, std::vector<std::string> * const archive_members,
                           const std::string &optional_suffix = "");


struct IssueInfo {
    std::string year_;
    std::string volume_;
    std::string issue_;
    std::string month_;
    std::string pages_;
};

IssueInfo ExtractYearVolumeIssue(const MARC::Record &record);


/** \brief Attempts to extract a K10+ PPN from a subfield.
 *  \return The extracted PPN or the empty string if nothing was found.
 */
std::string GetK10PlusPPNFromSubfield(const MARC::Record::Field &field, const char subfield_code);


} // namespace BSZUtil

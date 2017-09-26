/** \file   DirectoryEntry.cc
 *  \brief  Implementation of the DirectoryEntry class.
 *  \author Dr. Johannes Ruscheinski (johannes.ruscheinski@uni-tuebingen.de)
 *
 *  \copyright 2014-2017 Universitätsbibliothek Tübingen.  All rights reserved.
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
#include "DirectoryEntry.h"
#include "util.h"


bool DirectoryEntry::ParseDirEntries(const std::string &entries_string, std::vector<DirectoryEntry> * const entries,
                                     std::string * const err_msg) {
    entries->clear();
    if (unlikely((entries_string.length() % DIRECTORY_ENTRY_LENGTH) != 1)) {
        if (err_msg != nullptr)
            *err_msg = "Raw directory entries string must be a multiple of " + std::to_string(DIRECTORY_ENTRY_LENGTH)
                       + " in length plus 1 for the record separator at the end of the directory! "
                       "Length found was " + std::to_string(entries_string.length()) + ".)";
        return false;
    }

    if (unlikely(entries_string[entries_string.length() - 1] != '\x1E')) {
        if (err_msg != nullptr)
            *err_msg = "Missing field terminator at end of directory!";
        return false;
    }

    const unsigned count(entries_string.length() / DIRECTORY_ENTRY_LENGTH);
    entries->reserve(count);
    size_t offset(0);
    for (unsigned i(0); i < count; ++i) {
        entries->emplace_back(DirectoryEntry(entries_string.substr(offset, DIRECTORY_ENTRY_LENGTH)));
        offset += DIRECTORY_ENTRY_LENGTH;
    }

    return true;
}

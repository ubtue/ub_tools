/** \file   BeaconFile.cc
 *  \brief  Implementation for the BeaconFile class.
 *  \author Dr. Johannes Ruscheinski (johannes.ruscheinski@uni-tuebingen.de)
 *
 *  \copyright 2019 Universitätsbibliothek Tübingen.  All rights reserved.
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
#include "BeaconFile.h"
#include "FileUtil.h"
#include "StringUtil.h"


static bool LookForLine(File * const input, const std::string &line_prefix, unsigned * const line_no, std::string * const line) {
    while (not input->eof()) {
        *line = input->getLineAny();
        ++*line_no;

        if (StringUtil::StartsWith(*line, line_prefix))
            return true;
    }

    return false;
}


// In order to understand what we do here, have a look at: https://gbv.github.io/beaconspec/beacon.html
BeaconFile::BeaconFile(const std::string &filename): filename_(filename) {
    const auto input(FileUtil::OpenInputFileOrDie(filename));

    unsigned line_no(0);

    std::string line(input->getLineAny());
    if (line != "#FORMAT: BEACON")
        LOG_ERROR("expected \"#FORMAT: BEACON\" as the first line in \"" + filename + "\"!");
    ++line_no;

    line = input->getLineAny();
    if (line != "#PREFIX: http://d-nb.info/gnd/")
        LOG_ERROR("expected \"#PREFIX: http://d-nb.info/gnd/\" as the second line in \"" + filename + "\"!");
    ++line_no;

    if (not LookForLine(input.get(), "#TARGET:", &line_no, &line))
        LOG_ERROR("unexpected EOF while looking for \"#TARGET:\" in \"" + filename + "\"!");

    url_template_ = StringUtil::Trim(line.substr(__builtin_strlen("#TARGET:")));
    if (url_template_.find("{ID}") == std::string::npos)
        LOG_ERROR("{ID} is missing in URL template \"" + url_template_ + " in \"" + filename + "\"!");

    do {
        line = input->getLineAny();
        ++line_no;
    } while (not line.empty() and line[0] == '#');

    do {
        ++line_no;

        std::string gnd_number;
        unsigned count(0);
        std::string id_or_url;

        const auto first_vertical_bar_pos(line.find('|'));
        if (first_vertical_bar_pos == std::string::npos)
            gnd_number = StringUtil::TrimWhite(line);
        else {
            gnd_number = StringUtil::TrimWhite(line.substr(0, first_vertical_bar_pos));
            const auto second_vertical_bar_pos(line.find('|', first_vertical_bar_pos + 1));
            if (second_vertical_bar_pos == std::string::npos) {
                const std::string count_str(StringUtil::TrimWhite(line.substr(first_vertical_bar_pos + 1)));
                if (not count_str.empty() and not StringUtil::ToUnsigned(count_str, &count))
                    LOG_ERROR("bad count \"" + count_str + "\" on line \"" + std::to_string(line_no) + "! (1)");
            } else {
                const std::string count_str(StringUtil::TrimWhite(line.substr(first_vertical_bar_pos + 1,
                                                                              second_vertical_bar_pos - first_vertical_bar_pos - 1)));
                if (not count_str.empty() and not StringUtil::ToUnsigned(count_str, &count))
                    LOG_ERROR("bad count \"" + count_str + "\" on line \"" + std::to_string(line_no) + "! (2)");

                id_or_url = StringUtil::TrimWhite(line.substr(second_vertical_bar_pos + 1));
            }
        }
        entries_.emplace_back(gnd_number, count, id_or_url);

        line = input->getLineAny();
    } while (not input->eof());
}


std::string BeaconFile::getURL(const Entry &entry) const {
    if (not entry.id_or_url_.empty()) {
        if (StringUtil::StartsWith(entry.id_or_url_, "http://") or StringUtil::StartsWith(entry.id_or_url_, "https://"))
            return entry.id_or_url_;
        return StringUtil::ReplaceString("{ID}", entry.id_or_url_, url_template_);
    }

    return StringUtil::ReplaceString("{ID}", entry.gnd_number_, url_template_);
}

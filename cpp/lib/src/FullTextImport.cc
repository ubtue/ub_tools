 /*
 *  \brief   Implementation of classes and functions related to the importing of full-text into Elasticsearch
 *  \author  Madeeswaran Kannan
 *
 *  Copyright (C) 2018, Library of the University of Tübingen
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

#include "FullTextImport.h"


namespace FullTextImport {


const std::string CHUNK_DELIMITER("\n\n\n");
const std::string PARAGRAPH_DELIMITER("\n\n");


void WriteExtractedTextToDisk(const std::string &full_text, const std::string &title, const std::set<std::string> &authors,
                              const std::string &year, const std::string &doi, File * const output_file)
{
    output_file->writeln(title);
    output_file->writeln(StringUtil::Join(authors, '|'));
    output_file->writeln(year);
    output_file->writeln(doi);
    output_file->write(full_text);
}


void ReadExtractedTextFromDisk(File * const input_file, FullTextData * const full_text_data) {
    unsigned line_no(1);
    std::string full_text;
    while (not input_file->eof()) {
        const auto line(input_file->getline());

        switch (line_no) {
        case 1:
            full_text_data->title_ = line;
            break;
        case 2:
            StringUtil::Split(line, '|', &full_text_data->authors_);
            break;
        case 3:
            full_text_data->year_ = line;
            break;
        case 4:
            full_text_data->doi_ = line;
            break;
        default:
            full_text += line + "\n";
        }

        ++line_no;
    }

    // split the full-text into sections
    size_t chunk_search_start_index(0);
    size_t chunk_delimiter_index(full_text.find(CHUNK_DELIMITER));

    if (chunk_delimiter_index == std::string::npos) {
        // just one big chunk
        full_text_data->full_text_.emplace_back(full_text);
        return;
    }

    do {
        const auto chunk(StringUtil::Trim(full_text.substr(chunk_search_start_index,
                                                           chunk_delimiter_index - chunk_search_start_index), '\n'));
        if (chunk.length() != 0)
            full_text_data->full_text_.emplace_back(chunk);

        chunk_search_start_index = chunk_delimiter_index + CHUNK_DELIMITER.length();
        chunk_delimiter_index = full_text.find(CHUNK_DELIMITER, chunk_search_start_index);

    } while (chunk_delimiter_index != std::string::npos);
}


size_t CorrelateFullTextData(const std::vector<std::shared_ptr<FullTextData>> &full_text_data,
                             std::unordered_map<std::string, std::shared_ptr<FullTextData>> * const control_number_to_full_text_data_map)
{
    size_t exact_matches(0);
    ControlNumberGuesser ppn_guesser;
    for (const auto &full_text : full_text_data) {
        const auto matching_ppns(ppn_guesser.getGuessedControlNumbers(full_text->title_, full_text->authors_,
                                                                      full_text->year_, full_text->doi_));
        if (matching_ppns.empty())
            continue;
        else if (matching_ppns.size() != 1) {
            LOG_DEBUG("multiple control numbers found for full-text with title '" + full_text->title_ + "', authors '" +
                      StringUtil::Join(full_text->authors_, ",") + "'");
            LOG_DEBUG("control numbers: " + StringUtil::Join(matching_ppns, ","));

            continue;
        }

        ++exact_matches;
        const auto &exact_ppn(*matching_ppns.begin());
        const auto existing_match(control_number_to_full_text_data_map->find(exact_ppn));
        if (existing_match != control_number_to_full_text_data_map->end() and
            (existing_match->second->title_ != full_text->title_ or existing_match->second->authors_ != full_text->authors_))
        {
            LOG_WARNING("control number '" + exact_ppn + "' has multiple full-text data matches");
            LOG_WARNING("\texisting data: title '" + existing_match->second->title_ + "', authors '" +
                        StringUtil::Join(existing_match->second->authors_, ",") + "'");
            LOG_WARNING("\tincoming data: title '" + full_text->title_ + "', authors '" +
                        StringUtil::Join(full_text->authors_, ",") + "'");
        }

        (*control_number_to_full_text_data_map)[exact_ppn] = full_text;
    }

    return exact_matches;
}


} // namespace FullTextImport

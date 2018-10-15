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


void WriteExtractedTextToDisk(const std::string &full_text, const std::string &normalised_title,
                              const std::set<std::string> &normalised_authors, File * const output_file)
{

    output_file->write(normalised_title + "\n");

    const std::string authors_str(StringUtil::Join(normalised_authors, '|'));
    output_file->write(authors_str + "\n");
    output_file->write(full_text);
}


void ReadExtractedTextFromDisk(File * const input_file, FullTextData * const full_text_data) {
    unsigned line_no(1);
    std::string full_text;
    while (not input_file->eof()) {
        const auto line(input_file->getline());

        if (line_no == 1)
            full_text_data->normalised_title_ = line;
        else if (line_no == 2)
            StringUtil::Split(line, '|', &full_text_data->normalised_authors_);
        else
            full_text += line + "\n";

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


} // namespace FullTextImport

/*
 *  \brief   Implementation of classes and functions related to the importing of full-text into Elasticsearch
 *  \author  Madeeswaran Kannan
 *
 *  Copyright (c) 2018-2021 Library of the University of Tübingen
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
#include "StringUtil.h"


namespace FullTextImport {


std::string FullTextData::toString(const char separator) const {
    const std::string SEPARATOR(1, separator);
    std::string as_string;
    as_string += "FullTextData:" + SEPARATOR;
    if (not title_.empty())
        as_string += "title: \"" + title_ + "\"" + SEPARATOR;
    if (not authors_.empty())
        as_string += "authors: \"" + StringUtil::Join(authors_, ',') + "\"" + SEPARATOR;
    if (not year_.empty())
        as_string += "year: " + year_ + SEPARATOR;
    if (not doi_.empty())
        as_string += "doi: " + doi_ + SEPARATOR;
    if (not issn_.empty())
        as_string += "issn: " + issn_ + SEPARATOR;
    if (not isbn_.empty())
        as_string += "isbn: " + isbn_ + SEPARATOR;
    if (not full_text_.empty())
        as_string += "full text: \"" + full_text_ + "\"" + SEPARATOR;

    return as_string;
}


const std::string CHUNK_DELIMITER("\n\n\n");
const std::string PARAGRAPH_DELIMITER("\n\n");


void WriteExtractedTextToDisk(const std::string &full_text, const std::string &title, const std::set<std::string> &authors,
                              const std::string &year, const std::string &doi, const std::string &issn, const std::string &isbn,
                              const std::string &text_type, const std::string &full_text_location, File * const output_file) {
    output_file->writeln(title);
    output_file->writeln(StringUtil::Join(authors, '|'));
    output_file->writeln(year);
    output_file->writeln(doi);
    output_file->writeln(issn);
    output_file->writeln(isbn);
    output_file->writeln(text_type);
    output_file->writeln(full_text_location);
    output_file->write(full_text);
}


void ReadExtractedTextFromDisk(File * const input_file, FullTextData * const full_text_data) {
    unsigned line_no(1);
    while (not input_file->eof()) {
        const auto line(input_file->getline());

        switch (line_no) {
        case 1:
            full_text_data->title_ = line;
            break;
        case 2:
            StringUtil::Split(line, '|', &full_text_data->authors_, /* suppress_empty_components = */ true);
            break;
        case 3:
            full_text_data->year_ = line;
            break;
        case 4:
            full_text_data->doi_ = line;
            break;
        case 5:
            full_text_data->issn_ = line;
            break;
        case 6:
            full_text_data->isbn_ = line;
            break;
        case 7:
            full_text_data->text_type_ = line;
            break;
        case 8:
            full_text_data->full_text_location_ = line;
            break;
        default:
            full_text_data->full_text_ += line + "\n";
        }

        ++line_no;
    }
}


bool CorrelateFullTextData(const ControlNumberGuesser &control_number_guesser, const FullTextData &full_text_data,
                           std::set<std::string> * const control_numbers) {
    *control_numbers =
        control_number_guesser.getGuessedControlNumbers(full_text_data.title_, full_text_data.authors_, full_text_data.year_,
                                                        { full_text_data.doi_ }, { full_text_data.issn_ }, { full_text_data.isbn_ });
    if (control_numbers->empty())
        return false;

    return true;
}


bool CorrelateFullTextData(const ControlNumberGuesser &control_number_guesser, const FullTextData &full_text_data,
                           std::string * const control_number) {
    std::set<std::string> matching_ppns;
    if (unlikely(not CorrelateFullTextData(control_number_guesser, full_text_data, &matching_ppns)))
        return false;

    if (matching_ppns.size() > 1)
        LOG_WARNING(std::to_string(matching_ppns.size()) + " matching PPNs found!");

    *control_number = *matching_ppns.cbegin();
    return true;
}


} // namespace FullTextImport

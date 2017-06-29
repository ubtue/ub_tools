/** \file fix_iso_8859_1_garbage.cc
 *  \brief Tries to convert some ISO-8859-1 characters to UTF-8.
 *  \author Dr. Johannes Ruscheinski (johannes.ruscheinski@uni-tuebingen.de)
 *
 *  \copyright 2017 Universitätsbibliothek Tübingen.  All rights reserved.
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
#include <iostream>
#include <cstdlib>
#include <cstring>
#include "Compiler.h"
#include "FileUtil.h"
#include "StringUtil.h"
#include "util.h"


void Usage() {
    std::cerr << "Usage: " << ::progname << " input_filename output_filename\n";
   std::exit(EXIT_FAILURE);
}


inline bool IsConsonant(const char ch) {
    return std::strchr("bcdfghjklmnpqrstvwxyzBCDFGHJKLMNPQRSTVWXYZ", ch) != nullptr;
}


inline bool IsLowercaseISO889_15Vowel(const char ch) {
    return std::strchr("aeiou\344\366\374", ch) != nullptr;
}


void Convert(File * const input, File * const output) {
    bool last_char_was_consonant(false), last_char_was_lowercase_vowel(false), last_character_was_space(false);
    int ch;
    while ((ch = input->get()) != EOF) {
        switch (ch) {
        case '\344': // LATIN SMALL LETTER A WITH DIAERESIS
        case '\366': // LATIN SMALL LETTER O WITH DIAERESIS
        case '\374': // LATIN SMALL LETTER U WITH DIAERESIS
            if (last_char_was_consonant or last_character_was_space)
                output->write(StringUtil::ISO8859_15ToUTF8(static_cast<char>(ch)));
            else
                output->put(static_cast<char>(ch));
            break;
        case '\304': // LATIN LARGE LETTER A WITH DIAERESIS
        case '\326': // LATIN LARGE LETTER O WITH DIAERESIS
        case '\334': // LATIN LARGE LETTER U WITH DIAERESIS
            if (last_character_was_space)
                output->write(StringUtil::ISO8859_15ToUTF8(static_cast<char>(ch)));
            else
                output->put(static_cast<char>(ch));
            break;
        case '\337': // LATIN SMALL LETTER SHARP S
            if (last_char_was_lowercase_vowel)
                output->write(StringUtil::ISO8859_15ToUTF8(static_cast<char>(ch)));
            else
                output->put(static_cast<char>(ch));
            break;
        default:
            output->put(static_cast<char>(ch));
        }

        last_char_was_consonant       = IsConsonant(static_cast<char>(ch));
        last_char_was_lowercase_vowel = IsLowercaseISO889_15Vowel(static_cast<char>(ch));
        last_character_was_space      = (ch == ' ');
    }
}


int main(int argc, char *argv[]) {
    if (argc != 3)
        Usage();

    std::unique_ptr<File> input(FileUtil::OpenInputFileOrDie(argv[1]));
    std::unique_ptr<File> output(FileUtil::OpenOutputFileOrDie(argv[2]));
    Convert(input.get(), output.get());
}

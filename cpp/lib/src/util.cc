/** \file    util.cc
 *  \brief   Implementation of various utility functions.
 *  \author  Dr. Johannes Ruscheinski
 */

/*
    Copyright (C) 2015, Library of the University of Tübingen

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
#include "util.h"
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <cctype>
#include <cstdio>


char *progname; // Must be set in main() with "progname = argv[0];";


void Error(const std::string &msg) {
    if (progname == nullptr)
        std::cerr << "You must set \"progname\" in main() with \"progname = argv[0];\" in oder to use Error().\n";
    else
        std::cerr << progname << ": " << msg << '\n';
    std::exit(EXIT_FAILURE);
}


void Warning(const std::string &msg) {
    if (progname == nullptr)
        std::cerr << "You must set \"progname\" in main() with \"progname = argv[0];\" in oder to use Warning().\n";
    else
        std::cerr << progname << ": " << msg << '\n';
}


bool ReadFile(const std::string &filename, std::string * const contents) {
    FILE *input(std::fopen(filename.c_str(), "r"));
    if (input == nullptr)
        return false;

    contents->clear();
    while (not std::feof(input)) {
        char buf[BUFSIZ];
        const size_t byte_count(std::fread(buf, 1, sizeof buf, input));
        if (byte_count != sizeof(buf) and std::ferror(input)) {
            std::fclose(input);
            return false;
        }
        contents->append(buf, byte_count);
    }

    std::fclose(input);
    return true;
}


DSVReader::DSVReader(const std::string &filename, const char field_separator, const char field_delimiter)
    : field_separator_(field_separator), field_delimiter_(field_delimiter), line_no_(0), filename_(filename)
{
    input_ = std::fopen(filename.c_str(), "rm");
    if (input_ == nullptr)
	throw std::runtime_error("in DSVReader::DSVReader: can't open \"" + filename + "\" for reading!");
}


DSVReader::~DSVReader() {
    if (input_ != nullptr)
	std::fclose(input_);
}


namespace {


void SkipFieldPadding(FILE * const input) {
    int ch = std::fgetc(input);
    while (isblank(ch))
	ch = std::fgetc(input);
    std::ungetc(ch, input);
}


std::string ReadQuotedValue(FILE * const input, const char field_delimiter) {
    std::string value;
    bool delimiter_seen(false);
    for (;;) {
	const int ch(std::fgetc(input));
	if (ch == EOF)
	    throw std::runtime_error("unexpected EOF while reading a quoted DSV value!");
	if (ch == field_delimiter) {
	    if (delimiter_seen) {
		value += static_cast<char>(ch);
		delimiter_seen = false;
	    } else
		delimiter_seen = true;
	} else if (delimiter_seen) {
	    std::ungetc(ch, input);
	    return value;
	} else
	    value += static_cast<char>(ch);
    }
}


/** \brief Remove trailing spaces and tabs from "s". */
std::string TrimBlanks(std::string * s) {
    std::string::const_reverse_iterator it(s->crbegin());
    for (/* Empty! */; it != s->crend() and std::isblank(*it); ++it)
	/* Intentionally Empty! */;
    if (it != s->crbegin())
	*s = s->substr(0, std::distance(it, s->crend()));

    return *s;
}


std::string ReadNonQuotedValue(FILE * const input, const char field_separator) {
    std::string value;
    for (;;) {
	const int ch(std::fgetc(input));
	if (ch == EOF or ch == '\n' or ch == field_separator) {
	    std::ungetc(ch, input);
	    return TrimBlanks(&value);
	}
	value += static_cast<char>(ch);
    }
}


} // unnamed namespace


bool DSVReader::readLine(std::vector<std::string> * const values) {
    values->clear();
    ++line_no_;

    int ch;
    for (;;) {
	if (not values->empty()) {
	    SkipFieldPadding(input_);
	    ch = std::fgetc(input_);
	    if (ch == EOF)
		return false;
	    if (ch == '\n')
		return true;
	    if (ch != field_separator_)
		throw std::runtime_error("in DSVReader::readLine: on line " + std::to_string(line_no_)
					 + ": field separator expected, found '"
					 + std::string(1, static_cast<char>(ch)) + "' instead!");
	}

	SkipFieldPadding(input_);
	ch = std::fgetc(input_);
	if (ch == '\n')
	    return true;
	if (ch == EOF)
	    return false;
	if (ch == field_separator_) {
	    std::ungetc(ch, input_);
	    values->emplace_back("");
	} else if (ch == field_delimiter_)
	    values->emplace_back(ReadQuotedValue(input_, field_delimiter_));
	else {
	    std::ungetc(ch, input_);
	    values->emplace_back(ReadNonQuotedValue(input_, field_separator_));
	}
    }
}


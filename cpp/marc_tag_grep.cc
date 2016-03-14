/** \file marc_tag_grep.cc
 *  \brief Print the contents of MARC fields where the tags match a regular expression.
 *         MARC-21 records.
 *
 *  \author Dr. Johannes Ruscheinski (johannes.ruscheinski@uni-tuebingen.de)
 *
 *  \copyright 2016 Universitätsbiblothek Tübingen.  All rights reserved.
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
#include "DirectoryEntry.h"
#include "MarcUtil.h"
#include "RegexMatcher.h"
#include "util.h"


void Usage() {
    std::cerr << "Usage: " << progname << " tag_regex input_filename\n";
    std::exit(EXIT_FAILURE);
}


void TagGrep(const std::string &regex, const std::string &input_filename) {
    File input(input_filename, "r");
    if (not input)
        Error("can't open \"" + input_filename + "\" for reading!");

    std::string err_msg;
    RegexMatcher * const matcher(RegexMatcher::RegexMatcherFactory(regex, &err_msg));
    if (unlikely(matcher == nullptr))
	Error("bad regex: " + err_msg);

    unsigned count(0), field_matched_count(0), record_matched_count(0);
    while (const MarcUtil::Record record = MarcUtil::Record::XmlFactory(&input)) {
        ++count;

	const std::vector<std::string> &fields(record.getFields());
	const std::string &control_number(fields[0]);
	unsigned index(0);
	bool at_least_one_field_matched(false);
	for (const auto dir_entry : record.getDirEntries()) {
	    const std::string &tag(dir_entry.getTag());
	    if (matcher->matched(tag)) {
		std::cout << control_number << ':' << tag << ':' << fields[index] << '\n';
		++field_matched_count;
		at_least_one_field_matched = true;
	    }

	    ++index;
	}
	if (at_least_one_field_matched)
	    ++record_matched_count;
    }

    std::cerr << "Matched " << record_matched_count << " records of " << count << " overall records.\n";
    std::cerr << field_matched_count << " fields matched.\n";
}


int main(int argc, char **argv) {
    progname = argv[0];

    if (argc != 3)
        Usage();

    try {
	TagGrep(argv[1], argv[2]);
    } catch (const std::exception &x) {
	Error("caught exception: " + std::string(x.what()));
    }
}

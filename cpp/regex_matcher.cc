/** \brief Test program for the RegexMatcher class.
 *  \author Dr. Johannes Ruscheinski (johannes.ruscheinski@uni-tuebingen.de)
 *
 *  \copyright 2015 Universitätsbiblothek Tübingen.  All rights reserved.
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
#include <string>
#include <cstdlib>
#include <RegexMatcher.h>
#include <util.h>


void Usage() {
    std::cerr << "usage: " << ::progname << " regex test_string1 [test_string2 ... test_stringN]\n";
    std::exit(EXIT_FAILURE);
}


int main(int argc, char *argv[]) {
    ::progname = argv[0];

    if (argc < 3)
        Usage();

    try {
        std::string err_msg;
        RegexMatcher * const matcher(RegexMatcher::RegexMatcherFactory(argv[1], &err_msg));
        if (matcher == nullptr)
            Error("regex compile failed: " + err_msg);

        for (int arg_no(2); arg_no < argc; ++arg_no) {
            const std::string subject(argv[arg_no]);
            if (not matcher->matched(subject, &err_msg)) {
                if (err_msg.empty()) {
                    std::cout << '"' << subject << "\" was not matched!\n";
                    continue;
                }
                Error("match for subject \"" + subject +"\" failed! (" + err_msg + ")");
            }
            std::cout << subject << ":\n";
            for (unsigned group(0); group < matcher->getLastMatchCount(); ++group)
                std::cout << '\t' << (*matcher)[group] << '\n';
        }
    } catch (const std::exception &x) {
        Error("caught exception: " + std::string(x.what()));
    }
}

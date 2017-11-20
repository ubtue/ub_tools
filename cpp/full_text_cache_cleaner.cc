/** \brief Tool to delete old cache entries from the KrimDok full text cache.
 *  \author Dr. Johannes Ruscheinski (johannes.ruscheinski@uni-tuebingen.de)
 *
 *  \copyright 2015,2017 Universitätsbibliothek Tübingen.  All rights reserved.
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
#include <stdexcept>
#include <string>
#include <cstdlib>
#include <ctime>
#include "FullTextCache.h"
#include "util.h"


static void Usage() __attribute__((noreturn));


void Usage() {
    std::cerr << "Usage: " << ::progname << "\n";
    std::cerr << "       Deletes all expired records from the full text cache\n";
    std::exit(EXIT_FAILURE);
}


int main(int argc, char *argv[]) {
    ::progname = argv[0];

    if (argc != 1)
        Usage();

    try {
        FullTextCache cache;
        const unsigned size_before_deletion(cache.getSize());
        cache.expireEntries();
        const unsigned size_after_deletion(cache.getSize());

        std::cerr << "Deleted " << (size_before_deletion - size_after_deletion)
                  << " records from the full-text cache.\n";
    } catch (const std::exception &x) {
        logger->error("caught exception: " + std::string(x.what()));
    }
}

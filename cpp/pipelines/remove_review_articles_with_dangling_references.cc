/** \brief Delete review articles that have no referenced items.
 *  \author Dr. Johannes Ruscheinski
 *
 *  \copyright 2020 Universitätsbibliothek Tübingen.  All rights reserved.
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

#include <unordered_map>
#include <cstdio>
#include <cstdlib>
#include "Compiler.h"
#include "FileUtil.h"
#include "StringUtil.h"
#include "MARC.h"
#include "util.h"


namespace {


[[noreturn]] void Usage() {
    ::Usage("marc_input marc_output");
}


void CollectAllPPNs(MARC::Reader * const reader, std::unordered_set<std::string> * const all_ppns) {
   while (const auto record = reader->read())
      all_ppns->emplace(record.getControlNumber());
}


inline bool IsPartOfTitleData(const std::unordered_set<std::string> &all_ppns, const std::string &referenced_ppn) {
    return all_ppns.find(referenced_ppn) != all_ppns.cend();
}


const std::vector<MARC::Tag> REFERENCE_FIELDS{ MARC::Tag("787") };


void EliminateDanglingCrossReferences(MARC::Reader * const reader, MARC::Writer * const writer,
                                      const std::unordered_set<std::string> &all_ppns)
{
     unsigned unreferenced_ppns(0);
     while (const auto record = reader->read()) {
        for (const auto &field : record) {
            std::string referenced_ppn;
            if (record.isReviewArticle()
                and MARC::IsCrossLinkField(field, &referenced_ppn, REFERENCE_FIELDS)
                and not IsPartOfTitleData(all_ppns, referenced_ppn))
                continue;

            writer->write(record);
        }
     }
     LOG_INFO("Deleted " + std::to_string(unreferenced_ppns) + " records w/ dangling links.");
}


} // unnamed namespace


int Main(int argc, char *argv[]) {
    if (argc != 3)
        Usage();

    const auto marc_reader(MARC::Reader::Factory(argv[1]));
    const auto marc_writer(MARC::Writer::Factory(argv[2]));

    std::unordered_set<std::string> all_ppns;
    CollectAllPPNs(marc_reader.get(), &all_ppns);

    marc_reader->rewind();
    EliminateDanglingCrossReferences(marc_reader.get(), marc_writer.get(), all_ppns);

    return EXIT_SUCCESS;
}

/** \brief  A MARC-21 filter utility that adds ACO tags with entry $a set to 1 for article collections.
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
#include <string>
#include <unordered_set>
#include "MarcReader.h"
#include "MarcRecord.h"
#include "MarcUtil.h"
#include "MarcWriter.h"


void Usage() {
    std::cerr << "Usage: " << ::progname << " marc_input marc_output\n";
    std::exit(EXIT_FAILURE);
}


void CollectArticleCollectionPPNs(MarcReader * const reader,
                                  std::unordered_set<std::string> * const article_collection_ppns)
{
    article_collection_ppns->clear();
    while (const MarcRecord record = reader->read()) {
        if (MarcUtil::IsArticle(record)) {
            const std::string parent_ppn(MarcUtil::GetParentPPN(record));
            if (not parent_ppn.empty())
                article_collection_ppns->insert(parent_ppn);
        }
    }
}


void MarkArticleCollections(MarcReader * const reader, MarcWriter * const writer,
                            const std::unordered_set<std::string> &article_collection_ppns)
{
    unsigned count(0), modified_count(0);
    while (MarcRecord record = reader->read()) {
        ++count;

        bool is_article_collection(false);
        if (article_collection_ppns.find(record.getControlNumber()) != article_collection_ppns.end())
            is_article_collection = true;
        if (not is_article_collection and not MarcUtil::IsArticle(record)) {
            is_article_collection = MarcUtil::HasSubfieldWithValue(record, "935", 'c', "fe");
            if (not is_article_collection)
                is_article_collection = MarcUtil::HasSubfieldWithValue(record, "655", 'a', "Festschrift");
            if (not is_article_collection)
                is_article_collection = MarcUtil::HasSubfieldWithValue(record, "655", 'a', "Konferenzschrift");
            if (not is_article_collection)
                is_article_collection = MarcUtil::HasSubfieldWithValue(record, "689", 'a', "Konferenzschrift");
            if (not is_article_collection)
                is_article_collection = MarcUtil::HasSubfieldWithValue(record, "689", 'a', "Kongress");
            if (not is_article_collection)
                is_article_collection = MarcUtil::HasSubfieldWithValue(record, "935", 'c', "gkko");
        }

        if (is_article_collection) {
            record.insertSubfield("ACO", 'a', "1");
            ++modified_count;
        }

        writer->write(record);
    }

    std::cerr << "Read " << count << " records.\n";
    std::cerr << "Identified " << modified_count << " record(s) as an article collection.\n";
}


int main(int argc, char **argv) {
    ::progname = argv[0];

    if (argc != 3)
        Usage();
    
    std::unique_ptr<MarcReader> marc_reader(MarcReader::Factory(argv[1]));
    std::unique_ptr<MarcWriter> marc_writer(MarcWriter::Factory(argv[2]));
    try {
        std::unordered_set<std::string> article_collection_ppns;
        CollectArticleCollectionPPNs(marc_reader.get(), &article_collection_ppns);
        marc_reader->rewind();
        MarkArticleCollections(marc_reader.get(), marc_writer.get(), article_collection_ppns);
    } catch (const std::exception &x) {
        Error("caught exception: " + std::string(x.what()));
    }
}

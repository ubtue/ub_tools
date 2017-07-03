/** \file append_marc_xml.cc
 *  \brief Appends one MARC-XML file to another.
 *  \author Dr. Johannes Ruscheinski (johannes.ruscheinski@uni-tuebingen.de)
 *
 *  \copyright 2016,2017 Universitätsbibliothek Tübingen.  All rights reserved.
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
#include <string.h>
#include <sys/sendfile.h>
#include "Compiler.h"
#include "FileUtil.h"
#include "util.h"


void Usage() {
    std::cerr << "usage: " << progname << " source_marc_xml target_marc_xml\n";
    std::cerr << "       Appends \"source_marc_xml\" to \"target_marc_xml\".\n\n";
    std::exit(EXIT_FAILURE);
}


const off_t TAIL_OFFSET(20);


void PositionFileBeforeClosingCollectionTag(File * const file) {
    if (unlikely(file->size() < TAIL_OFFSET))
        Error("\"" + file->getPath() + "\" is too small to look for the </marc:collection> tag!");

    if (unlikely(not file->seek(-TAIL_OFFSET, SEEK_END)))
        Error("seek failed on \"" + file->getPath() + "\"! (" + std::string(::strerror(errno)) + ")");

    std::string tail(TAIL_OFFSET, '\0');
    if (unlikely(file->read((void *)tail.data(), TAIL_OFFSET) != TAIL_OFFSET))
        Error("this should never happen!");

    const std::string::size_type COLLECTION_CLOSING_TAG_START(tail.find("</marc:collection>"));
    if (unlikely(COLLECTION_CLOSING_TAG_START == std::string::npos))
        Error("could not find </marc:collection> in the last " + std::to_string(TAIL_OFFSET) + " bytes of \""
              + file->getPath() + "\"!");

    if (unlikely(not file->seek(-TAIL_OFFSET + COLLECTION_CLOSING_TAG_START - 1, SEEK_END)))
        Error("seek to byte immediately before </marc:collection> failed on \"" + file->getPath() + "\"! ("
              + std::string(::strerror(errno)) + ")");
}


const off_t MIN_SOURCE_SIZE(350);


off_t PositionFileAtFirstRecordStart(File * const file) {
    if (unlikely(file->size() < MIN_SOURCE_SIZE))
        Error("\"" + file->getPath() + "\" is too small to look for the first <marc:record> tag!");

    std::string head(MIN_SOURCE_SIZE, '\0');
    if (unlikely(file->read((void *)head.data(), MIN_SOURCE_SIZE) != MIN_SOURCE_SIZE))
        Error("this should never happen (2)!");

    const std::string::size_type RECORD_TAG_START(head.find("<marc:record>"));
    if (unlikely(RECORD_TAG_START == std::string::npos))
        Error("could not find <marc:record> in the first " + std::to_string(MIN_SOURCE_SIZE) + " bytes of \""
              + file->getPath() + "\"!");

    if (unlikely(not file->seek(RECORD_TAG_START)))
        Error("seek failed on \"" + file->getPath() + "\"! (" + std::string(::strerror(errno)) + ")");

    return RECORD_TAG_START;
}


/** \brief Appends "source" to "target". */
void Append(File * const source, File * const target) {
    PositionFileBeforeClosingCollectionTag(target);
    const off_t record_tag_start(PositionFileAtFirstRecordStart(source));
    if (unlikely(not FileUtil::Copy(source, target, source->size() - record_tag_start)))
        Error("copying failed: " + std::string(::strerror(errno)));
}


int main(int argc, char *argv[]) {
    ::progname = argv[0];

    if (argc != 3)
        Usage();

    const std::unique_ptr<File> source(FileUtil::OpenInputFileOrDie(argv[1]));
    std::unique_ptr<File> target(new File(argv[2], "r+"));
    if (not target->fail())
        Error("can't open \"" + std::string(argv[2]) + "\" for reading and writing!");

    try {
        Append(source.get(), target.get());
    } catch (const std::exception &x) {
        Error("caught exception: " + std::string(x.what()));
    }
}

/** \brief A tool to compare two marc files, regardless of the file format.
 *  \author Oliver Obenland (oliver.obenland@uni-tuebingen.de)
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
#include <string>
#include "FileUtil.h"
#include "Leader.h"
#include "MarcReader.h"
#include "MarcRecord.h"
#include "MarcWriter.h"
#include "MediaTypeUtil.h"
#include "util.h"


void Usage() {
    std::cerr << "Usage: " << ::progname << " marc_lhs marc_rhs\n\n";
    std::exit(EXIT_FAILURE);
}


static bool lhs_is_xml(false);
static bool rhs_is_xml(false);


void compare(File * const lhs_file, File * const rhs_file) {
    while(true) {
        MarcRecord lhs = lhs_is_xml ? MarcReader::ReadXML(lhs_file) : MarcReader::Read(lhs_file);
        MarcRecord rhs = rhs_is_xml ? MarcReader::ReadXML(rhs_file) : MarcReader::Read(rhs_file);

        if (not lhs and not rhs)
            break;
        if (not lhs)
            Error(lhs_file->getPath() + " has less records than " + rhs_file->getPath());
        if (not rhs)
            Error(lhs_file->getPath() + " has more records than " + rhs_file->getPath());

        if (lhs.getControlNumber() != rhs.getControlNumber())
            Error("PPN mismatch:\nLHS: " + lhs.getControlNumber() + "\nRHS: " + rhs.getControlNumber());

        if (lhs.getNumberOfFields() != rhs.getNumberOfFields())
            Error("Number of fields (" + lhs.getControlNumber() + "):\nLHS: " + std::to_string(lhs.getNumberOfFields()) + "\nRHS: " + std::to_string(rhs.getNumberOfFields()));

        for (size_t index(0); index < lhs.getNumberOfFields(); ++index) {
            if (lhs.getTag(index) != rhs.getTag(index))
                Error("Tag mismatch (" + lhs.getControlNumber() + "):\nLHS: " + lhs.getTag(index).to_string() + "\nRHS: " + rhs.getTag(index).to_string());

            std::string lhs_data(lhs.getFieldData(index));
            std::string rhs_data(rhs.getFieldData(index));
            while (lhs_data.find("\x1F") != std::string::npos)
                lhs_data.replace(lhs_data.find("\x1F"), 1, " $");
            while (rhs_data.find("\x1F") != std::string::npos)
                rhs_data.replace(rhs_data.find("\x1F"), 1, " $");
            if (lhs_data.compare(rhs_data))
                Error("Subfield mismatch (" + lhs.getControlNumber() + ", Tag: " + lhs.getTag(index).to_string() + "): \nLHS:" + lhs_data + "\nRHS:" + rhs_data);
        }
    }
}


bool IsXML(File * const file) {
    const std::string media_type(MediaTypeUtil::GetFileMediaType(file->getPath()));
    if (unlikely(media_type.empty()))
        Error("can't determine media type of \"" + file->getPath() + "\"!");
    if (media_type != "application/xml" and media_type != "application/marc")
        Error("\"" + file->getPath() + "\" is neither XML nor MARC-21 data! It is: " + media_type);
    return (media_type == "application/xml");
}


int main(int argc, char **argv) {
    ::progname = argv[0];

    if (argc < 3)
        Usage();

    std::unique_ptr<File> lhs(FileUtil::OpenInputFileOrDie(argv[1]));
    std::unique_ptr<File> rhs(FileUtil::OpenInputFileOrDie(argv[2]));

    lhs_is_xml = IsXML(lhs.get());
    rhs_is_xml = IsXML(rhs.get());

    compare(lhs.get(), rhs.get());

    return 0;
}
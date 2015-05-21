/** \file   PdfUtil.cc
 *  \brief  Implementation of functions relating to PDF documents.
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
#include "PdfUtil.h"
#include <unistd.h>
#include "ExecUtil.h"
#include "FileUtil.h"
#include "util.h"


const std::string PDFFONTS("/usr/bin/pdffonts");


bool PdfFileContainsNoText(const std::string &path) {
    char filename_template[] = "/tmp/PdfFileContainsNoTextXXXXXX";
    const std::string output_filename(::mktemp(filename_template));
    const AutoDeleteFile auto_delete(output_filename);
    std::vector<std::string> args{ path };
    const int retval = Exec(PDFFONTS, args, output_filename);
    if (retval == 0) {
	std::string output;
	if (not ReadFile(output_filename, &output))
	    return false;
	return output.length() == 188; // Header only?
    }

    return retval == 0;
}


bool PdfDocContainsNoText(const std::string &document) {
    char filename_template[] = "/tmp/PdfDocContainsNoTextXXXXXX";
    const std::string output_filename(::mktemp(filename_template));
    const AutoDeleteFile auto_delete(output_filename);
    if (not WriteString(output_filename, document))
	return false;
    return PdfFileContainsNoText(output_filename);
}

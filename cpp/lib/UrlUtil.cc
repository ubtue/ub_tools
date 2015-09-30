/** \file   UrlUtil.cc
 *  \brief  Utility functions related to the processing or genewration of URLs.
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
#include "UrlUtil.h"
#include <iomanip>
#include <sstream>
#include <cctype>


namespace UrlUtil {


std::string UrlEncode(const std::string &s) {
    std::ostringstream encoded;
    encoded.fill('0');
    encoded << std::hex;

    for (const char ch : s) {
        // Keep alphanumeric and other accepted characters intact:
        if (std::isalnum(ch) || ch == '-' || ch == '_' || ch == '.' || ch == '~')
            encoded << ch;
	else { // Any other characters are percent-encoded.
	    encoded << std::uppercase;
	    encoded << '%' << std::setw(2) << static_cast<int>(static_cast<unsigned char>(ch));
	    encoded << std::nouppercase;
	}
    }

    return encoded.str();
}


} // namespace UrlUtil

/** \file    MiscUtil.cc
 *  \brief   Declarations of miscellaneous utility functions.
 *  \author  Dr. Johannes Ruscheinski
 *  \author  Dr. Gordon W. Paynter
 */

/*
 *  Copyright 2002-2008 Project iVia.
 *  Copyright 2002-2008 The Regents of The University of California.
 *
 *  This file is part of the libiViaCore package.
 *
 *  The libiViaCore package is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public License as
 *  published by the Free Software Foundation; either version 2 of the License,
 *  or (at your option) any later version.
 *
 *  libiViaCore is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public License
 *  along with libiViaCore; if not, write to the Free Software Foundation, Inc.,
 *  59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "MiscUtil.h"
#include "util.h"


namespace MiscUtil {


char HexDigit(const unsigned value) {
    switch (value) {
    case 0:
	return '0';
    case 1:
	return '1';
    case 2:
	return '2';
    case 3:
	return '3';
    case 4:
	return '4';
    case 5:
	return '5';
    case 6:
	return '6';
    case 7:
	return '7';
    case 8:
	return '8';
    case 9:
	return '9';
    case 0xA:
	return 'A';
    case 0xB:
	return 'B';
    case 0xC:
	return 'C';
    case 0xD:
	return 'D';
    case 0xE:
	return 'E';
    case 0xF:
	return 'F';
    default:
	Error("in MiscUtil::HexDigit: invalid value " + std::to_string(value) + "!");
    }
}


} // namespace MiscUtil

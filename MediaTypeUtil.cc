/** \file    MediaTypeUtil.cc
 *  \brief   Implementation of Media Type utility functions.
 *  \author  Dr. Gordon W. Paynter
 *  \author  Dr. Johannes Ruscheinski
 */

/*
 *  Copyright 2004-2008 Project iVia.
 *  Copyright 2004-2008 The Regents of The University of California.
 *
 *  This file is part of the libiViaCore package.
 *
 *  The libiViaCore package is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public License as published
 *  by the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  libiViaCore is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public License
 *  along with libiViaCore; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "MediaTypeUtil.h"
#include <stdexcept>
#include <cctype>
#include <magic.h>
#include "StringUtil.h"


namespace MediaTypeUtil {


// GetMediaType -- Get the media type of a document.
//
std::string GetMediaType(const std::string &document, const bool auto_simplify)
{
	if (document.empty())
		return "";

	// 1. Next try libmagic:
	const magic_t cookie = ::magic_open(MAGIC_MIME);
	if (unlikely(cookie == NULL))
		throw std::runtime_error("in MediaTypeUtil::GetMediaType: could not open libmagic!");

	// Load the default "magic" definitions file:
	if (unlikely(::magic_load(cookie, NULL /* use default magic file */) != 0)) {
		::magic_close(cookie);
		throw std::runtime_error("in MediaTypeUtil::GetMediaType: could not load libmagic ("
					 + std::string(::magic_error(cookie)) + ").");
	}

	// Use magic to get the mime type of the buffer:
 	const char *magic_mime_type = ::magic_buffer(cookie, document.c_str(), document.length());
	if (unlikely(magic_mime_type == NULL)) {
		::magic_close(cookie);
		throw std::runtime_error("in MediaTypeUtil::GetMediaType: error in libmagic ("
					 + std::string(::magic_error(cookie)) + ").");
	}

	// Attempt to remove possible leading junk (no idea why libmagic behaves in this manner every now and then):
	if (std::strncmp(magic_mime_type, "\\012- ", 6) == 0)
		magic_mime_type += 6;

	std::string media_type(magic_mime_type);

	// Close the library:
	::magic_close(cookie);

	// 2. If libmagic could not determine the document's MIME type, test for XML:
	if (media_type.empty() and document.size() > 5)
		return std::strncmp(document.c_str(), "<?xml", 5) == 0 ? "text/xml" : "";

	if (auto_simplify)
		SimplifyMediaType(&media_type);

	return media_type;
}


bool SimplifyMediaType(std::string * const media_type)
{
	if (media_type->empty())
		return false;

	// This is the format of the Content-Type field for Web pages:
	// 	media-type     = type "/" subtype *( ";" parameter )
	// 	type           = token
	//      subtype        = token
	// See: http://www.w3.org/Protocols/rfc2616/rfc2616-sec3.html#sec3.7

	const std::string initial_media_type(*media_type);

	// Search for a semicolon and delete any 'parameter' parts:
	const std::string::size_type semicolon_pos(media_type->find(';'));
	if (semicolon_pos != std::string::npos)
		media_type->resize(semicolon_pos);
	else { // Try a space instead of a semicolon.
		const std::string::size_type space_pos(media_type->find(' '));
		if (space_pos != std::string::npos)
			media_type->resize(space_pos);
	}

	StringUtil::TrimWhite(media_type);

	// Return if "media_type" has changed:
	return initial_media_type != *media_type;
}


} // namespace MediaTypeUtil

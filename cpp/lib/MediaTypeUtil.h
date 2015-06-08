/** \file    MediaTypeUtil.h
 *  \brief   Declarations of MIME/media type utility functions.
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

#ifndef MEDIA_TYPE_UTIL_H
#define MEDIA_TYPE_UTIL_H


#include <string>


/** \namespace  MediaTypeUtil
 *  \brief      Utility functions for determining the MIME media type of a resource.
 *
 *  In the functions below, the media type will be a media type (a.k.a. "MIME type") as defined in the MIME RFC 2046.
 *  (Note the HTTP 1.1 RFC 2616 has an error in Section 3.7 when referring to media types: see the RFC 2046 errata.)
 *
 */
namespace MediaTypeUtil {


/** \brief  Get the media type of a document.
 *  \param  document       The document to analyse.
 *  \param  auto_simplify  If "true", calls SimplifyMediaType() before returning the type.
 *  \return The media type of the document, or an empty string if it is unknown.
 *
 *  This function will attempt to get the media type of the document, first by using GetHtmlMediaType, then by using libmagic, and, if that
 *  fails it will test for XML by comparing the document's first five characters against "&lt;?xml".
 */
std::string GetMediaType(const std::string &document, const bool auto_simplify = true);


/** \brief  Cleans up a media type by trimming parameters.
 *  \param  media_type  The media type to clean up.
 *  \return True if the media type has changed, otherwise false.
 */
bool SimplifyMediaType(std::string * const media_type);


} // namespace MediaTypeUtil


#endif // ifndef MEDIA_TYPE_UTIL_H

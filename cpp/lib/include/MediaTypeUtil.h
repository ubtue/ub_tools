/** \file    MediaTypeUtil.h
 *  \brief   Declarations of MIME/media type utility functions.
 *  \author  Dr. Gordon W. Paynter
 *  \author  Dr. Johannes Ruscheinski
 */

/*
 *  Copyright 2004-2008 Project iVia.
 *  Copyright 2004-2008 The Regents of The University of California.
 *  Copyright 2016 Universitätsbibliothek Tübingen.
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


// Forward declarations:
class HttpHeader;
class Url;


/** \namespace  MediaTypeUtil
 *  \brief      Utility functions for determining the MIME media type of a resource.
 *
 *  In the functions below, the media type will be a media type (a.k.a. "MIME type") as defined in the MIME RFC 2046.
 *  (Note the HTTP 1.1 RFC 2616 has an error in Section 3.7 when referring to media types: see the RFC 2046 errata.)
 *
 */
namespace MediaTypeUtil {


/** \brief  Get the HTML media type of a document, if any.
 *  \param  document  The doument to analyse.
 *  \return Either "text/html", "text/xhtml" or nothing if the document does not appear to contain HTML.
 */
std::string GetHtmlMediaType(const std::string &document);


/** \brief  Get the media type of a document.
 *  \param  document       The document to analyse.
 *  \param  auto_simplify  If "true", calls SimplifyMediaType() before returning the type.
 *  \return The media type of the document, or an empty string if it is unknown.
 *
 *  This function will attempt to get the media type of the document, first by using GetHtmlMediaType, then by using libmagic, and, if that
 *  fails it will test for XML by comparing the document's first five characters against "&lt;?xml".
 */
std::string GetMediaType(const std::string &document, const bool auto_simplify = true);


/** \brief  Get the media type of a document.
 *  \param  filename       The file to analyse.
 *  \param  auto_simplify  If "true", calls SimplifyMediaType() before returning the type.
 *  \return The media type of the document, or an empty string if it is unknown.
 *
 *  This function will attempt to get the media type of the document, first by using GetHtmlMediaType, then by using libmagic, and, if that
 *  fails it will test for XML by comparing the document's first five characters against "&lt;?xml".
 */
std::string GetFileMediaType(const std::string &filename, const bool auto_simplify = true);


/** \brief  Get the media type of a Web document given its HTTP header and body.
 *  \param  page_header    The HTTP header of the document to analyse.
 *  \param  page_body      The document to analyse.
 *  \param  auto_simplify  If "true", calls SimplifyMediaType() before returning the type.
 *  \return The media type of the document, or an empty string if it is unknown.
 *
 *  This function will first attempt to get the Media Type of the document base on the HTTP header, and if that fails, it
 *  will use libmagic on the body.if that fails it will test for XML by comparing the document's first five characters
 *  against "&lt;?xml".
 */
std::string GetMediaType(const HttpHeader &http_header, const std::string &page_body, const bool auto_simplify = true);


/** \brief  Get the media type of a Web document given its HTTP header and body.
 *  \param  page_header    The HTTP header of the document to analyse.
 *  \param  page_body      The document to analyse.
 *  \param  auto_simplify  If "true", calls SimplifyMediaType() before returning the type.
 *  \return The media type of the document, or an empty string if it is unknown.
 *
 *  This function will first attempt to get the Media Type of the document base on the HTTP header, and if that fails, it
 *  will use libmagic on the body.if that fails it will test for XML by comparing the document's first five characters
 *  against "&lt;?xml".
 */
std::string GetMediaType(const std::string &page_header, const std::string &page_body, const bool auto_simplify = true);


/** \brief  Get the MediaType of a Web page.
 *  \param  url            The URL for the Web page.
 *  \param  http_header    The HTTP header for the Web page.
 *  \param  page_content   The document content (a.k.a. page body) for the Web page.
 *  \param  media_type     On success this will hold the media type of the Web page or at least a good guess as to what the media type might be.
 *  \param  auto_simplify  If "true", calls SimplifyMediaType() before returning the type.
 *  \return True if "media_type" has been determined, else false.
 */
bool GetMediaType(const Url &url, const HttpHeader &http_header, const std::string &page_content, std::string * const media_type,
                  const bool auto_simplify = true);


/** \brief  Get the MediaType of a Web page.
 *  \param  url            The URL for the Web page.
 *  \param  http_header    The HTTP header for the Web page.
 *  \param  page_content   The document content (a.k.a. page body) for the Web page.
 *  \param  media_type     On success this will hold the media type of the Web page or at least a good guess as to what the media type might be.
 *  \param  auto_simplify  If "true", calls SimplifyMediaType() before returning the type.
 *  \return True if "media_type" has been determined, else false.
 */
bool GetMediaType(const std::string &url, const HttpHeader &http_header, const std::string &page_content,
                  std::string * const media_type, const bool auto_simplify = true);


/** \brief  Cleans up a media type by trimming parameters.
 *  \param  media_type  The media type to clean up.
 *  \return True if the media type has changed, otherwise false.
 */
bool SimplifyMediaType(std::string * const media_type);


} // namespace MediaTypeUtil


#endif // ifndef MEDIA_TYPE_UTIL_H

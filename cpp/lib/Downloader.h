/** \file   Downloader.h
 *  \brief  Functions for downloading of web resources.
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
#ifndef DOWNLOADER_H
#define DOWNLOADER_H


#include <string>


/** \brief Downloads a Web document.
 *  \param url              The address.
 *  \param output_filename  Where to store the downloaded document.
 *  \param timeout          Max. amount of time to try to download a document.
 *  \param cookie_file      Cookies will be read before the attenpted download and later stored here.
 *  \return Exit code of the child process.  0 upon success.
 */
int Download(const std::string &url, const std::string &output_filename, const unsigned timeout,
             const std::string &cookie_file = "");


/** \brief Downloads a Web document.
 *  \param url      The address.
 *  \param timeout  Max. amount of time to try to download a document.
 *  \param output   Where to store the downloaded document.
 *  \param cookie_file      Cookies will be read before the attenpted download and later stored here.
 *  \return Exit code of the child process.  0 upon success.
 */
int Download(const std::string &url, const unsigned timeout, std::string * const output,
             const std::string &cookie_file = "");


#endif // ifndef DOWNLOADER_H

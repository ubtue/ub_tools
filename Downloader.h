#ifndef DOWNLOADER_H
#define DOWNLOADER_H


#include <string>


/** \brief Downloads a Web document.
 *  \param url              The address.
 *  \param output_filename  Where to store the downloaded document.
 *  \param timeout          Max. amount of time to try to download a document.
 *  \return Exit code of the child process.  0 upon success.
 */
int Download(const std::string &url, const std::string &output_filename, const unsigned timeout);


/** \brief Downloads a Web document.
 *  \param url      The address.
 *  \param timeout  Max. amount of time to try to download a document.
 *  \param output   Where to store the downloaded document.
 *  \return Exit code of the child process.  0 upon success.
 */
int Download(const std::string &url, const unsigned timeout, std::string * const output);


#endif // ifndef DOWNLOADER_H

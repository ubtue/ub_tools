/** \file    WebUtil.cc
 *  \brief   Implementation of WWW related utility functions.
 *  \author  Dr. Johannes Ruscheinski
 *  \author  Dr. Gordon W. Paynter
 *  \author  Artur Kedzierski
 */

/*
 *  Copyright 2002-2009 Project iVia.
 *  Copyright 2002-2009 The Regents of The University of California.
 *  Copyright 2017 Universitätsbibliothek Tübingen
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

#include "WebUtil.h"
#include <iostream>
#include <sstream>
#include "Compiler.h"
#include "FileDescriptor.h"
#include "FileUtil.h"
#include "HtmlParser.h"
#include "HttpHeader.h"
#include "MiscUtil.h"
#include "SslConnection.h"
#include "SocketUtil.h"
#include "StringUtil.h"
#include "TextUtil.h"
#include "UrlUtil.h"


namespace WebUtil {


std::string WwwFormUrlEncode(const StringMap &post_args,
                             const bool generate_content_type_and_content_length_headers) {
    std::string name_value_pairs;
    for (const auto &name_value_pair : post_args) {
        if (not name_value_pairs.empty())
            name_value_pairs += '&';

        std::string name(name_value_pair.first);
        UrlUtil::UrlEncode(&name);
        std::string value(name_value_pair.second);
        UrlUtil::UrlEncode(&value);

        name_value_pairs += name;
        name_value_pairs += '=';
        name_value_pairs += value;
    }

    std::string form_data;
    if (generate_content_type_and_content_length_headers) {
        form_data = "Content-Type: application/x-www-form-urlencoded\r\n";
        form_data += "Content-Length: ";
        form_data += std::to_string(name_value_pairs.length());
        form_data += "\r\n\r\n";
    }

    form_data += name_value_pairs;
    return form_data;
}


bool ProcessPOST(const std::string &username_password, const std::string &address, const unsigned short port,
                 const TimeLimit &time_limit, const std::string &path, const StringMap &post_args,
                 std::string * const document_source, std::string * const error_message, const std::string &accept,
                 const bool include_http_header)
{
    document_source->clear();
    error_message->clear();

    try {
        std::string tcp_connect_error_message;
        const FileDescriptor socket_fd(SocketUtil::TcpConnect(address, port, time_limit, &tcp_connect_error_message));
        if (socket_fd == -1) {
            *error_message = "Could not open TCP connection to " + address + ", port "
                + std::to_string(port) + ": " + tcp_connect_error_message;
            *error_message += " (Time remaining: " + std::to_string(time_limit.getRemainingTime())
                + ").";
            return false;
        }

        std::string data_to_be_sent("POST ");
        data_to_be_sent += path;
        data_to_be_sent += " HTTP/1.0\r\n";
        data_to_be_sent += "Host: ";
        data_to_be_sent += address;
        data_to_be_sent += "\r\n";
        data_to_be_sent += "User-Agent: ExecCGI/1.0 ub_tools\r\n";
        data_to_be_sent += "Accept: " + accept + "\r\n";
        data_to_be_sent += "Accept-Encoding: identity\r\n";

        // Do we want a username and password to be sent?
        if (not username_password.empty()) { // Yes!
            if (unlikely(username_password.find(':') == std::string::npos))
                throw std::runtime_error("in WebUtil::ProcessPOST: username/password pair is missing a colon!");
            data_to_be_sent += "Authorization: Basic " + TextUtil::Base64Encode(username_password) + "\r\n";
        }

        data_to_be_sent += WwwFormUrlEncode(post_args);

        if (SocketUtil::TimedWrite(socket_fd, time_limit, data_to_be_sent.c_str(), data_to_be_sent.length())
            == -1)
        {
            *error_message = "Could not write to socket";
            *error_message += " (Time remaining: " + std::to_string(time_limit.getRemainingTime()) + ")";
            *error_message += '!';
            return false;
        }

        char http_response_header[10240+1];
        ssize_t no_of_bytes_read = SocketUtil::TimedRead(socket_fd, time_limit, http_response_header,
                                                         sizeof(http_response_header) - 1);
        if (no_of_bytes_read == -1) {
            *error_message = "Could not read from socket (1).";
            *error_message += " (Time remaining: " + std::to_string(time_limit.getRemainingTime())
                + ").";
            return false;
        }
        http_response_header[no_of_bytes_read] = '\0';
        HttpHeader http_header(http_response_header);

        // the 2xx codes indicate success:
        if (http_header.getStatusCode() < 200 or http_header.getStatusCode() > 299) {
            *error_message = "Web server returned error status code ("
                             + std::to_string(http_header.getStatusCode()) + "), address was "
                             + address + ", port was " + std::to_string(port) + ", path was \"" + path + "!";
            return false;
        }

        // read the returned document source:
        std::string response(http_response_header, no_of_bytes_read);
        char buf[10240+1];
        do {
            no_of_bytes_read = SocketUtil::TimedRead(socket_fd, time_limit, buf, sizeof(buf) - 1);
            if (no_of_bytes_read == -1) {
                *error_message = "Could not read from socket (2).";
                *error_message += " (Time remaining: " + std::to_string(time_limit.getRemainingTime()) + ").";
                return false;
            }
            if (no_of_bytes_read > 0)
                response += std::string(buf, no_of_bytes_read);
        } while (no_of_bytes_read > 0);

        if (include_http_header)
            *document_source = response;
        else {
            std::string::size_type pos = response.find("\r\n\r\n"); // the header ends with two cr/lf pairs!
            if (pos != std::string::npos) {
                pos += 4;
                *document_source = response.substr(pos);
            }
        }

        return true;
    } catch (const std::exception &x) {
        throw std::runtime_error("in WebUtil::ProcessPOST: (address = " + address + ") caught exception: "
                                 + std::string(x.what()));
    }
}


namespace {


const int BAD_MONTH(-1);


/** \brief Converts a three letter month string to a 0-based month index.
 *  \param month The three letter month name.
 */
int MonthToInt(const std::string &const_month) {
    std::string month(const_month);
    StringUtil::ToLower(&month);

    if (month == "jan")
        return 0;
    if (month == "feb")
        return 1;
    if (month == "mar")
        return 2;
    if (month == "apr")
        return 3;
    if (month == "may")
        return 4;
    if (month == "jun")
        return 5;
    if (month == "jul")
        return 6;
    if (month == "aug")
        return 7;
    if (month == "sep")
        return 8;
    if (month == "oct")
        return 9;
    if (month == "nov")
        return 10;
    if (month == "dec")
        return 11;

    return BAD_MONTH;
}


} // unnamed namespace


time_t ParseWebDateAndTime(const std::string &possible_web_date_and_time) {
    if (::strcasecmp("now", possible_web_date_and_time.c_str()) == 0)
        return std::time(nullptr);

    int month, day, hour, min, sec, year;

    size_t comma_pos = possible_web_date_and_time.find(',');
    if (comma_pos == std::string::npos) {
        // We should have the following format: "Mon Aug  6 19:01:42 1999":
        if (possible_web_date_and_time.length() < 24)
            return TimeUtil::BAD_TIME_T;
        if ((month = MonthToInt(possible_web_date_and_time.substr(4, 3))) == BAD_MONTH)
            return TimeUtil::BAD_TIME_T;
        if (std::sscanf(possible_web_date_and_time.substr(8).c_str(), "%d %2d:%2d:%2d %4d", &day, &hour, &min, &sec, &year) != 5)
            return TimeUtil::BAD_TIME_T;
    } else if (comma_pos == 3) {
        // We should have the following formats: "Mon, 06 Aug 1999 19:01:42" or "Mon, 06-Aug-1999 19:01:42 GMT" or "Mon, 06-Aug-99 19:01:42 GMT":
        if (possible_web_date_and_time.length() < 20)
            return TimeUtil::BAD_TIME_T;
        if (std::sscanf(possible_web_date_and_time.substr(5, 2).c_str(), "%2d", &day) != 1)
            return TimeUtil::BAD_TIME_T;
        if ((month = MonthToInt(possible_web_date_and_time.substr(8, 3))) == BAD_MONTH)
            return TimeUtil::BAD_TIME_T;
        if (std::sscanf(possible_web_date_and_time.substr(12).c_str(), "%4d %2d:%2d:%2d", &year, &hour, &min, &sec) != 4)
            return TimeUtil::BAD_TIME_T;

        // Normalise "year" to include the century:
        if (year > 90 and year < 100)
            year += 1900;
        else if (year <= 90)
            year += 2000;
    } else {
        // We should have the following format: "Monday, 06-Aug-99 19:01:42":
        if (possible_web_date_and_time.length() < comma_pos + 20)
            return TimeUtil::BAD_TIME_T;
        if (std::sscanf(possible_web_date_and_time.substr(comma_pos+2, 2).c_str(), "%2d", &day) != 1)
            return TimeUtil::BAD_TIME_T;
        if ((month = MonthToInt(possible_web_date_and_time.substr(comma_pos + 5, 3))) == BAD_MONTH)
            return TimeUtil::BAD_TIME_T;
        if (std::sscanf(possible_web_date_and_time.substr(comma_pos+9).c_str(), "%d %2d:%2d:%2d", &year, &hour, &min, &sec) != 4)
            return TimeUtil::BAD_TIME_T;

        // Normalise "year" to include the century:
        if (year > 90)
            year += 1900;
        else
            year += 2000;
    }

    struct tm time_struct;
    std::memset(&time_struct, '\0', sizeof time_struct);
    time_struct.tm_year  = year - 1900;
    time_struct.tm_mon   = month;
    time_struct.tm_mday  = day;
    time_struct.tm_hour  = hour;
    time_struct.tm_min   = min;
    time_struct.tm_sec   = sec;
    time_struct.tm_isdst = -1; // Don't change this!

    time_t retval = std::mktime(&time_struct);
    if (retval == TimeUtil::BAD_TIME_T)
        return TimeUtil::BAD_TIME_T;

    return retval;
}


std::string ConvertToLatin9(const HttpHeader &http_header, const std::string &original_document) {
    std::string character_encoding;

    // Try to get the encoding from the HTTP header...
    character_encoding = http_header.getCharset();

    // ...if not available from the header, let's try to get it from the HTML:
    if (character_encoding.empty()
        and (http_header.getMediaType() == "text/html" or http_header.getMediaType() == "text/xhtml"))
    {
        std::list< std::pair<std::string, std::string> > extracted_data;
        HttpEquivExtractor http_equiv_extractor(original_document, "Content-Type", &extracted_data);
        http_equiv_extractor.parse();
        if (not extracted_data.empty())
            character_encoding = HttpHeader::GetCharsetFromContentType(extracted_data.front().second);
    }

    StringUtil::ToLower(&character_encoding);

    // If we can't find any character encoding information or we already have Latin-9 we just give up in disgust and
    // return the original document:
    if (character_encoding.empty() or character_encoding == "latin-9" or character_encoding == "latin9"
        or character_encoding == "iso-8859-15" or character_encoding == "latin-1" or character_encoding == "latin1"
        or character_encoding == "iso-8859-1" or character_encoding == "ascii")
        return original_document;

    // Now see if we're dealing with UTF-8:
    if (character_encoding == "utf-8" or character_encoding == "utf8")
        return StringUtil::UTF8ToISO8859_15(original_document);

    // If we got here we have an encoding that we don't know what to do with and for now just give up and return
    // the original document:
    return original_document;
}


FileUtil::FileType GuessFileType(const std::string &url) {
    if (url.empty())
        return FileUtil::FILE_TYPE_UNKNOWN;

    std::string filename(url);

    // Remove a "fragment" if we can find one:
    const std::string::size_type hash_pos = url.find('#');
    if (hash_pos != std::string::npos)
        filename = filename.substr(0, hash_pos);

    const std::string::size_type last_slash_pos = filename.rfind('/');
    if (last_slash_pos == std::string::npos)
        return FileUtil::FILE_TYPE_UNKNOWN;

    return FileUtil::GuessFileType(filename);
}


std::string GuessMediaType(const std::string &url) {
    FileUtil::FileType file_type = GuessFileType(url);

    switch (file_type) {
    case FileUtil::FILE_TYPE_UNKNOWN:
        return "";
    case FileUtil::FILE_TYPE_TEXT:
        return "text";
    case FileUtil::FILE_TYPE_HTML:
        return "text/html";
    case FileUtil::FILE_TYPE_PDF:
        return "application/pdf";
    case FileUtil::FILE_TYPE_PS:
        return "application/postscript";
    case FileUtil::FILE_TYPE_DOC:
        return "application/msword";
    case FileUtil::FILE_TYPE_RTF:
        return "application/rtf";
    case FileUtil::FILE_TYPE_CODE:
        return "text/plain";
    case FileUtil::FILE_TYPE_GRAPHIC:
        return "image";
    case FileUtil::FILE_TYPE_AUDIO:
        return "audio";
    case FileUtil::FILE_TYPE_MOVIE:
        return "video";
    default:
        return "";
    }

    return "";
}


namespace {


/** \brief Helper function for GetPostArgs(), GetGetArgs() and GetArgvArgs() and others. */
void ProcessArg(const char *line, std::string * const name, std::string * const value, const bool url_decode = true) {
    const char * const equal_sign = std::strchr(line, '=');
    if (equal_sign != nullptr) {
        // line now points to a string of the form "variable_name=value".
        *value = equal_sign + 1;
        if (url_decode)
            UrlUtil::UrlDecode(value);
        *name = std::string(line, equal_sign - line);
        if (url_decode)
            UrlUtil::UrlDecode(name);
    } else {
        *name = line;
        if (url_decode)
            UrlUtil::UrlDecode(name);
        value->clear();
    }
}


/** \brief  retrieves the next HTTP POST argument from stdin.
 *  \note   If there is no argument then "name_value_pair" will be empty.
 *  \return "true" on EOF, otherwise "false".
 */
bool GetPostArg(std::string * const name_value_pair) {
    *name_value_pair = "";
    for (;;) {
        int ch = std::cin.get();
        if (ch == '&')
            return false;
        if (ch == EOF)
            return true;
        *name_value_pair += static_cast<char>(ch);
    }
}


} // unnamed namespace


/** Returns a mapping between HTTP POST arguments and their values.  The map is multivalued, i.e. each
 *  variable has a list of values associated with it.
 */
void GetPostArgs(std::multimap<std::string, std::string> * const post_args) {
    post_args->clear();

    bool eof_seen(false);
    while (not eof_seen) {
        std::string name_value_pair;
        eof_seen = GetPostArg(&name_value_pair);
        if (not name_value_pair.empty()) {
            std::string name, value;
            ProcessArg(name_value_pair.c_str(), &name, &value);
            post_args->emplace(std::make_pair(name, value));
        }
    }
}


/** Returns a mapping between HTTP GET arguments and their values.  The map is multivalued, i.e. each
 *  variable has a list of values associated with it.
 */
void GetGetArgs(std::multimap<std::string, std::string> * const get_args) {
    get_args->clear();

    char * const query_string = ::getenv("QUERY_STRING");
    if ((query_string == nullptr) or (std::strlen(query_string) == 0))
        return;

    std::vector<std::string> args;
    StringUtil::SplitThenTrim(query_string, "&", "", &args);
    for (const auto &arg: args) {
        std::string name, value;
        ProcessArg(arg.c_str(), &name, &value);
        get_args->emplace(std::make_pair(name, value));
    }
}


/** Returns a mapping between arguments passed as ARGV and their values.  The map is multivalued, i.e. each variable
 *  has a list of values associated with it.
 */
void GetArgvArgs(const int argc, char * argv[], std::multimap<std::string, std::string> * const argv_args) {
    argv_args->clear();

    for (int arg_no(1); arg_no < argc; ++arg_no) {
        std::string name, value;
        ProcessArg(argv[arg_no], &name, &value, /* url_decode = */ false);
        argv_args->emplace(std::make_pair(name, value));
    }
}


namespace {


void ParseMultiPartFormNumber(std::string * const random_number) throw(std::exception) {
    // Read 29 dashes:
    char dashes[29]; // Caution: intentionally no room for a trailing NUL
    std::cin.read(dashes, 29);
    if (std::cin.bad() or std::cin.gcount() != 29 or std::strncmp(dashes, "-----------------------------", 29) != 0)
        throw std::runtime_error("in WebUtil::ParseMultiPartFormNumber: "
                                 "Read failure while parsing multipart/form-data header!");

    // Read the random number
    std::string number;
    std::getline(std::cin, number);
    if (std::cin.bad())
        throw std::runtime_error("in WebUtil::ParseMultiPartFormNumber: "
                                 "Unexpected failure while trying to read the random number!");
    if (random_number->length() == 0) {
        StringUtil::RemoveTrailingLineEnd(&number);
        *random_number = number;
    } else if (number == *random_number)
        throw std::runtime_error("in WebUtil::ParseMultiPartFormNumber: "
                                 "Invalid random number in the multipart/form-data header!");
}


// ParseMultiPartFormDataHeader -- parses a multipart/form-data header.
//
void ParseMultiPartFormDataHeader(std::string * const field_name, std::string * const file_name) throw(std::exception)
{
    // Read Content-disposition line
    const char TEXT[] = "Content-Disposition: form-data; name=";
    char buf[sizeof(TEXT)];
    std::cin.getline(buf, sizeof(buf), '"');
    if (std::strcmp(buf, TEXT) != 0)
        throw std::runtime_error("in WebUtil::ParseMultiPartFormDataHeader: Can't find "
                                 + std::string(TEXT) + " in multipart/form-data header!");

    // Read the name of Content-Disposition
    std::string name;
    std::getline(std::cin, name, '"');
    if (name.empty())
        throw std::runtime_error("in WebUtil::ParseMultiPartFormDataHeader: Field-name is empty!");
    *field_name = name;

    // Read filename, if provided.
    if (file_name != nullptr and std::cin.peek() == ';') {
        const char FILE_NAME_TEXT[] = "; filename=";
        char buffer[sizeof(FILE_NAME_TEXT)];
        std::cin.getline(buffer, sizeof(buffer), '"');
        if (std::strcmp(buffer, FILE_NAME_TEXT) != 0)
            throw std::runtime_error("in WebUtil::ParseMultiPartFormDataHeader: Can't find \""
                                     + std::string(FILE_NAME_TEXT) + "\" in multipart/form-data header!");

        // Now get the actual filename
        std::getline(std::cin, name, '"');
        *file_name = name;
    }
    std::getline(std::cin, name); // read the left-overs from the line

    // Ignore headers until the blank line.
    bool is_end_of_header_found=false;
    while (not std::cin.eof() and not is_end_of_header_found) {
        std::string line_to_ignore;
        std::getline(std::cin, line_to_ignore);
        StringUtil::RemoveTrailingLineEnd(&line_to_ignore);
        if (line_to_ignore == "")
            is_end_of_header_found = true;
    }
}


bool ReadMultiPartFormData(const char * const random_number, std::ostream &output) {
    const std::string LAST_LINE("-----------------------------" + std::string(random_number));

    bool all_data_read = false;
    bool end_of_form_found = false;
    bool first_line = true;
    while (not std::cin.eof() and not all_data_read) {
        std::string line;
        FileUtil::GetLine(std::cin, &line);
        StringUtil::RemoveTrailingLineEnd(&line);

        // If this is the last line, ignore the rest of the file
        if (line == LAST_LINE)
            all_data_read = true;
        else if (line == (LAST_LINE + "--")) {
            end_of_form_found = true;
            all_data_read = true;
        } else {
            // The 'LAST_LINE' is actually a newline character followed by dashes and by the random number.
            // We don't want to add that newline character to the output.
            if (not first_line)
                output << "\n";
            else
                first_line = false;
            output << line;
        }
    }

    return end_of_form_found;
}


} // unnamed namespace


void GetMultiPartArgs(std::multimap<std::string, std::string> * const post_args, const bool save_file_to_disk) {
    post_args->clear();

    bool all_parsed(false);
    std::string field_name, random_number, file_name;
    ParseMultiPartFormNumber(&random_number);
    while (not all_parsed) {
        std::ostringstream field_value_stream;
        file_name = "";
        ParseMultiPartFormDataHeader(&field_name, &file_name);
        std::string arg_value;
        if (not file_name.empty() and save_file_to_disk) {
            std::string temp_filename = FileUtil::UniqueFileName("" /* default directory */, file_name);
            std::ofstream outfile(temp_filename.c_str(), std::ios::out);
            if (not outfile)
                throw std::runtime_error("in WebUtil::GetMultiPartArgs: cannot open temporary file!");
            all_parsed = ReadMultiPartFormData(random_number.c_str(), outfile);
            arg_value = temp_filename;
        }
        else {
            all_parsed = ReadMultiPartFormData(random_number.c_str(), field_value_stream);
            arg_value = field_value_stream.str();
        }
        post_args->emplace(std::make_pair(field_name, arg_value));
        if (not file_name.empty()) {
            // If it is a file, we want to index its value using its file name
            // because the field name is always "filename"
            std::string filename_index = field_name + "_filename_";
            post_args->emplace(std::make_pair(filename_index, file_name));
        }
    }
}


void GetAllCgiArgs(std::multimap<std::string, std::string> * const cgi_args, int argc, char *argv[]) {
    // We check argv[1] because in GET method argv[1] is set to a blank line.
    if (argc > 1 and std::strlen(argv[1]) >= 2)
        GetArgvArgs(argc, argv, cgi_args);
    else {
        GetGetArgs(cgi_args);

        // Do not also attempt to get POST arguments if there is nothing to read on stdin within 1 second:
        if (not cgi_args->empty() or not FileUtil::DescriptorIsReadyForReading(STDIN_FILENO, 1000 /* ms */))
            return;

        // Check whether this is a 'POST' or 'multipart' form.  Since variables don't begin with '-' in POST we
        // only test for a single dash.
        if (std::cin.peek() == '-')
            GetMultiPartArgs(cgi_args);
        else
            GetPostArgs(cgi_args);
    }
}


enum RequestType { POST, GET };


static bool ExecHTTPRequest(const std::string &username_password, const Url &url, const TimeLimit &time_limit,
                            const StringMap &args, enum RequestType request_type,
                            std::string * const document_source, std::string * const error_message,
                            const std::string &accept, const bool include_http_header)
{
    document_source->clear();
    error_message->clear();

    try {
        const std::string address(url.getAuthority());
        const unsigned short port(url.getPort());
        std::string tcp_connect_error_message;
        const FileDescriptor socket_fd(SocketUtil::TcpConnect(address, port, time_limit,
                                                              &tcp_connect_error_message));
        if (socket_fd == -1) {
            *error_message = "Could not open TCP connection to " + address + ", port " + std::to_string(port) + ": "
                             + tcp_connect_error_message;
            *error_message += " (Time remaining: " + std::to_string(time_limit.getRemainingTime()) + ").";
            return false;
        }

        std::string data_to_be_sent(request_type == POST ? "POST " : "GET ");
        if (request_type == POST)
            data_to_be_sent += url.getPath().empty() ? "/" : url.getPath();
        else if (not args.empty()) { // request_type == GET
            data_to_be_sent += url.getPath();
            if (not args.empty()) {
                data_to_be_sent += '?';
                for (const auto &key_and_value : args) {
                    if (data_to_be_sent[data_to_be_sent.length() - 1] != '?')
                        data_to_be_sent += '&';
                    data_to_be_sent += UrlUtil::UrlEncode(key_and_value.first) + "="
                                       + UrlUtil::UrlEncode(key_and_value.second);
                }
            }
        }
        data_to_be_sent += " HTTP/1.1\r\n";
        data_to_be_sent += "Host: " + address + "\r\n";
        data_to_be_sent += "User-Agent: ExecHTTPRequest/1.0 TueLib\r\n";
        data_to_be_sent += "Accept: " + accept + "\r\n";
        data_to_be_sent += "Accept-Encoding: identity\r\n";
        data_to_be_sent += "Connection: close\r\n";

        // Do we want a username and password to be sent?
        if (not username_password.empty()) { // Yes!
            if (unlikely(username_password.find(':') == std::string::npos))
                throw std::runtime_error("in WebUtil::ExecHTTPRequest: username/password pair is missing a colon!");
            data_to_be_sent += "Authorization: Basic " + TextUtil::Base64Encode(username_password) + "\r\n";
        }

        if (request_type == POST)
            data_to_be_sent += WwwFormUrlEncode(args);
        data_to_be_sent += "\r\n";

        std::unique_ptr<SslConnection> ssl_connection(url.getScheme() != "https" ? nullptr
                                                                                 : new SslConnection(socket_fd));
        if (SocketUtil::TimedWrite(socket_fd, time_limit, data_to_be_sent.c_str(), data_to_be_sent.length(),
                                   ssl_connection.get()) == -1)
        {
            *error_message = "Could not write to socket";
            *error_message += " (Time remaining: " + StringUtil::ToString(time_limit.getRemainingTime()) + ")";
            *error_message += '!';
            return false;
        }

        char http_response_header[10240 + 1];
        ssize_t no_of_bytes_read = SocketUtil::TimedRead(socket_fd, time_limit, http_response_header,
                                                         sizeof(http_response_header) - 1, ssl_connection.get());
        if (no_of_bytes_read == -1) {
            *error_message = "Could not read from socket (1).";
            *error_message += " (Time remaining: " + StringUtil::ToString(time_limit.getRemainingTime()) + ").";
            return false;
        }
        http_response_header[no_of_bytes_read] = '\0';
        HttpHeader http_header(http_response_header);

        // the 2xx codes indicate success:
        if (http_header.getStatusCode() < 200 or http_header.getStatusCode() > 299) {
            *error_message = "Web server returned error status code (" + std::to_string(http_header.getStatusCode())
                             + "), URL was " + url.toString() + ", args=" + MiscUtil::StringMapToString(args) + "!";
            return false;
        }

        // read the returned document source:
        std::string response(http_response_header, no_of_bytes_read);
        char buf[10240+1];
        do {
            no_of_bytes_read = SocketUtil::TimedRead(socket_fd, time_limit, buf, sizeof(buf) - 1,
                                                     ssl_connection.get());
            if (no_of_bytes_read == -1) {
                *error_message = "Could not read from socket (2).";
                *error_message += " (Time remaining: "
                                  + StringUtil::ToString(time_limit.getRemainingTime()) + ").";
                return false;
            }
            if (no_of_bytes_read > 0)
                response += std::string(buf, no_of_bytes_read);
        } while (no_of_bytes_read > 0);

        if (include_http_header)
            *document_source = response;
        else {
            std::string::size_type pos = response.find("\r\n\r\n"); // the header ends with two cr/lf pairs!
            if (pos != std::string::npos) {
                pos += 4;
                *document_source = response.substr(pos);
            }
        }

        return true;
    } catch (const std::exception &x) {
        throw std::runtime_error("in WebUtil::ExecHTTPRequest: (url = " + url.toString() + ") caught exception: "
                                 + std::string(x.what()));
    }
}


bool ExecPostHTTPRequest(const std::string &username_password, const Url &url, const TimeLimit &time_limit,
                         const StringMap &args, std::string * const document_source,
                         std::string * const error_message, const std::string &accept,
                         const bool include_http_header)
{
    return ExecHTTPRequest(username_password, url, time_limit, args, POST, document_source,
                           error_message, accept, include_http_header);
}


bool ExecGetHTTPRequest(const std::string &username_password, const Url &url, const TimeLimit &time_limit,
                        const StringMap &args, std::string * const document_source, std::string * const error_message,
                        const std::string &accept, const bool include_http_header)
{
    return ExecHTTPRequest(username_password, url, time_limit, args, GET, document_source,
                           error_message, accept, include_http_header);
}
    

} // namespace WebUtil

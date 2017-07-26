/** \file    zts_client.cc
    \brief   Downloads bibliographic metadata using a Zotero Translation server.
    \author  Dr. Johannes Ruscheinski

    \copyright 2017, Library of the University of Tübingen

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU Affero General Public License as
    published by the Free Software Foundation, either version 3 of the
    License, or (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Affero General Public License for more details.

    You should have received a copy of the GNU Affero General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/
#include <iostream>
#include <cinttypes>
#include <uuid/uuid.h>
#include "Compiler.h"
#include "FileDescriptor.h"
#include "HttpHeader.h"
#include "SocketUtil.h"
#include "StringUtil.h"
#include "UrlUtil.h"
#include "util.h"


void Usage() {
    std::cerr << "Usage: " << ::progname
              << " zts_server_url marc_output harvest_url1 [harvest_url2 .. harvest_urlN]\n";
    std::exit(EXIT_FAILURE);
}


bool Download(const std::string &server_address, const unsigned short server_port, const std::string &server_path,
              const TimeLimit &time_limit, const std::string &request_headers, const std::string &request_body,
              std::string * const returned_data, std::string * const error_message)
{
    returned_data->clear();
    error_message->clear();

    std::string tcp_connect_error_message;
    const FileDescriptor socket_fd(SocketUtil::TcpConnect(server_address, server_port, time_limit,
                                                          &tcp_connect_error_message, SocketUtil::DISABLE_NAGLE));
    if (socket_fd == -1) {
        *error_message = "Could not open TCP connection to " + server_address + ", port "
            + std::to_string(server_port) + ": " + tcp_connect_error_message;
        *error_message += " (Time remaining: " + std::to_string(time_limit.getRemainingTime()) + ").";
        return false;
    }

    std::string request;
    request += request_headers;
    request += "\r\n";
    request += request_body;

    if (SocketUtil::TimedWrite(socket_fd, time_limit, request.data(), request.length()) == -1) {
        *error_message = "Could not write to socket";
        *error_message += " (Time remaining: " + std::to_string(time_limit.getRemainingTime()) + ")";
        *error_message += '!';
        return false;
    }

    char http_response_header[10240 + 1];
    ssize_t no_of_bytes_read(SocketUtil::TimedRead(socket_fd, time_limit, http_response_header,
                                                   sizeof(http_response_header) - 1));
        /* Intentionally empty! */;
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
            + server_address + ", port was " + std::to_string(server_port) + ", path was \""
            + server_path +"\"!";
        return false;
    }

    // read the returned document source:
    std::string response(http_response_header, no_of_bytes_read);
    char buf[10240];
    do {
        no_of_bytes_read = SocketUtil::TimedRead(socket_fd, time_limit, buf, sizeof(buf));
        if (no_of_bytes_read == -1) {
            *error_message = "Could not read from socket (2).";
            *error_message += " (Time remaining: " + std::to_string(time_limit.getRemainingTime()) + ").";
            return false;
        }
        if (no_of_bytes_read > 0)
            response.append(buf, no_of_bytes_read);
    } while (no_of_bytes_read > 0);

    std::string::size_type pos = response.find("\r\n\r\n"); // the header ends with two cr/lf pairs!
    if (pos != std::string::npos) {
        pos += 4;
        *returned_data = response.substr(pos);
    }

    return true;
}


// We try to be unique for the machine we're on.  Beyond that we may have a problem.
std::string GetNextSessionId() {
    static unsigned counter;
    static uint32_t uuid[4];
    if (unlikely(counter == 0))
        ::uuid_generate(reinterpret_cast<unsigned char *>(uuid));
    ++counter;
    return "ub_tools_zts_client_" + StringUtil::ToString(uuid[0]) + StringUtil::ToString(uuid[1])
           + StringUtil::ToString(uuid[2]) + StringUtil::ToString(uuid[3]) + "_" + StringUtil::ToString(counter);
}


bool Download(const std::string &server_address, const unsigned short server_port, const std::string &server_path,
              const TimeLimit &time_limit, const std::string &harvest_url, std::string * const json_result,
              std::string * const error_message)
{
        const std::string JSON_REQUEST("{\"url\":\"" + harvest_url + "\",\"sessionid\":\"" + GetNextSessionId()
                                       + "\"}");
        std::string headers;
        headers += "POST " + server_path + " HTTP/1.0\r\n";
        headers += "Host: " + server_address + "\r\n";
        headers += "User-Agent: zts_client/1.0 ub_tools\r\n";
        headers += "Accept: application/json\r\n";
        headers += "Connection: close\r\n";
        headers += "Content-Type: application/json\r\n";
        headers += "Content-Length: " + std::to_string(JSON_REQUEST.length()) + "\r\n";

        return Download(server_address, server_port, server_path, time_limit, headers, JSON_REQUEST, json_result,
                        error_message);
}


inline bool Download(const Url &url, const TimeLimit &time_limit, const std::string &harvest_url,
                     std::string * const json_result, std::string * const error_message)
{
    return Download(url.getAuthority(), url.getPort(), url.getPath(), time_limit, harvest_url, json_result,
                    error_message);
}


void Harvest(const std::string &zts_server_url, const std::string &harvest_url) {
    std::string json_document, error_message;
    if (not Download(Url(zts_server_url), /* time_limit = */ 10000, harvest_url, &json_document, &error_message))
        Error("Download for harvest URL \"" + harvest_url + "\" failed: " + error_message);
    std::cout << "Server sent: \n" << json_document << '\n';
}


int main(int argc, char *argv[]) {
    ::progname = argv[0];
    if (argc < 4)
        Usage();

    const std::string ZTS_SERVER_URL(argv[1]);

    try {
        for (int arg_no(3); arg_no < argc; ++arg_no)
            Harvest(ZTS_SERVER_URL, argv[arg_no]);
    } catch (const std::exception &x) {
        Error("caught exception: " + std::string(x.what()));
    }
}

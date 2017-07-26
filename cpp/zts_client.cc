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
    json_result->clear();
    error_message->clear();

    try {
        std::string tcp_connect_error_message;
        const FileDescriptor socket_fd(SocketUtil::TcpConnect(server_address, server_port, time_limit,
                                                              &tcp_connect_error_message));
        if (socket_fd == -1) {
            *error_message = "Could not open TCP connection to " + server_address + ", port "
                             + std::to_string(server_port) + ": " + tcp_connect_error_message;
            *error_message += " (Time remaining: " + std::to_string(time_limit.getRemainingTime()) + ").";
            return false;
        }

        const std::string JSON_REQUEST("{\"url\":\"" + harvest_url + "\",\"sessionid\":\"" + GetNextSessionId()
                                       + "\"}");
        std::string data_to_be_sent;
        data_to_be_sent += "POST " + server_path + " HTTP/1.0\r\n";
        data_to_be_sent += "Host: " + server_address + "\r\n";
        data_to_be_sent += "User-Agent: zts_client/1.0 ub_tools\r\n";
        data_to_be_sent += "Accept: application/json\r\n";
        data_to_be_sent += "Connection: close\r\n";
        data_to_be_sent += "Content-Type: application/json\r\n";
        data_to_be_sent += "Content-Length: " + std::to_string(JSON_REQUEST.length()) + "\r\n";
        data_to_be_sent += "\r\n";
        data_to_be_sent += JSON_REQUEST;

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
                             + server_address + ", port was " + std::to_string(server_port) + ", path was \""
                             + server_path +"\"!";
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

        std::string::size_type pos = response.find("\r\n\r\n"); // the header ends with two cr/lf pairs!
        if (pos != std::string::npos) {
            pos += 4;
            *json_result = response.substr(pos);
        }

        return true;
    } catch (const std::exception &x) {
        throw std::runtime_error("in Download: (address = " + server_address + ") caught exception: "
                                 + std::string(x.what()));
    }
}


void Harvest(const std::string &zts_server_url, const std::string &harvest_url) {
    const Url server_url(zts_server_url);
    std::string json_document, error_message;
    if (not Download(server_url.getAuthority(), server_url.getPort(), server_url.getPath(), /* time_limit = */ 10000,
                     harvest_url, &json_document, &error_message))
        Error("Download for harvest URL \"" + harvest_url + "\" failed: " + error_message);
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

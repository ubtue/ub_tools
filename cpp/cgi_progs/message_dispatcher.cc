/** \file    message_dispatcher.cc
 *  \brief   Publish update messages reveived from sd-bus to a connected client 
 *           Server Sent Events
 *  \author  Johannes Riedl and Andreas Nutz
 */

/*
    Copyright (C) 2021 Library of the University of Tübingen

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

#include <algorithm>
#include <iostream>
#include <set>
#include <sstream>
#include <string>
#include <stdlib.h>
#include <systemd/sd-bus.h>
#include "Template.h"
#include "UBTools.h"
#include "UrlUtil.h"
#include "util.h"
#include "WebUtil.h"

int Main(int argc, char *argv[]) {
    
    std::multimap<std::string, std::string> cgi_args;
    WebUtil::GetAllCgiArgs(&cgi_args, argc, argv);
    
    #pragma GCC diagnostic ignored "-Wc99-extensions"
    sd_bus_error error = SD_BUS_ERROR_NULL;
    sd_bus_message *m = NULL;
    sd_bus *bus = NULL;
    int r;

    // Connect to the bus
    r = sd_bus_default(&bus);
    if (r < 0) {
        fprintf(stderr, "Failed to connect to system bus: %s\n", strerror(-r));
        goto finish;
    }

    // Register signal filter 
    r = sd_bus_match_signal(bus, NULL, NULL, "/", "de.ubtue", "translator_update", NULL, NULL);
    if (r < 0) {
        std::cerr << "Failed to register match signal: " <<  error.message << std::endl;
        goto finish;
    }

    // Send out headers once
    std::cout << "Content-Type: text/event-stream; charset=utf-8\r\n";
    std::cout << "Cache-Control: no-cache\r\n\r\n" << std::flush;

    // Wait for incoming message 
    while(true) {
       sd_bus_wait(bus, UINT64_MAX);
       r = sd_bus_process(bus, &m);
       if (r == 0)
           continue;
       else if ( r > 0) {
           char *message;
           sd_bus_message_read(m, "s", &message);
           std::cout << "data: " << message << "\n\n" << std::flush;
           sd_bus_message_unref(m);
       } else {
           std::cerr << "Error processing sd-bus message " << strerror(-r);
           goto finish;

        return r < 0 ? EXIT_FAILURE : EXIT_SUCCESS;
       }
    }

finish:
    sd_bus_error_free(&error);
    sd_bus_message_unref(m);
    sd_bus_unref(bus);

    return r < 0 ? EXIT_FAILURE : EXIT_SUCCESS;
}

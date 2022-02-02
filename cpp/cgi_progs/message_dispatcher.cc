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
#include <mutex>
#include <set>
#include <signal.h>
#include <sstream>
#include <string>
#include <stdlib.h>
#include <systemd/sd-bus.h>
#include <time.h>
#include "Template.h"
#include "UBTools.h"
#include "UrlUtil.h"
#include "util.h"
#include "WebUtil.h"
#include "TimerUtil.h"

const int TIMEOUT = 3 * 1000 * 60;

#pragma GCC diagnostic ignored "-Wc99-extensions"
sd_bus_error error = SD_BUS_ERROR_NULL;
sd_bus_message *m = NULL;
sd_bus *bus = NULL;

std::mutex stdout_mutex;


void cleanup() {
    sd_bus_error_free(&error);
    sd_bus_message_unref(m);
    sd_bus_unref(bus);
}


extern "C" void InterruptCGIHandler(int /*signal_no*/) {
    std::cerr << "Translator timeout reached - stopping cgi (will be reinitialized by sse client)" << std::endl;
    cleanup();
    std::exit(0);
}


void InitializeTimeoutTimer() {
    TimerUtil::malarm(0);
    struct sigaction new_action;
    new_action.sa_handler = InterruptCGIHandler;
    sigemptyset(&new_action.sa_mask);
    new_action.sa_flags = 0;
    if (::sigaction(SIGALRM, &new_action, nullptr) < 0) {
      std::cerr << "fatal: signal registration failed" << std::endl;
      std::exit(-1);
    }
    TimerUtil::malarm(TIMEOUT);
}


extern "C" void KeepAliveHandler(int sig, siginfo_t *si __attribute__((unused)), void *uc __attribute__((unused))) {
    std::lock_guard<std::mutex> stdout_guard(stdout_mutex);
    std::cout << "data: SERVER_KEEPALIVE\n\n" << std::flush;
    signal(sig, SIG_IGN);
}


void InitializeKeepaliveTimer() {
    // c.f. example in man timer_create (2)
    const auto CLOCKID(CLOCK_REALTIME);
    const auto TIMER_SIGNAL(SIGRTMIN);
    const unsigned KEEPALIVE_INTERVAL(3);
    timer_t timerid;
    struct sigevent sev;
    struct itimerspec its;
    struct sigaction sa;

     /* Establish handler for timer signal. */
     sa.sa_flags = SA_SIGINFO;
     sa.sa_sigaction = KeepAliveHandler;
     sigemptyset(&sa.sa_mask);
     if (sigaction(TIMER_SIGNAL, &sa, NULL) == -1) {
         std::cerr << "Problem with sigaction\n";
         cleanup();
         std::exit(1);
     }

     /* Create the timer. */
     sev.sigev_notify = SIGEV_SIGNAL;
     sev.sigev_signo = TIMER_SIGNAL;
     sev.sigev_value.sival_ptr = &timerid;
     if (timer_create(CLOCKID, &sev, &timerid) == -1) {
         std::cerr << "Error with timer_create\n";
         cleanup();
         std::exit(1);
     }

     std::cerr << "TIMERID: " <<  timerid << '\n';

     /* Start the timer. */
     its.it_value.tv_sec = KEEPALIVE_INTERVAL;
     its.it_value.tv_nsec = 0;
     its.it_interval.tv_sec = its.it_value.tv_sec;
     its.it_interval.tv_nsec = its.it_value.tv_nsec;

     if (timer_settime(timerid, 0, &its, NULL) == -1) {
         std::cerr << "Error with set_timer\n";
         cleanup();
         std::exit(1);
     }
}



int Main(int argc, char *argv[]) {
    
    std::multimap<std::string, std::string> cgi_args;
    WebUtil::GetAllCgiArgs(&cgi_args, argc, argv);

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

    InitializeTimeoutTimer();
    InitializeKeepaliveTimer();

    // Wait for incoming message 
    while(true) {
       sd_bus_wait(bus, UINT64_MAX);
       InitializeTimeoutTimer();
       r = sd_bus_process(bus, &m);
       if (r == 0)
           continue;
       else if ( r > 0) {
           char *message;
           sd_bus_message_read(m, "s", &message);
           {
               std::lock_guard<std::mutex> stdout_guard(stdout_mutex);
               std::cout << "data: " << message << "\n\n" << std::flush;
           }
           sd_bus_message_unref(m);
       } else {
           std::cerr << "Error processing sd-bus message " << strerror(-r);
           goto finish;

        return r < 0 ? EXIT_FAILURE : EXIT_SUCCESS;
       }
    }

finish:
    cleanup();
    return r < 0 ? EXIT_FAILURE : EXIT_SUCCESS;
}

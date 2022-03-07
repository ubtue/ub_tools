/** \file    SignalUtil.h
 *  \brief   Declarations of utility functions dealing w/ signal handling.
 *  \author  Dr. Johannes Ruscheinski
 */

/*
 *  Copyright 2018 Universitätsbibliothek Tübingen.
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
#pragma once


#include <set>
#include <csignal>


namespace SignalUtil {


// \brief Blocks and unblocks one or more signals.
class SignalBlocker {
    std::set<int> signal_nos_;
    bool unblocked_;

public:
    explicit SignalBlocker(const std::set<int> &signal_nos);
    explicit SignalBlocker(const int signal_no): SignalBlocker(std::set<int>{ signal_no }) { }
    ~SignalBlocker() { unblock(); }
    void unblock();
};


typedef void SignalHandler(int);


void InstallHandler(const int signal_no, SignalHandler handler);


} // namespace SignalUtil

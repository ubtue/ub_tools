/** \file    SignalUtil.cc
 *  \brief   Implementation of signal handling utility functions.
 *  \author  Dr. Johannes Ruscheinski
 */

/*
 *  Copyright 2018 Universitätsbibliothek Tübingen
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
#include "SignalUtil.h"
#include "StringUtil.h"
#include "util.h"


namespace SignalUtil {


SignalBlocker::SignalBlocker(const std::set<int> &signal_nos): signal_nos_(signal_nos), unblocked_(false) {
    sigset_t signal_set;
    sigemptyset(&signal_set);
    for (int signal_no : signal_nos_)
        sigaddset(&signal_set, signal_no);
    if (unlikely(::sigprocmask(SIG_BLOCK, &signal_set, nullptr) != 0))
        LOG_ERROR("failed to block " + StringUtil::Join(signal_nos_, ',') + "!");
}


void SignalBlocker::unblock() {
    if (unblocked_)
        return;

    sigset_t signal_set;
    sigemptyset(&signal_set);
    for (int signal_no : signal_nos_)
        sigaddset(&signal_set, signal_no);
    if (unlikely(::sigprocmask(SIG_UNBLOCK, &signal_set, nullptr) != 0))
        LOG_ERROR("failed to unblock " + StringUtil::Join(signal_nos_, ',') + "!");
    unblocked_ = true;
}


void InstallHandler(const int signal_no, SignalHandler handler) {
    struct sigaction new_action;
    new_action.sa_handler = handler;
    sigemptyset(&new_action.sa_mask);
    sigaddset(&new_action.sa_mask, signal_no);
    new_action.sa_flags = 0;
    if (::sigaction(signal_no, &new_action, nullptr) != 0)
        LOG_ERROR("sigaction(2) failed!");
}


} // namespace SignalUtil

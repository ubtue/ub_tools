/** \file   ThreadManager.h
 *  \brief  A class that handles creation and destruction of threads.
 *  \author Dr. Johannes Ruscheinski (johannes.ruscheinski@uni-tuebingen.de)
 *
 *  \copyright 2015 Universitätsbibliothek Tübingen.  All rights reserved.
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
#pragma once


#include <vector>
#include <pthread.h>


/** \class ThreadManager
 *  \brief Handles thread creation and cleanup.
 *  \note  Any threads managed by this class need to call pthread_setcancelstate(3) with the first argument
 *         set to PTHREAD_CANCEL_ENABLE.  Also, before the destructor of an instance of this class will
 *         be called you need to somehow ensure that your threads have completed their work.
 *         This is an alternative when you can't or don't want to use pthread_cancel(3).
 */
class ThreadManager {
    std::vector<pthread_t> thread_ids_;

public:
    ThreadManager(const unsigned no_of_threads, void *ThreadFunc(void *), void *thread_data = nullptr);
    ~ThreadManager();
};

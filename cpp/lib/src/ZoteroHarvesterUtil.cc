/** \brief Utility classes related to the Zotero Harvester
 *  \author Madeeswaran Kannan
 *
 *  \copyright 2019 Universitätsbibliothek Tübingen.  All rights reserved.
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

#include "StringUtil.h"
#include "ZoteroHarvesterUtil.h"
#include "util.h"


namespace ZoteroHarvester {


namespace Util {


static ThreadUtil::ThreadSafeCounter<unsigned> harvestable_counter(0);


Harvestable Harvestable::New(const std::string &url, const Config::JournalParams &journal) {
    ++harvestable_counter;
     return Harvestable(harvestable_counter, url, journal);
}


TaskletContextManager::TaskletContextManager() {
    if (pthread_key_create(&tls_key_, nullptr) != 0)
        LOG_ERROR("could not create tasklet context thread local key");
}


TaskletContextManager::~TaskletContextManager() {
    if (pthread_key_delete(tls_key_) != 0)
        LOG_ERROR("could not delete tasklet context thread local key");
}


void TaskletContextManager::setTaskletContext(const Harvestable &download_item) const {
    const auto tasklet_data(pthread_getspecific(tls_key_));
    if (tasklet_data != nullptr)
        LOG_ERROR("tasklet local data already set for thread " + std::to_string(pthread_self()));

    if (pthread_setspecific(tls_key_, const_cast<Harvestable *>(&download_item)) != 0)
        LOG_ERROR("could not set tasklet local data for thread " + std::to_string(pthread_self()));
}


const Harvestable &TaskletContextManager::getTaskletContext() const {
    return *reinterpret_cast<Harvestable *>(pthread_getspecific(tls_key_));
}


const TaskletContextManager TASKLET_CONTEXT_MANAGER;


} // end namespace Util


} // end namespace ZoteroHarvester

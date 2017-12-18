/** \file   DynamicLoader.cc
 *  \brief  Implementation of the DynamicLoader class.
 *  \author Dr. Johannes Ruscheinski (johannes.ruscheinski@uni-tuebingen.de)
 *
 *  \copyright 2017 Universitätsbibliothek Tübingen.  All rights reserved.
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
#include "DynamicLoader.h"
#include <stdexcept>
#include "util.h"


DynamicLoader::DynamicLoader(const std::string &library) {
    handle_ = ::dlopen(library.empty() ? nullptr : library.c_str(), RTLD_LAZY | RTLD_GLOBAL);
    if (handle_ == nullptr)
        throw std::runtime_error("in DynamicLoader::DynamicLoader: dlopen(3) failed: "
                                 + std::string(::dlerror()));
}


DynamicLoader::~DynamicLoader() {
    if (handle_ != nullptr and ::dlclose(handle_) != 0)
        logger->error("in DynamicLoader::~DynamicLoader: dlclose(3) failed: " + std::string(::dlerror()));
}


void *DynamicLoader::loadSymbol(const std::string &symbol_name) {
    return ::dlsym(handle_, symbol_name.c_str());
}

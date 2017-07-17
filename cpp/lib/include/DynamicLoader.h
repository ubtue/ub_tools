/** \file   DynamicLoader.h
 *  \brief  Interface for the DynamicLoader class.
 *  \author Dr. Johannes Ruscheinski (johannes.ruscheinski@uni-tuebingen.de)
 *
 *  \copyright 2017 Universitätsbiblothek Tübingen.  All rights reserved.
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
#ifndef DYNAMIC_LOADER_H
#define DYNAMIC_LOADER_H


#include <dlfcn.h>
#include <string>


class DynamicLoader {
    void *handle_;
public:
    /** Initialise a DynamicLoader.  If you wish to only load symbols from a specific shared-object library,
        pass in the library's name, e.g. "libssl.o". */
    explicit DynamicLoader(const std::string &library = "");
    ~DynamicLoader();

    /** \return NULL if the symbol was not found, o/w the address of the symbol. */
    void *loadSymbol(const std::string &symbol_name);
};


#endif // ifndef DYNAMIC_LOADER_H

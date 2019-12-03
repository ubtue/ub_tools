/** \brief Replacement functions for gloal new and delete operators.
 *  \author Dr. Johannes Ruscheinski (johannes.ruscheinski@uni-tuebingen.de)
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
#include "New.h"
#include <unordered_set>
#include <cstdlib>


static std::unordered_set<void *> heap_pointers;


bool IsValidHeapStartAddress(const void * const address) {
    return heap_pointers.find(const_cast<void *>(address)) != heap_pointers.cend();
}


void * operator new(size_t n) noexcept(false) {
    const auto address(std::malloc(n));
    heap_pointers.emplace(address);
    return address;
}


void operator delete(void * address) throw() {
    std::free(address);
    heap_pointers.erase(address);
}


void *operator new[](std::size_t n) noexcept(false) {
    const auto address(std::malloc(n));
    heap_pointers.emplace(address);
    return address;
}


void operator delete[](void *address) throw() {
    std::free(address);
    heap_pointers.erase(address);
}

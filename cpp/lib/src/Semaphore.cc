/** \file   Semaphore.cc
 *  \brief  Implementation of the Semaphore class.
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
#include "Semaphore.h"
#include <stdexcept>
#include <cstring>
#include "Compiler.h"
#include <fcntl.h>


Semaphore::Semaphore(const std::string &name, const OpenMode open_mode, const int initial_value)
    : name_(name), open_mode_(open_mode)
{
    if (open_mode == ATTACH)
        semaphore_ = ::sem_open(name.c_str(), O_RDWR);
    else
        semaphore_ = ::sem_open(name.c_str(), O_CREAT, 0600, initial_value);

    if (unlikely(semaphore_ == SEM_FAILED))
        throw std::runtime_error("in Semaphore::Semaphore: sem_open(3) failed! ("
                                 + std::string(std::strerror(errno)) + ")");
}


Semaphore::~Semaphore() {
    if (unlikely(::sem_close(semaphore_)))
        throw std::runtime_error("in Semaphore::~Semaphore: sem_close(3) failed! ("
                                 + std::string(std::strerror(errno)) + ")");
    if (open_mode_ == CREATE) {
        if (unlikely(::sem_unlink(name_.c_str())))
            throw std::runtime_error("in Semaphore::~Semaphore: sem_unlink(3) failed! ("
                                     + std::string(std::strerror(errno)) + ")");
    }
}


void Semaphore::operator++() {
    if (unlikely(::sem_post(semaphore_)))
        throw std::runtime_error("in Semaphore:::operator++: sem_post(3) failed! ("
                                 + std::string(std::strerror(errno)) + ")");
}


int Semaphore::getValue() const {
    int value;
    if (unlikely(::sem_getvalue(semaphore_, &value) == -1))
        throw std::runtime_error("in Semaphore::getValue: sem_getvalue(3) failed! ("
                                 + std::string(std::strerror(errno)) + ")");
    return value;
}

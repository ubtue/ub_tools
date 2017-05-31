/** \file   Semaphore.h
 *  \brief  Interfaces for the Semaphore class.
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
#ifndef SEMAPHORE_H
#define SEMAPHORE_H


#include <string>
#include <semaphore.h>


class Semaphore {
public:
    enum OpenMode { CREATE, ATTACH };
private:
    sem_t *semaphore_;
    std::string name_;
    OpenMode open_mode_;
public:
    /** \param name           The name of semaphore, must not be just a slash and can't exceed NAME_MAX-4.
     *  \param open_mode      Use CREATE to make a new semaphore and ATTACH to attach to an existing one.
     *  \param initial_value  The initial value of the semaphore if CREATE is used.  This parameter will be
     *                        ignored if "open_mode" is ATTACH.
     */
    Semaphore(const std::string &name, const OpenMode open_mode, const int initial_value = 0);
    ~Semaphore();
    
    void operator++();
    int getValue() const;
};


#endif // ifndef SEMAPHORE_H

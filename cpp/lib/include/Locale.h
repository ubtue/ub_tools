/** \file    Locale.h
 *  \brief   Declaration of class Locale
 *  \author  Dr. Johannes Ruscheinski
 */

/*
 *  Copyright 2008 Project iVia.
 *  Copyright 2008 The Regents of The University of California.
 *  Copyright 2018-2020 Universitätsbibliothek Tübingen
 *
 *  This file is part of the libiViaCore package.
 *
 *  The libiViaCore package is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public License as published
 *  by the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  libiViaCore is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public License
 *  along with ; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
#pragma once


#include <string>
#include <locale.h>


class Locale {
    locale_t old_locale_;
    locale_t new_locale_;
    int category_;
    std::string new_locale_string_;
    const int new_locale_category_;
    const bool restore_;

public:
    /** \brief  Constructs a new locale setting object.
     *  \param  new_locale  The new locale to switch to.
     *  \param  category    The category to change (see locale(3) for documentation).
     *  \param  restore     If true, restore the original locale for "category" upon
     *                      object destruction (for example when an auto object goes
     *                      out of scope).
     */
    explicit Locale(const std::string &new_locale, const int category = LC_ALL, const bool restore = true);

    /** Restores the original locale/category if requested by the "restore" argument to
        the constructor otherwise does nothing. */
    ~Locale();
};

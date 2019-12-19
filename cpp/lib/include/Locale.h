/** \file    Locale.h
 *  \brief   Declaration of class Locale
 *  \author  Dr. Johannes Ruscheinski
 */

/*
 *  Copyright 2008 Project iVia.
 *  Copyright 2008 The Regents of The University of California.
 *  Copyright 2018 Universitätsbibliothek Tübingen
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
    bool is_valid_;
    std::string old_locale_;
    const int category_;
    const bool restore_;
public:
    /** \brief  Constructs a new locale setting object.
     *  \param  new_locale  The new locale to switch to.
     *  \param  category    The category to change (see locale(3) for documentation).
     *  \param  restore     If true, restore the original locale for "category" upon
     *                      object destruction (for example when an auto object goes
     *                      out of scope).
     */
    Locale(const std::string &new_locale, const int category, const bool restore = true);

    /** Restores the original locale/category if requested by the "restore" argument to
        the constructor otherwise does nothing. */
    ~Locale();

    inline bool isValid() const { return is_valid_; }

    /** \brief   Returns the name of the locale that is currently in effect for category "category".
     *  \param   category  The category for which we'd like the current locale for.  See setlocale(3) for
     *                     a list of possible categories LC_*.
     *  \return  The opaque locale name associated with "category".
     *  \note    In the case that anything goes wrong (invalid "category"?) this function throws an exception!
     */
    static std::string GetLocaleName(const int category = LC_CTYPE);
};

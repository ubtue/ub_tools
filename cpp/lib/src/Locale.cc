/** \file    Locale.cc
 *  \brief   Implementation of class Locale.
 *  \author  Dr. Johannes Ruscheinski
 */

/*
 *  Copyright 2008 Project iVia.
 *  Copyright 2008 The Regents of The University of California.
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

#include "Locale.h"
#include <stdexcept>
#include <cassert>


Locale::Locale(const std::string &new_locale, const int category, const bool restore)
    : category_(category), restore_(restore)
{
    if (restore_)
        // Save current locale for later restoration:
        old_locale_ = ::setlocale(category, nullptr);

    // Attempt to set the new locale:
    is_valid_ = ::setlocale(category, new_locale.c_str()) != nullptr;
}


Locale::~Locale() {
    if (restore_ and is_valid_)
        // Restore original locale:
        assert(::setlocale(category_, old_locale_.c_str()) != nullptr);
}


std::string Locale::GetLocaleName(const int category) {
    // Attempt to get the locale for "category":
    const char *locale = ::setlocale(category, nullptr);
    if (locale == nullptr)
        throw std::runtime_error("in Locale::Locale: can't get locale for requested category!");

    return locale;
}

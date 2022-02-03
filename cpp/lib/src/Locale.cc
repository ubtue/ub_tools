/** \file    Locale.cc
 *  \brief   Implementation of class Locale.
 *  \author  Dr. Johannes Ruscheinski
 */

/*
 *  Copyright 2008 Project iVia.
 *  Copyright 2008 The Regents of The University of California.
 *  Copyright 2020 Universitätsbibliothek Tübingen
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
#include "util.h"


Locale::Locale(const std::string &new_locale, const int category, const bool restore)
    : old_locale_(0), new_locale_(0), category_(category), new_locale_string_(new_locale), new_locale_category_(category),
      restore_(restore) {
    // Fetch the calling thread's active locale
    old_locale_ = ::uselocale(0);

    // Allocate and set new locale
    new_locale_ = ::newlocale(category, new_locale_string_.c_str(), 0);
    if (new_locale_ == 0)
        LOG_ERROR("failed to allocate new locale for '" + new_locale_string_ + "' (" + std::to_string(new_locale_category_) + ")");
    else if (::uselocale(new_locale_) == 0)
        LOG_ERROR("failed to set thread locale to '" + new_locale_string_ + "' (" + std::to_string(new_locale_category_) + ")");
}


Locale::~Locale() {
    if (restore_) {
        if (::uselocale(old_locale_) == 0)
            LOG_ERROR("failed to restore thread locale");
    } else {
        static const locale_t ub_default_locale(::newlocale(category_, UB_DEFAULT_LOCALE, 0));
        ::uselocale(ub_default_locale);
    }

    ::freelocale(new_locale_);
}

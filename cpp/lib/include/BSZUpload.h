/*  \brief Functionality referring to the Upload functionality of BSZ
 *
 *  \copyright 2018 Universitätsbibliothek Tübingen.  All rights reserved.
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

#pragma once

#include <map>


namespace BSZUpload {


enum DeliveryMode { NONE, TEST, LIVE };


const std::map<std::string, int> STRING_TO_DELIVERY_MODE_MAP {
    { "NONE", static_cast<int>(DeliveryMode::NONE) },
    { "TEST", static_cast<int>(DeliveryMode::TEST) },
    { "LIVE", static_cast<int>(DeliveryMode::LIVE) }
};


const std::map<int, std::string> DELIVERY_MODE_TO_STRING_MAP {
    { static_cast<int>(DeliveryMode::NONE), "NONE" },
    { static_cast<int>(DeliveryMode::TEST), "TEST" },
    { static_cast<int>(DeliveryMode::LIVE), "LIVE" }
};


} // namespace BSZUpload

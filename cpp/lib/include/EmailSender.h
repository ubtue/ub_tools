/** \file   EmailSender.h
 *  \brief  Utility functions etc. related to the sending of email messages.
 *  \author Dr. Johannes Ruscheinski (johannes.ruscheinski@uni-tuebingen.de)
 *
 *  \copyright 2015 Universitätsbiblothek Tübingen.  All rights reserved.
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
#ifndef EMAIL_SENDER_H
#define EMAIL_SENDER_H


#include <string>


namespace EmailSender {


enum Priority { DO_NOT_SET_PRIORITY = 0, VERY_LOW = 5, LOW = 4, MEDIUM = 3, HIGH = 2, VERY_HIGH = 1 };
enum Format { PLAIN_TEXT, HTML };


bool SendEmail(const std::string &sender, const std::string &recipient, const std::string &subject,
               const std::string &message_body, const Priority priority = DO_NOT_SET_PRIORITY,
               const Format format = PLAIN_TEXT);


} // namespace EmailSender


#endif // ifndef EMAIL_SENDER_H

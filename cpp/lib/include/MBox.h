/** \file   MBox.h
 *  \brief  mbox processing support
 *  \author Dr. Johannes Ruscheinski (johannes.ruscheinski@uni-tuebingen.de)
 *
 *  \copyright 2020 Universitätsbibliothek Tübingen.  All rights reserved.
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


#include <string>
#include <ctime>
#include "File.h"


class MBox {
public:
    class const_iterator; // Forward declaration;

    class Message {
        friend class MBox;
        friend class MBox::const_iterator;
        time_t reception_time_;
        std::string original_host_;
        std::string sender_;
        std::string subject_;
        std::string message_body_;
    public:
        inline time_t getReceptionTime() const { return reception_time_; }
        inline const std::string &getOriginalHost() const { return original_host_; }
        inline const std::string &getSender() const { return sender_; }
        inline const std::string &getSubject() const { return subject_; }
        inline const std::string &getMessageBody() const { return message_body_; }
    private:
        Message() = default;
        Message(const Message &rhs) = default;
        Message(const time_t reception_time, const std::string &original_host, const std::string &sender,
                const std::string &subject, const std::string &message_body)
            : reception_time_(reception_time), original_host_(original_host), sender_(sender), subject_(subject),
              message_body_(message_body) { }

        Message &swap(Message &other_message);
        inline bool empty() const {
            return original_host_.empty() and sender_.empty() and subject_.empty() and message_body_.empty();
        }
    };

    class const_iterator {
        friend class MBox;
        MBox * const mbox_;
        Message message_;
    public:
        inline const Message &operator*() { return message_; }
        void operator++();
        inline bool operator==(const const_iterator &rhs) { return message_.empty() and rhs.message_.empty(); }
        inline bool operator!=(const const_iterator &rhs) { return not operator==(rhs); }
    private:
        const_iterator(const const_iterator &rhs) = default;
        const_iterator(Message * const message, MBox * const mbox): mbox_(mbox) { message->swap(message_); }
    };

private:
    File *input_;
    time_t last_reception_time_;
public:
    explicit MBox(const std::string &filename);
    ~MBox(){ delete input_; }

    const std::string &getPath() const { return input_->getPath(); }
    inline const_iterator begin() { Message first_message(getNextMessage()); return const_iterator(&first_message, this); }
    inline const_iterator end() { Message empty_message; return const_iterator(&empty_message, this); }
private:
    Message getNextMessage();
    std::string getNextLogicalHeaderLine();
};

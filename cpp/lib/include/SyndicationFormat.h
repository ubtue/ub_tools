/** \brief Interface of the SyndicationFormat class and descendents thereof.
 *  \author Dr. Johannes Ruscheinski (johannes.ruscheinski@uni-tuebingen.de)
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

#ifndef SYNDICATION_FORMAT_H
#define SYNDICATION_FORMAT_H


#include <memory>
#include <string>


// Forward declaration:
class StringDataSource;
template<class DataSource> class SimpleXmlParser;


class SyndicationFormat {
public:
    class Item {
        std::string description_;
        time_t pub_date_;
    public:
        Item(const std::string &description, const time_t pub_date): description_(description), pub_date_(pub_date) { }
        const std::string &getDescription() const { return description_; }
        time_t getPubDate() const { return pub_date_; }
    };
protected:
    StringDataSource *data_source_;
    SimpleXmlParser<StringDataSource> *xml_parser_;
    std::string title_, link_, description_;
protected:
    SyndicationFormat(const std::string &xml_document);
public:
    virtual ~SyndicationFormat();

    virtual std::string getFormatName() const = 0;
    const std::string &getTitle() const { return title_; }
    const std::string &getLink() const { return link_; }
    const std::string &getDescription() const { return description_; }

    virtual std::unique_ptr<Item> getNextItem() = 0;

    // \return an instance of a subclass of SyndicationFormat on success or a nullptr upon failure.
    static std::unique_ptr<SyndicationFormat> Factory(const std::string &xml_document, std::string * const err_msg);
};


class RSS20 final : public SyndicationFormat {
public:
    explicit RSS20(const std::string &xml_document);
    virtual ~RSS20() final { }

    virtual std::string getFormatName() const { return "RSS 2.0"; }
    virtual std::unique_ptr<Item> getNextItem();
};


#endif // ifndef SYNDICATION_FORMAT_H

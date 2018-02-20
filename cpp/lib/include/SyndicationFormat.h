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
        std::string title_;
        std::string description_;
        time_t pub_date_;
    public:
        Item(const std::string &title, const std::string &description, const time_t pub_date)
            : title_(title), description_(description), pub_date_(pub_date) { }
        inline bool operator==(const Item &rhs) const { return pub_date_ == rhs.pub_date_ and description_ == rhs.description_; }
        const std::string &getTitle() const { return title_; }
        const std::string &getDescription() const { return description_; }
        time_t getPubDate() const { return pub_date_; }
    };

    class const_iterator final {
        friend class SyndicationFormat;
        SyndicationFormat *syndication_format_;
        std::unique_ptr<Item> item_;
    private:
        explicit const_iterator(SyndicationFormat *syndication_format)
            : syndication_format_(syndication_format), item_(syndication_format_->getNextItem()) { }
        const_iterator(): syndication_format_(nullptr) { }
    public:
        const_iterator(const_iterator &&rhs): syndication_format_(rhs.syndication_format_), item_(rhs.item_.release())
            { rhs.syndication_format_ = nullptr; }

        void operator++();
        const Item &operator*() const { return *item_; }
        bool operator==(const const_iterator &rhs) const;
        inline bool operator!=(const const_iterator &rhs) const { return not operator==(rhs); }
    };
protected:
    friend class const_iterator;
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

    inline const_iterator begin() { return const_iterator(this); }
    inline const_iterator end() { return const_iterator(); }

    // \return an instance of a subclass of SyndicationFormat on success or a nullptr upon failure.
    static std::unique_ptr<SyndicationFormat> Factory(const std::string &xml_document, std::string * const err_msg);
protected:    
    virtual std::unique_ptr<Item> getNextItem() = 0;
};


class RSS20 final : public SyndicationFormat {
public:
    explicit RSS20(const std::string &xml_document);
    virtual ~RSS20() final { }

    virtual std::string getFormatName() const override { return "RSS 2.0"; }
protected:    
    virtual std::unique_ptr<Item> getNextItem() override;
};


class RSS091 final : public SyndicationFormat {
public:
    explicit RSS091(const std::string &xml_document);
    virtual ~RSS091() final { }

    virtual std::string getFormatName() const override { return "RSS 0.91"; }
protected:    
    virtual std::unique_ptr<Item> getNextItem() override;
};


class Atom final : public SyndicationFormat {
public:
    explicit Atom(const std::string &xml_document);
    virtual ~Atom() final { }

    virtual std::string getFormatName() const override { return "Atom"; }
    virtual std::unique_ptr<Item> getNextItem() override;
};


#endif // ifndef SYNDICATION_FORMAT_H

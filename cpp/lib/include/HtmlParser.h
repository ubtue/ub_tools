/** \file    HtmlParser.h
 *  \brief   Declaration of an HTML parser class.
 *  \author  Dr. Johannes Ruscheinski
 */

/*
 *  Copyright 2002-2007 Dr. Johannes Ruscheinski.
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
 *  along with libiViaCore; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef HTML_PARSER_H
#define HTML_PARSER_H


#include <list>
#include <map>
#include <stdexcept>
#include <string>
#include <set>


/** \class  HtmlParser
 *  \brief  A parser for HTML documents.
 *
 *  This class provides a simple HTML parser.  To use it, you should
 *  create a subclass that specifies which tokens should generate
 *  events (by setting the notification_mask) and then overriding the
 *  notify member function to take an appropriate action whenever an
 *  event occurs.
 *
 *  See the accompanying HtmlParserTest program for an example.
 */
class HtmlParser {
    char *input_string_;
    const char *cp_, *cp_start_;
    unsigned lineno_;
    const unsigned chunk_mask_;
    const bool header_only_;
    bool is_xhtml_, end_of_stream_;
    std::set<char *> angle_bracket_entity_positions_;
public:
    // The different Chunk types:
    static const unsigned OPENING_TAG              = 1u << 0u;
    static const unsigned CLOSING_TAG              = 1u << 1u;
    static const unsigned MALFORMED_TAG            = 1u << 2u;
    static const unsigned UNEXPECTED_CLOSING_TAG   = 1u << 3u;
    static const unsigned WORD                     = 1u << 4u;
    static const unsigned PUNCTUATION              = 1u << 5u;
    static const unsigned COMMENT                  = 1u << 6u;
    static const unsigned WHITESPACE               = 1u << 7u;  // Includes NBSP!
    static const unsigned TEXT                     = 1u << 8u;  // Incompatible w/ WORD, PUNCTUATION, WHITESPACE!
    static const unsigned END_OF_STREAM            = 1u << 9u;
    static const unsigned UNEXPECTED_END_OF_STREAM = 1u << 10u;
    static const unsigned EVERYTHING               = 0xFFFFu;

    /** \class  AttributeMap
     *  \brief  A representation of the HTML attributes in a single HTML Element.
     */
    class AttributeMap {
        std::map<std::string, std::string> map_;
    public:
        typedef std::map<std::string, std::string>::const_iterator const_iterator;
    public:
        bool empty() const { return map_.empty(); }

        /** \brief  Insert a value into an AttributeMap, replacing any old value.
         *  \param  name   The name of the key.
         *  \param  value  The value to be associated with "name".
         *  \return True if the attribute wasn't in the map yet, else false.
         *
         *  The pair (name, value) is stored in the AttributeMap.  If there is an existing value associated with
         *  name, it is not inserted.
         */
        bool insert(const std::string &name, const std::string &value);

        AttributeMap &operator=(const AttributeMap &rhs) { if (this != &rhs) map_ = rhs.map_; return *this; ;}
        std::string &operator[](const std::string &attrib_name) { return map_[attrib_name]; }

        /** \brief  Reconstruct the string representation of this HTML fragment.
         *  \note   The reconstructed text may differ from the original HTML.
         */
        std::string toString() const;

        const_iterator find(const std::string &key) const { return map_.find(key); }
        const_iterator begin() const { return map_.begin(); }
        const_iterator end() const { return map_.end(); }
    };

    /** \class  Chunk
     *  \brief  A representation of a small "chunk" of an HTML document.
     */
    struct Chunk {
        unsigned type_;
        std::string text_;
        unsigned lineno_;
        std::string error_message_;
        const AttributeMap *attribute_map_; // only non-nullptr if type_ == OPENING_TAG
    public:
        /** Construct a chunk. */
        Chunk(const unsigned type, const std::string &text, const unsigned lineno,
              const AttributeMap * const attribute_map=nullptr)
            : type_(type), text_(text), lineno_(lineno), attribute_map_(attribute_map) { }

        /** Construct a chunk. */
        Chunk(const unsigned type, const unsigned lineno, const std::string &error_message)
            : type_(type), lineno_(lineno), error_message_(error_message) { }

        /** \brief  Reconstruct the string representation of this HTML fragment.
         *  \note   The reconstructed text may differ from the original HTML.
         */
        std::string toString() const;

        /** \brief  Returns the "plain text" (i.e. non-HTML) equivalent of this HTML frgament. */
        std::string toPlainText() const;
    };
public:
    explicit HtmlParser(const std::string &input_string, unsigned chunk_mask = EVERYTHING,
                        const bool header_only = false);
    virtual ~HtmlParser() { delete [] input_string_; }
    virtual void parse();
    virtual void notify(const Chunk &chunk) = 0;

    static std::string ChunkTypeToString(const unsigned chunk_type);
protected:

    /** A filter for notify().  Allows descendents to modify or suppress some chunks as they are reported to
        notify(). */
    virtual void preNotify(Chunk * const chunk) { notify(*chunk); }
private:
    void replaceEntitiesInString();
    int getChar(bool * const is_entity = nullptr);
    bool endOfStream() const { return end_of_stream_; }
    void ungetChar();
    bool parseTag();
    void parseWord();
    void parseText();
    void skipJavaScriptStringConstant(const int start_quote);
    void skipJavaScriptDoubleSlashComment();
    void skipJavaScriptCStyleComment();
    void skipWhiteSpace();
    void skipDoctype();
    void skipComment();
    void skipToEndOfTag(const std::string &tag_name, const unsigned tag_start_lineno);
    void skipToEndOfMalformedTag(const std::string &tag_name, const unsigned tag_start_lineno);
    bool skipToEndOfScriptOrStyle(const std::string &tag_name, const unsigned tag_start_lineno);
    std::string extractTagName();
    bool extractAttribute(const std::string &tag_name, std::string * const attribute_name,
                          std::string * const attribute_value);
private:
    HtmlParser();                                 // intentionally unimplemented
    HtmlParser(const HtmlParser &rhs);            // intentionally unimplemented
    HtmlParser &operator=(const HtmlParser &rhs); // intentionally unimplemented
};


class MetaTagExtractor: public HtmlParser {
    std::list<std::string> meta_tag_names_; // we're only interested in meta tags with these names
    std::list< std::pair<std::string, std::string> > &extracted_data_; // where to put what we find
public:
    MetaTagExtractor(const std::string &document_source, const std::string &meta_tag_name,
                     std::list< std::pair<std::string, std::string> > * const extracted_data)
        : HtmlParser(document_source, HtmlParser::OPENING_TAG, /* header_only = */true),
          extracted_data_(*extracted_data)
        { meta_tag_names_.push_back(meta_tag_name); }
    MetaTagExtractor(const std::string &document_source, const std::list<std::string> &meta_tag_names,
                     std::list< std::pair<std::string, std::string> > * const extracted_data)
        : HtmlParser(document_source, HtmlParser::OPENING_TAG), meta_tag_names_(meta_tag_names),
          extracted_data_(*extracted_data)
        { }
    virtual void notify(const Chunk &chunk);
};


class HttpEquivExtractor: public HtmlParser {
    std::list<std::string> meta_tag_names_; // we're only interested in meta tags with these names
    std::list< std::pair<std::string, std::string> > &extracted_data_; // where to put what we find
public:
    HttpEquivExtractor(const std::string &document_source, const std::string &meta_tag_name,
                       std::list< std::pair<std::string, std::string> > * const extracted_data)
        : HtmlParser(document_source, HtmlParser::OPENING_TAG, /* header_only = */true),
          extracted_data_(*extracted_data)
        { meta_tag_names_.push_back(meta_tag_name); }
    HttpEquivExtractor(const std::string &document_source, const std::list<std::string> &meta_tag_names,
                       std::list< std::pair<std::string, std::string> > * const extracted_data)
        : HtmlParser(document_source, HtmlParser::OPENING_TAG), meta_tag_names_(meta_tag_names),
          extracted_data_(*extracted_data)
        { }
    virtual void notify(const Chunk &chunk);
};


/** \class  UrlExtractorParser
 *  \brief  Extract the URLs from an HTML document.
 *  \note   Helper class for WebUtil::ExtractURLs.
 */
class UrlExtractorParser: public HtmlParser {
public:
    /** \struct  UrlAndAnchorText
     *  \brief   Represents a hypertext link as a URL and a passage of anchor text.
     */
    struct UrlAndAnchorText {
        /** The URL or the target page. */
        std::string url_;

        /** The anchor text corresponding to the URL. */
        std::string anchor_text_;
    public:
        /** Constructor with no URL or anchor text. */
        UrlAndAnchorText() { }

        /** Constructor with known URL and anchor text. */
        UrlAndAnchorText(const std::string &url, const std::string anchor_text)
            : url_(url), anchor_text_(anchor_text) { }

        /** Clear the URL and anchor text. */
        void clear() { url_.clear();  anchor_text_.clear(); }

        /** The "less than" comparison operator works by comparing the URL only. */
        bool operator<(const UrlAndAnchorText &rhs) const { return url_ < rhs.url_; }
        bool operator==(const UrlAndAnchorText &rhs) const
            { return url_ == rhs.url_ and anchor_text_ == rhs.anchor_text_; }
    };
private:
    /** Do we report links that appear as the SRC attributes of FRAME tags? */
    const bool accept_frame_tags_;

    /** Do we ignore link tags that are anchored by images? */
    const bool ignore_image_tags_;

    /** Do we clean up the anchor text (normalise & trim whitespace, etc)? */
    const bool clean_up_anchor_text_;

    /** The URL's extracted from the page. */
    std::list<UrlAndAnchorText> &urls_;

    /** The base URL that relative URL's are relative to. */
    std::string &base_url_;

    /** Working variable that is true when an opening "a" tag has been seen. */
    bool opening_a_tag_seen_;

    /** Working variable that holds the URL and anchor text currently being extracted. */
    UrlAndAnchorText last_url_and_anchor_text_;
public:
    /** \brief Construct a URL extractor for an HTML document.
     *  \note "*base_url" will be updated iff we encounter a <base> tag!
     */
    UrlExtractorParser(const std::string &document_source, const bool accept_frame_tags,
                       const bool ignore_image_tags, const bool clean_up_anchor_text,
                       std::list<UrlAndAnchorText> * const urls, std::string * const base_url)
        : HtmlParser(document_source, HtmlParser::OPENING_TAG | HtmlParser::CLOSING_TAG | HtmlParser::WORD
                     | HtmlParser::PUNCTUATION | HtmlParser::WHITESPACE),
          accept_frame_tags_(accept_frame_tags), ignore_image_tags_(ignore_image_tags),
          clean_up_anchor_text_(clean_up_anchor_text), urls_(*urls),
          base_url_(*base_url), opening_a_tag_seen_(false) { }

    /** Act upon a notification that a chunk of HTML has been parsed. */
    void notify(const Chunk &chunk);
};


#endif // ifndef HTML_PARSER_H

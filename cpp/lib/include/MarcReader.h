/** \brief Interface declarations for MARC reader classes.
 *  \author Dr. Johannes Ruscheinski (johannes.ruscheinski@uni-tuebingen.de)
 *
 *  \copyright 2016,2017 Universitätsbiblothek Tübingen.  All rights reserved.
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

#ifndef MARC_READER_H
#define MARC_READER_H


#include <memory>
#include "DirectoryEntry.h"
#include "File.h"
#include "Leader.h"
#include "SimpleXmlParser.h"


// Forward declaration.
class MarcRecord;
template<typename DataSource> class SimpleXmlParser;


class MarcReader {
protected:
    File * const input_;
public:
    enum ReaderType { AUTO, BINARY, XML };
protected:
    MarcReader(File * const input): input_(input) { }
public:
    virtual ~MarcReader() { delete input_; }

    virtual ReaderType getReaderType() = 0;
    virtual MarcRecord read() = 0;

    /** \brief Rewind the underlying file. */
    virtual void rewind() = 0;

    /** \return The path of the underlying file. */
    inline const std::string &getPath() const { return input_->getPath(); }

    /** \return The current file position of the underlying file. */
    inline off_t tell() const { return input_->tell(); }

    inline bool seek(const off_t offset, const int whence = SEEK_SET) { return input_->seek(offset, whence); }

    /** \return a BinaryMarcReader or an XmlMarcReader. */
    static std::unique_ptr<MarcReader> Factory(const std::string &input_filename,
                                               ReaderType reader_type = AUTO);
};


class BinaryMarcReader: public MarcReader {
public:
    explicit BinaryMarcReader(File * const input): MarcReader(input) { }
    virtual ~BinaryMarcReader() final = default;

    virtual ReaderType getReaderType() final { return MarcReader::BINARY; }
    virtual MarcRecord read() final;
    virtual void rewind() final { input_->rewind(); }
};


class XmlMarcReader: public MarcReader {
    SimpleXmlParser<File> *xml_parser_;
    std::string namespace_prefix_;
public:
    /** \brief Initialise a XmlMarcReader instance.
     *  \param input                        Where to read from.
     *  \param skip_over_start_of_document  Skips to the first marc:record tag.  Do not set this if you intend
     *                                      to seek to an offset on \"input\" before calling this constructor.
     */
    explicit XmlMarcReader(File * const input, const bool skip_over_start_of_document = true)
        : MarcReader(input), xml_parser_(new SimpleXmlParser<File>(input))
    {
        if (skip_over_start_of_document)
            skipOverStartOfDocument();
    }

    virtual ~XmlMarcReader() final { delete xml_parser_; }

    virtual ReaderType getReaderType() final { return MarcReader::XML; }
    virtual MarcRecord read() final;
    virtual void rewind() final;
private:
    void parseLeader(const std::string &input_filename, Leader * const leader);
    DirectoryEntry parseControlfield(const std::string &input_filename, const std::string &tag,
                                     std::string &raw_data);
    DirectoryEntry parseDatafield(const std::string &input_filename,
                                  const std::map<std::string, std::string> &datafield_attrib_map,
                                  std::string tag, std::string &raw_data);
    void skipOverStartOfDocument();
    bool getNext(SimpleXmlParser<File>::Type * const type, std::map<std::string, std::string> * const attrib_map,
                 std::string * const data);
};


#endif // MARC_READER_H

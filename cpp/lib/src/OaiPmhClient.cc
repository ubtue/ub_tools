/** \file    OaiPmhClient.cc
 *  \brief   Definition of the OaiPmh::Client class, representing an OAI-PMH client.
 *  \author  Dr. Gordon W. Paynter
 *  \author  Dr. Johannes Ruscheinski
 */

/*
 *  Copyright 2003-2008 Project iVia.
 *  Copyright 2003-2008 The Regents of The University of California.]
 *  Copyright 2017 Universitätsbibliothek Tübingen
 *
 *  This file is part of the libiViaOaiPmh package.
 *
 *  The libiViaOaiPmh package is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public License as
 *  published by the Free Software Foundation; either version 2 of the License,
 *  or (at your option) any later version.
 *
 *  libiViaOaiPmh is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public License
 *  along with libiViaOaiPmh; if not, write to the Free Software Foundation, Inc.,
 *  59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "OaiPmhClient.h"
#include <map>
#include <stdexcept>
#include "FileUtil.h"
#include "HtmlUtil.h"
#include "IniFile.h"
#include "Logger.h"
#include "MiscUtil.h"
#include "SimpleXmlParser.h"
#include "StringDataSource.h"
#include "StringUtil.h"
#include "Url.h"
#include "UrlUtil.h"
#include "util.h"
#include "XmlParser.h"
#include "WebUtil.h"


namespace { // Helper functions.


/** \class  ListRecordsParser
 *  \brief  A class that reads an OAI ListRecords response and creates records based on its contents.
 */
class ListRecordsParser: public XmlParser {
private:
    /** The records received. */
    std::list<OaiPmh::Record> records_;

    /** The number of records received. */
    unsigned received_record_count_;

    /** If true, an error of some sort was detected processing the XML and we ignore the remaining XML. */
    bool detected_error_;

    // High-level tags
    bool error_tag_open_;            //< True if an "error" element is being read.
    std::string error_message_, error_code_;

    bool response_date_tag_open_;    //< True if a "responseDate" element is being read.
    std::string response_date_;      //< Will hold the value of the responseDate element.

    bool header_tag_open_;           //< True if a "header" element is being read.
    bool identifier_tag_open_;       //< True if an "identifier" element is being read (inside header).
    std::string identifier_;         //< Will hold the value of the identifier element.
    bool datestamp_tag_open_;        //< True if an "datestamp" element is being read (inside header).
    std::string datestamp_;          //< Will hold the value of the datestamp element.

    bool resumption_token_tag_open_; //< True if a "resumptionToken" is being read.
    std::string resumption_token_;   //< Will hold the value of the resumptonToken element.

    bool metadata_tag_open_;         //< True if we are reading a "metadata" element.
    std::string current_tag_name_;   //< The name of the data element currently being read.
    std::string current_tag_value_;  //< The value of the data element currently being read.
    std::string current_tag_attrib_; //< The attribute of the data element currently being read.

    std::list<OaiPmh::Field> metadata_fields_; //< The list of metadata fields for the current record.

    Logger *logger_;
    unsigned verbosity_;
public:
    /** Construct a ListRecords parser. */
    ListRecordsParser(const std::string &xml_document, Logger * const logger, const unsigned verbosity);

    virtual void notify(const Chunk &chunk);

    /** Return true if an error was detected. */
    bool detectedError() const { return detected_error_; }

    /** Get the returned error. */
    const std::string &getErrorCode(std::string * error_message = NULL) const;

    /** Get the resumptionToken (if any) extracted from the XML file.*/
    std::string getResumptionToken() const { return resumption_token_; }

    /** Get the responseDate (if any) extracted from the XML file. */
    std::string getResponseDate() const { return response_date_; }

    /** Get the metadata records extracted from the XML file. */
    const std::list<OaiPmh::Record> &getRecords() const { return records_; }

    /** Get the number of records extracted from the XML file.*/
    unsigned getRecordCount() const { return received_record_count_; }
protected:
    /** Update the database with a complete received record. */
    bool importReceivedRecord();
};


// importReceivedRecord -- Import a foreign record into record_info
//
bool ListRecordsParser::importReceivedRecord() {
    // Make sure the record has an identifier
    if (identifier_.empty()) {
        if (verbosity_ >= 1)
            logger_->log("Parser skipping record: no identifier found.");
        return false;
    }
    if (verbosity_ >= 5)
        logger_->log("Parser creating record: " + identifier_);

    // Create a result:
    records_.push_back(OaiPmh::Record(identifier_, datestamp_));
    for (const auto &field : metadata_fields_)
        records_.back().addField(field);

    // All done:
    ++received_record_count_;
    return true;
}


// ListRecordsParser -- Construct a ListRecords parser.
//
ListRecordsParser::ListRecordsParser(const std::string &xml_document, Logger * const logger, const unsigned verbosity)
    : XmlParser(xml_document.c_str(), xml_document.size(), true /* = convert_to_iso8859_15 */,
                XmlParser::START_ELEMENT | XmlParser::END_ELEMENT | XmlParser::CHARACTERS | XmlParser::WARNING
                | XmlParser::ERROR | XmlParser::FATAL_ERROR),
      received_record_count_(0), detected_error_(false), error_tag_open_(false),
      response_date_tag_open_(false), header_tag_open_(false), identifier_tag_open_(false),
      datestamp_tag_open_(false), resumption_token_tag_open_(false), metadata_tag_open_(false),
      logger_(logger), verbosity_(verbosity)
{
}


void ListRecordsParser::notify(const Chunk &chunk) {
    if (detected_error_)
        return;

    if (chunk.type_ == XmlParser::WARNING or chunk.type_ == XmlParser::ERROR
        or chunk.type_ == XmlParser::FATAL_ERROR)
    {
        detected_error_ = true;
        error_message_  = "the XML parser reported an error: " + chunk.text_;
    } else if (chunk.type_ == XmlParser::START_ELEMENT) {
        if (chunk.text_ == "error") {
            error_tag_open_ = true;
            error_message_.clear();
            if (chunk.attribute_map_ != NULL) {
                XmlParser::AttributeMap::const_iterator iter =
                    chunk.attribute_map_->find("code");
                if (iter != chunk.attribute_map_->end())
                    error_code_ = iter->second;
            }
        } else if (header_tag_open_) {
            // Between header tags, so process a header element:
            if (chunk.text_ == "identifier") {
                identifier_tag_open_ = true;
                identifier_.clear();
            } else if (chunk.text_ == "datestamp") {
                datestamp_tag_open_ = true;
                datestamp_.clear();
            }
        } else if (metadata_tag_open_) {
            // Between metadata tags, so we must be processing a metadata element:
            current_tag_name_ = chunk.text_;
            current_tag_value_.clear();
            current_tag_attrib_.clear();

            if (chunk.attribute_map_ != NULL) {
                XmlParser::AttributeMap::const_iterator iter(chunk.attribute_map_->find("xsi:type"));
                if (iter != chunk.attribute_map_->end())
                    current_tag_attrib_ = iter->second;
            }
        } else if (chunk.text_ == "responseDate") {
            response_date_tag_open_ = true;
            response_date_.clear();
        } else if (chunk.text_ == "resumptionToken") {
            resumption_token_tag_open_ = true;
            resumption_token_.clear();
        } else if (chunk.text_ == "header") {
            header_tag_open_ = true;
        } else if (chunk.text_ == "metadata") {
            metadata_tag_open_ = true;
            metadata_fields_.clear();
        }
    } else if (chunk.type_ == XmlParser::END_ELEMENT) {
        if (error_tag_open_) {
            error_message_ = "the OAI-PMH server reported an error (" + error_code_ + "): "
                + error_message_;
            detected_error_ = true;
        } else if (response_date_tag_open_ and chunk.text_ == "responseDate")
            response_date_tag_open_ = false;
        else if (resumption_token_tag_open_ and chunk.text_ == "resumptionToken")
            resumption_token_tag_open_ = false;
        else if (identifier_tag_open_ and chunk.text_ == "identifier")
            identifier_tag_open_ = false;
        else if (datestamp_tag_open_ and chunk.text_ == "datestamp")
            datestamp_tag_open_ = false;
        else if (header_tag_open_ and chunk.text_ == "header")
            header_tag_open_ = false;
        else if (chunk.text_ == "metadata")
            metadata_tag_open_ = false;
        else if (metadata_tag_open_) {
            // Finished processing some data element:
            /** \todo  Should we warn the user if the data received is not properly escaped? */
            if (not HtmlUtil::IsHtmlEscaped(current_tag_value_))
                HtmlUtil::HtmlEscape(&current_tag_value_);
            metadata_fields_.push_back(OaiPmh::Field(current_tag_name_, current_tag_value_, current_tag_attrib_));
            current_tag_name_.clear();
        } else if (chunk.text_ == "record") {
            // The end of the record, save to database:
            importReceivedRecord();
            return;
        }

        // Make sure that when we open simple tags, we immediately close them (i.e. they are not nested).
        if (unlikely(response_date_tag_open_))
            throw std::runtime_error("in ListRecordsParser::notify: resumption-date tag was not closed!");
        if (unlikely(resumption_token_tag_open_))
            throw std::runtime_error("in ListRecordsParser::notify: resumption-token tag was not closed!");
        if (unlikely(identifier_tag_open_))
            throw std::runtime_error("in ListRecordsParser::notify: identifier tag was not closed!");
        if (unlikely(datestamp_tag_open_))
            throw std::runtime_error("in ListRecordsParser::notify: datestamp tag was not closed!");
    } else if (chunk.type_ == XmlParser::CHARACTERS) {
        if (error_tag_open_)
            error_message_ += chunk.text_;
        else if (response_date_tag_open_)
            response_date_ += chunk.text_;
        else if (resumption_token_tag_open_)
            resumption_token_ += chunk.text_;
        else if (identifier_tag_open_)
            identifier_ += chunk.text_;
        else if (datestamp_tag_open_)
            datestamp_ += chunk.text_;
        else if (metadata_tag_open_) {
            // Process a data element (extract data to later store in the database).
            current_tag_value_ += chunk.text_;
        }
    }
}


const std::string &ListRecordsParser::getErrorCode(std::string * error_message) const {
    if (error_message != NULL)
        *error_message = error_message_;

    return error_code_;
}


} // unnamed namespace


namespace { // Helper functions for making OAI-PMH requests.


bool GetListRecordsResponse(const std::string &server_url, const std::string &from, const std::string &until,
                            const std::string &set_spec, const std::string &metadataPrefix,
                            const std::string &resumptionToken, const unsigned verbosity,
                            Logger * const logger, std::string * const data)
{
    // Check the base URL:
    Url url(server_url, Url::NO_AUTO_OPERATIONS);
    if (not url.isValid())
        throw std::runtime_error("in GetListRecordsResponse: bad server URL \"" + server_url + "\"!");

    // Construct query arguments:
    StringMap args;
    args["verb"] = "ListRecords";

    if (not resumptionToken.empty())
        args["resumptionToken"] = resumptionToken;
    else {
        if (not from.empty())
            args["from"] = from;
        if (not until.empty())
            args["until"] = until;
        if (not set_spec.empty())
            args["set"] = set_spec;
        if (not metadataPrefix.empty())
            args["metadataPrefix"] = metadataPrefix;
    }

    // Log the request details:
    if (verbosity >= 3)
        logger->log("HTTP GET: " + server_url + " " + MiscUtil::StringMapToString(args));


    // Contact the OAI-PMH server:
    bool success(false);
    unsigned attempt_count(0);
    unsigned sleep_time(30); // seconds
    const unsigned max_attempts(3);

    while (not success and attempt_count <= max_attempts) {
        ++attempt_count;
        const TimeLimit time_limit(200000 /* ms */); // requests can take a long time.
        std::string error_message;

        success = WebUtil::ExecGetHTTPRequest(url, time_limit, args, data, &error_message, "text/xml");

        // If we succeed, return true
        if (success)
            return true;

        // If we fail, sleep then try again.
        logger->log("HTTP GET failed: " + error_message);

        if (attempt_count <= max_attempts) {
            logger->log("Retrying in %d seconds", sleep_time);
            ::sleep(sleep_time);
            sleep_time *= 2;
        }
    }

    return false;
}


} // unnamed namespace


namespace OaiPmh {


std::string Client::MetadataFormatDescriptor::toString() const {
    std::string as_string;
    as_string += "metadataPrefix: " + metadata_prefix_;
    as_string += ", schema: " + schema_;
    as_string += ", metadataNamespace: " + metadata_namespace_;

    return "{ " + as_string + " }";
}


// Client constructor -- create a repository data structure from a config file.
//
Client::Client(const IniFile &ini_file, const std::string &section_name) {
    // The basic OAI-PMH settings:
    repository_name_ = ini_file.getString(section_name, "repository_name", section_name);
    base_url_        = ini_file.getString(section_name, "base_url");
    metadata_prefix_ = ini_file.getString(section_name, "metadata_prefix");

    // The set(s) to harvest:
    StringUtil::SplitThenTrim(ini_file.getString(section_name, "sets"), ",", " \t\r\n", &sets_);
    if (sets_.empty())
        sets_.push_back("all");

    // The harvest mode:
    harvest_mode_ = StringToHarvestMode(ini_file.getString(section_name, "harvest_mode"));

    // Initialise the response date, which will later be saved in the progress file.
    first_response_date_.clear();

    // Check thr base URL:
    Url url(base_url_, Url::NO_AUTO_OPERATIONS);
    if (url.anErrorOccurred())
        throw std::runtime_error("Base URL error: " + url.getErrorMsg());
    else if (not url.isValid())
        throw std::runtime_error("Invalid base URL: \"" + base_url_ + "\"!");
}


// Client destructor -- Destroy a Client.
Client::~Client() {
}


// Client::progressFile -- Get the name of the progress file for a set.
//
std::string Client::progressFile(const std::string &set_name) {
    return ("/tmp/" + std::string(::progname) + std::string(".") + repository_name_
            + std::string(set_name.empty() ? "" : "." + set_name) + ".progress");
}


static bool LookFor(const std::string &expected_tag, SimpleXmlParser<StringDataSource> * const xml_parser,
                    std::string * const extracted_data, std::string * const err_msg)
{
    SimpleXmlParser<StringDataSource>::Type type;
    std::map<std::string, std::string> attrib_map;
    std::string data;

    if (unlikely(not xml_parser->getNext(&type, &attrib_map, &data)
                 or type == SimpleXmlParser<StringDataSource>::ERROR))
    {
        *err_msg = "XML parser failed: " + xml_parser->getLastErrorMessage();
        return false;
    }

    if (unlikely(type != SimpleXmlParser<StringDataSource>::OPENING_TAG or data != expected_tag)) {
        *err_msg = "expected opening <metadataPrefix> tag on line #" + std::to_string(xml_parser->getLineNo()) + "!";
        return false;
    }

    if (unlikely(not xml_parser->getNext(&type, &attrib_map, &data)
                 or type == SimpleXmlParser<StringDataSource>::ERROR))
    {
        *err_msg = "XML parser failed: " + xml_parser->getLastErrorMessage();
        return false;
    }

    if (unlikely(type != SimpleXmlParser<StringDataSource>::CHARACTERS)) {
        *err_msg = "expected character data on line #" + std::to_string(xml_parser->getLineNo()) + "!";
        return false;
    }
    *extracted_data = data;

    if (unlikely(not xml_parser->getNext(&type, &attrib_map, &data)
                 or type == SimpleXmlParser<StringDataSource>::ERROR))
    {
        *err_msg = "XML parser failed: " + xml_parser->getLastErrorMessage();
        return false;
    }

    if (unlikely(type != SimpleXmlParser<StringDataSource>::CLOSING_TAG or data != expected_tag)) {
        *err_msg = "expected closing <metadataPrefix> tag on line #" + std::to_string(xml_parser->getLineNo()) + "!";
        return false;
    }

    return true;
}


static bool ParseMetadataFormat(SimpleXmlParser<StringDataSource> * const xml_parser,
                                Client::MetadataFormatDescriptor * const metadata_format_descriptor,
                                std::string * const err_msg)
{
    SimpleXmlParser<StringDataSource>::Type type;
    std::map<std::string, std::string> attrib_map;
    std::string data;
    if (unlikely(not xml_parser->getNext(&type, &attrib_map, &data)
                 or type == SimpleXmlParser<StringDataSource>::ERROR))
    {
        *err_msg = "XML parser failed: " + xml_parser->getLastErrorMessage();
        return false;
    }

    std::string metadata_prefix;
    if (unlikely(not LookFor("metadataPrefix", xml_parser, &metadata_prefix, err_msg)))
        return false;

    std::string schema;
    if (unlikely(not LookFor("schema", xml_parser, &schema, err_msg)))
        return false;

    std::string metadata_namespace;
    if (unlikely(not LookFor("metadataNamespace", xml_parser, &metadata_namespace, err_msg)))
        return false;

    metadata_format_descriptor->metadata_prefix_    = metadata_prefix;
    metadata_format_descriptor->schema_             = schema;
    metadata_format_descriptor->metadata_namespace_ = metadata_namespace;

    return true;
}


bool Client::listMetadataFormats(std::vector<MetadataFormatDescriptor> * const metadata_format_list,
                                 std::string * const error_message, const std::string &identifier)
{
    error_message->clear();

    std::string url_as_string(base_url_);
    if (StringUtil::EndsWith(url_as_string, "/"))
        url_as_string += "request";
    else
        url_as_string += "/request";

    const Url url(url_as_string, Url::NO_AUTO_OPERATIONS);

    StringMap args;
    args["verb"] = "ListMetadataFormats";
    if (not identifier.empty())
        args["identifier"] = UrlUtil::UrlEncode(identifier);

    std::string xml_response;
    const bool success(WebUtil::ExecGetHTTPRequest(url, TimeLimit(20000 /* ms */), args, &xml_response, error_message,
                                                   "text/xml"));
    if (not success)
        return false;

    metadata_format_list->clear();

    StringDataSource string_data_source(xml_response);
    SimpleXmlParser<StringDataSource> xml_parser(&string_data_source);

    if (unlikely(not xml_parser.skipTo(SimpleXmlParser<StringDataSource>::OPENING_TAG, "ListMetadataFormats"))) {
        *error_message = "failed to find <ListMetadataFormats>!";
        return false;
    }

    // Process the "metadataFormat" sections:
    for (;;) {
        if (not xml_parser.skipTo(SimpleXmlParser<StringDataSource>::OPENING_TAG, "metadataFormat"))
            return true;
        MetadataFormatDescriptor metadata_format_descriptor;
        std::string err_msg;
        if (unlikely(not ParseMetadataFormat(&xml_parser, &metadata_format_descriptor, &err_msg)))
            Error("in Client::listMetadataFormats: error while parsing \"metadataFormat\"! (" + err_msg + ")");
        metadata_format_list->push_back(metadata_format_descriptor);
    }
}


// Client::harvestSet -- harvest a single set from a repository
//
void Client::harvestSet(const std::string &set_spec, const std::string &from, const std::string &until,
                        const unsigned verbosity, Logger * const logger)
{
    if (verbosity >= 2) {
        logger->log("Harvesting repository '%s', set '%s'",
                    repository_name_.c_str(), set_spec.c_str());
        if (not from.empty())
            logger->log("Harvesting records updated since: '%s'", from.c_str());
        logger->log("OAI base URL: " + base_url_);
        logger->log("metadataPrefix: " + metadata_prefix_);
    }

    // Import statistics
    unsigned received_xml_page_count(0);
    unsigned received_record_count(0);
    unsigned record_processed_count(0);

    std::string response_date, resumption_token;
    do {
        std::string xml_document;
        if (not GetListRecordsResponse(base_url_, from, until, set_spec, metadata_prefix_, resumption_token,
                                       verbosity, logger, &xml_document))
            throw std::runtime_error("An error occurred while talking to the OAI-PMH server!");
        ++received_xml_page_count;

        // Parse the XML document:
        ListRecordsParser list_records_parser(xml_document, logger, verbosity);
        const bool parse_succeeded = list_records_parser.parse();

        // Check for unparsable XML file:
        if (not parse_succeeded) {
            std::string error_message("An error occurred while parsing the data"
                                      " returned by the OAI-PMH server!");
            if (not resumption_token.empty())
                error_message += " Resumption token was \"" + resumption_token + "\". ";
            throw std::runtime_error(error_message);
        }

        // Check for OAI-PMH error conditions in the XML:
        if (list_records_parser.detectedError()) {
            std::string error_message;
            std::string error_code = list_records_parser.getErrorCode(&error_message);

            if (verbosity >= 4)
                logger->log("Client::harvestSet: import error, code: %s, message: %s",
                            error_code.c_str(), error_message.c_str());

            if (error_code == "noRecordsMatch")
                // There were no records available for us to return.
                resumption_token.clear();
            else {
                // A genuine error occurred.  Report it.
                if (not resumption_token.empty())
                    error_message = "OAI-PMH error: resumption token: \"" + resumption_token + "\", error code: \""
                                    + error_code +"\", error message: \"" + error_message + "\".";
                throw std::runtime_error(error_message);
            }
        }

        // Store the first response date:
        if (first_response_date_.empty())
            first_response_date_ = list_records_parser.getResponseDate();

        // Handle the parsed records:
        received_record_count += list_records_parser.getRecordCount();
        const std::list<OaiPmh::Record> &records(list_records_parser.getRecords());
        for (std::list<OaiPmh::Record>::const_iterator record(records.begin()); record != records.end();
             ++record)
        {
            if (processRecord(*record, verbosity, logger))
                ++record_processed_count;
        }

        // Grab the new resumption token:
        resumption_token = list_records_parser.getResumptionToken();
    } while (not resumption_token.empty());

    if (verbosity >= 2) {
        logger->log("Finished harvesting repository '" + repository_name_ + "',"
                    " set '" + set_spec + "', updated since '" + from + "'.\n"
                    "\t" + StringUtil::ToString(received_xml_page_count) + " XML pages were retrieved.\n"
                    "\t" + StringUtil::ToString(received_record_count)   + " OAI-PMH records were retrieved.");
    }
}


// Client::harvest -- Harvest a single set from a repository
//
void Client::harvest(const std::string &set_name, const unsigned verbosity, Logger * const logger) {
    if (verbosity >= 3)
        logger->log("Harvesting set '" + set_name + "' from '" + repository_name_ + "'.");

    // Get the date of the last harvest from the progress file:
    std::string last_harvest_date;
    std::string progress_file(progressFile(set_name));
    if (FileUtil::Exists(progress_file)) {
        if (harvest_mode_ == FULL) {
            // Ignore the progress file and perform a full harvest:
            FileUtil::DeleteFile(progress_file);
        } else {
            // Perform an incremental harvest:
            FileUtil::ReadString(progress_file, &last_harvest_date);
            if (verbosity >= 3)
                logger->log("Last harvest date read from " + progress_file + ": " +
                            last_harvest_date);
        }
    }

    // Harvest each set in turn:
    try {
        std::string actual_set_name((set_name == "all") ? "" : set_name);
        harvestSet(actual_set_name, last_harvest_date, "" /* until */, verbosity, logger);
    } catch (const std::exception &x) {
        logger->log("An error occurred while harvesting %s/%s: %s",
                    repository_name_.c_str(), set_name.c_str(), x.what());
        throw;
    }

    // Write the date of this harvest to the progress file:
    FileUtil::WriteString(progress_file, first_response_date_);
    if (verbosity >= 3)
        logger->log("Harvest date written to " + progress_file + ": " + first_response_date_);
}


// Client::harvest -- Harvest a repository
//
void Client::harvest(const unsigned verbosity, Logger * const logger) {
    if (verbosity >= 3)
        logger->log("Harvesting all known sets for repository '" + repository_name_ + "'.");

    // Import each set
    for (std::list<std::string>::const_iterator set_name(sets_.begin()); set_name != sets_.end(); ++set_name)
        harvest(*set_name, verbosity, logger);
}


// Client::identify -- Return the repository's response to an Identify request.
//
bool Client::identify(std::string * const xml_response, std::string * const error_message) {
    xml_response->clear();
    error_message->clear();

    std::string url_as_string(base_url_);
    if (StringUtil::EndsWith(url_as_string, "/"))
        url_as_string += "request";
    else
        url_as_string += "/request";

    Url url(url_as_string, Url::NO_AUTO_OPERATIONS);
    StringMap args;
    args["verb"] = "Identify";

    return WebUtil::ExecGetHTTPRequest(url, TimeLimit(20000 /* ms */), args, xml_response, error_message, "text/xml");
}


} // namespace OaiPmh

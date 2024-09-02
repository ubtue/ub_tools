/** \file    OaiPmhClient.cc
 *  \brief   Definition of the OaiPmh::Client class, representing an OAI-PMH client.
 *  \author  Dr. Gordon W. Paynter
 *  \author  Dr. Johannes Ruscheinski
 */

/*
 *  Copyright 2003-2008 Project iVia.
 *  Copyright 2003-2008 The Regents of The University of California.]
 *  Copyright 2017-2019 Universitätsbibliothek Tübingen
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
#include <cerrno>
#include "FileUtil.h"
#include "HtmlUtil.h"
#include "IniFile.h"
#include "MiscUtil.h"
#include "StringUtil.h"
#include "Url.h"
#include "UrlUtil.h"
#include "WebUtil.h"
#include "XMLParser.h"
#include "util.h"


namespace { // Helper functions.


/** \class  ListRecordsParser
 *  \brief  A class that reads an OAI ListRecords response and creates records based on its contents.
 */
class ListRecordsParser {
    unsigned verbosity_;

    /** If true, an error of some sort was detected processing the XML and we ignore the remaining XML. */
    bool detected_error_;
    std::string error_code_;
    std::string error_message_;

    std::string response_date_;                //< Will hold the value of the "responseDate" element.
    std::string resumption_token_;             //< Will hold the value of the "resumptionToken" element.
    std::string identifier_;                   //< Will hold the value of the "identifier" element.
    std::string datestamp_;                    //< Will hold the value of the "datestamp" element.
    std::list<OaiPmh::Field> metadata_fields_; //< The list of metadata fields for the current record.

    /** The records received. */
    std::list<OaiPmh::Record> records_;
    /** The number of records received. */
    unsigned received_record_count_;

    /** Update the database with a complete received record. */
    bool importReceivedRecord();

public:
    /** Construct a ListRecords parser. */
    ListRecordsParser(): detected_error_(false), received_record_count_(0) { }
    void parse(const std::string &xml_document, const unsigned verbosity);

    /** Return true if an error was detected. */
    bool detectedError() const { return detected_error_; }

    /** Get the returned error. */
    const std::string &getErrorCode(std::string *error_message = NULL) const;

    /** Get the resumptionToken (if any) extracted from the XML file.*/
    std::string getResumptionToken() const { return resumption_token_; }

    /** Get the responseDate (if any) extracted from the XML file. */
    std::string getResponseDate() const { return response_date_; }

    /** Get the metadata records extracted from the XML file. */
    const std::list<OaiPmh::Record> &getRecords() const { return records_; }

    /** Get the number of records extracted from the XML file.*/
    unsigned getRecordCount() const { return received_record_count_; }
};


// importReceivedRecord -- Import a foreign record into record_info
//
bool ListRecordsParser::importReceivedRecord() {
    // Make sure the record has an identifier
    if (identifier_.empty()) {
        if (verbosity_ >= 1)
            logger->info("Parser skipping record: no identifier found.");
        return false;
    }
    if (verbosity_ >= 5)
        logger->info("Parser creating record: " + identifier_);

    // Create a result:
    records_.push_back(OaiPmh::Record(identifier_, datestamp_));
    for (const auto &field : metadata_fields_)
        records_.back().addField(field);

    // All done:
    ++received_record_count_;
    return true;
}


void ListRecordsParser::parse(const std::string &xml_document, const unsigned verbosity) {
    verbosity_ = verbosity;

    XMLParser parser(xml_document, XMLParser::XML_STRING);
    XMLParser::XMLPart xml_part;

    bool error_tag_open(false);
    bool response_date_tag_open(false);
    bool header_tag_open(false);
    bool identifier_tag_open(false);
    bool datestamp_tag_open(false);
    bool resumption_token_tag_open(false);
    bool metadata_tag_open(false);

    std::string current_tag_name, current_tag_value, current_tag_attrib;

    while (parser.getNext(&xml_part)) {
        if (xml_part.type_ == XMLParser::XMLPart::OPENING_TAG) {
            if (xml_part.data_ == "error") {
                error_tag_open = true;
                error_message_.clear();
                const auto error_code(xml_part.attributes_.find("code"));
                if (error_code != xml_part.attributes_.end())
                    error_code_ = error_code->second;
            } else if (header_tag_open) {
                // Between header tags, so process a header element:
                if (xml_part.data_ == "identifier") {
                    identifier_tag_open = true;
                    identifier_.clear();
                } else if (xml_part.data_ == "datestamp") {
                    datestamp_tag_open = true;
                    datestamp_.clear();
                }
            } else if (metadata_tag_open) {
                // Between metadata tags, so we must be processing a metadata element:
                current_tag_name = xml_part.data_;
                current_tag_value.clear();
                current_tag_attrib.clear();

                const auto xsi_type(xml_part.attributes_.find("xsi:type"));
                if (xsi_type != xml_part.attributes_.end())
                    current_tag_attrib = xsi_type->second;
            } else if (xml_part.data_ == "responseDate") {
                response_date_tag_open = true;
                response_date_.clear();
            } else if (xml_part.data_ == "resumptionToken") {
                resumption_token_tag_open = true;
                resumption_token_.clear();
            } else if (xml_part.data_ == "header")
                header_tag_open = true;
            else if (xml_part.data_ == "metadata") {
                metadata_tag_open = true;
                metadata_fields_.clear();
            }
        } else if (xml_part.type_ == XMLParser::XMLPart::CLOSING_TAG) {
            if (error_tag_open) {
                error_message_ = "the OAI-PMH server reported an error (" + error_code_ + "): " + error_message_;
                detected_error_ = true;
                break;
            } else if (response_date_tag_open and xml_part.data_ == "responseDate")
                response_date_tag_open = false;
            else if (resumption_token_tag_open and xml_part.data_ == "resumptionToken")
                resumption_token_tag_open = false;
            else if (identifier_tag_open and xml_part.data_ == "identifier")
                identifier_tag_open = false;
            else if (datestamp_tag_open and xml_part.data_ == "datestamp")
                datestamp_tag_open = false;
            else if (header_tag_open and xml_part.data_ == "header")
                header_tag_open = false;
            else if (xml_part.data_ == "metadata")
                metadata_tag_open = false;
            else if (metadata_tag_open) {
                // Finished processing some data element:
                /** \todo  Should we warn the user if the data received is not properly escaped? */
                if (not HtmlUtil::IsHtmlEscaped(current_tag_value))
                    HtmlUtil::HtmlEscape(&current_tag_value);
                metadata_fields_.push_back(OaiPmh::Field(current_tag_name, current_tag_value, current_tag_attrib));
                current_tag_name.clear();
            } else if (xml_part.data_ == "record") {
                // The end of the record, save to database:
                importReceivedRecord();
                return;
            }

            // Make sure that when we open simple tags, we immediately close them (i.e. they are not nested).
            if (unlikely(response_date_tag_open))
                throw std::runtime_error("in ListRecordsParser: responseDate tag was not closed!");
            if (unlikely(resumption_token_tag_open))
                throw std::runtime_error("in ListRecordsParser: resumptionToken tag was not closed!");
            if (unlikely(identifier_tag_open))
                throw std::runtime_error("in ListRecordsParser: identifier tag was not closed!");
            if (unlikely(datestamp_tag_open))
                throw std::runtime_error("in ListRecordsParser: datestamp tag was not closed!");
        } else if (xml_part.type_ == XMLParser::XMLPart::CHARACTERS) {
            if (error_tag_open)
                error_message_ += xml_part.data_;
            else if (response_date_tag_open)
                response_date_ += xml_part.data_;
            else if (resumption_token_tag_open)
                resumption_token_ += xml_part.data_;
            else if (identifier_tag_open)
                identifier_ += xml_part.data_;
            else if (datestamp_tag_open)
                datestamp_ += xml_part.data_;
            else if (metadata_tag_open) {
                // Process a data element (extract data to later store in the database).
                current_tag_value += xml_part.data_;
            }
        }
    }
}


const std::string &ListRecordsParser::getErrorCode(std::string *error_message) const {
    if (error_message != NULL)
        *error_message = error_message_;

    return error_code_;
}


} // unnamed namespace


namespace { // Helper functions for making OAI-PMH requests.


bool GetListRecordsResponse(const std::string &server_url, const std::string &from, const std::string &until, const std::string &set_spec,
                            const std::string &metadataPrefix, const std::string &resumptionToken, const unsigned verbosity,
                            Logger * const logger, std::string * const data) {
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
        logger->info("HTTP GET: " + server_url + " " + MiscUtil::StringMapToString(args));


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
        logger->info("HTTP GET failed: " + error_message);

        if (attempt_count <= max_attempts) {
            logger->info("Retrying in " + std::to_string(sleep_time) + " seconds");
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
    base_url_ = ini_file.getString(section_name, "base_url");
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
    return ("/tmp/" + std::string(::program_invocation_name) + std::string(".") + repository_name_
            + std::string(set_name.empty() ? "" : "." + set_name) + ".progress");
}


static bool LookFor(const std::string &expected_tag, XMLParser * const xml_parser, std::string * const extracted_data,
                    std::string * const err_msg) {
    XMLParser::XMLPart xml_part;
    if (unlikely(not xml_parser->getNext(&xml_part) or xml_part.type_ != XMLParser::XMLPart::OPENING_TAG or xml_part.data_ != expected_tag))
    {
        *err_msg = "expected opening <metadataPrefix> tag!";
        return false;
    }

    if (unlikely(not xml_parser->getNext(&xml_part) or xml_part.type_ != XMLParser::XMLPart::CHARACTERS)) {
        *err_msg = "expected character data!";
        return false;
    }
    *extracted_data = xml_part.data_;

    if (unlikely(not xml_parser->getNext(&xml_part) or xml_part.type_ != XMLParser::XMLPart::CLOSING_TAG or xml_part.data_ != expected_tag))
    {
        *err_msg = "expected closing <metadataPrefix> tag!";
        return false;
    }

    return true;
}


static bool ParseMetadataFormat(XMLParser * const xml_parser, Client::MetadataFormatDescriptor * const metadata_format_descriptor,
                                std::string * const err_msg) {
    XMLParser::XMLPart xml_part;
    if (unlikely(not xml_parser->getNext(&xml_part))) {
        *err_msg = "could not parse metadata format!";
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

    metadata_format_descriptor->metadata_prefix_ = metadata_prefix;
    metadata_format_descriptor->schema_ = schema;
    metadata_format_descriptor->metadata_namespace_ = metadata_namespace;

    return true;
}


bool Client::listMetadataFormats(std::vector<MetadataFormatDescriptor> * const metadata_format_list, std::string * const error_message,
                                 const std::string &identifier) {
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
    const bool success(WebUtil::ExecGetHTTPRequest(url, TimeLimit(20000 /* ms */), args, &xml_response, error_message, "text/xml"));
    if (not success)
        return false;

    metadata_format_list->clear();

    XMLParser xml_parser(xml_response, XMLParser::XML_STRING);
    if (unlikely(not xml_parser.skipTo(XMLParser::XMLPart::OPENING_TAG, "ListMetadataFormats"))) {
        *error_message = "failed to find <ListMetadataFormats>!";
        return false;
    }

    // Process the "metadataFormat" sections:
    for (;;) {
        if (not xml_parser.skipTo(XMLParser::XMLPart::OPENING_TAG, "metadataFormat"))
            return true;
        MetadataFormatDescriptor metadata_format_descriptor;
        std::string err_msg;
        if (unlikely(not ParseMetadataFormat(&xml_parser, &metadata_format_descriptor, &err_msg)))
            logger->error("in Client::listMetadataFormats: error while parsing \"metadataFormat\"! (" + err_msg + ")");
        metadata_format_list->push_back(metadata_format_descriptor);
    }
}


// Client::harvestSet -- harvest a single set from a repository
//
void Client::harvestSet(const std::string &set_spec, const std::string &from, const std::string &until, const unsigned verbosity,
                        Logger * const logger) {
    if (verbosity >= 2) {
        logger->info("Harvesting repository '" + repository_name_ + "', set '" + set_spec + "'");
        if (not from.empty())
            logger->info("Harvesting records updated since: '" + from + "'");
        logger->info("OAI base URL: " + base_url_);
        logger->info("metadataPrefix: " + metadata_prefix_);
    }

    // Import statistics
    unsigned received_xml_page_count(0);
    unsigned received_record_count(0);

    std::string response_date, resumption_token;
    do {
        std::string xml_document;
        if (not GetListRecordsResponse(base_url_, from, until, set_spec, metadata_prefix_, resumption_token, verbosity, logger,
                                       &xml_document))
            throw std::runtime_error("An error occurred while talking to the OAI-PMH server!");
        ++received_xml_page_count;

        // Parse the XML document:
        ListRecordsParser list_records_parser;
        try {
            list_records_parser.parse(xml_document, verbosity);
        } catch (const std::runtime_error &exc) {
            std::string error_message("An error occurred while parsing the data returned by the OAI-PMH server! (" + std::string(exc.what())
                                      + ")");
            if (not resumption_token.empty())
                error_message += " Resumption token was \"" + resumption_token + "\". ";
            throw std::runtime_error(error_message);
        }

        // Check for OAI-PMH error conditions in the XML:
        if (list_records_parser.detectedError()) {
            std::string error_message;
            std::string error_code(list_records_parser.getErrorCode(&error_message));

            if (verbosity >= 4)
                logger->info("Client::harvestSet: import error, code: " + error_code + ", message: " + error_message);

            if (error_code == "noRecordsMatch")
                // There were no records available for us to return.
                resumption_token.clear();
            else {
                // A genuine error occurred.  Report it.
                if (not resumption_token.empty())
                    error_message = "OAI-PMH error: resumption token: \"" + resumption_token + "\", error code: \"" + error_code
                                    + "\", error message: \"" + error_message + "\".";
                throw std::runtime_error(error_message);
            }
        }

        // Store the first response date:
        if (first_response_date_.empty())
            first_response_date_ = list_records_parser.getResponseDate();

        // Handle the parsed records:
        received_record_count += list_records_parser.getRecordCount();
        const std::list<OaiPmh::Record> &records(list_records_parser.getRecords());
        for (std::list<OaiPmh::Record>::const_iterator record(records.begin()); record != records.end(); ++record) {
            processRecord(*record, verbosity, logger)
        }

        // Grab the new resumption token:
        resumption_token = list_records_parser.getResumptionToken();
    } while (not resumption_token.empty());

    if (verbosity >= 2) {
        logger->info("Finished harvesting repository '" + repository_name_ + "',"
                     " set '" + set_spec + "', updated since '" + from + "'.\n"
                     "\t" + StringUtil::ToString(received_xml_page_count) + " XML pages were retrieved.\n"
                     "\t" + StringUtil::ToString(received_record_count)   + " OAI-PMH records were retrieved.");
    }
}


// Client::harvest -- Harvest a single set from a repository
//
void Client::harvest(const std::string &set_name, const unsigned verbosity, Logger * const logger) {
    if (verbosity >= 3)
        logger->info("Harvesting set '" + set_name + "' from '" + repository_name_ + "'.");

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
                logger->info("Last harvest date read from " + progress_file + ": " + last_harvest_date);
        }
    }

    // Harvest each set in turn:
    try {
        std::string actual_set_name((set_name == "all") ? "" : set_name);
        harvestSet(actual_set_name, last_harvest_date, "" /* until */, verbosity, logger);
    } catch (const std::exception &x) {
        logger->info("An error occurred while harvesting " + repository_name_ + "/" + set_name + ": " + std::string(x.what()));
        throw;
    }

    // Write the date of this harvest to the progress file:
    FileUtil::WriteString(progress_file, first_response_date_);
    if (verbosity >= 3)
        logger->info("Harvest date written to " + progress_file + ": " + first_response_date_);
}


// Client::harvest -- Harvest a repository
//
void Client::harvest(const unsigned verbosity, Logger * const logger) {
    if (verbosity >= 3)
        logger->info("Harvesting all known sets for repository '" + repository_name_ + "'.");

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

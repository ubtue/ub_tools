/** \file oai_pmh_harvester.cc
 *  \author Dr. Johannes Ruscheinski (johannes.ruscheinski@uni-tuebingen.de)
 *
 *  \copyright 2017-2021 Universitätsbibliothek Tübingen.  All rights reserved.
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
#include <iostream>
#include <cctype>
#include "Compiler.h"
#include "Downloader.h"
#include "FileUtil.h"
#include "HttpHeader.h"
#include "KeyValueDB.h"
#include "MARC.h"
#include "StringUtil.h"
#include "UrlUtil.h"
#include "XMLParser.h"
#include "UBTools.h"
#include "util.h"


namespace {


std::string ExtractResumptionToken(const std::string &xml_document, std::string * const cursor,
                                   std::string * const complete_list_size)
{
    cursor->clear();
    complete_list_size->clear();

    XMLParser xml_parser(xml_document, XMLParser::XML_STRING);
    if (not xml_parser.skipTo(XMLParser::XMLPart::OPENING_TAG, "resumptionToken"))
        return "";

    XMLParser::XMLPart xml_part;
    if (not xml_parser.getNext(&xml_part) or xml_part.type_ == XMLParser::XMLPart::CLOSING_TAG)
        return "";
    if (xml_part.type_ != XMLParser::XMLPart::CHARACTERS)
        LOG_ERROR("strange resumption token XML structure!");

    // Extract the "cursor" attribute:
    auto name_and_value(xml_part.attributes_.find("cursor"));
    if (name_and_value != xml_part.attributes_.end())
        *cursor = name_and_value->second;

    // Extract the "completeListSize" attribute:
    name_and_value = xml_part.attributes_.find("completeListSize");
    if (name_and_value != xml_part.attributes_.end())
        *complete_list_size = name_and_value->second;

    return xml_part.data_;
}


// Helper for ExtractEncapsulatedRecordData.  Removes the trailing whitespace and </metadata>.
bool StripOffTrailingGarbage(std::string * const extracted_records) {
    // 1. back skip over the "</metadata>":
    size_t pos(extracted_records->rfind('<'));
    if (unlikely(pos == std::string::npos))
        return false;

    // 2. Now remove any trailing whitespace:
    while (likely(pos > 0) and isspace((*extracted_records)[--pos]))
        /* Intentionally empty! */;

    extracted_records->resize(pos + 1);
    return true;
}


// Returns the number of extracted records.
unsigned ExtractEncapsulatedRecordData(XMLParser * const xml_parser, std::string * const extracted_records) {
    unsigned record_count(0);
    while (xml_parser->skipTo(XMLParser::XMLPart::OPENING_TAG, "record")) {
        ++record_count;
        if (not xml_parser->skipTo(XMLParser::XMLPart::OPENING_TAG, "metadata"))
            LOG_ERROR("no <metadata> tag found after a <record> tag!");

        XMLParser::XMLPart closing_tag;
        if (not xml_parser->skipTo(XMLParser::XMLPart::CLOSING_TAG, "metadata", &closing_tag, extracted_records))
            LOG_ERROR("no </metadata> tag found after a <metadata> tag!");

        StripOffTrailingGarbage(extracted_records);
        *extracted_records += '\n';
    }

    return record_count;
}


bool ListRecords(const std::string &url, const unsigned time_limit_in_seconds_per_request, const bool ignore_ssl_certificates,
                 File * const output, std::string * const resumption_token, std::string * const cursor,
                 std::string * const complete_list_size, unsigned * total_record_count)
{
    const TimeLimit time_limit(time_limit_in_seconds_per_request * 1000);
    Downloader::Params params(Downloader::DEFAULT_USER_AGENT_STRING,
                              Downloader::DEFAULT_ACCEPTABLE_LANGUAGES,
                              Downloader::DEFAULT_MAX_REDIRECTS,
                              Downloader::DEFAULT_DNS_CACHE_TIMEOUT,
                              false, /*honour_robots_dot_txt*/
                              Downloader::TRANSPARENT,
                              PerlCompatRegExps(),
                              false, /*debugging*/
                              true,/*follow_redirects*/
                              Downloader::DEFAULT_META_REDIRECT_THRESHOLD,
                              ignore_ssl_certificates, /*ignore SSL certificates*/
                              "", /*proxy_host_and_port*/
                              {}, /*additional headers*/
                              "" /*post_data*/);
    Downloader downloader(url, params, time_limit);
    if (downloader.anErrorOccurred())
        LOG_ERROR("harvest failed: " + downloader.getLastErrorMessage());

    const HttpHeader http_header(downloader.getMessageHeader());
    const unsigned status_code(http_header.getStatusCode());
    if (status_code < 200 or status_code > 299)
        LOG_ERROR("server returned a status code of " + std::to_string(status_code) + "!");

    const std::string message_body(downloader.getMessageBody());
    std::string extracted_records;
    XMLParser xml_parser(message_body, XMLParser::XML_STRING);
    const unsigned record_count(ExtractEncapsulatedRecordData(&xml_parser, &extracted_records));
    LOG_INFO("Extracted " + std::to_string(record_count));
    if (record_count == 0) {
        xml_parser.rewind();
        XMLParser::XMLPart xml_part;
        if (not xml_parser.skipTo(XMLParser::XMLPart::OPENING_TAG, "error", &xml_part))
            return 0;
        const auto key_and_value(xml_part.attributes_.find("code"));
        std::string error_msg;
        if (key_and_value != xml_part.attributes_.cend())
            error_msg += key_and_value->second + ": ";

        if (xml_parser.getNext(&xml_part) and xml_part.type_ == XMLParser::XMLPart::CHARACTERS)
            error_msg += xml_part.data_;
        LOG_ERROR("OAI-PMH server returned an error: " + error_msg + " (We sent \"" + url + "\")");
    } else { // record_count > 0
        *total_record_count += record_count;
        if (not output->write(extracted_records))
            LOG_ERROR("failed to write to \"" + output->getPath() + "\"! (Disc full?)");
    }

    *resumption_token = ExtractResumptionToken(message_body, cursor, complete_list_size);
    return not resumption_token->empty();
}


std::string MakeRequestURL(const std::string &base_url, const std::string &metadata_prefix,
                           const std::string &harvest_set_or_identifier, const std::string &resumption_token)
{
    std::string request_url;
    if (not resumption_token.empty())
        request_url = base_url + "?verb=ListRecords&resumptionToken=" + UrlUtil::UrlEncode(resumption_token);
    else if (harvest_set_or_identifier.empty())
        request_url = base_url + "?verb=ListRecords&metadataPrefix=" + metadata_prefix;
    else
        request_url = base_url + "?verb=ListRecords&metadataPrefix=" + metadata_prefix + "&" + harvest_set_or_identifier;
    LOG_INFO("Request URL = " + request_url);

    return request_url;
}


std::unique_ptr<KeyValueDB> CreateOrOpenKeyValueDB(const std::string &path_to_dups_database) {
    if (not FileUtil::Exists(path_to_dups_database))
        KeyValueDB::Create(path_to_dups_database);

    return std::unique_ptr<KeyValueDB>(new KeyValueDB(path_to_dups_database));
}


// \return True, if the record was written and false o/w.
bool WriteIfNotDupe(const MARC::Record &record, KeyValueDB * const dups_db, MARC::Writer * const marc_writer) {
    if (dups_db == nullptr) {
        marc_writer->write(record);
        return true;
    }

    const std::string checksum(MARC::CalcChecksum(record));
    if (dups_db->keyIsPresent(checksum)) {
        LOG_DEBUG("found a dupe w/ checksum \"" + checksum + "\".");
        return false;
    }
    dups_db->addOrReplace(checksum, TimeUtil::GetCurrentDateAndTime());
    marc_writer->write(record);
    return true;
}


// Mostly uses the mapping found at https://www.loc.gov/marc/dccross.html to map DC to MARC.
void GenerateValidatedOutputFromOAI_DC(KeyValueDB * const dups_db, XMLParser * const xml_parser,
                                       const std::string &control_number_prefix, MARC::Writer * const marc_writer)
{
    unsigned record_number(0), counter(0);
    while (xml_parser->skipTo(XMLParser::XMLPart::OPENING_TAG, "oai_dc:dc")) {
        ++record_number;
        MARC::Record new_record(MARC::Record::TypeOfRecord::LANGUAGE_MATERIAL, MARC::Record::BibliographicLevel::UNDEFINED,
                                control_number_prefix + StringUtil::ToString(record_number, /* radix = */10, /* width = */6,
                                                                             /* padding_char = */'0'));
        new_record.insertField(MARC::Tag("935"), { { 'a', control_number_prefix }, { '2', "LOK" } });

        XMLParser::XMLPart xml_part;
        std::string last_data;
        while (xml_parser->getNext(&xml_part)) {
            if (xml_part.type_ == XMLParser::XMLPart::CHARACTERS)
                xml_part.data_.swap(last_data);
            else if (xml_part.type_ == XMLParser::XMLPart::CLOSING_TAG and StringUtil::StartsWith(xml_part.data_, "dc:")) {
                const std::string tag(xml_part.data_.substr(3));
                if (tag == "title")
                    new_record.insertField(MARC::Tag("245"), 'a', last_data);
                else if (tag == "contributor")
                    new_record.insertField(MARC::Tag("720"), 'a', last_data);
                else if (tag == "creator")
                    new_record.insertField(MARC::Tag("720"), { { 'a', last_data}, { 'e', "author" } });
                else if (tag == "description")
                    new_record.insertField(MARC::Tag("520"), 'a', last_data);
                else if (tag == "format")
                    new_record.insertField(MARC::Tag("856"), 'q', last_data);
                else if (tag == "identifier") {
                    if (StringUtil::StartsWith(last_data, "http://doi.org/")) {
                        new_record.insertField(MARC::Tag("024"),
                                               { { 'a', last_data.substr(__builtin_strlen("http://doi.org/")) },
                                                 { '2', "doi" } });
                        new_record.insertField(MARC::Tag("856"), 'u', last_data);
                    }
                } else if (tag == "date")
                    new_record.insertField(MARC::Tag("260"), 'c', last_data);
                else if (tag == "language")
                    new_record.insertField(MARC::Tag("546"), 'a', last_data);
                else if (tag == "publisher")
                    new_record.insertField(MARC::Tag("260"), 'b', last_data);
                else if (tag == "relation")
                    new_record.insertField(MARC::Tag("787"), 'n', last_data, /* indicator1 = */'0');
                else if (tag == "rights")
                    new_record.insertField(MARC::Tag("540"), 'a', last_data);
                else if (tag == "type")
                    new_record.insertField(MARC::Tag("655"), 'a', last_data, ' ', '7');
                else if (tag == "source")
                    new_record.insertField(MARC::Tag("786"), 'n', last_data, '0', ' ');
                else if (tag == "coverage")
                    new_record.insertField(MARC::Tag("500"), 'a', last_data);
                else if (tag == "subject")
                    new_record.insertField(MARC::Tag("653"), 'a', last_data);
                else
                    LOG_ERROR("Unhandled tag: \"" + xml_part.data_ + "\"!");
            } else if (xml_part.data_ == "oai_dc:dc") {
                if (WriteIfNotDupe(new_record, dups_db, marc_writer))
                    counter++;
                last_data.clear();
                break;
            }
        }
    }

    LOG_INFO("Generated " + std::to_string(counter) + " MARC record(s).");
}


void GenerateValidatedOutputFromMARC(KeyValueDB * const dups_db, MARC::Reader * const marc_reader,
                                     const std::string &control_number_prefix, MARC::Writer * const marc_writer)
{
    unsigned record_number(0), counter(0);
    while (MARC::Record record = marc_reader->read()) {
        if (not record.hasValidLeader())
            continue;
        ++record_number;

        if (record.getControlNumber().empty()) {
            const std::string control_number(control_number_prefix
                                             + StringUtil::Map(StringUtil::ToString(record_number, 10, 10), ' ', '0'));
            record.insertField("001", control_number);
        }

        if (WriteIfNotDupe(record, dups_db, marc_writer))
            counter++;
    }

    LOG_INFO("Extracted " + std::to_string(counter) + " MARC record(s).");
}


} // unnamed namespace


int Main(int argc, char **argv) {
    if (argc > 1 and std::strcmp(argv[1], "--skip-dups") == 0)
        --argc, ++argv;

    bool ignore_ssl_certificates(false);
    if (argc > 1 and std::strcmp(argv[1], "--ignore-ssl-certificates") == 0) {
       ignore_ssl_certificates = true;
       --argc, ++argv;
    }

    if (argc != 7 and argc != 8)
        ::Usage("[--skip-dups] [--ignore-ssl-certificates] base_url metadata_prefix [harvest_set_or_identifier] "
                "control_number_prefix output_filename time_limit_per_request path_to_dups_database\n"
                "If \"--skip-dups\" has been specified, records that we already encountered in the past won't\n"
                "included in the output file.\n"
                "\"harvest_set_or_identifier\" must start with \"set=\" or \"identifier=\".\n"
                "\"control_number_prefix\" will be used if the received records have no control numbers\n"
                "to autogenerate our own control numbers.  \"time_limit_per_request\" is in seconds. (Some\n"
                "servers are very slow so we recommend at least 20 seconds!)\n");

    const std::string base_url(argv[1]);

    const std::string metadata_prefix(argv[2]);
    if (metadata_prefix != "marc" and metadata_prefix != "oai_dc")
        LOG_ERROR("metadata_prefix must be \"marc\" or \"oai_dc\"!");

    const std::string harvest_set_or_identifier(argc == 7 ? argv[3] : "");
    if (not harvest_set_or_identifier.empty()
        and (not StringUtil::StartsWith(harvest_set_or_identifier, "set=")
             and not StringUtil::StartsWith(harvest_set_or_identifier, "identifier=")))
        LOG_ERROR("harvest_set_or_identifier must start with set= or identifier=!");

    const std::string control_number_prefix(argc == 7 ? argv[3] : argv[2]);
    const std::string output_filename(argc == 7 ? argv[5] : argv[4]);
    const std::string time_limit_per_request_as_string(argc == 7 ? argv[6] : argv[5]);
    const std::string path_to_dups_database(argc == 7 ? argv[7] : argv[6]);

    unsigned time_limit_per_request_in_seconds;
    if (not StringUtil::ToUnsigned(time_limit_per_request_as_string, &time_limit_per_request_in_seconds))
        LOG_ERROR("\"" + time_limit_per_request_as_string + "\" is not a valid time limit!");

    const std::string TEMP_FILENAME("/tmp/oai_pmh_harvester.temp.xml");
    std::unique_ptr<File> temp_output(FileUtil::OpenOutputFileOrDie(TEMP_FILENAME));

    std::string collection_open;
    if (metadata_prefix == "oai_dc")
        collection_open = "<harvest>";
    else if (metadata_prefix == "marc")
        collection_open = "<collection xmlns=\"http://www.loc.gov/MARC21/slim\" "
                          "xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\" "
                          "xsi:schemaLocation=\"http://www.loc.gov/standards/marcxml/schema/MARC21slim.xsd\">";
    temp_output->writeln("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n" + collection_open + "\n");

    std::string resumption_token, cursor, complete_list_size;
    unsigned total_record_count(0);
    while (ListRecords(MakeRequestURL(base_url, metadata_prefix, harvest_set_or_identifier, resumption_token),
                       time_limit_per_request_in_seconds, ignore_ssl_certificates, temp_output.get(),
                       &resumption_token, &cursor, &complete_list_size, &total_record_count))
        LOG_INFO("Continuing download, resumption token was: \"" + resumption_token + "\" (cursor=" + cursor
                 + ", completeListSize=" + complete_list_size + ").");

    std::string collection_close;
    if (metadata_prefix == "oai_dc")
        collection_close = "</harvest>";
    else if (metadata_prefix == "marc")
        collection_close = "</collection>";
    temp_output->writeln(collection_close + "\n");

    temp_output->close();
    LOG_INFO("Downloaded " + std::to_string(total_record_count) + " record(s).");

    const std::unique_ptr<KeyValueDB> dups_db(CreateOrOpenKeyValueDB(path_to_dups_database));
    const std::unique_ptr<MARC::Writer> marc_writer(MARC::Writer::Factory(output_filename));
    if (metadata_prefix == "oai_dc") {
        XMLParser xml_parser(TEMP_FILENAME, XMLParser::XML_FILE);
        GenerateValidatedOutputFromOAI_DC(dups_db.get(), &xml_parser, control_number_prefix, marc_writer.get());
    } else if (metadata_prefix == "marc") {
        std::unique_ptr<MARC::Reader> marc_reader(MARC::Reader::Factory(TEMP_FILENAME, MARC::FileType::XML));
        GenerateValidatedOutputFromMARC(dups_db.get(), marc_reader.get(), control_number_prefix, marc_writer.get());
    } else
        LOG_ERROR("unsupported metadata_prefix \"" + metadata_prefix + "!");
    ::unlink(TEMP_FILENAME.c_str());

    return EXIT_SUCCESS;
}

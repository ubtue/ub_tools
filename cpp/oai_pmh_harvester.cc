/** \file oai_pmh_harvester.cc
 *  \author Dr. Johannes Ruscheinski (johannes.ruscheinski@uni-tuebingen.de)
 *
 *  \copyright 2017-2019 Universitätsbibliothek Tübingen.  All rights reserved.
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


//https://memory.loc.gov/cgi-bin/oai2_0?verb=ListRecords&metadataPrefix=marc21&set=mussm
[[noreturn]] void Usage() {
    std::cerr << "Usage: " << ::progname
              << " [--skip-dups] [--ignore-ssl-certificates] base_url metadata_prefix [harvest_set] control_number_prefix output_filename"
              << " time_limit_per_request\n"
              << "       If \"--skip-dups\" has been specified, records that we already encountered in the past won't\n"
              << "       included in the output file.\n"
              << "       \"control_number_prefix\" will be used if the received records have no control numbers\n"
              << "       to autogenerate our own control numbers.  \"time_limit_per_request\" is in seconds. (Some\n"
              << "       servers are very slow so we recommend at least 20 seconds!)\n\n";
    std::exit(EXIT_FAILURE);
}


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


std::string MakeRequestURL(const std::string &base_url, const std::string &metadata_prefix, const std::string &harvest_set,
                           const std::string &resumption_token)
{
    std::string request_url;
    if (not resumption_token.empty())
        request_url = base_url + "?verb=ListRecords&resumptionToken=" + UrlUtil::UrlEncode(resumption_token);
    else if (harvest_set.empty())
        request_url = base_url + "?verb=ListRecords&metadataPrefix=" + metadata_prefix;
    else
        request_url = base_url + "?verb=ListRecords&metadataPrefix=" + metadata_prefix + "&set=" + harvest_set;
    LOG_INFO("Request URL = " + request_url);

    return request_url;
}


const std::string OAI_DUPS_DB_FILENAME(UBTools::GetTuelibPath() + "oai_dups.db");


std::unique_ptr<KeyValueDB> CreateOrOpenKeyValueDB() {
    if (not FileUtil::Exists(OAI_DUPS_DB_FILENAME))
        KeyValueDB::Create(OAI_DUPS_DB_FILENAME);

    return std::unique_ptr<KeyValueDB>(new KeyValueDB(OAI_DUPS_DB_FILENAME));
}


void GenerateValidatedOutput(KeyValueDB * const dups_db, MARC::Reader * const marc_reader,
                             const std::string &control_number_prefix, MARC::Writer * const marc_writer)
{
    unsigned counter(0);
    while (MARC::Record record = marc_reader->read()) {
        if (not record.hasValidLeader())
            continue;

        if (dups_db != nullptr) {
            const std::string checksum(MARC::CalcChecksum(record));
            if (dups_db->keyIsPresent(checksum)) {
                LOG_DEBUG("found a dupe w/ checksum \"" + checksum + "\".");
                continue;
            }
            dups_db->addOrReplace(checksum, TimeUtil::GetCurrentDateAndTime());
        }

        if (record.getControlNumber().empty()) {
            const std::string control_number(control_number_prefix + StringUtil::Map(StringUtil::ToString(++counter, 10, 10), ' ', '0'));
            record.insertField("001", control_number);
        }

        marc_writer->write(record);
    }
}


} // unnamed namespace


int Main(int argc, char **argv) {
    std::unique_ptr<KeyValueDB> dups_db;
    if (argc > 1 and std::strcmp(argv[1], "--skip-dups") == 0) {
        dups_db = CreateOrOpenKeyValueDB();
        --argc, ++argv;
    }

    bool ignore_ssl_certificates(false);
    if (argc > 1 and std::strcmp(argv[1], "--ignore-ssl-certificates") == 0) {
       ignore_ssl_certificates = true;
       --argc, ++argv;
    }

    if (argc != 6 and argc != 7)
        Usage();

    const std::string base_url(argv[1]);
    const std::string metadata_prefix(argv[2]);
    const std::string harvest_set(argc == 7 ? argv[3] : "");
    const std::string control_number_prefix(argc == 7 ? argv[3] : argv[2]);
    const std::string output_filename(argc == 7 ? argv[5] : argv[4]);
    const std::string time_limit_per_request_as_string(argc == 7 ? argv[6] : argv[5]);

    unsigned time_limit_per_request_in_seconds;
    if (not StringUtil::ToUnsigned(time_limit_per_request_as_string, &time_limit_per_request_in_seconds))
        LOG_ERROR("\"" + time_limit_per_request_as_string + "\" is not a valid time limit!");

    const std::string TEMP_FILENAME("/tmp/oai_pmh_harvester.temp.xml");
    std::unique_ptr<File> temp_output(FileUtil::OpenOutputFileOrDie(TEMP_FILENAME));

    const std::string COLLECTION_OPEN(
                                      "<collection xmlns=\"http://www.loc.gov/MARC21/slim\" "
                                      "xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\" "
                                      "xsi:schemaLocation=\"http://www.loc.gov/standards/marcxml/schema/MARC21slim.xsd\">");
    temp_output->writeln("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n" + COLLECTION_OPEN);

    std::string resumption_token, cursor, complete_list_size;
    unsigned total_record_count(0);
    while (ListRecords(MakeRequestURL(base_url, metadata_prefix, harvest_set, resumption_token),
                       time_limit_per_request_in_seconds, ignore_ssl_certificates, temp_output.get(),
                       &resumption_token, &cursor, &complete_list_size, &total_record_count))
        LOG_INFO("Continuing download, resumption token was: \"" + resumption_token + "\" (cursor=" + cursor
                 + ", completeListSize=" + complete_list_size + ").");

    const std::string COLLECTION_CLOSE("</collection>");
    temp_output->writeln(COLLECTION_CLOSE);
    temp_output->close();
    LOG_INFO("Downloaded " + std::to_string(total_record_count) + " record(s).");

    std::unique_ptr<MARC::Reader> marc_reader(MARC::Reader::Factory(TEMP_FILENAME, MARC::FileType::XML));
    std::unique_ptr<MARC::Writer> marc_writer(MARC::Writer::Factory(output_filename));
    GenerateValidatedOutput(dups_db.get(), marc_reader.get(), control_number_prefix, marc_writer.get());
    ::unlink(TEMP_FILENAME.c_str());

    return EXIT_SUCCESS;
}

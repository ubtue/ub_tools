/** \file oai_pmh_harvester.cc
 *  \author Dr. Johannes Ruscheinski (johannes.ruscheinski@uni-tuebingen.de)
 *
 *  \copyright 2017 Universitätsbibliothek Tübingen.  All rights reserved.
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
#include "MarcRecord.h"
#include "MarcReader.h"
#include "MarcWriter.h"
#include "SimpleXmlParser.h"
#include "StringDataSource.h"
#include "StringUtil.h"
#include "UrlUtil.h"
#include "util.h"


//https://memory.loc.gov/cgi-bin/oai2_0?verb=ListRecords&metadataPrefix=marc21&set=mussm
void Usage() {
    std::cerr << "Usage: " << ::progname
              << " base_url metadata_prefix [harvest_set] control_number_prefix output_filename"
              << " time_limit_per_request\n"
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

    StringDataSource data_source(xml_document);
    SimpleXmlParser<StringDataSource> xml_parser(&data_source);
    if (not xml_parser.skipTo(SimpleXmlParser<StringDataSource>::OPENING_TAG, "resumptionToken"))
        return "";

    SimpleXmlParser<StringDataSource>::Type type;
    std::map<std::string, std::string> attrib_map;
    std::string data;
    if (not xml_parser.getNext(&type, &attrib_map, &data) or type == SimpleXmlParser<StringDataSource>::CLOSING_TAG)
        return "";
    if (type != SimpleXmlParser<StringDataSource>::CHARACTERS)
        logger->error("strange resumption token XML structure!");

    // Extract the "cursor" attribute:
    auto name_and_value(attrib_map.find("cursor"));
    if (name_and_value != attrib_map.end())
        *cursor = name_and_value->second;

    // Extract the "completeListSize" attribute:
    name_and_value = attrib_map.find("completeListSize");
    if (name_and_value != attrib_map.end())
        *complete_list_size = name_and_value->second;

    return data;
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
unsigned ExtractEncapsulatedRecordData(SimpleXmlParser<StringDataSource> * const xml_parser,
                                       std::string * const extracted_records)
{
    unsigned record_count(0);
    while (xml_parser->skipTo(SimpleXmlParser<StringDataSource>::OPENING_TAG, "record")) {
        ++record_count;
        if (not xml_parser->skipTo(SimpleXmlParser<StringDataSource>::OPENING_TAG, "metadata"))
            logger->error("no <metadata> tag found after a <record> tag!");
        xml_parser->skipWhiteSpace();

        if (not xml_parser->skipTo(SimpleXmlParser<StringDataSource>::CLOSING_TAG, "metadata",
                                   /* attrib_map = */ nullptr, extracted_records))
            logger->error("no </metadata> tag found after a <metadata> tag!");

        StripOffTrailingGarbage(extracted_records);
        *extracted_records += '\n';
    }

    return record_count;
}


bool ListRecords(const std::string &url, const unsigned time_limit_in_seconds_per_request, File * const output,
                 std::string * const resumption_token, std::string * const cursor,
                 std::string * const complete_list_size, unsigned * total_record_count)
{
    const TimeLimit time_limit(time_limit_in_seconds_per_request * 1000);
    Downloader downloader(url, Downloader::Params(), time_limit);
    if (downloader.anErrorOccurred())
        logger->error("harvest failed: " + downloader.getLastErrorMessage());

    const HttpHeader http_header(downloader.getMessageHeader());
    const unsigned status_code(http_header.getStatusCode());
    if (status_code < 200 or status_code > 299)
        logger->error("server returned a status code of " + std::to_string(status_code) + "!");

    const std::string message_body(downloader.getMessageBody());
    std::string extracted_records;
    StringDataSource data_source(message_body);
    SimpleXmlParser<StringDataSource> xml_parser(&data_source);
    const unsigned record_count(ExtractEncapsulatedRecordData(&xml_parser, &extracted_records));
    if (record_count == 0) {
        xml_parser.rewind();
        std::map<std::string, std::string> attrib_map;
        if (not xml_parser.skipTo(SimpleXmlParser<StringDataSource>::OPENING_TAG, "error", &attrib_map))
            return 0;
        const auto key_and_value(attrib_map.find("code"));
        std::string error_msg;
        if (key_and_value != attrib_map.cend())
            error_msg += key_and_value->second + ": ";
        SimpleXmlParser<StringDataSource>::Type type;
        std::string data;
        if (xml_parser.getNext(&type, &attrib_map, &data) and type == SimpleXmlParser<StringDataSource>::CHARACTERS)
            error_msg += data;
        logger->error("OAI-PMH server returned an error: " + error_msg + " (We sent \"" + url + "\")");
    } else { // record_count > 0
        *total_record_count += record_count;
        if (not output->write(extracted_records))
            logger->error("failed to write to \"" + output->getPath() + "\"! (Disc full?)");
    }

    *resumption_token = ExtractResumptionToken(message_body, cursor, complete_list_size);
    return not resumption_token->empty();
}


std::string MakeRequestURL(const std::string &base_url, const std::string &metadata_prefix,
                           const std::string &harvest_set, const std::string &resumption_token)
{
    std::string request_url;
    if (not resumption_token.empty())
        request_url = base_url + "?verb=ListRecords&resumptionToken=" + UrlUtil::UrlEncode(resumption_token);
    else if (harvest_set.empty())
        request_url = base_url + "?verb=ListRecords&metadataPrefix=" + metadata_prefix;
    else
        request_url = base_url + "?verb=ListRecords&metadataPrefix=" + metadata_prefix + "&set=" + harvest_set;
    std::cerr << "Request URL = " << request_url << '\n';

    return request_url;
}


void GenerateValidatedOutput(MarcReader * const marc_reader, const std::string &control_number_prefix,
                             MarcWriter * const marc_writer)
{
    unsigned counter(0);
    while (MarcRecord record = marc_reader->read()) {
        std::string control_number(record.getFieldData("001"));
        if (control_number.empty()) {
            control_number = control_number_prefix + StringUtil::Map(StringUtil::ToString(++counter, 10, 10),
                                                                     ' ', '0');
            record.insertField("001", control_number);
        }

        marc_writer->write(record);
    }
}


int main(int argc, char **argv) {
    ::progname = argv[0];

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
        logger->error("\"" + time_limit_per_request_as_string + "\" is not a valid time limit!");

    try {
        const std::string TEMP_FILENAME("/tmp/oai_pmh_harvester.temp.xml");
        std::unique_ptr<File> temp_output(FileUtil::OpenOutputFileOrDie(TEMP_FILENAME));

        const std::string COLLECTION_OPEN(
            "<collection xmlns=\"http://www.loc.gov/MARC21/slim\" "
            "xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\" "
            "xsi:schemaLocation=\"http://www.loc.gov/standards/marcxml/schema/MARC21slim.xsd\">");
        temp_output->write("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n" + COLLECTION_OPEN + "\n");

        std::string resumption_token, cursor, complete_list_size;
        unsigned total_record_count(0);
        while (ListRecords(MakeRequestURL(base_url, metadata_prefix, harvest_set, resumption_token),
                           time_limit_per_request_in_seconds, temp_output.get(), &resumption_token,
                           &cursor, &complete_list_size, &total_record_count))
            std::cerr << "Continuing download, resumption token was: \"" << resumption_token << "\" (cursor="
                      << cursor << ", completeListSize=" << complete_list_size << ").\n";

        const std::string COLLECTION_CLOSE("</collection>");
        temp_output->write(COLLECTION_CLOSE + "\n");
        temp_output->close();
        std::cerr << "Downloaded " << total_record_count << " record(s).\n";

        std::unique_ptr<MarcReader> marc_reader(MarcReader::Factory(TEMP_FILENAME, MarcReader::XML));
        std::unique_ptr<MarcWriter> marc_writer(MarcWriter::Factory(output_filename));
        GenerateValidatedOutput(marc_reader.get(), control_number_prefix, marc_writer.get());
        ::unlink(TEMP_FILENAME.c_str());
    } catch (const std::exception &x) {
        logger->error("caught exception: " + std::string(x.what()));
    }
}

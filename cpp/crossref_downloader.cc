/** \file    crossref_downloader.cc
 *  \brief   Downloads metadata from crossref.org and generates MARC-21 records.
 *  \author  Dr. Johannes Ruscheinski
 *
 *  \copyright (C) 2017, Library of the University of Tübingen
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
#include <stdexcept>
#include <unordered_set>
#include <vector>
#include <cstdlib>
#include <cstring>
#include <kchashdb.h>
#include "Compiler.h"
#include "Downloader.h"
#include "FileUtil.h"
#include "HttpHeader.h"
#include "JSON.h"
#include "MarcRecord.h"
#include "MarcWriter.h"
#include "MiscUtil.h"
#include "StringUtil.h"
#include "TextUtil.h"
#include "UrlUtil.h"
#include "util.h"


void Usage() {
    std::cerr << "Usage: " << ::progname << " [--timeout seconds] journal_list marc_output\n";
    std::exit(EXIT_FAILURE);
}


class CrossrefDate {
    unsigned year_, month_, day_;
public:
    CrossrefDate(const JSON::ObjectNode &tree, const std::string &field);
    bool isValid() const { return year_ != 0; }
    unsigned getYear() const { return year_; }
    unsigned getMonth() const { return month_; }
    unsigned getDay() const { return day_; }
    std::string toString() const;
};


// Parses a JSON subtree that, should it exist looks like [[YYYY, MM, DD]] where the day as well as the
// month may be missing.
CrossrefDate::CrossrefDate(const JSON::ObjectNode &object, const std::string &field) {
    const JSON::ObjectNode * const subtree(dynamic_cast<const JSON::ObjectNode *>(object.getValue(field)));
    if (subtree == nullptr) {
        year_ = month_ = day_ = 0;
        return;
    }

    const JSON::ArrayNode * const array_node(dynamic_cast<const JSON::ArrayNode *>(subtree->getValue("date-parts")));
    if (unlikely(array_node == nullptr))
        Error("in CrossrefDate::CrossrefDate: \"date-parts\" does not exist or is not a JSON array!");

    if (unlikely(array_node->empty()))
        Error("in CrossrefDate::CrossrefDate: nested child of \"" + field + "\" does not exist!");

    const JSON::ArrayNode * const array_node2(dynamic_cast<const JSON::ArrayNode *>(array_node->getValue(0)));
    if (unlikely(array_node2 == nullptr))
        Error("in CrossrefDate::CrossrefDate: inner nested child of \"" + field + "\" is not a JSON array!");

    auto date_component_iter(array_node2->cbegin());
    const auto &date_end(array_node2->cend());
    if (unlikely(date_component_iter == date_end))
        Error("in CrossrefDate::CrossrefDate: year is missing for the \"" + field + "\" date field!");

    const JSON::IntegerNode *year_node(dynamic_cast<const JSON::IntegerNode *>(*date_component_iter));
    if (year_node == nullptr or year_node->getValue() < 0)
        Error("in CrossrefDate::CrossrefDate: cannot convert year component \"" + (*date_component_iter)->toString()
              + "\" to an unsigned integer!");
    year_ = static_cast<unsigned>(year_node->getValue());
    if (unlikely(year_ < 1000 or year_ > 3000))
        Error("in CrossrefDate::CrossrefDate: year component \"" + std::to_string(year_)
              + "\" is unlikely to be a year!");

    ++date_component_iter;
    if (date_component_iter == date_end) {
        month_ = day_ = 0;
        return;
    }

    const JSON::IntegerNode *month_node(dynamic_cast<const JSON::IntegerNode *>(*date_component_iter));
    if (month_node == nullptr or month_node->getValue() < 0)
        Error("in CrossrefDate::CrossrefDate: cannot convert month component \"" + (*date_component_iter)->toString()
              + "\" to an unsigned integer!");
    month_ = static_cast<unsigned>(month_node->getValue());
    if (unlikely(month_ < 1 or month_ > 12))
        Error("in CrossrefDate::CrossrefDate: month component \"" + std::to_string(month_) + "\" is not a month!");

    ++date_component_iter;
    if (date_component_iter == date_end) {
        day_ = 0;
        return;
    }

    const JSON::IntegerNode *day_node(dynamic_cast<const JSON::IntegerNode *>(*date_component_iter));
    if (day_node == nullptr or day_node->getValue() < 0)
        Error("in CrossrefDate::CrossrefDate: cannot convert day component \"" + (*date_component_iter)->toString()
              + "\" to an unsigned integer!");
    day_ = static_cast<unsigned>(day_node->getValue());
    if (unlikely(day_ < 1 or day_ > 31))
        Error("in CrossrefDate::CrossrefDate: day component \"" + std::to_string(day_) + "\" is not a day!");
}


std::string CrossrefDate::toString() const {
    if (month_ == 0)
        return std::to_string(year_);

    std::string month_as_string;
    switch (month_) {
    case 1:
        month_as_string += "January";
        break;
    case 2:
        month_as_string += "February";
        break;
    case 3:
        month_as_string += "March";
        break;
    case 4:
        month_as_string += "April";
        break;
    case 5:
        month_as_string += "May";
        break;
    case 6:
        month_as_string += "June";
        break;
    case 7:
        month_as_string += "July";
        break;
    case 8:
        month_as_string += "August";
        break;
    case 9:
        month_as_string += "September";
        break;
    case 10:
        month_as_string += "October";
        break;
    case 11:
        month_as_string += "November";
        break;
    case 12:
        month_as_string += "December";
        break;
    default:
        Error("in CrossrefDate::toString: " + std::to_string(month_) + " is not a valid month!");
    }

    if (day_ == 0)
        return month_as_string + ", " + std::to_string(year_);

    return month_as_string + " " + std::to_string(day_) + ", " + std::to_string(year_);
}


/** \class MapDescriptor
 *  \brief Describes a mapping from a Crossref JSON field to a MARC-21 field.
 */
class MapDescriptor {
public:
    enum FieldType { STRING, AUTHOR_VECTOR, STRING_VECTOR, YEAR };
protected:
    std::string json_field_;
    FieldType field_type_;
    std::string marc_subfield_;
public:
    MapDescriptor(const std::string &json_field, const FieldType field_type, const std::string &marc_subfield)
        : json_field_(json_field), field_type_(field_type), marc_subfield_(marc_subfield) { }
    virtual ~MapDescriptor() { }

    inline const std::string &getJsonField() const { return json_field_; }
    inline FieldType getFieldType() const { return field_type_; }
    inline const std::string &getMarcSubfield() const { return marc_subfield_; }
    virtual void insertMarcData(const std::string &subfield_value, MarcRecord * const record);
};


void MapDescriptor::insertMarcData(const std::string &subfield_value, MarcRecord * const record) {
    const std::string tag(marc_subfield_.substr(0, DirectoryEntry::TAG_LENGTH));
    const char subfield_code(marc_subfield_.back());
    record->insertSubfield(tag, subfield_code, subfield_value);
}


class DOIMapDescriptor: public MapDescriptor {
public:
    DOIMapDescriptor(): MapDescriptor("DOI", MapDescriptor::STRING, "024a") { }
    virtual void insertMarcData(const std::string &subfield_value, MarcRecord * const record);
};


inline std::string CreateSubfield(const char subfield_code, const std::string &subfield_data) {
    return "\x1F" + std::string(1, subfield_code) + subfield_data;
}


void DOIMapDescriptor::insertMarcData(const std::string &subfield_value, MarcRecord * const record) {
    const std::string tag(marc_subfield_.substr(0, DirectoryEntry::TAG_LENGTH));
    const char subfield_code(marc_subfield_.back());
    record->insertField(tag, "7 " + CreateSubfield(subfield_code, subfield_value)
                        + CreateSubfield('2', "doi"));
}


void InitCrossrefToMarcMapping(std::vector<MapDescriptor *> * const map_descriptors) {
    map_descriptors->emplace_back(new MapDescriptor("URL", MapDescriptor::STRING, "856u"));
    map_descriptors->emplace_back(new MapDescriptor("author", MapDescriptor::AUTHOR_VECTOR, "100a"));
    map_descriptors->emplace_back(new MapDescriptor("title", MapDescriptor::STRING, "245a"));
    map_descriptors->emplace_back(new MapDescriptor("publisher", MapDescriptor::STRING, "260a"));
    map_descriptors->emplace_back(new MapDescriptor("ISSN", MapDescriptor::STRING_VECTOR, "022a"));
    map_descriptors->emplace_back(new DOIMapDescriptor());
}


// Compares "s1" and "s2" while ignoring any occurences of characters found in "ignore_chars".
bool EqualIgnoreChars(const std::string &s1, const std::string &s2, const std::string &ignore_chars) {
    auto ch1(s1.cbegin());
    auto ch2(s2.cbegin());
    while (ch1 != s1.cend() and ch2 != s2.cend()) {
        if (ignore_chars.find(*ch1) != std::string::npos)
            ++ch1;
        else if (ignore_chars.find(*ch2) != std::string::npos)
            ++ch2;
        else if (*ch1 != *ch2)
            return false;
        else
            ++ch1, ++ch2;
    }

    while (ch1 != s1.cend() and ignore_chars.find(*ch1) != std::string::npos)
        ++ch1;
    while (ch2 != s2.cend() and ignore_chars.find(*ch2) != std::string::npos)
        ++ch2;

    return ch1 == s1.cend() and ch2 == s2.cend();
}


bool FuzzyTextMatch(const std::string &s1, const std::string &s2) {
    std::string lowercase_s1;
    if (unlikely(not TextUtil::UTF8ToLower(s1, &lowercase_s1)))
        Error("failed to convert supposed UTF-8 string \"" + s1 + "\" to a wide-character string! (1)");

    std::string lowercase_s2;
    if (unlikely(not TextUtil::UTF8ToLower(s2, &lowercase_s2)))
        Error("failed to convert supposed UTF-8 string \"" + s2 + "\" to a wide-character string! (2)");

    static const std::string IGNORE_CHARS(" :-");
    return EqualIgnoreChars(lowercase_s1, lowercase_s2, IGNORE_CHARS);
}


std::vector<std::string> ExtractString(const JSON::ObjectNode &object_node, const std::string &json_field_name) {
    std::vector<std::string> extracted_values;
    const JSON::JSONNode * const node(object_node.getValue(json_field_name));
    if (node == nullptr or node->getType() != JSON::JSONNode::STRING_NODE)
        return extracted_values;

    extracted_values.emplace_back(reinterpret_cast<const JSON::StringNode *>(node)->getValue());

    return extracted_values;
}


std::string ExtractAuthor(const JSON::ObjectNode &object_node) {
    const JSON::StringNode * const name_node(
        dynamic_cast<const JSON::StringNode *>(object_node.getValue("name")));
    if (name_node != nullptr)
        return name_node->getValue();

    const JSON::StringNode * const family_node(
        dynamic_cast<const JSON::StringNode *>(object_node.getValue("family")));
    if (unlikely(family_node == nullptr))
        Error("in ExtractAuthor: missing or invalid \"family\" node!");

    std::string author(family_node->getValue());
    const JSON::StringNode * const given_node(
        dynamic_cast<const JSON::StringNode *>(object_node.getValue("given")));
    if (given_node != nullptr)
        author += ", " + given_node->getValue();

    return author;
}


std::vector<std::string> ExtractAuthorVector(const JSON::ObjectNode &object_node,
                                             const std::string &json_field_name)
{
    std::vector<std::string> extracted_values;

    const JSON::ArrayNode *array_node(dynamic_cast<const JSON::ArrayNode *>(object_node.getValue(json_field_name)));
    if (array_node == nullptr)
        return extracted_values;

    for (JSON::ArrayNode::const_iterator array_entry(array_node->cbegin()); array_entry != array_node->cend();
         ++array_entry)
    {
        const JSON::ObjectNode *author_node(dynamic_cast<const JSON::ObjectNode *>(*array_entry));
        if (author_node != nullptr)
            extracted_values.emplace_back(ExtractAuthor(*author_node));
    }

    return extracted_values;
}


std::vector<std::string> ExtractStringVector(const JSON::ObjectNode &object_node,
                                             const std::string &json_field_name)
{
    std::vector<std::string> extracted_values;

    const JSON::ArrayNode *array_node(dynamic_cast<const JSON::ArrayNode *>(object_node.getValue(json_field_name)));
    if (array_node == nullptr)
        return extracted_values;

    for (JSON::ArrayNode::const_iterator array_entry(array_node->cbegin()); array_entry != array_node->cend();
         ++array_entry)
    {
        const JSON::StringNode *string_node(dynamic_cast<const JSON::StringNode *>(*array_entry));
        if (unlikely(string_node == nullptr))
            Error("in ExtractStringVector: expected a string node!");
        extracted_values.emplace_back(string_node->getValue());
    }

    return extracted_values;
}


static std::string GetOptionalStringValue(const JSON::ObjectNode &object_node, const std::string &json_field_name) {
    const JSON::StringNode *string_node(dynamic_cast<const JSON::StringNode *>(
        object_node.getValue(json_field_name)));
    return (string_node == nullptr) ? "" : string_node->getValue();

}


void AddIssueInfo(const JSON::ObjectNode &message_tree, MarcRecord * const marc_record) {
    std::string field_data;
    const CrossrefDate issued_date(message_tree, "issued");
    if (issued_date.getDay() != 0)
        field_data += CreateSubfield('b', std::to_string(issued_date.getDay()));
    if (issued_date.getMonth() != 0)
        field_data += CreateSubfield('c', std::to_string(issued_date.getMonth()));

    const std::string optional_volume(GetOptionalStringValue(message_tree, "volume"));
    if (not optional_volume.empty())
        field_data += CreateSubfield('d', optional_volume);

    const std::string optional_issue(GetOptionalStringValue(message_tree, "issue"));
    if (not optional_issue.empty())
        field_data += CreateSubfield('e', optional_issue);

    const std::string optional_page(GetOptionalStringValue(message_tree, "page"));
    if (not optional_page.empty())
        field_data += CreateSubfield('h', optional_page);

    field_data += CreateSubfield('j', std::to_string(issued_date.getYear()));
    marc_record->insertField("936", "uw" + field_data);
}


/** \return True, if we wrote a record and false if we suppressed a duplicate. */
bool CreateAndWriteMarcRecord(MarcWriter * const marc_writer, kyotocabinet::HashDB * const notified_db,
                              const std::string &DOI, const JSON::ObjectNode &message_tree,
                              const std::vector<MapDescriptor *> &map_descriptors)
{
    MarcRecord record;
    record.getLeader().setBibliographicLevel('a'); // We have an article.
    static unsigned control_number(0);
    record.insertField("001", std::to_string(++control_number));

    for (const auto &map_descriptor : map_descriptors) {
        std::vector<std::string> field_values;
        switch (map_descriptor->getFieldType()) {
        case MapDescriptor::STRING:
            field_values = ExtractString(message_tree, map_descriptor->getJsonField());
            break;
        case MapDescriptor::AUTHOR_VECTOR:
            field_values = ExtractAuthorVector(message_tree, map_descriptor->getJsonField());
            break;
        case MapDescriptor::STRING_VECTOR:
            field_values = ExtractStringVector(message_tree, map_descriptor->getJsonField());
            break;
        default:
            Error("in CreateAndWriteMarcRecord: unexpected field type!");
        }

        for (const auto field_value : field_values)
            map_descriptor->insertMarcData(field_value, &record);
        AddIssueInfo(message_tree, &record);
    }

    // If we have already encountered the exact same record in the past we skip writing it:
    std::string old_hash, new_hash(record.calcChecksum());
    if (notified_db->get(DOI, &old_hash)) {
        if (old_hash == new_hash)
            return false;
    }
    if (unlikely(not notified_db->set(DOI, new_hash)))
        Error("in CreateAndWriteMarcRecord: failed to write the DOI \"" + DOI + "\" into \"" + notified_db->path()
              + "\"! (" + std::string(notified_db->error().message()) + ")");

    marc_writer->write(record);
    return true;
}


// Expects "line" to look like "XXXX-XXXX,YYYY-YYYY,...ZZZZ-ZZZZ JJJ" where "XXXX-XXXX", "YYYY-YYYY" and "ZZZZ-ZZZZ"
// are ISSN's and "JJJ" a journal title.
bool GetISSNsAndJournalName(const std::string &line, std::vector<std::string> * const issns,
                            std::string * const journal_name)
{
    const size_t first_space_pos(line.find(' '));
    if (unlikely(first_space_pos == std::string::npos or first_space_pos == 0)) {
        Warning("No space found!");
        return false;
    }

    if (StringUtil::Split(line.substr(0, first_space_pos), ',', issns) == 0) {
        Warning("No ISSNS found!");
        return false;
    }

    for (const auto &issn : *issns) {
        if (unlikely(not MiscUtil::IsPossibleISSN(issn))) {
            Warning(issn + " is not a valid ISSN!");
            return false;
        }
    }

    *journal_name = StringUtil::TrimWhite(line.substr(first_space_pos + 1));
    return not journal_name->empty();
}


void ProcessISSN(const std::string &issn, const unsigned timeout, MarcWriter * const marc_writer,
                 kyotocabinet::HashDB * const notified_db, const std::vector<MapDescriptor *> &map_descriptors,
                 std::unordered_set<std::string> * const already_seen, unsigned * const written_count,
                 unsigned * const suppressed_count)
{
    *written_count = *suppressed_count = 0;

    const std::string DOWNLOAD_URL("https://api.crossref.org/v1/journals/" + issn + "/works");
    Downloader downloader(DOWNLOAD_URL, Downloader::Params(),
                          timeout * 1000);
    if (downloader.anErrorOccurred()) {
        std::cerr << "Error while downloading metadata for ISSN " << issn << ": " << downloader.getLastErrorMessage()
                  << '\n';
        return;
    }

    // Check for rate limiting and error status codes:
    const HttpHeader http_header(downloader.getMessageHeader());
    if (http_header.getStatusCode() == 429)
        Error("we got rate limited!");
    else if (http_header.getStatusCode() != 200) {
        Warning("Crossref returned HTTP status code " + std::to_string(http_header.getStatusCode()) + "!");
        return;
    }

    const std::string &json_document(downloader.getMessageBody());
    JSON::JSONNode *full_tree;
    JSON::Parser parser(json_document);
    if (not parser.parse(&full_tree))
        Error("failed to parse JSON (" + parser.getErrorMessage() + "), download URL was: " + DOWNLOAD_URL);

    const JSON::ObjectNode * const top_node(dynamic_cast<const JSON::ObjectNode *>(full_tree));
    if (unlikely(top_node == nullptr))
        Error("JSON returned from Crossref is not an object! (URL was " + DOWNLOAD_URL + ")");

    const JSON::ObjectNode * const message_node(
        dynamic_cast<const JSON::ObjectNode *>(top_node->getValue("message")));
    if (unlikely(message_node == nullptr))
        return;

    const JSON::ArrayNode * const items(dynamic_cast<const JSON::ArrayNode *>(message_node->getValue("items")));
    if (unlikely(items == nullptr))
        return;

    for (auto item_iter(items->cbegin()); item_iter != items->cend(); ++item_iter) {
        const JSON::ObjectNode * const item(dynamic_cast<JSON::ObjectNode *>(*item_iter));
        if (unlikely(item == nullptr))
            Error("item is JSON \"items\" array as returned by Crossref is not an object!");

        static const std::string EMPTY_STRING;
        const std::string DOI(JSON::LookupString("/DOI", item, &EMPTY_STRING));
        if (unlikely(DOI.empty()))
            Error("No \"DOI\" for an item returned for the ISSN " + issn + "!");

        // Have we already seen this item?
        if (already_seen->find(DOI) != already_seen->cend()) {
            ++*suppressed_count;
            continue;
        }
        already_seen->insert(DOI);

        if (CreateAndWriteMarcRecord(marc_writer, notified_db, DOI, *item, map_descriptors))
            ++*written_count;
        else
            ++*suppressed_count;
    }
}


void ProcessJournal(const unsigned timeout, const std::string &line, MarcWriter * const marc_writer,
                    kyotocabinet::HashDB * const notified_db, const std::vector<MapDescriptor *> &map_descriptors,
                    unsigned * const total_written_count, unsigned * const total_suppressed_count)
{
    std::vector<std::string> issns;
    std::string journal_name;
    if (unlikely(not GetISSNsAndJournalName(line, &issns, &journal_name)))
        Error("bad input line \"" + line + "\"!");
    std::cout << "Processing " << journal_name << '\n';

    std::unordered_set<std::string> already_seen;
    for (const auto &issn : issns) {
        unsigned written_count, suppressed_count;
        ProcessISSN(issn, timeout, marc_writer, notified_db, map_descriptors, &already_seen, &written_count,
                    &suppressed_count);
        *total_written_count    += written_count;
        *total_suppressed_count += suppressed_count;
    }
}


std::unique_ptr<kyotocabinet::HashDB> CreateOrOpenKeyValueDB() {
    const std::string DB_FILENAME("/var/lib/tuelib/crossref_downloader/notified.db");
    std::unique_ptr<kyotocabinet::HashDB> db(new kyotocabinet::HashDB());
    if (not (db->open(DB_FILENAME,
                      kyotocabinet::HashDB::OWRITER | kyotocabinet::HashDB::OREADER | kyotocabinet::HashDB::OCREATE)))
        Error("failed to open or create \"" + DB_FILENAME + "\"!");
    return db;
}


int main(int argc, char *argv[]) {
    ::progname = argv[0];

    if (argc != 3 and argc != 5)
        Usage();

    const unsigned DEFAULT_TIMEOUT(20); // seconds
    unsigned timeout(DEFAULT_TIMEOUT);
    if (std::strcmp(argv[1], "--timeout") == 0) {
        if (not StringUtil::ToUnsigned(argv[2], &timeout))
            Error("bad timeout \"" + std::string(argv[2]) + "\"!");
        argc -= 2;
        argv += 2;
    }

    if (argc != 3)
        Usage();

    const std::string journal_list_filename(argv[1]);
    const std::string marc_output_filename(argv[2]);

    try {
        std::unique_ptr<kyotocabinet::HashDB> notified_db(CreateOrOpenKeyValueDB());

        const std::unique_ptr<File> journal_list_file(FileUtil::OpenInputFileOrDie(journal_list_filename));
        const std::unique_ptr<MarcWriter> marc_writer(MarcWriter::Factory(marc_output_filename));

        std::vector<MapDescriptor *> map_descriptors;
        InitCrossrefToMarcMapping(&map_descriptors);

        unsigned success_count(0), total_written_count(0), total_success_count(0);
        while (not journal_list_file->eof()) {
            std::string line;
            journal_list_file->getline(&line);
            StringUtil::Trim(&line);
            if (not line.empty()) {
                const unsigned old_total_written_count(total_written_count);
                ProcessJournal(timeout, line, marc_writer.get(), notified_db.get(), map_descriptors,
                               &total_written_count , &total_success_count);
                if (old_total_written_count < total_written_count)
                    ++success_count;
            }
        }

        std::cout << "Downloaded metadata for at least one article from " << success_count << " journals.\n";
        std::cout << "The total number of articles for which metadata was downloaded and written out is "
                  << total_written_count
                  << ".\nAnd the number of articles that were identical to previous downloads and therefore "
                  << "suppressed is " << total_success_count << ".\n";
        return success_count == 0 ? EXIT_FAILURE : EXIT_SUCCESS;
    } catch (const std::exception &e) {
        Error("Caught exception: " + std::string(e.what()));
    }
}

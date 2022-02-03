/** \file    crossref_downloader.cc
 *  \brief   Downloads metadata from crossref.org and generates MARC-21 records.
 *  \author  Dr. Johannes Ruscheinski
 *
 *  \copyright (C) 2017-2019, Library of the University of Tübingen
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
#include "Compiler.h"
#include "Downloader.h"
#include "FileUtil.h"
#include "HttpHeader.h"
#include "JSON.h"
#include "KeyValueDB.h"
#include "MARC.h"
#include "MiscUtil.h"
#include "StringUtil.h"
#include "UBTools.h"
#include "util.h"


namespace {


[[noreturn]] void Usage() {
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
    std::string toString() const __attribute__((unused));
};


// Parses a JSON subtree that, should it exist looks like [[YYYY, MM, DD]] where the day as well as the
// month may be missing.
CrossrefDate::CrossrefDate(const JSON::ObjectNode &object, const std::string &field) {
    const std::shared_ptr<const JSON::ObjectNode> subtree(object.getOptionalObjectNode(field));
    if (subtree == nullptr) {
        year_ = month_ = day_ = 0;
        return;
    }

    const std::shared_ptr<const JSON::ArrayNode> array_node(subtree->getArrayNode("date-parts"));
    const std::shared_ptr<const JSON::ArrayNode> array_node2(array_node->getArrayNode(0));
    auto date_component_iter(array_node2->begin());
    const auto &date_end(array_node2->end());
    if (unlikely(date_component_iter == date_end))
        LOG_ERROR("year is missing for the \"" + field + "\" date field!");

    std::shared_ptr<const JSON::IntegerNode> year_node(JSON::JSONNode::CastToIntegerNodeOrDie("date-parts[0]", *date_component_iter));
    if (year_node == nullptr or year_node->getValue() < 0) {
        LOG_WARNING("cannot convert year component \"" + (*date_component_iter)->toString() + "\" to an unsigned integer!");
        return;
    }

    year_ = static_cast<unsigned>(year_node->getValue());
    if (unlikely(year_ < 1000 or year_ > 3000))
        LOG_ERROR("year component \"" + std::to_string(year_) + "\" is unlikely to be a year!");

    ++date_component_iter;
    if (date_component_iter == date_end) {
        month_ = day_ = 0;
        return;
    }

    std::shared_ptr<const JSON::IntegerNode> month_node(JSON::JSONNode::CastToIntegerNodeOrDie("date-parts[1]", *date_component_iter));
    if (month_node == nullptr or month_node->getValue() < 0)
        LOG_ERROR("cannot convert month component \"" + (*date_component_iter)->toString() + "\" to an unsigned integer!");
    month_ = static_cast<unsigned>(month_node->getValue());
    if (unlikely(month_ < 1 or month_ > 12))
        LOG_ERROR("month component \"" + std::to_string(month_) + "\" is not a month!");

    ++date_component_iter;
    if (date_component_iter == date_end) {
        day_ = 0;
        return;
    }

    std::shared_ptr<const JSON::IntegerNode> day_node(JSON::JSONNode::CastToIntegerNodeOrDie("date-parts[2]", *date_component_iter));
    if (day_node == nullptr or day_node->getValue() < 0)
        LOG_ERROR("cannot convert day component \"" + (*date_component_iter)->toString() + "\" to an unsigned integer!");
    day_ = static_cast<unsigned>(day_node->getValue());
    if (unlikely(day_ < 1 or day_ > 31))
        LOG_ERROR("day component \"" + std::to_string(day_) + "\" is not a day!");
}


std::string CrossrefDate::toString() const {
    if (not isValid())
        LOG_ERROR("can't convert an invalid CrossrefDate to a string!");

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
        LOG_ERROR(std::to_string(month_) + " is not a valid month!");
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
    enum FieldType { STRING, STRING_VECTOR, YEAR };

protected:
    std::string json_field_;
    FieldType field_type_;
    std::string marc_subfield_;
    bool repeatable_;

public:
    MapDescriptor(const std::string &json_field, const FieldType field_type, const std::string &marc_subfield,
                  const bool repeatable = false)
        : json_field_(json_field), field_type_(field_type), marc_subfield_(marc_subfield), repeatable_(repeatable) { }
    virtual ~MapDescriptor() { }

    inline const std::string &getJsonField() const { return json_field_; }
    inline FieldType getFieldType() const { return field_type_; }
    inline const std::string &getMarcSubfield() const { return marc_subfield_; }
    inline bool isRepeatable() const { return repeatable_; }
    virtual void insertMarcData(const std::string &subfield_value, MARC::Record * const record);
};


void MapDescriptor::insertMarcData(const std::string &subfield_value, MARC::Record * const record) {
    const std::string tag(marc_subfield_.substr(0, MARC::Record::TAG_LENGTH));
    const char subfield_code(marc_subfield_.back());
    record->insertField(tag, { { subfield_code, subfield_value } });
}


class DOIMapDescriptor : public MapDescriptor {
public:
    DOIMapDescriptor(): MapDescriptor("DOI", MapDescriptor::STRING, "024a") { }
    virtual void insertMarcData(const std::string &subfield_value, MARC::Record * const record) override final;
};


inline std::string CreateSubfield(const char subfield_code, const std::string &subfield_data) {
    return "\x1F" + std::string(1, subfield_code) + subfield_data;
}


void DOIMapDescriptor::insertMarcData(const std::string &subfield_value, MARC::Record * const record) {
    const std::string tag(marc_subfield_.substr(0, MARC::Record::TAG_LENGTH));
    const char subfield_code(marc_subfield_.back());
    record->insertField(tag, "7 " + CreateSubfield(subfield_code, subfield_value) + CreateSubfield('2', "doi"));
}


void InitCrossrefToMarcMapping(std::vector<MapDescriptor *> * const map_descriptors) {
    map_descriptors->emplace_back(new MapDescriptor("URL", MapDescriptor::STRING, "856u"));
    map_descriptors->emplace_back(new MapDescriptor("subject", MapDescriptor::STRING_VECTOR, "653a"));
    map_descriptors->emplace_back(new MapDescriptor("publisher", MapDescriptor::STRING, "260b"));
    map_descriptors->emplace_back(new DOIMapDescriptor());
}


std::vector<std::string> ExtractString(const JSON::ObjectNode &object_node, const std::string &json_field_name) {
    std::vector<std::string> extracted_values;
    const std::shared_ptr<const JSON::StringNode> node(object_node.getOptionalStringNode(json_field_name));
    if (node == nullptr)
        return extracted_values;

    extracted_values.emplace_back(node->getValue());
    return extracted_values;
}


std::vector<std::string> ExtractStringVector(const JSON::ObjectNode &object_node, const std::string &json_field_name,
                                             const bool is_repeatable) {
    std::vector<std::string> extracted_values;

    std::shared_ptr<const JSON::ArrayNode> array_node(object_node.getOptionalArrayNode(json_field_name));
    if (array_node == nullptr)
        return extracted_values;

    for (auto &array_entry : *array_node) {
        std::shared_ptr<const JSON::StringNode> string_node(JSON::JSONNode::CastToStringNodeOrDie("ExtractStringVector", array_entry));
        extracted_values.emplace_back(string_node->getValue());
        if (not is_repeatable)
            break;
    }

    return extracted_values;
}


std::string ExtractName(const std::shared_ptr<const JSON::ObjectNode> object_node) {
    const std::shared_ptr<const JSON::StringNode> given(object_node->getOptionalStringNode("given"));
    const std::shared_ptr<const JSON::StringNode> family(object_node->getOptionalStringNode("family"));
    std::string name;

    if (given != nullptr)
        name = given->getValue();

    if (family != nullptr) {
        if (not name.empty())
            name += ' ';
        name += family->getValue();
    }

    return name;
}


void AddAuthors(const std::string &DOI, const std::string &ISSN, const JSON::ObjectNode &message_tree, MARC::Record * const marc_record) {
    std::shared_ptr<const JSON::ArrayNode> authors(message_tree.getOptionalArrayNode("author"));
    if (authors == nullptr) {
        LOG_WARNING("no author node found, DOI was \"" + DOI + "\", ISSN was \"" + ISSN + "\"!");
        return;
    }

    bool first(true);
    for (auto author : *authors) {
        const std::shared_ptr<const JSON::ObjectNode> author_node(JSON::JSONNode::CastToObjectNodeOrDie("author", author));
        const std::string author_name(ExtractName(author_node));
        if (unlikely(author_name.empty()))
            continue;

        if (first) {
            first = false;
            marc_record->insertField("100", "  " + CreateSubfield('a', author_name));
        } else
            marc_record->insertField("700", "0 " + CreateSubfield('0', "aut") + CreateSubfield('a', author_name));
    }
}


void AddEditors(const JSON::ObjectNode &message_tree, MARC::Record * const marc_record) {
    std::shared_ptr<const JSON::ArrayNode> editors(message_tree.getOptionalArrayNode("editor"));
    if (editors == nullptr)
        return;

    for (auto editor : *editors) {
        const std::shared_ptr<const JSON::ObjectNode> editor_node(JSON::JSONNode::CastToObjectNodeOrDie("editor", editor));
        const std::string editor_name(ExtractName(editor_node));
        if (unlikely(editor_name.empty()))
            continue;

        marc_record->insertField("700", "0 " + CreateSubfield('0', "edt") + CreateSubfield('a', editor_name));
    }
}


void AddIssueInfo(const JSON::ObjectNode &message_tree, MARC::Record * const marc_record) {
    std::string field_data;
    const CrossrefDate issued_date(message_tree, "issued");
#ifndef __clang__
#pragma GCC diagnostic ignored "-Wmaybe-uninitialized"
#endif
    if (issued_date.isValid()) {
#ifndef __clang__
#pragma GCC diagnostic ignored "+Wmaybe-uninitialized"
#endif
        if (issued_date.getDay() != 0)
            field_data += CreateSubfield('b', std::to_string(issued_date.getDay()));
        if (issued_date.getMonth() != 0)
            field_data += CreateSubfield('c', std::to_string(issued_date.getMonth()));
    }

    const std::string optional_volume(message_tree.getOptionalStringValue("volume"));
    if (not optional_volume.empty())
        field_data += CreateSubfield('d', optional_volume);

    const std::string optional_issue(message_tree.getOptionalStringValue("issue"));
    if (not optional_issue.empty())
        field_data += CreateSubfield('e', optional_issue);

    const std::string optional_page(message_tree.getOptionalStringValue("page"));
    if (not optional_page.empty())
        field_data += CreateSubfield('h', optional_page);

    field_data += CreateSubfield('j', std::to_string(issued_date.getYear()));
    marc_record->insertField("936", "uw" + field_data);
}


// First tries to extract data from an optional "issn-type" JSON list, if that doesn't exists tries its luck with an
// optional "ISSN" list.
// Specifically, if an "issn-type" JSON list exists with one or more nodes of "type" "electronic" we always use the
// first ISSN associated with such a node.  If no nodes in an "issn-type" JSON list exists we look for nodes in a
// list called "ISSN" and take the first ISSN from such a list, should it exist.  If neither of these two lists exist
// or contain ISSNs we will not set any ISSN in "marc_record".
void AddISSN(const JSON::ObjectNode &message_tree, MARC::Record * const marc_record) {
    const std::shared_ptr<const JSON::ArrayNode> issn_types(message_tree.getOptionalArrayNode("issn-type"));
    if (issn_types != nullptr) {
        std::string issn;
        for (auto issn_type : *issn_types) {
            const std::shared_ptr<const JSON::ObjectNode> issn_type_node(JSON::JSONNode::CastToObjectNodeOrDie("issn-type[n]", issn_type));
            if (unlikely(issn_type_node == nullptr)) {
                LOG_WARNING("strange, issn-type entry is not a JSON object!");
                continue;
            }

            const std::shared_ptr<const JSON::StringNode> value_node(issn_type_node->getOptionalStringNode("value"));
            const std::shared_ptr<const JSON::StringNode> type_node(issn_type_node->getOptionalStringNode("type"));
            if (unlikely(value_node == nullptr or type_node == nullptr)) {
                LOG_WARNING("strange, issn-type entry is missing a \"value\" or \"type\" string subnode!");
                continue;
            }

            issn = value_node->getValue();
            if (type_node->getValue() == "electronic") {
                marc_record->insertField("022", { { 'a', issn } });
                return;
            }
        }

        if (not issn.empty()) {
            marc_record->insertField("022", { { 'a', issn } });
            return;
        }
    }

    const std::shared_ptr<const JSON::ArrayNode> issns(message_tree.getOptionalArrayNode("ISSN"));
    if (issns == nullptr)
        return;
    if (unlikely(issns->empty())) {
        LOG_WARNING("bizarre, ISSN list is empty!");
        return;
    }
    const std::shared_ptr<const JSON::StringNode> first_issn(issns->getOptionalStringNode(0));
    if (likely(first_issn != nullptr))
        marc_record->insertField("022", { { 'a', first_issn->getValue() } });
    else
        LOG_WARNING("first entry of ISSN list is not a string node!");
}


bool AddTitle(const JSON::ObjectNode &message_tree, MARC::Record * const marc_record) {
    const std::shared_ptr<const JSON::ArrayNode> titles(message_tree.getOptionalArrayNode("title"));
    if (unlikely(titles == nullptr or titles->empty()))
        return false;
    const std::shared_ptr<const JSON::StringNode> first_title(titles->getOptionalStringNode(0));
    if (unlikely(first_title == nullptr))
        return false;
    marc_record->insertField("245", { { 'a', first_title->getValue() } });

    const std::shared_ptr<const JSON::ArrayNode> subtitles(message_tree.getOptionalArrayNode("subtitle"));
    if (subtitles != nullptr and not subtitles->empty()) {
        const std::shared_ptr<const JSON::StringNode> first_subtitle_node(subtitles->getOptionalStringNode(0));
        if (likely(first_subtitle_node != nullptr))
            marc_record->addSubfield("245", 'b', first_subtitle_node->getValue());
    }

    return true;
}


/** \return True, if we wrote a record and false if we suppressed a duplicate. */
bool CreateAndWriteMarcRecord(MARC::Writer * const marc_writer, KeyValueDB * const notified_db, const std::string &DOI,
                              const std::string &ISSN, const JSON::ObjectNode &message_tree,
                              const std::vector<MapDescriptor *> &map_descriptors) {
    MARC::Record record(MARC::Record::TypeOfRecord::LANGUAGE_MATERIAL, MARC::Record::BibliographicLevel::SERIAL_COMPONENT_PART);
    static unsigned control_number(0);
    record.insertField("001", std::to_string(++control_number));

    AddISSN(message_tree, &record);
    if (unlikely(not AddTitle(message_tree, &record))) {
        LOG_WARNING("no title found for DOI \"" + DOI + "\" and ISSN \"" + ISSN + "\".  Record skipped!");
        return false;
    }
    AddAuthors(DOI, ISSN, message_tree, &record);
    AddEditors(message_tree, &record);
    for (const auto &map_descriptor : map_descriptors) {
        std::vector<std::string> field_values;
        switch (map_descriptor->getFieldType()) {
        case MapDescriptor::STRING:
            field_values = ExtractString(message_tree, map_descriptor->getJsonField());
            break;
        case MapDescriptor::STRING_VECTOR:
            field_values = ExtractStringVector(message_tree, map_descriptor->getJsonField(), map_descriptor->isRepeatable());
            break;
        default:
            LOG_ERROR("unexpected field type!");
        }

        for (const auto &field_value : field_values)
            map_descriptor->insertMarcData(field_value, &record);
    }
    AddIssueInfo(message_tree, &record);

    // If we have already encountered the exact same record in the past we skip writing it:
    std::string new_hash(MARC::CalcChecksum(record));
    if (notified_db->keyIsPresent(DOI)) {
        if (notified_db->getValue(DOI) == new_hash)
            return false;
    }
    notified_db->addOrReplace(DOI, new_hash);

    marc_writer->write(record);
    return true;
}


// Expects "line" to look like "XXXX-XXXX,YYYY-YYYY,...ZZZZ-ZZZZ JJJ" where "XXXX-XXXX", "YYYY-YYYY" and "ZZZZ-ZZZZ"
// are ISSN's and "JJJ" a journal title.
bool GetISSNsAndJournalName(const std::string &line, std::vector<std::string> * const issns, std::string * const journal_name) {
    const size_t first_space_pos(line.find(' '));
    if (unlikely(first_space_pos == std::string::npos or first_space_pos == 0)) {
        LOG_WARNING("No space found!");
        return false;
    }

    if (StringUtil::Split(line.substr(0, first_space_pos), ',', issns, /* suppress_empty_components = */ true) == 0) {
        LOG_WARNING("No ISSNS found!");
        return false;
    }

    for (const auto &issn : *issns) {
        if (unlikely(not MiscUtil::IsPossibleISSN(issn))) {
            LOG_WARNING(issn + " is not a valid ISSN!");
            return false;
        }
    }

    *journal_name = StringUtil::TrimWhite(line.substr(first_space_pos + 1));
    return not journal_name->empty();
}


void ProcessISSN(const std::string &ISSN, const unsigned timeout, MARC::Writer * const marc_writer, KeyValueDB * const notified_db,
                 const std::vector<MapDescriptor *> &map_descriptors, std::unordered_set<std::string> * const already_seen,
                 unsigned * const written_count, unsigned * const suppressed_count) {
    *written_count = *suppressed_count = 0;

    const std::string DOWNLOAD_URL("https://api.crossref.org/v1/journals/" + ISSN + "/works");
    Downloader downloader(DOWNLOAD_URL, Downloader::Params(), timeout * 1000);
    if (downloader.anErrorOccurred()) {
        LOG_WARNING("Error while downloading metadata for ISSN " + ISSN + ": " + downloader.getLastErrorMessage());
        return;
    }

    // Check for rate limiting and error status codes:
    const HttpHeader http_header(downloader.getMessageHeader());
    if (http_header.getStatusCode() == 429)
        LOG_ERROR("we got rate limited!");
    else if (http_header.getStatusCode() != 200) {
        LOG_WARNING("Crossref returned HTTP status code " + std::to_string(http_header.getStatusCode()) + "!");
        return;
    }

    const std::string &json_document(downloader.getMessageBody());
    std::shared_ptr<JSON::JSONNode> full_tree;
    JSON::Parser parser(json_document);
    if (not parser.parse(&full_tree))
        LOG_ERROR("failed to parse JSON (" + parser.getErrorMessage() + "), download URL was: " + DOWNLOAD_URL);

    const std::shared_ptr<const JSON::ObjectNode> top_node(JSON::JSONNode::CastToObjectNodeOrDie("full_tree", full_tree));
    if (unlikely(top_node == nullptr))
        LOG_ERROR("JSON returned from Crossref is not an object! (URL was " + DOWNLOAD_URL + ")");

    const std::shared_ptr<const JSON::ObjectNode> message_node(top_node->getOptionalObjectNode("message"));
    if (unlikely(message_node == nullptr))
        return;

    const std::shared_ptr<const JSON::ArrayNode> items(message_node->getOptionalArrayNode("items"));
    if (unlikely(items == nullptr))
        return;

    for (auto item_iter : *items) {
        const std::shared_ptr<const JSON::ObjectNode> item(JSON::JSONNode::CastToObjectNodeOrDie("items", item_iter));
        const std::string DOI(JSON::LookupString("/DOI", item, /* default_value = */ ""));
        if (unlikely(DOI.empty()))
            LOG_ERROR("No \"DOI\" for an item returned for the ISSN " + ISSN + "!");

        // Have we already seen this item?
        if (already_seen->find(DOI) != already_seen->cend()) {
            ++*suppressed_count;
            continue;
        }
        already_seen->insert(DOI);

        if (CreateAndWriteMarcRecord(marc_writer, notified_db, DOI, ISSN, *item, map_descriptors))
            ++*written_count;
        else
            ++*suppressed_count;
    }
}


void ProcessJournal(const unsigned timeout, const std::string &line, MARC::Writer * const marc_writer, KeyValueDB * const notified_db,
                    const std::vector<MapDescriptor *> &map_descriptors, unsigned * const total_written_count,
                    unsigned * const total_suppressed_count) {
    std::vector<std::string> issns;
    std::string journal_name;
    if (unlikely(not GetISSNsAndJournalName(line, &issns, &journal_name)))
        LOG_ERROR("bad input line \"" + line + "\"!");
    std::cout << "Processing " << journal_name << '\n';

    std::unordered_set<std::string> already_seen;
    for (const auto &issn : issns) {
        unsigned written_count, suppressed_count;
        ProcessISSN(issn, timeout, marc_writer, notified_db, map_descriptors, &already_seen, &written_count, &suppressed_count);
        *total_written_count += written_count;
        *total_suppressed_count += suppressed_count;
    }
}


std::unique_ptr<KeyValueDB> CreateOrOpenKeyValueDB() {
    const std::string DB_FILENAME(UBTools::GetTuelibPath() + "crossref_downloader/notified.db");
    if (not FileUtil::Exists(DB_FILENAME))
        KeyValueDB::Create(DB_FILENAME);

    return std::unique_ptr<KeyValueDB>(new KeyValueDB(DB_FILENAME));
}


} // unnamed namespace


int Main(int argc, char *argv[]) {
    if (argc != 3 and argc != 5)
        Usage();

    const unsigned DEFAULT_TIMEOUT(20); // seconds
    unsigned timeout(DEFAULT_TIMEOUT);
    if (std::strcmp(argv[1], "--timeout") == 0) {
        if (not StringUtil::ToUnsigned(argv[2], &timeout))
            LOG_ERROR("bad timeout \"" + std::string(argv[2]) + "\"!");
        argc -= 2;
        argv += 2;
    }

    if (argc != 3)
        Usage();

    const std::string journal_list_filename(argv[1]);
    const std::string marc_output_filename(argv[2]);

    std::unique_ptr<KeyValueDB> notified_db(CreateOrOpenKeyValueDB());

    const auto journal_list_file(FileUtil::OpenInputFileOrDie(journal_list_filename));
    const auto marc_writer(MARC::Writer::Factory(marc_output_filename));

    std::vector<MapDescriptor *> map_descriptors;
    InitCrossrefToMarcMapping(&map_descriptors);

    unsigned journal_success_count(0), total_written_count(0), total_suppressed_count(0);
    while (not journal_list_file->eof()) {
        std::string line;
        journal_list_file->getline(&line);
        StringUtil::Trim(&line);
        if (not line.empty()) {
            const unsigned old_total_written_count(total_written_count);
            ProcessJournal(timeout, line, marc_writer.get(), notified_db.get(), map_descriptors, &total_written_count,
                           &total_suppressed_count);
            if (old_total_written_count < total_written_count)
                ++journal_success_count;
        }
    }

    std::cout << "Downloaded metadata for at least one article from " << journal_success_count << " journals.\n";
    std::cout << "The total number of articles for which metadata was downloaded and written out is " << total_written_count
              << ".\nAnd the number of articles that were identical to previous downloads and therefore "
              << "suppressed is " << total_suppressed_count << ".\n";

    return EXIT_SUCCESS;
}

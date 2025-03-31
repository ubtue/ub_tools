/** \brief Convert DPT Book chapters to MARC 21 Records
 *  \author Johannes Riedl
 *
 *  \copyright 2025 Universitätsbibliothek Tübingen.  All rights reserved.
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

#include <fstream>
#include <functional>
#include <iomanip>
#include <iostream>
#include <map>
#include <sstream>
#include <nlohmann/json.hpp>
#include "DbConnection.h"
#include "FileUtil.h"
#include "HtmlUtil.h"
#include "IniFile.h"
#include "MARC.h"
#include "StringUtil.h"
#include "TimeUtil.h"
#include "UBTools.h"
#include "util.h"


namespace {

const char SEPARATOR_CHAR('|');

struct GNDAndName {
    std::string gnd_;
    std::string name_;
    enum class ENTITY_TYPE { PERSON, CORPORATE, CONGRESS };
    ENTITY_TYPE type_;
};


struct PPNAndISBNType {
    enum class ISBN_TYPE { PRINT_ISBN, ONLINE_ISBN };
    std::string ppn_;
    ISBN_TYPE type_;
};


struct PPNAndType {
    enum class PPN_TYPE { PRINT, ONLINE };
    std::string ppn_;
    PPN_TYPE type_;
};


struct BookInformation {
    std::string book_id_;
    std::string title_;
    std::string subtitle_;
    std::string language_;
    std::string description_;
    std::string print_isbn_;
    std::string online_isbn_;
    std::string total_pages_;
    std::string size_information_;
    std::string publisher_;
    inline std::string toString() const {
        std::string as_string;
        as_string += "book_id: " + book_id_ + '\n';
        as_string += "title: " + title_ + '\n';
        as_string += "subtitle: " + subtitle_ + '\n';
        as_string += "language: " + language_ + '\n';
        as_string += "description: " + description_ + '\n';
        as_string += "print_isbn: " + print_isbn_ + '\n';
        as_string += "online_isbn: " + online_isbn_ + '\n';
        as_string += "total_pages: " + total_pages_ + '\n';
        as_string += "size_information: " + size_information_ + '\n';
        as_string += "publisher: " + publisher_ + '\n';

        return as_string;
    }
    std::ostream &operator<<(std::ostream &os) const { return os << toString(); };
    friend std::ostream &operator<<(std::ostream &output, const BookInformation &book_information) {
        return output << book_information.toString();
    }
};


using DPTIDToGNDAndNameMap = std::unordered_map<std::string, GNDAndName>;
using DPTBookIdsToPPNsMap = std::unordered_multimap<std::string, PPNAndISBNType>;
using SuperiorPPNToYearMap = std::unordered_map<std::string, std::string>;


[[noreturn]] void Usage() {
    ::Usage(
        "--extract-only-book-information dpt_books.json |\n"
        "[--skip-derive-years] dpt_books.json author_dpt_id_gnd_mapping.txt book_ppn_mapping.txt superior_ppn_to_year_mapping.txt "
        "marc_output]");
}


GNDAndName::ENTITY_TYPE GetEntityType(const std::string &entity) {
    if (StringUtil::ASCIIToLower(entity) == "corporate")
        return GNDAndName::ENTITY_TYPE::CORPORATE;
    if (StringUtil::ASCIIToLower(entity) == "congress")
        return GNDAndName::ENTITY_TYPE::CONGRESS;
    LOG_ERROR("Unknown entity type: \"" + entity + "\"");
}


void CreateIDToGNDAndNameMap(File * const mapping_file, DPTIDToGNDAndNameMap * const dpt_to_gnds_and_names) {
    while (not mapping_file->eof()) {
        std::string line;
        mapping_file->getline(&line);
        StringUtil::Trim(&line);
        std::vector<std::string> mapping;
        StringUtil::SplitThenTrim(line, SEPARATOR_CHAR, " \t", &mapping);
        if (unlikely(mapping.size() != 3 && mapping.size() != 4)) {
            LOG_WARNING("Invalid line \"" + line + "\"");
            continue;
        }

        GNDAndName::ENTITY_TYPE type((mapping.size() == 4) ? GetEntityType(mapping[3]) : GNDAndName::ENTITY_TYPE::PERSON);
        dpt_to_gnds_and_names->emplace(std::make_pair(mapping[0], GNDAndName({ mapping[1], mapping[2], type })));
    }
}


MARC::Record *CreateNewRecord(const std::string &dpt_id) {
    std::ostringstream formatted_number;
    formatted_number << std::setfill('0') << std::setw(8) << std::atoi(dpt_id.c_str());
    const std::string prefix("DPT");
    const std::string ppn(prefix + formatted_number.str());

    MARC::Record *new_record(
        new MARC::Record(MARC::Record::TypeOfRecord::LANGUAGE_MATERIAL, MARC::Record::BibliographicLevel::MONOGRAPHIC_COMPONENT_PART, ppn));
    new_record->insertField("003", "DE-2619");
    new_record->insertField("007", "cr|||||");
    new_record->insertField("912", 'a', "NOMM");
    return new_record;
}


void InsertTitle(MARC::Record * const marc_record, const std::string &title) {
    if (title.empty())
        return;
    std::string title_clean(HtmlUtil::ReplaceEntitiesUTF8(title));
    marc_record->insertField("245", 'a', title_clean, '1', '0');
}


std::string GetAuthorTag(const bool is_first_author, const auto &gnd_and_name) {
    if (gnd_and_name->second.type_ == GNDAndName::ENTITY_TYPE::CORPORATE)
        return is_first_author ? "110" : "710";
    if (gnd_and_name->second.type_ == GNDAndName::ENTITY_TYPE::CONGRESS)
        return is_first_author ? "111" : "711";
    return is_first_author ? "100" : "700";
}


void InsertAuthors(MARC::Record * const marc_record, const auto &authors, const DPTIDToGNDAndNameMap &dpt_to_gnds_and_names) {
    bool is_first_author(true);
    for (const auto &author : authors) {
        const std::string author_id(author["ID"]);
        const auto gnd_and_name(dpt_to_gnds_and_names.find(author_id));
        if (gnd_and_name == dpt_to_gnds_and_names.end()) {
            LOG_WARNING("Unable to associate author with ID " + author_id);
            continue;
        }
        std::string author_tag(GetAuthorTag(is_first_author, gnd_and_name));
        const std::string &name(gnd_and_name->second.name_);
        const std::string &gnd(gnd_and_name->second.gnd_);

        if (gnd.empty()) {
            LOG_WARNING("No gnd given for Author ID " + author_id);
            continue;
        }

        if (name.empty()) {
            LOG_WARNING("No name given for Author ID " + author_id);
        }

        marc_record->insertField(author_tag, { { 'a', name }, { 'e', "VerfasserIn" }, { '0', "(DE-588)" + gnd }, { '4', "aut" } }, '1',
                                 ' ');

        is_first_author = is_first_author ? false : is_first_author;
    }
}


PPNAndType GetSuperiorPPN(const BookInformation &book_information, const DPTBookIdsToPPNsMap &dpt_book_ids_to_ppns) {
    PPNAndType superior_ppn_and_type;
    const auto begin_end(dpt_book_ids_to_ppns.equal_range(book_information.book_id_));
    if (begin_end.first != begin_end.second) {
        for (auto pair(begin_end.first); pair != begin_end.second; ++pair) {
            if (pair->second.type_ == PPNAndISBNType::ISBN_TYPE::ONLINE_ISBN)
                return PPNAndType({ pair->second.ppn_, PPNAndType::PPN_TYPE::ONLINE });
            superior_ppn_and_type =
                superior_ppn_and_type.ppn_.empty() ? PPNAndType({ pair->second.ppn_, PPNAndType::PPN_TYPE::PRINT }) : superior_ppn_and_type;
        }
    }
    return superior_ppn_and_type;
}


void InsertSuperiorWorkInformation(MARC::Record * const marc_record, const BookInformation &book_information,
                                   const DPTBookIdsToPPNsMap &dpt_book_ids_to_ppns) {
    PPNAndType superior_ppn_and_type(GetSuperiorPPN(book_information, dpt_book_ids_to_ppns));
    marc_record->insertField("773",
                             { { 'i', "Enthalten in" },
                               { 't', book_information.title_ },
                               { 'd', book_information.publisher_ },
                               { 'h', book_information.total_pages_ },
                               { 'w', "(DE-627)" + superior_ppn_and_type.ppn_ } },
                             '0', '8');
}


void InsertSelectors(MARC::Record * const marc_record) {
    marc_record->insertField("084", { { 'a', "2,1" }, { '2', "ssgn" } });
    marc_record->insertField("935", { { 'a', "mkri" } });
    marc_record->insertField("935", { { 'a', "kdpt" }, { '2', "LOK" } });
}


const std::string DPT_ARTICLE_BASE_URL("https://www.praeventionstag.de/nano.cms/vortraege/id/");

void InsertLinks(MARC::Record * const marc_record, const std::string dpt_id) {
    marc_record->insertField("856", 'u', DPT_ARTICLE_BASE_URL + dpt_id, '4', '0');
}

void InsertLanguage(MARC::Record * const marc_record, const std::string dpt_language) {
    std::string lang;
    if (dpt_language == "Deutsch")
        lang = "ger";
    else if (dpt_language == "Englisch")
        lang = "eng";
    else
        lang = "mis";
    marc_record->insertField("041", { { 'a', lang } });
}


void InsertYear(MARC::Record * const marc_record, const SuperiorPPNToYearMap &superior_ppn_to_year) {
    const std::string superior_ppn(marc_record->getSuperiorControlNumber());
    if (superior_ppn_to_year.find(superior_ppn) != superior_ppn_to_year.end()) {
        const std::string year(superior_ppn_to_year.at(superior_ppn));
        marc_record->insertField("264", { { 'c', year } }, ' ', '1');
        marc_record->addSubfield("773", 'g', "(" + year + ")");
        marc_record->insertField("936", 'j', year, 'u', 'w');
    }
}


void ExtractBookInformation(auto &book, BookInformation * const book_information) {
    book_information->book_id_ = StringUtil::TrimWhite(std::to_string(book.value("ID", 0)));
    book_information->title_ = HtmlUtil::ReplaceEntitiesUTF8(StringUtil::TrimWhite(book.value("Titel", "")));
    book_information->subtitle_ = HtmlUtil::ReplaceEntitiesUTF8(StringUtil::TrimWhite(book.value("Untertitel", "")));
    book_information->language_ = StringUtil::TrimWhite(book.value("Sprache", ""));
    book_information->description_ =
        HtmlUtil::ReplaceEntitiesUTF8(HtmlUtil::StripHtmlTags(StringUtil::TrimWhite(book.value("Beschreibung", ""))));
    book_information->print_isbn_ = StringUtil::TrimWhite(book.value("ISBN-Print", ""));
    book_information->online_isbn_ = StringUtil::TrimWhite(book.value("ISBN-eBook", ""));
    book_information->total_pages_ = StringUtil::TrimWhite(std::to_string(book.value("Seiten", 0)));
    book_information->size_information_ = StringUtil::TrimWhite(book.value("Groesse", ""));
    book_information->publisher_ = StringUtil::TrimWhite(book.value("Verlag", ""));
}


void CreateBookIDToPPNMap(File * const mapping_file, DPTBookIdsToPPNsMap * const dpt_book_ids_to_ppns) {
    bool in_print_part = true;
    while (not mapping_file->eof()) {
        std::string line;
        mapping_file->getline(&line);
        StringUtil::Trim(&line);
        // Split one separator
        static ThreadSafeRegexMatcher part_separator("^---+$");
        if (part_separator.match(line)) {
            in_print_part = false;
            continue;
        }

        std::vector<std::string> mapping;
        StringUtil::SplitThenTrim(line, ',', " \t", &mapping);
        if (unlikely(mapping.size() < 3)) {
            LOG_WARNING("Invalid line \"" + line + "\"");
            continue;
        }

        std::string ppn(mapping.size() >= 4 ? mapping[3] : "");
        dpt_book_ids_to_ppns->emplace(std::make_pair(
            mapping[0],
            PPNAndISBNType({ ppn, in_print_part ? PPNAndISBNType::ISBN_TYPE::PRINT_ISBN : PPNAndISBNType::ISBN_TYPE::ONLINE_ISBN })));
    }
}


void CreateSuperiorPPNToYearMap(File * const mapping_file, SuperiorPPNToYearMap * const superior_ppn_to_year) {
    while (not mapping_file->eof()) {
        std::string line;
        mapping_file->getline(&line);
        StringUtil::Trim(&line);
        std::vector<std::string> mapping;
        StringUtil::SplitThenTrim(line, ':', " \t", &mapping);
        if (unlikely(mapping.size() != 2))
            LOG_WARNING("Invalid line \"" + line + "\"");
        superior_ppn_to_year->emplace(std::make_pair(mapping[0], mapping[1]));
    }
}


void ConvertArticles(MARC::Writer * const marc_writer, File * const dpt_books_file, const DPTIDToGNDAndNameMap &dpt_to_gnds_and_names,
                     const DPTBookIdsToPPNsMap &dpt_book_ids_to_ppns, const SuperiorPPNToYearMap &superior_ppn_to_year) {
    std::ifstream dpt_books(dpt_books_file->getPath());
    nlohmann::json books_json(nlohmann::json::parse(dpt_books));
    for (const auto &book : books_json["Bücher"]) {
        BookInformation book_information;
        ExtractBookInformation(book, &book_information);
        for (const auto &chapter : book["Kapitel"]) {
            const std::string dpt_id(chapter["ID"]);
            MARC::Record * const new_record(CreateNewRecord(dpt_id));
            InsertLanguage(new_record, book_information.language_);
            InsertAuthors(new_record, chapter["Autoren"], dpt_to_gnds_and_names);
            InsertTitle(new_record, chapter["Titel"]);
            InsertSuperiorWorkInformation(new_record, book_information, dpt_book_ids_to_ppns);
            InsertYear(new_record, superior_ppn_to_year);
            InsertLinks(new_record, dpt_id);
            InsertSelectors(new_record);
            marc_writer->write(*new_record);
            delete new_record;
        }
    }
}


void ExtractBookInformation(File * const dpt_books_file) {
    std::ifstream dpt_books(dpt_books_file->getPath());
    nlohmann::json books_json(nlohmann::json::parse(dpt_books));
    for (const auto &book : books_json["Bücher"]) {
        BookInformation book_information;
        ExtractBookInformation(book, &book_information);
        std::cout << book_information << "##############################\n\n";
    }
}

} // unnamed namespace


int Main(int argc, char *argv[]) {
    if (argc < 3)
        Usage();

    bool skip_derive_years(false);
    if (std::strcmp("--skip-derive-years", argv[1]) == 0) {
        skip_derive_years = true;
        ++argv, --argc;
    }

    bool extract_only_book_information(false);
    if (std::strcmp("--extract-only-book-information", argv[1]) == 0) {
        extract_only_book_information = true;
        ++argv, --argc;
    }

    const std::string dpt_books_file_path(argv[1]);
    std::unique_ptr<File> dpt_books_file(FileUtil::OpenInputFileOrDie(dpt_books_file_path));

    if (extract_only_book_information) {
        ExtractBookInformation(dpt_books_file.get());
        std::exit(0);
    }

    if (argc < 6)
        Usage();

    const std::string dpt_id_gnd_mapping_file_path(argv[2]);
    const std::string dpt_id_and_isbn_to_ppn_mapping_file_path(argv[3]);
    const std::string marc_output_path(argv[argc - 1]);

    std::unique_ptr<File> dpt_id_gnd_mapping_file(FileUtil::OpenInputFileOrDie(dpt_id_gnd_mapping_file_path));
    std::unique_ptr<File> dpt_id_and_isbn_to_ppn_mapping_file(FileUtil::OpenInputFileOrDie(dpt_id_and_isbn_to_ppn_mapping_file_path));

    SuperiorPPNToYearMap superior_ppn_to_year;
    if (not skip_derive_years) {
        const std::string superior_ppn_to_year_mapping_file_path(argv[4]);
        std::unique_ptr<File> superior_ppn_to_year_mapping_file(FileUtil::OpenInputFileOrDie(superior_ppn_to_year_mapping_file_path));
        CreateSuperiorPPNToYearMap(superior_ppn_to_year_mapping_file.get(), &superior_ppn_to_year);
    }

    const std::unique_ptr<MARC::Writer> marc_writer(MARC::Writer::Factory(marc_output_path));
    std::unordered_map<std::string, GNDAndName> dpt_ids_to_gnds_and_names;
    CreateIDToGNDAndNameMap(dpt_id_gnd_mapping_file.get(), &dpt_ids_to_gnds_and_names);
    DPTBookIdsToPPNsMap dpt_book_ids_to_ppns;
    CreateBookIDToPPNMap(dpt_id_and_isbn_to_ppn_mapping_file.get(), &dpt_book_ids_to_ppns);
    ConvertArticles(marc_writer.get(), dpt_books_file.get(), dpt_ids_to_gnds_and_names, dpt_book_ids_to_ppns, superior_ppn_to_year);

    return EXIT_SUCCESS;
}

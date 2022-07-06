/** \brief Convert the result of the semantic HKl extract to MARC
 *  \author Johannes Riedl
 *
 *  \copyright 2022 Universitätsbibliothek Tübingen.  All rights reserved.
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
#include <iostream>
#include <iomanip>
#include <map>
#include <sstream>
#include "FileUtil.h"
#include "IniFile.h"
#include "MARC.h"
#include "StringUtil.h"
#include "TimeUtil.h"
#include "UBTools.h"
#include "util.h"

#include <nlohmann/json.hpp>
using json = nlohmann::json;

namespace {
   [[noreturn]] void Usage() {
       ::Usage("hkl.json marc_authority_output_file");
   }

   using HKlElementType =  enum HKl_Element_Type { BIB_INFO, COMMENT, INTERNAL_REFERENCE, YEAR_AND_PLACE };

   struct HKlElement {
       HKlElementType hkl_element_type_;
       std::string hkl_element_value_;
       HKlElement(HKlElementType type, const std::string &value) : hkl_element_type_(type), hkl_element_value_(value) {}
       public:
           static HKlElementType GetHKlElementType(const std::string &type_description);
           HKlElementType GetType() const { return hkl_element_type_; }
           std::string GetTypeAsString() const;
           std::string getValue() const { return hkl_element_value_; }
   };


   HKlElementType HKlElement::GetHKlElementType(const std::string &type_description) {
       const std::map<std::string, HKlElementType> STRING_TO_HKL_ELEMENT_TYPES {
           { "bib_info", BIB_INFO },
           { "comment", COMMENT },
           { "internal_reference", INTERNAL_REFERENCE },
           { "year_and_place", YEAR_AND_PLACE }
       };
       if (STRING_TO_HKL_ELEMENT_TYPES.find(type_description) == STRING_TO_HKL_ELEMENT_TYPES.end())
           LOG_ERROR("Unknown HKlElementType: \"" + type_description + "\"");

       return STRING_TO_HKL_ELEMENT_TYPES.at(type_description);

   }

   std::string HKlElement::GetTypeAsString() const {
       const std::map<HKlElementType, std::string> HKL_ELEMENT_TYPES_TO_STRING {
            { BIB_INFO, "bib_info" },
            { COMMENT, "comment" },
            { INTERNAL_REFERENCE, "internal_reference" },
            { YEAR_AND_PLACE, "year_and_place" }
       };
       return HKL_ELEMENT_TYPES_TO_STRING.at(hkl_element_type_);
   }


   struct HKlTitleEntry {
       std::string title_;
       std::vector<HKlElement> elements_;
       public:
           HKlTitleEntry(const std::string &title, std::vector<HKlElement> elements = {})
               : title_(title), elements_(elements) {}
           void appendElement(const HKlElement &element) { elements_.push_back(element); }
           std::string getTitle() const { return title_;}
           std::vector<HKlElement> getElements() const { return elements_; }
   };


   struct HKlAuthorEntry {
       std::string author_;
       std::vector<HKlTitleEntry> title_entries_;
       public:
           explicit HKlAuthorEntry(const std::string &author, std::vector<HKlTitleEntry> title_entries = {})
                : author_(author), title_entries_(title_entries) {}
           void setAuthor(const std::string &author)  { author_ = author; }
           std::string getAuthor() const  { return author_; }
           void appendTitleEntry(const HKlTitleEntry &hkl_title_entry) { title_entries_.push_back(hkl_title_entry); }
           void setTitleEntries(const std::vector<HKlTitleEntry> &title_entries) { title_entries_ = title_entries; }
           std::vector<HKlTitleEntry> getTitleEntries() const { return title_entries_; }
   };


   void ConvertToMARC(const std::vector<HKlAuthorEntry> &hkl_author_entries) {
       for (const auto &author : hkl_author_entries) {
           std::cout << "~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\n";
           std::cout << "AUTHOR: " << author.getAuthor() << '\n';
            for (const auto &title : author.getTitleEntries()) {
                std::cout << "****************************************\n";
                std::cout << "\tTITLE1: " + title.getTitle() << '\n';
                for (const auto& element : title.getElements())
                    std::cout << "\t\t" << element.GetTypeAsString() << " YYYY " << element.getValue() << '\n';
            }
       }
   }

} // unnamed namespace

int Main(int argc, char *argv[]) {
    if (argc != 3)
        Usage();
    const std::string hkl_json_file(argv[1]);
    const std::string marc_authority_output_file(argv[2]);

    const std::unique_ptr<MARC::Writer> marc_writer(MARC::Writer::Factory(marc_authority_output_file));
    std::vector<HKlAuthorEntry> hkl_author_entries;

    std::ifstream json_input(hkl_json_file);
    if (not json_input)
        LOG_ERROR("Unable to open file \"" + hkl_json_file + "\"");
    json hkl_json;
    json_input >> hkl_json;

    for (const auto &author : hkl_json) {
        HKlAuthorEntry hkl_author_entry(author["author"]);
        for (const auto &title_and_elements : author["titles"]) {
             HKlTitleEntry hkl_title_entry(title_and_elements["title"]);
             if (title_and_elements.contains("elements")) {
                 for (const auto &element : title_and_elements["elements"]) {
                     for (const auto item : std::map<std::string, std::string>(element))
                         hkl_title_entry.appendElement(HKlElement(HKlElement::GetHKlElementType(item.first), item.second));
                 }
             }
             hkl_author_entry.appendTitleEntry(hkl_title_entry);
        }
        hkl_author_entries.push_back(hkl_author_entry);
    }

    ConvertToMARC(hkl_author_entries);

    return EXIT_SUCCESS;
}


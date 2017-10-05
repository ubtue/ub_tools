/** \file make_marc_xml.cc
 *  \brief Converts XML blobs downloaded from the BSZ into proper MARC-XML records.
 *  \author Dr. Johannes Ruscheinski (johannes.ruscheinski@uni-tuebingen.de)
 *
 *  \copyright 2016 Universitätsbiblothek Tübingen.  All rights reserved.
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
#include <unordered_map>
#include "Compiler.h"
#include "FileUtil.h"
#include "RegexMatcher.h"
#include "StringUtil.h"
#include "util.h"


void Usage() {
    std::cerr << "usage: " << progname << " [--append] input_blob output_marc_xml\n";
    std::exit(EXIT_FAILURE);
}


const std::unordered_map<std::string, std::string> from_to{
    { "<record>", "<marc:record>" },
    { "</record>", "</marc:record>" },
    { "<leader>", "<marc:leader>" },
    { "</leader>", "</marc:leader>" },
    { "<controlfield", "<marc:controlfield" },
    { "</controlfield>", "</marc:controlfield>" },
    { "<datafield", "<marc:datafield" },
    { "</datafield>", "</marc:datafield>" },
    { "<subfield", "<marc:subfield" },
    { "</subfield>", "</marc:subfield>" },
};


std::string GetNextToken(File * const input) {
    std::string token;

    const int first_ch(input->get());
    if (first_ch == EOF)
        return ""; // Empty string signal EOF.

    token += static_cast<char>(first_ch);
    if (likely(first_ch != '<'))
        return token;

    
    int ch;
    while ((ch = input->get()) != EOF) {
        token += static_cast<char>(ch);
        if (ch == '>')
            return token;
    }

    return token;
}


class XMLComponent {
    std::string text_;
public:
    explicit XMLComponent(const std::string &text);
    std::string toString() const;
    bool operator<(const XMLComponent &rhs) const { return getTag() < rhs.getTag(); }
private:
    std::string getTag() const;
};


XMLComponent::XMLComponent(const std::string &text)
    : text_(text)
{
    if (StringUtil::StartsWith(text_, "<datafield")) {
        static const RegexMatcher * const matcher(RegexMatcher::RegexMatcherFactory(
            "(tag=\"...\")( +ind1=\".\")?( +ind2=\".\")?"));
        if (matcher->matched(text_) and matcher->getLastMatchCount() != 4) {
            const size_t first_closing_angle_bracket_pos(text_.find('>'));
            text_ = "<datafield " + (*matcher)[1] + " ind1=\" \" ind2=\" \""
                    + text_.substr(first_closing_angle_bracket_pos);
        }
    }
}


std::string XMLComponent::getTag() const {
    if (StringUtil::StartsWith(text_, "<record>"))
        return "\1\1\1";
    if (StringUtil::StartsWith(text_, "<leader>"))
        return "\2\2\2";
    if (StringUtil::EndsWith(text_, "</record>"))
        return "\255\255\255";
    static const RegexMatcher * const matcher(RegexMatcher::RegexMatcherFactory(" tag=\"(...)\""));
    return matcher->matched(text_) ? (*matcher)[1] : "";
}


std::string XMLComponent::toString() const {
    std::string converted_text;

    // 1. Process opening tags:
    for (const auto original_and_replacement : from_to) {
        if (StringUtil::StartsWith(text_, original_and_replacement.first)) {
            converted_text = original_and_replacement.second + text_.substr(original_and_replacement.first.length());
            break;
        }
    }
    if (converted_text.empty())
        converted_text = text_;

    // 2. Process closing tags:
    for (const auto original_and_replacement : from_to) {
        if (StringUtil::EndsWith(converted_text, original_and_replacement.first)) {
            converted_text = converted_text.substr(0, converted_text.length()
                                                   - original_and_replacement.first.length())
                             + original_and_replacement.second;
            break;
        }
    }

    // 3. Process subfields tags:
    StringUtil::ReplaceString("<subfield", "<marc:subfield", &converted_text);
    StringUtil::ReplaceString("</subfield", "</marc:subfield", &converted_text);

    return converted_text;
}


void Convert(File * const input, File * const output) {
    bool converting(false);

    std::vector<XMLComponent> xml_components;
    std::string token(GetNextToken(input));
    bool leader_open_seen(false); // We only like to see one of these.
    std::string component_text;
    const RegexMatcher * const open_tag_matcher(RegexMatcher::RegexMatcherFactory("^<controlfield|^<datafield"));
    const RegexMatcher * const close_tag_matcher(
        RegexMatcher::RegexMatcherFactory("</controlfield>|</datafield>|</leader>"));
    while (not token.empty()) {
        if (converting) {
            if (token == "</record>") {
                converting = false;
                xml_components.emplace_back(token);
                break;
            } else if (token == "<leader>") { // If we get a 2nd <leader> we close our <record> and quit.
                if (leader_open_seen) {
                    xml_components.emplace_back("</record>");
                    break;
                } else {
                    leader_open_seen = true;
                    component_text = token;
                }
            } else {
                if (open_tag_matcher->matched(token))
                    component_text = token;
                else if (close_tag_matcher->matched(token))
                    xml_components.emplace_back(component_text + token);
                else
                    component_text += token;
            }
        } else if (token == "<record>") {
            xml_components.emplace_back(token);
            converting = true;
        }

        token = GetNextToken(input);
    }

    std::sort(xml_components.begin(), xml_components.end());
    for (const auto &component : xml_components)
        (*output) << component.toString() << '\n';
}


int main(int argc, char *argv[]) {
    ::progname = argv[0];

    if (argc < 2)
        Usage();

    bool append(false);
    if (std::strcmp("--append", argv[1]) == 0) {
        append = true;
        --argc, ++argv;
    }

    if (argc != 3)
        Usage();

    const std::unique_ptr<File> input(FileUtil::OpenInputFileOrDie(argv[1]));
    const std::unique_ptr<File> output(append ? FileUtil::OpenForAppendingOrDie(argv[2])
                                              : FileUtil::OpenOutputFileOrDie(argv[2]));

    try {
        Convert(input.get(), output.get());
    } catch (const std::exception &x) {
        Error("caught exception: " + std::string(x.what()));
    }
}


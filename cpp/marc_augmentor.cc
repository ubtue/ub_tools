/** \brief A MARC filter that can modify fields.
 *  \author Dr. Johannes Ruscheinski (johannes.ruscheinski@uni-tuebingen.de)
 *
 *  \copyright 2018 Universitätsbibliothek Tübingen.  All rights reserved.
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
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "Compiler.h"
#include "FileUtil.h"
#include "MARC.h"
#include "RegexMatcher.h"
#include "StringUtil.h"
#include "TextUtil.h"
#include "util.h"


namespace {


void Usage() {
    std::cerr << "Usage: " << ::progname << " marc_input marc_output [--input-format=(marc-xml|marc-21)]\n"
              << "       [--output-format=(marc-xml|marc-21)] op1 [op2 .. opN]\n"
              << "       where each operation must start with the operation type. Operation-type flags are\n"
              << "           --insert-field field_or_subfield_spec new_field_or_subfield_data\n"
              << "               field_or_subfield_spec must be a field tag followed by an optional subfield code\n"
              << "               A new field will be inserted.  If it is a non-repeatable field and a field with the\n"
              << "               same tag already exists, the program aborts with an error message.\n"
              << "           --replace-field field_or_subfield_spec new_field_or_subfield_data\n"
              << "               field_or_subfield_spec must be a field tag followed by an optional subfield code\n"
              << "               Any field with a matching tag and subfield code, if specified, will have its\n"
              << "               contents replaced.\n"
              << "           --add-subfield field_and_subfield_spec new_subfield_data\n"
              << "               Any field with a matching tag will have a new subfield inserted.\n"
              << "           --insert-field-if field_or_subfield_spec field_or_subfield_spec_and_pcre_regex\n"
              << " new_field_or_subfield_data\n"
              << "               Keep only records that have a bibliographic level matching any of the specified\n"
              << "               characters.  (Comparsion against leader position 07.)\n"
              << "           --replace-field-if field_or_subfield_spec field_or_subfield_spec_and_pcre_regex"
              << " new_field_or_subfield_data\n"
              << "               Any fields that matched or that have subfields that matched will be dropped.\n"
              << "           --add-subfield-if field_or_subfield_spec field_or_subfield_spec_and_pcre_regex"
              << " new_field_or_subfield_data\n"
              << "               Any field with a matching tag will have a new subfield inserted if the regex matched.\n"
              << "           --config-path filename\n"
              << "               If --config-path has been specified, no other operation may be used.\n"
              << "       Field or subfield data may contain any of the following escapes:\n"
              << "         \\n, \\t, \\b, \\r, \\f, \\v, \\a, \\\\, \\uNNNN and \\UNNNNNNNN\n"
              << "       \"field_or_subfield_spec_and_pcre_regex\" consists of a 3-character tag, an optional 1-character\n"
              << "       subfield code, a colon and a PCRE regex.\n\n";

    std::exit(EXIT_FAILURE);
}


class CompiledPattern {
    std::string tag_;
    char subfield_code_;
    RegexMatcher matcher_;
public:
    static const char NO_SUBFIELD_CODE;
public:
    CompiledPattern(const std::string &tag, const char subfield_code,  const RegexMatcher &matcher)
        : tag_(tag), subfield_code_(subfield_code), matcher_(matcher) {}
    CompiledPattern(const CompiledPattern &other) = default;
    bool matched(const MARC::Record &record);
};


const char CompiledPattern::NO_SUBFIELD_CODE('\0');


bool CompiledPattern::matched(const MARC::Record &record) {
    for (const auto &field : record) {
        if (field.getTag() != tag_)
            continue;

        std::string err_msg;
        if (subfield_code_ == NO_SUBFIELD_CODE) { // Match a field.
            if (matcher_.matched(field.getContents(), &err_msg))
                return true;
            if (unlikely(not err_msg.empty()))
                ERROR("Unexpected error while trying to match \"" + matcher_.getPattern() + "\" against a field: " + err_msg);
        } else { // Match a subfield.
            const MARC::Subfields subfields(field.getSubfields());
        }
    }

    return false;
}


enum class OutputFormat { MARC_XML, MARC_21, SAME_AS_INPUT };
enum class AugmentorType { INSERT_FIELD, REPLACE_FIELD, ADD_SUBFIELD, INSERT_FIELD_IF, REPLACE_FIELD_IF, ADD_SUBFIELD_IF };


class AugmentorDescriptor {
private:
    AugmentorType augmentor_type_;
    MARC::Tag tag_;
    char subfield_code_;
    std::string text_to_insert_;
    CompiledPattern *compiled_pattern_;
public:
    inline AugmentorType getAugmentorType() const { return augmentor_type_; }

    inline const MARC::Tag &getTag() const { return tag_; }
    inline char getSubfieldCode() const { return subfield_code_; }
    inline const std::string &getInsertionText() const { return text_to_insert_; }
    inline CompiledPattern *getCompiledPattern() { return compiled_pattern_; }

    inline static AugmentorDescriptor MakeInsertFieldAugmentor(const MARC::Tag &tag, const char subfield_code,
                                                               const std::string &text_to_insert)
    {
        AugmentorDescriptor descriptor(AugmentorType::INSERT_FIELD, tag, subfield_code);
        descriptor.text_to_insert_ = text_to_insert;
        return descriptor;
    }

    inline static AugmentorDescriptor MakeReplaceFieldAugmentor(const MARC::Tag &tag, const char subfield_code,
                                                                const std::string &text_to_insert)
    {
        AugmentorDescriptor descriptor(AugmentorType::REPLACE_FIELD, tag, subfield_code);
        descriptor.text_to_insert_ = text_to_insert;
        return descriptor;
    }

    inline static AugmentorDescriptor MakeAddSubfieldAugmentor(const MARC::Tag &tag, const char subfield_code,
                                                               const std::string &text_to_insert)
    {
        AugmentorDescriptor descriptor(AugmentorType::ADD_SUBFIELD, tag, subfield_code);
        descriptor.text_to_insert_ = text_to_insert;
        return descriptor;
    }

    inline static AugmentorDescriptor MakeInsertFieldIfAugmentor(const MARC::Tag &tag, const char subfield_code,
                                                                 CompiledPattern * const compiled_pattern,
                                                                 const std::string &text_to_insert)
    {
        AugmentorDescriptor descriptor(AugmentorType::INSERT_FIELD_IF, tag, subfield_code, compiled_pattern);
        descriptor.text_to_insert_ = text_to_insert;
        return descriptor;
    }

    inline static AugmentorDescriptor MakeReplaceFieldIfAugmentor(const MARC::Tag &tag, const char subfield_code,
                                                                  CompiledPattern * const compiled_pattern,
                                                                  const std::string &text_to_insert)
    {
        AugmentorDescriptor descriptor(AugmentorType::REPLACE_FIELD_IF, tag, subfield_code, compiled_pattern);
        descriptor.text_to_insert_ = text_to_insert;
        return descriptor;
    }

    inline static AugmentorDescriptor MakeAddSubfieldIfAugmentor(const MARC::Tag &tag, const char subfield_code,
                                                                 CompiledPattern * const compiled_pattern,
                                                                 const std::string &text_to_insert)
    {
        AugmentorDescriptor descriptor(AugmentorType::ADD_SUBFIELD_IF, tag, subfield_code, compiled_pattern);
        descriptor.text_to_insert_ = text_to_insert;
        return descriptor;
    }
private:
    AugmentorDescriptor(const AugmentorType augmentor_type, const MARC::Tag &tag, const char subfield_code,
                        CompiledPattern * const compiled_pattern = nullptr )
        : augmentor_type_(augmentor_type), tag_(tag), subfield_code_(subfield_code), compiled_pattern_(compiled_pattern) { }
};


// Returns true, if we modified the record, else false.
bool InsertField(MARC::Record * const record, const MARC::Tag &tag, const char subfield_code, const std::string &insertion_text,
                 std::string * const error_message, CompiledPattern * const condition = nullptr)
{
    error_message->clear();
    if (condition != nullptr) {
        if (not condition->matched(*record))
            return false;
    }

    if (subfield_code == CompiledPattern::NO_SUBFIELD_CODE) {
        if (not record->insertField(tag, insertion_text)) {
            *error_message = "failed to insert " + tag.to_string()
                             + " field! (Probably due to a duplicate non-repeatable field.)";
            return false;
        }
    } else {
        if (not record->insertField(tag, { { subfield_code, insertion_text } })) {
            *error_message = "failed to insert " + tag.to_string() + std::string(1, subfield_code)
                             + " subfield! (Probably due to a duplicate non-repeatable field.)";
            return false;
        }
    }

    return true;
}


// Returns true, if we modified the record, else false.
bool ReplaceField(MARC::Record * const record, const MARC::Tag &tag, const char subfield_code,
                  const std::string &replacement_text, CompiledPattern * const condition = nullptr)
{
    if (condition != nullptr) {
        if (not condition->matched(*record))
            return false;
    }

    bool replaced_at_least_one(false);
    for (auto &field : *record) {
        if (field.getTag() != tag)
            continue;

        if (subfield_code == CompiledPattern::NO_SUBFIELD_CODE) {
            field.setContents(replacement_text);
            replaced_at_least_one = true;
        } else {
            MARC::Subfields subfields(field.getContents());
            if (subfields.replaceFirstSubfield(subfield_code, replacement_text)) {
                field.setContents(std::string(1, field.getIndicator1()) + std::string(1, field.getIndicator2())
                                  + subfields.toString());
                replaced_at_least_one = true;
            }
        }
    }

    return replaced_at_least_one;
}


void Augment(std::vector<AugmentorDescriptor> &augmentors, MARC::Reader * const marc_reader, MARC::Writer * const marc_writer) {
    unsigned total_count(0), modified_count(0);
    std::string error_message;
    while (MARC::Record record = marc_reader->read()) {
        ++total_count;
        bool modified_record(false);
        for (auto &augmentor : augmentors) {
            if (augmentor.getAugmentorType() == AugmentorType::INSERT_FIELD) {
                if (InsertField(&record, augmentor.getTag(), augmentor.getSubfieldCode(), augmentor.getInsertionText(),
                                &error_message))
                    modified_record = true;
                else if (not error_message.empty())
                    WARNING(error_message);
            } else if (augmentor.getAugmentorType() == AugmentorType::INSERT_FIELD_IF) {
                if (InsertField(&record, augmentor.getTag(), augmentor.getSubfieldCode(), augmentor.getInsertionText(),
                                &error_message, augmentor.getCompiledPattern()))
                    modified_record = true;
                else if (not error_message.empty())
                    WARNING(error_message);
            } else if (augmentor.getAugmentorType() == AugmentorType::REPLACE_FIELD) {
                if (ReplaceField(&record, augmentor.getTag(), augmentor.getSubfieldCode(), augmentor.getInsertionText()))
                    modified_record = true;
            } else if (augmentor.getAugmentorType() == AugmentorType::REPLACE_FIELD_IF) {
                if (ReplaceField(&record, augmentor.getTag(), augmentor.getSubfieldCode(), augmentor.getInsertionText(),
                                 augmentor.getCompiledPattern()))
                    modified_record = true;
            } else
                ERROR("unhandled Augmentor type!");
        }

        if (modified_record)
            ++modified_count;
        marc_writer->write(record);
    }

    std::cerr << "Processed a total of " << total_count << " record(s).\n";
    std::cerr << "Modified " << modified_count << " record(s).\n";
}


void ExtractCommandArgs(char ***argvp, MARC::Tag * const tag, char * const subfield_code,
                        std::string * const field_or_subfield_contents)
{
    const std::string command(**argvp);
    ++*argvp;

    const std::string tag_and_optional_subfield_code(**argvp);
    if (tag_and_optional_subfield_code.length() != MARC::Record::TAG_LENGTH
        and tag_and_optional_subfield_code.length() != MARC::Record::TAG_LENGTH + 1)
        ERROR("invalid tag and optional subfield code after \"" + command + "\"!");
    *tag = MARC::Tag(tag_and_optional_subfield_code.substr(0, MARC::Record::TAG_LENGTH));
    *subfield_code = (tag_and_optional_subfield_code.length() > MARC::Record::TAG_LENGTH)
                     ? tag_and_optional_subfield_code[MARC::Record::TAG_LENGTH] : CompiledPattern::NO_SUBFIELD_CODE;
    ++*argvp;

    *field_or_subfield_contents = **argvp;
    ++*argvp;
}


void ExtractCommandArgs(char ***argvp, MARC::Tag * const tag, char * const subfield_code,
                        CompiledPattern **compiled_pattern, std::string * const field_or_subfield_contents)
{
    const std::string command(**argvp);
    ++*argvp;

    const std::string tag_and_optional_subfield_code(**argvp);
    if (tag_and_optional_subfield_code.length() != MARC::Record::TAG_LENGTH
        and tag_and_optional_subfield_code.length() != MARC::Record::TAG_LENGTH + 1)
        ERROR("invalid tag and optional subfield code after \"" + command + "\"!");
    *tag = MARC::Tag(tag_and_optional_subfield_code.substr(0, MARC::Record::TAG_LENGTH));
    *subfield_code = (tag_and_optional_subfield_code.length() > MARC::Record::TAG_LENGTH)
                     ? tag_and_optional_subfield_code[MARC::Record::TAG_LENGTH] : CompiledPattern::NO_SUBFIELD_CODE;
    ++*argvp;

    const std::string tag_optional_subfield_code_and_regex(**argvp);
    const auto first_colon_pos(tag_optional_subfield_code_and_regex.find(':'));
    if (first_colon_pos != MARC::Record::TAG_LENGTH and first_colon_pos != MARC::Record::TAG_LENGTH + 1)
        ERROR("invalid tag and optional subfield code after \"" + command + "\"!");
    const std::string match_tag(tag_optional_subfield_code_and_regex.substr(0, MARC::Record::TAG_LENGTH));
    const char match_subfield_code(
        (first_colon_pos == MARC::Record::TAG_LENGTH) ? CompiledPattern::NO_SUBFIELD_CODE
                                                      : tag_optional_subfield_code_and_regex[MARC::Record::TAG_LENGTH]);
    const std::string regex_string(tag_optional_subfield_code_and_regex.substr(first_colon_pos + 1));
    std::string err_msg;
    RegexMatcher * const new_matcher(RegexMatcher::RegexMatcherFactory(regex_string, &err_msg));
    if (new_matcher == nullptr)
        ERROR("failed to compile regular expression: \"" + regex_string + "\" for \"" + command + "\"! (" + err_msg +")");
    *compiled_pattern = new CompiledPattern(match_tag, match_subfield_code, std::move(*new_matcher));
    ++*argvp;

    *field_or_subfield_contents = **argvp;
    ++*argvp;
}


void ProcessAugmentorArgs(char **argv, std::vector<AugmentorDescriptor> * const augmentors) {
    MARC::Tag tag;
    char subfield_code;
    CompiledPattern *compiled_pattern;
    std::string field_or_subfield_contents;

    while (*argv != nullptr) {
        if (std::strcmp(*argv, "--insert-field") == 0) {
            ExtractCommandArgs(&argv, &tag, &subfield_code, &field_or_subfield_contents);
            augmentors->emplace_back(AugmentorDescriptor::MakeInsertFieldAugmentor(tag, subfield_code,
                                                                                   field_or_subfield_contents));
        } else if (std::strcmp(*argv, "--replace-field") == 0) {
            ExtractCommandArgs(&argv, &tag, &subfield_code, &field_or_subfield_contents);
            augmentors->emplace_back(AugmentorDescriptor::MakeReplaceFieldAugmentor(tag, subfield_code,
                                                                                    field_or_subfield_contents));
        } else if (std::strcmp(*argv, "--add-subfield") == 0) {
            ExtractCommandArgs(&argv, &tag, &subfield_code, &field_or_subfield_contents);
            augmentors->emplace_back(AugmentorDescriptor::MakeAddSubfieldAugmentor(tag, subfield_code,
                                                                                   field_or_subfield_contents));
        } else if (std::strcmp(*argv, "--insert-field-if") == 0) {
            ExtractCommandArgs(&argv, &tag, &subfield_code, &compiled_pattern, &field_or_subfield_contents);
            augmentors->emplace_back(AugmentorDescriptor::MakeInsertFieldIfAugmentor(tag, subfield_code, compiled_pattern,
                                                                                   field_or_subfield_contents));
        } else if (std::strcmp(*argv, "--replace-field-if") == 0) {
            ExtractCommandArgs(&argv, &tag, &subfield_code, &compiled_pattern, &field_or_subfield_contents);
            augmentors->emplace_back(AugmentorDescriptor::MakeReplaceFieldIfAugmentor(tag, subfield_code, compiled_pattern,
                                                                                    field_or_subfield_contents));
        } else if (std::strcmp(*argv, "--add-subfield-if") == 0) {
            ExtractCommandArgs(&argv, &tag, &subfield_code, &compiled_pattern, &field_or_subfield_contents);
            augmentors->emplace_back(AugmentorDescriptor::MakeAddSubfieldIfAugmentor(tag, subfield_code, compiled_pattern,
                                                                                     field_or_subfield_contents));
        } else
            ERROR("unknown operation type \"" + std::string(*argv) + "\"!");
    }
}


typedef char *CharPointer;


void MakeArgumentListFromFile(const std::string &config_file_path, char ***argvp) {
    std::vector<std::string> lines;
    std::unique_ptr<File> input(FileUtil::OpenInputFileOrDie(config_file_path));
    while (not input->eof()) {
        std::string line;
        input->getline(&line);
        StringUtil::TrimWhite(&line);
        if (not line.empty())
            lines.emplace_back(line);
    }

    *argvp = new CharPointer[lines.size() + 1];
    char **next_arg(*argvp);
    for (const auto &arg : lines) {
        *next_arg = ::strdup(arg.c_str());
        ++next_arg;
    }
    *next_arg = nullptr;
}


} // unnamed namespace


int main(int argc, char **argv) {
    ::progname = argv[0];
    ++argv;

    if (argc < 4)
        Usage();

    const std::string input_filename(*argv++);
    const std::string output_filename(*argv++);

    MARC::Reader::ReaderType reader_type(MARC::Reader::AUTO);
    if (std::strcmp("--input-format=marc-xml", *argv) == 0) {
        reader_type = MARC::Reader::XML;
        ++argv;
    } else if (std::strcmp("--input-format=marc-21", *argv) == 0) {
        reader_type = MARC::Reader::BINARY;
        ++argv;
    } else if (StringUtil::StartsWith(*argv, "--input-format="))
        ERROR("unknown input format \"" + std::string(*argv + __builtin_strlen("--input-format="))
              + "\" use \"marc-xml\" or \"marc-21\"!");
    std::unique_ptr<MARC::Reader> marc_reader(MARC::Reader::Factory(input_filename, reader_type));

    MARC::Writer::WriterType writer_type(MARC::Writer::AUTO);
    if (std::strcmp("--output-format=marc-xml", *argv) == 0) {
        writer_type = MARC::Writer::XML;
        ++argv;
    } else if (std::strcmp("--output-format=marc-21", *argv) == 0) {
        writer_type = MARC::Writer::BINARY;
        ++argv;
    } else if (StringUtil::StartsWith(*argv, "--output-format="))
        ERROR("unknown output format \"" + std::string(*argv + __builtin_strlen("--output-format="))
              + "\" use \"marc-xml\" or \"marc-21\"!");
    std::unique_ptr<MARC::Writer> marc_writer(MARC::Writer::Factory(output_filename, writer_type));

    try {
        std::vector<AugmentorDescriptor> augmentors;
        if (*(argv + 1) != nullptr and std::strcmp(*(argv + 1), "--config-path") == 0) {
            argv += 2;
            if (*argv == nullptr)
                ERROR("missing config filename after \"--config-path\"!");
            const std::string config_filename(*argv);
            ++argv;
            if (*argv != nullptr)
                ERROR("unexpected argument after config filename \"" + std::string(*argv) + "\"!");
            char **file_argv;
            MakeArgumentListFromFile(config_filename, &file_argv);
            ProcessAugmentorArgs(file_argv, &augmentors);
        } else
            ProcessAugmentorArgs(argv, &augmentors);

        Augment(augmentors, marc_reader.get(), marc_writer.get());
    } catch (const std::exception &x) {
        ERROR("caught exception: " + std::string(x.what()));
    }
}

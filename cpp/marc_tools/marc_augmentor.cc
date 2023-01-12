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

#include <algorithm>
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
    std::cerr
        << "Usage: " << ::progname << " marc_input marc_output op1 [op2 .. opN]\n"
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
        << "           --insert-field-if field_or_subfield_spec field_or_subfield_spec_and_pcre_regex"
        << " new_field_or_subfield_data\n"
        << "               Like \"--insert-field\" but the insertion only happens if we find a field or subfield\n"
        << "               with contents matching the PCRE.\n"
        << "           --insert-field-if-regex field_or_subfield_spec field_or_subfield_spec_and_pcre_regex"
        << " new_field_or_subfield_data\n"
        << "               Like \"--insert-field-if\" but the match can be referenced in the field to write.\n"
        << "           --replace-field-if field_or_subfield_spec field_or_subfield_spec_and_pcre_regex"
        << " new_field_or_subfield_data\n"
        << "               Any fields that matched or that have subfields that matched will be dropped.\n"
        << "           --replace-subfield-if-regex field_or_subfield_spec field_or_subfield_spec_and_pcre_regex"
        << " replace_regex (i.e. /pattern/replacement/)\n"
        << "               Backreferences are supported\n"
        << "           --add-subfield-if field_or_subfield_spec field_or_subfield_spec_and_pcre_regex"
        << " new_field_or_subfield_data\n"
        << "           --add-subfield-if-matching field_or_subfield_spec field_or_subfield_spec_and_pcre_regex"
        << " new_field_or_subfield_data\n"
        << "               Fields with a matching tag and matching the regex will have a new subfield inserted if the regex matched.\n"
        << "           --add-subfield-if-regex field_or_subfield_spec field_or_subfield_spec_and_pcre_regex"
        << "               Like \"--add-subfield-if\" but the match can be referenced in the field to write.\n"
        << "           --config-path filename\n"
        << "               If --config-path has been specified, no other operation may be used.\n"
        << "       Field or subfield data may contain any of the following escapes:\n"
        << "         \\n, \\t, \\b, \\r, \\f, \\v, \\a, \\\\, \\uNNNN and \\UNNNNNNNN as well as \\o, \\oo and \\ooo\n"
        << "         octal escape sequences.\n"
        << "       \"field_or_subfield_spec_and_pcre_regex\" consists of a 3-character tag, an optional 1-character\n"
        << "       subfield code, a colon and a PCRE regex.  \"field_or_subfield_spec_pair\" consists of 2 field or\n"
        << "       field or subfield references separated by a colon.\n\n";

    std::exit(EXIT_FAILURE);
}


class CompiledPattern {
    std::string tag_;
    char subfield_code_;
    RegexMatcher matcher_;

public:
    static const char NO_SUBFIELD_CODE;

public:
    CompiledPattern(const std::string &tag, const char subfield_code, const RegexMatcher &matcher)
        : tag_(tag), subfield_code_(subfield_code), matcher_(matcher) { }
    CompiledPattern(const CompiledPattern &other) = default;
    bool matched(const MARC::Record &record, std::string * const value, size_t * const start_pos, size_t * const end_pos);
    std::string getTag() { return tag_; }
    char getSubfieldCode() { return subfield_code_; }
    RegexMatcher getMatcher() { return matcher_; }
};


const char CompiledPattern::NO_SUBFIELD_CODE('\0');


bool CompiledPattern::matched(const MARC::Record &record, std::string * const value = nullptr, size_t * const start_pos = nullptr,
                              size_t * const end_pos = nullptr) {
    for (const auto &field : record.getTagRange(tag_)) {
        std::string err_msg;
        if (subfield_code_ == NO_SUBFIELD_CODE) { // Match a field.
            if (matcher_.matched(field.getContents(), &err_msg, start_pos, end_pos)) {
                if (value != nullptr)
                    *value = field.getContents();
                return true;
            }
            if (unlikely(not err_msg.empty()))
                LOG_ERROR("Unexpected error while trying to match \"" + matcher_.getPattern() + "\" against a field: " + err_msg);
        } else { // Match a subfield.
            const MARC::Subfields subfields(field.getSubfields());
            for (const auto &subfield : subfields) {
                if (subfield.code_ == subfield_code_ and matcher_.matched(subfield.value_, &err_msg, start_pos, end_pos)) {
                    if (value != nullptr)
                        *value = subfield.value_;
                    return true;
                }
                if (unlikely(not err_msg.empty()))
                    LOG_ERROR("Unexpected error while trying to match \"" + matcher_.getPattern() + "\" against a field: " + err_msg);
            }
        }
    }

    return false;
}


enum class OutputFormat { MARC_XML, MARC_21, SAME_AS_INPUT };
enum class AugmentorType {
    INSERT_FIELD,
    REPLACE_FIELD,
    ADD_SUBFIELD,
    INSERT_FIELD_IF,
    REPLACE_FIELD_IF,
    REPLACE_SUBFIELD_IF_REGEX,
    ADD_SUBFIELD_IF,
    INSERT_FIELD_IF_REGEX,
    ADD_SUBFIELD_IF_MATCHING,
    ADD_SUBFIELD_IF_REGEX
};


class AugmentorDescriptor {
private:
    AugmentorType augmentor_type_;
    MARC::Tag tag_, tag2_;
    char subfield_code_, subfield_code2_;
    std::string text_to_insert_;
    CompiledPattern *compiled_pattern_;
    std::string replace_regex_;

public:
    inline AugmentorType getAugmentorType() const { return augmentor_type_; }

    inline const MARC::Tag &getTag() const { return tag_; }
    inline const MARC::Tag &getTag2() const { return tag2_; }
    inline char getSubfieldCode() const { return subfield_code_; }
    inline char getSubfieldCode2() const { return subfield_code2_; }
    inline const std::string &getInsertionText() const { return text_to_insert_; }
    inline const std::string &getReplaceRegex() const { return replace_regex_; }
    inline CompiledPattern *getCompiledPattern() { return compiled_pattern_; }

    inline static AugmentorDescriptor MakeInsertFieldAugmentor(const MARC::Tag &tag, const char subfield_code,
                                                               const std::string &text_to_insert) {
        AugmentorDescriptor descriptor(AugmentorType::INSERT_FIELD, tag, subfield_code);
        descriptor.text_to_insert_ = TextUtil::CStyleUnescape(text_to_insert);
        return descriptor;
    }

    inline static AugmentorDescriptor MakeReplaceFieldAugmentor(const MARC::Tag &tag, const char subfield_code,
                                                                const std::string &text_to_insert) {
        AugmentorDescriptor descriptor(AugmentorType::REPLACE_FIELD, tag, subfield_code);
        descriptor.text_to_insert_ = TextUtil::CStyleUnescape(text_to_insert);
        return descriptor;
    }

    inline static AugmentorDescriptor MakeAddSubfieldAugmentor(const MARC::Tag &tag, const char subfield_code,
                                                               const std::string &text_to_insert) {
        AugmentorDescriptor descriptor(AugmentorType::ADD_SUBFIELD, tag, subfield_code);
        descriptor.text_to_insert_ = TextUtil::CStyleUnescape(text_to_insert);
        return descriptor;
    }

    inline static AugmentorDescriptor MakeInsertFieldIfAugmentor(const MARC::Tag &tag, const char subfield_code,
                                                                 CompiledPattern * const compiled_pattern,
                                                                 const std::string &text_to_insert) {
        AugmentorDescriptor descriptor(AugmentorType::INSERT_FIELD_IF, tag, subfield_code, compiled_pattern);
        descriptor.text_to_insert_ = TextUtil::CStyleUnescape(text_to_insert);
        return descriptor;
    }


    inline static AugmentorDescriptor MakeInsertFieldIfRegexAugmentor(const MARC::Tag &tag, const char subfield_code,
                                                                      CompiledPattern * const compiled_pattern,
                                                                      const std::string &replace_regex) {
        AugmentorDescriptor descriptor(AugmentorType::INSERT_FIELD_IF_REGEX, tag, subfield_code, compiled_pattern);
        descriptor.replace_regex_ = replace_regex;
        return descriptor;
    }


    inline static AugmentorDescriptor MakeReplaceFieldIfAugmentor(const MARC::Tag &tag, const char subfield_code,
                                                                  CompiledPattern * const compiled_pattern,
                                                                  const std::string &text_to_insert) {
        AugmentorDescriptor descriptor(AugmentorType::REPLACE_FIELD_IF, tag, subfield_code, compiled_pattern);
        descriptor.text_to_insert_ = TextUtil::CStyleUnescape(text_to_insert);
        return descriptor;
    }

    inline static AugmentorDescriptor MakeReplaceFieldIfRegexAugmentor(const MARC::Tag &tag, const char subfield_code,
                                                                       CompiledPattern * const compiled_pattern,
                                                                       const std::string &replace_regex) {
        AugmentorDescriptor descriptor(AugmentorType::REPLACE_SUBFIELD_IF_REGEX, tag, subfield_code, compiled_pattern);
        descriptor.replace_regex_ = replace_regex;
        return descriptor;
    }


    inline static AugmentorDescriptor MakeAddSubfieldIfAugmentor(const MARC::Tag &tag, const char subfield_code,
                                                                 CompiledPattern * const compiled_pattern,
                                                                 const std::string &text_to_insert) {
        AugmentorDescriptor descriptor(AugmentorType::ADD_SUBFIELD_IF, tag, subfield_code, compiled_pattern);
        descriptor.text_to_insert_ = TextUtil::CStyleUnescape(text_to_insert);
        return descriptor;
    }


    inline static AugmentorDescriptor MakeAddSubfieldIfMatchingAugmentor(const MARC::Tag &tag, const char subfield_code,
                                                                         CompiledPattern * const compiled_pattern,
                                                                         const std::string &text_to_insert) {
        AugmentorDescriptor descriptor(AugmentorType::ADD_SUBFIELD_IF_MATCHING, tag, subfield_code, compiled_pattern);
        descriptor.text_to_insert_ = TextUtil::CStyleUnescape(text_to_insert);
        return descriptor;
    }

    inline static AugmentorDescriptor MakeAddSubfieldIfRegexAugmentor(const MARC::Tag &tag, const char subfield_code,
                                                                      CompiledPattern * const compiled_pattern,
                                                                      const std::string &replace_regex) {
        AugmentorDescriptor descriptor(AugmentorType::ADD_SUBFIELD_IF_REGEX, tag, subfield_code, compiled_pattern);
        descriptor.replace_regex_ = replace_regex;
        return descriptor;
    }


private:
    AugmentorDescriptor(const AugmentorType augmentor_type, const MARC::Tag &tag, const char subfield_code,
                        CompiledPattern * const compiled_pattern = nullptr)
        : augmentor_type_(augmentor_type), tag_(tag), subfield_code_(subfield_code), compiled_pattern_(compiled_pattern) { }
    AugmentorDescriptor(const AugmentorType augmentor_type, const MARC::Tag &tag, const char subfield_code, const MARC::Tag &tag2,
                        const char subfield_code2, const std::string &map_filename);
};


// Returns true, if we modified the record, else false.
bool InsertField(MARC::Record * const record, const MARC::Tag &tag, const char subfield_code, const std::string &insertion_text,
                 std::string * const error_message, CompiledPattern * const condition = nullptr) {
    error_message->clear();
    if (condition != nullptr) {
        if (not condition->matched(*record))
            return false;
    }

    if (subfield_code == CompiledPattern::NO_SUBFIELD_CODE) {
        if (not record->insertField(tag, insertion_text)) {
            *error_message = "failed to insert " + tag.toString() + " field! (Probably due to a duplicate non-repeatable field.)";
            return false;
        }
    } else {
        if (not record->insertField(tag, { { subfield_code, insertion_text } })) {
            *error_message = "failed to insert " + tag.toString() + std::string(1, subfield_code)
                             + " subfield! (Probably due to a duplicate non-repeatable field.)";
            return false;
        }
    }

    return true;
}


bool HasGlobalFlag(const std::string &replacement_regex) {
    return *(replacement_regex.rbegin()) == 'g' and *(replacement_regex.rbegin() + 1) == '/';
}


std::string EscapeRegexBackreferences(const std::string &replacement_regex) {
    std::string escaped_regex(replacement_regex);
    for (auto it(escaped_regex.begin()); it != escaped_regex.end(); it += std::min<int>(escaped_regex.end() - it, 1)) {
        if (*it == '\\' and (it + 1 != escaped_regex.end()) and StringUtil::IsDigit(*(it + 1)))
            it = escaped_regex.insert(it, '\\') + 1;
    }
    return escaped_regex;
}


std::vector<std::string> GetSplitReplacementRegexWithoutGlobalFlagAndEscapedBackrefs(const std::string &replacement_regex) {
    const std::string replacement_regex_without_global_flag(
        HasGlobalFlag(replacement_regex) ? replacement_regex.substr(0, replacement_regex.length() - 2) : replacement_regex);
    const std::string replacement_regex_with_escaped_backreferences(EscapeRegexBackreferences(replacement_regex_without_global_flag));
    return StringUtil::Split(replacement_regex_with_escaped_backreferences, '/', '\\' /* escape_char */,
                             true /* suppress_empty_components */);
}


bool InsertFieldRegex(MARC::Record * const record, const MARC::Tag &tag, const char subfield_code, const std::string &replacement_regex,
                      std::string * const error_message, CompiledPattern * const condition = nullptr) {
    if (subfield_code == CompiledPattern::NO_SUBFIELD_CODE)
        LOG_ERROR("Insert Field Regex replacement without subfield not supported");

    error_message->clear();
    if (condition == nullptr)
        LOG_ERROR("Condition required for InsertFieldRegex");

    std::string subfield_value;
    size_t start_pos, end_pos;
    if (not condition->matched(*record, &subfield_value, &start_pos, &end_pos))
        return false;

    bool global(HasGlobalFlag(replacement_regex));
    std::vector<std::string> pattern_and_replacement(GetSplitReplacementRegexWithoutGlobalFlagAndEscapedBackrefs(replacement_regex));
    if (unlikely(pattern_and_replacement.size() != 2))
        LOG_ERROR("Invalid replacement pattern :\"" + replacement_regex + "\"\nMust follow /pattern/replacement/g? scheme");

    ThreadSafeRegexMatcher replace_matcher(pattern_and_replacement[0]);
    const std::string insertion_text(replace_matcher.replaceWithBackreferences(subfield_value, pattern_and_replacement[1], global));


    if (not record->insertField(tag, { { subfield_code, insertion_text } })) {
        *error_message = "failed to insert " + tag.toString() + std::string(1, subfield_code)
                         + " subfield! (Probably due to a duplicate non-repeatable field.)";
        return false;
    }
    return true;
}


// Returns true, if we modified the record, else false.
bool ReplaceField(MARC::Record * const record, const MARC::Tag &tag, const char subfield_code, const std::string &replacement_text,
                  CompiledPattern * const condition = nullptr) {
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
                field.setSubfields(subfields);
                replaced_at_least_one = true;
            }
        }
    }

    return replaced_at_least_one;
}


bool ReplaceSubfieldRegex(MARC::Record * const record, const MARC::Tag &tag, const char subfield_code, const std::string &replacement_regex,
                          CompiledPattern * const condition = nullptr) {
    if (condition != nullptr) {
        if (not condition->matched(*record))
            return false;
    }

    bool replaced_at_least_one(false);
    for (auto &field : *record) {
        if (field.getTag() != tag)
            continue;

        if (subfield_code == CompiledPattern::NO_SUBFIELD_CODE)
            LOG_ERROR("Regex Replacement without subfield not supported");

        std::string text_to_replace;
        MARC::Subfields subfields(field.getContents());
        if (not subfields.hasSubfield(subfield_code))
            continue;


        bool global(HasGlobalFlag(replacement_regex));
        std::vector<std::string> pattern_and_replacement(GetSplitReplacementRegexWithoutGlobalFlagAndEscapedBackrefs(replacement_regex));
        if (unlikely(pattern_and_replacement.size() != 2))
            LOG_ERROR("Invalid replacement pattern :\"" + replacement_regex + "\"\nMust follow /pattern/replacement/g? scheme");

        const std::string subfield_value(subfields.getFirstSubfieldWithCode(subfield_code));
        ThreadSafeRegexMatcher replace_matcher(pattern_and_replacement[0]);

        const std::string replacement_text(replace_matcher.replaceWithBackreferences(subfield_value, pattern_and_replacement[1], global));

        if (subfields.replaceFirstSubfield(subfield_code, replacement_text)) {
            field.setSubfields(subfields);
            replaced_at_least_one = true;
        }
    }

    return replaced_at_least_one;
}


// Returns true, if we modified the record, else false.
bool AddSubfield(MARC::Record * const record, const MARC::Tag &tag, const char subfield_code, const std::string &insertion_text,
                 CompiledPattern * const condition = nullptr, const bool only_matching = false) {
    if (condition != nullptr) {
        if (not condition->matched(*record))
            return false;
    }

    bool modified_at_least_one(false);
    for (auto &field : *record) {
        if (field.getTag() == tag) {
            MARC::Subfields subfields(field.getSubfields());
            if (only_matching) {
                if (tag != condition->getTag())
                    LOG_ERROR("Target tag und condition tag must match");
                const std::vector<std::string> subfield_values_to_match(subfields.extractSubfields(condition->getSubfieldCode()));
                if (std::none_of(
                        subfield_values_to_match.cbegin(), subfield_values_to_match.cend(),
                        [condition](const std::string &subfield_value) { return condition->getMatcher().matched(subfield_value); }))
                    continue;
            }
            subfields.addSubfield(subfield_code, insertion_text);
            field.setSubfields(subfields);
            modified_at_least_one = true;
        }
    }

    return modified_at_least_one;
}


bool AddSubfieldRegex(MARC::Record * const record, const MARC::Tag &tag, const char subfield_code, const std::string &replacement_regex,
                      CompiledPattern * const condition = nullptr) {
    if (subfield_code == CompiledPattern::NO_SUBFIELD_CODE)
        LOG_ERROR("Add Subfield Regex without subfield not supported");

    if (condition == nullptr)
        LOG_ERROR("Condition required for AddSubfieldFieldRegex");

    std::string subfield_value;
    size_t start_pos, end_pos;
    if (not condition->matched(*record, &subfield_value, &start_pos, &end_pos))
        return false;

    bool modified_at_least_one(false);
    for (auto &field : *record) {
        if (field.getTag() == tag) {
            MARC::Subfields subfields(field.getSubfields());
            const std::vector<std::string> subfield_values_to_match(subfields.extractSubfields(condition->getSubfieldCode()));
            if (std::none_of(subfield_values_to_match.cbegin(), subfield_values_to_match.cend(),
                             [condition](const std::string &subfield_value) { return condition->getMatcher().matched(subfield_value); }))
                continue;
            bool global(HasGlobalFlag(replacement_regex));
            std::vector<std::string> pattern_and_replacement(
                GetSplitReplacementRegexWithoutGlobalFlagAndEscapedBackrefs(replacement_regex));
            if (pattern_and_replacement.size() != 2)
                LOG_ERROR("Invalid replacement pattern :\"" + replacement_regex + "\"\nMust follow /pattern/replacement/g? scheme");
            ThreadSafeRegexMatcher replace_matcher(pattern_and_replacement[0]);
            const std::string insertion_text(replace_matcher.replaceWithBackreferences(subfield_value, pattern_and_replacement[1], global));
            subfields.addSubfield(subfield_code, insertion_text);
            field.setSubfields(subfields);
            modified_at_least_one = true;
        }
    }

    return modified_at_least_one;
}


void Augment(std::vector<AugmentorDescriptor> &augmentors, MARC::Reader * const marc_reader, MARC::Writer * const marc_writer) {
    unsigned total_count(0), modified_count(0);
    std::string error_message;
    while (MARC::Record record = marc_reader->read()) {
        ++total_count;
        bool modified_record(false);
        for (auto &augmentor : augmentors) {
            if (augmentor.getAugmentorType() == AugmentorType::INSERT_FIELD) {
                if (InsertField(&record, augmentor.getTag(), augmentor.getSubfieldCode(), augmentor.getInsertionText(), &error_message))
                    modified_record = true;
                else if (not error_message.empty())
                    LOG_WARNING(error_message);
            } else if (augmentor.getAugmentorType() == AugmentorType::INSERT_FIELD_IF) {
                if (InsertField(&record, augmentor.getTag(), augmentor.getSubfieldCode(), augmentor.getInsertionText(), &error_message,
                                augmentor.getCompiledPattern()))
                    modified_record = true;
                else if (not error_message.empty())
                    LOG_WARNING(error_message);
            } else if (augmentor.getAugmentorType() == AugmentorType::REPLACE_FIELD) {
                if (ReplaceField(&record, augmentor.getTag(), augmentor.getSubfieldCode(), augmentor.getInsertionText()))
                    modified_record = true;
            } else if (augmentor.getAugmentorType() == AugmentorType::REPLACE_FIELD_IF) {
                if (ReplaceField(&record, augmentor.getTag(), augmentor.getSubfieldCode(), augmentor.getInsertionText(),
                                 augmentor.getCompiledPattern()))
                    modified_record = true;
            } else if (augmentor.getAugmentorType() == AugmentorType::REPLACE_SUBFIELD_IF_REGEX) {
                if (ReplaceSubfieldRegex(&record, augmentor.getTag(), augmentor.getSubfieldCode(), augmentor.getReplaceRegex(),
                                         augmentor.getCompiledPattern()))
                    modified_record = true;
            } else if (augmentor.getAugmentorType() == AugmentorType::ADD_SUBFIELD) {
                if (AddSubfield(&record, augmentor.getTag(), augmentor.getSubfieldCode(), augmentor.getInsertionText()))
                    modified_record = true;
            } else if (augmentor.getAugmentorType() == AugmentorType::ADD_SUBFIELD_IF) {
                if (AddSubfield(&record, augmentor.getTag(), augmentor.getSubfieldCode(), augmentor.getInsertionText(),
                                augmentor.getCompiledPattern()))
                    modified_record = true;
            } else if (augmentor.getAugmentorType() == AugmentorType::INSERT_FIELD_IF_REGEX) {
                if (InsertFieldRegex(&record, augmentor.getTag(), augmentor.getSubfieldCode(), augmentor.getReplaceRegex(), &error_message,
                                     augmentor.getCompiledPattern()))
                    modified_record = true;
            } else if (augmentor.getAugmentorType() == AugmentorType::ADD_SUBFIELD_IF_MATCHING) {
                if (AddSubfield(&record, augmentor.getTag(), augmentor.getSubfieldCode(), augmentor.getInsertionText(),
                                augmentor.getCompiledPattern(), true /* only matching fields */))
                    modified_record = true;
            } else if (augmentor.getAugmentorType() == AugmentorType::ADD_SUBFIELD_IF_REGEX) {
                if (AddSubfieldRegex(&record, augmentor.getTag(), augmentor.getSubfieldCode(), augmentor.getReplaceRegex(),
                                     augmentor.getCompiledPattern()))
                    modified_record = true;
            } else
                LOG_ERROR("unhandled Augmentor type!");
        }

        if (modified_record)
            ++modified_count;
        marc_writer->write(record);
    }

    std::cerr << "Processed a total of " << total_count << " record(s).\n";
    std::cerr << "Modified " << modified_count << " record(s).\n";
}


void ExtractCommandArgs(char ***argvp, MARC::Tag * const tag, char * const subfield_code, std::string * const field_or_subfield_contents) {
    const std::string command(**argvp);
    ++*argvp;

    const std::string tag_and_optional_subfield_code(**argvp);
    const auto first_colon_pos(tag_and_optional_subfield_code.find(':'));
    if (first_colon_pos != MARC::Record::TAG_LENGTH and first_colon_pos != MARC::Record::TAG_LENGTH + 1)
        LOG_ERROR("invalid tag and optional subfield code after \"" + command + "\": \"" + tag_and_optional_subfield_code + "\"!");
    *tag = MARC::Tag(tag_and_optional_subfield_code.substr(0, MARC::Record::TAG_LENGTH));
    *subfield_code = (first_colon_pos > MARC::Record::TAG_LENGTH) ? tag_and_optional_subfield_code[MARC::Record::TAG_LENGTH]
                                                                  : CompiledPattern::NO_SUBFIELD_CODE;
    *field_or_subfield_contents = tag_and_optional_subfield_code.substr(first_colon_pos + 1);
    if (field_or_subfield_contents->empty())
        LOG_ERROR("text after colon for \"" + command + "\" must not be empty!");
    ++*argvp;
}


void ExtractCommandArgs(char ***argvp, MARC::Tag * const tag, char * const subfield_code, CompiledPattern **compiled_pattern,
                        std::string * const field_or_subfield_contents) {
    const std::string command(**argvp);
    ++*argvp;

    const std::string tag_and_optional_subfield_code(**argvp);
    auto first_colon_pos(tag_and_optional_subfield_code.find(':'));
    if (first_colon_pos != MARC::Record::TAG_LENGTH and first_colon_pos != MARC::Record::TAG_LENGTH + 1)
        LOG_ERROR("invalid tag and optional subfield code after \"" + command + "\"!");
    *tag = MARC::Tag(tag_and_optional_subfield_code.substr(0, MARC::Record::TAG_LENGTH));
    *subfield_code = (tag_and_optional_subfield_code.length() > MARC::Record::TAG_LENGTH)
                         ? tag_and_optional_subfield_code[MARC::Record::TAG_LENGTH]
                         : CompiledPattern::NO_SUBFIELD_CODE;

    *field_or_subfield_contents = tag_and_optional_subfield_code.substr(first_colon_pos + 1);
    if (field_or_subfield_contents->empty())
        LOG_ERROR("text after colon for \"" + command + "\" must not be empty!");
    ++*argvp;

    const std::string tag_optional_subfield_code_and_regex(**argvp);
    first_colon_pos = tag_optional_subfield_code_and_regex.find(':');
    if (first_colon_pos != MARC::Record::TAG_LENGTH and first_colon_pos != MARC::Record::TAG_LENGTH + 1)
        LOG_ERROR("invalid tag and optional subfield code after \"" + command + "\"!");
    const std::string match_tag(tag_optional_subfield_code_and_regex.substr(0, MARC::Record::TAG_LENGTH));
    const char match_subfield_code((first_colon_pos == MARC::Record::TAG_LENGTH)
                                       ? CompiledPattern::NO_SUBFIELD_CODE
                                       : tag_optional_subfield_code_and_regex[MARC::Record::TAG_LENGTH]);
    const std::string regex_string(tag_optional_subfield_code_and_regex.substr(first_colon_pos + 1));
    std::string err_msg;
    RegexMatcher * const new_matcher(RegexMatcher::RegexMatcherFactory(regex_string, &err_msg));
    if (new_matcher == nullptr)
        LOG_ERROR("failed to compile regular expression: \"" + regex_string + "\" for \"" + command + "\"! (" + err_msg + ")");
    *compiled_pattern = new CompiledPattern(match_tag, match_subfield_code, std::move(*new_matcher));
    ++*argvp;
}


void ProcessAugmentorArgs(char **argv, std::vector<AugmentorDescriptor> * const augmentors) {
    MARC::Tag tag, tag2;
    char subfield_code;
    CompiledPattern *compiled_pattern;
    std::string field_or_subfield_contents, map_filename;

    while (*argv != nullptr) {
        if (std::strcmp(*argv, "--insert-field") == 0) {
            ExtractCommandArgs(&argv, &tag, &subfield_code, &field_or_subfield_contents);
            augmentors->emplace_back(AugmentorDescriptor::MakeInsertFieldAugmentor(tag, subfield_code, field_or_subfield_contents));
        } else if (std::strcmp(*argv, "--replace-field") == 0) {
            ExtractCommandArgs(&argv, &tag, &subfield_code, &field_or_subfield_contents);
            augmentors->emplace_back(AugmentorDescriptor::MakeReplaceFieldAugmentor(tag, subfield_code, field_or_subfield_contents));
        } else if (std::strcmp(*argv, "--add-subfield") == 0) {
            ExtractCommandArgs(&argv, &tag, &subfield_code, &field_or_subfield_contents);
            if (subfield_code == CompiledPattern::NO_SUBFIELD_CODE)
                LOG_ERROR("missing subfield code for --add-subfield operation!");
            augmentors->emplace_back(AugmentorDescriptor::MakeAddSubfieldAugmentor(tag, subfield_code, field_or_subfield_contents));
        } else if (std::strcmp(*argv, "--insert-field-if") == 0) {
            ExtractCommandArgs(&argv, &tag, &subfield_code, &compiled_pattern, &field_or_subfield_contents);
            augmentors->emplace_back(
                AugmentorDescriptor::MakeInsertFieldIfAugmentor(tag, subfield_code, compiled_pattern, field_or_subfield_contents));
        } else if (std::strcmp(*argv, "--insert-field-if-regex") == 0) {
            ExtractCommandArgs(&argv, &tag, &subfield_code, &compiled_pattern, &field_or_subfield_contents);
            augmentors->emplace_back(
                AugmentorDescriptor::MakeInsertFieldIfRegexAugmentor(tag, subfield_code, compiled_pattern, field_or_subfield_contents));

        } else if (std::strcmp(*argv, "--replace-field-if") == 0) {
            ExtractCommandArgs(&argv, &tag, &subfield_code, &compiled_pattern, &field_or_subfield_contents);
            augmentors->emplace_back(
                AugmentorDescriptor::MakeReplaceFieldIfAugmentor(tag, subfield_code, compiled_pattern, field_or_subfield_contents));
        } else if (std::strcmp(*argv, "--replace-subfield-if-regex") == 0) {
            ExtractCommandArgs(&argv, &tag, &subfield_code, &compiled_pattern, &field_or_subfield_contents);
            augmentors->emplace_back(
                AugmentorDescriptor::MakeReplaceFieldIfRegexAugmentor(tag, subfield_code, compiled_pattern, field_or_subfield_contents));
        } else if (std::strcmp(*argv, "--add-subfield-if") == 0) {
            ExtractCommandArgs(&argv, &tag, &subfield_code, &compiled_pattern, &field_or_subfield_contents);
            if (subfield_code == CompiledPattern::NO_SUBFIELD_CODE)
                LOG_ERROR("missing subfield code for --add-subfield-if operation!");
            augmentors->emplace_back(
                AugmentorDescriptor::MakeAddSubfieldIfAugmentor(tag, subfield_code, compiled_pattern, field_or_subfield_contents));
        } else if (std::strcmp(*argv, "--add-subfield-if-matching") == 0) {
            ExtractCommandArgs(&argv, &tag, &subfield_code, &compiled_pattern, &field_or_subfield_contents);
            if (subfield_code == CompiledPattern::NO_SUBFIELD_CODE)
                LOG_ERROR("missing subfield code for --add-subfield-if operation!");
            augmentors->emplace_back(
                AugmentorDescriptor::MakeAddSubfieldIfMatchingAugmentor(tag, subfield_code, compiled_pattern, field_or_subfield_contents));
        } else if (std::strcmp(*argv, "--add-subfield-if-regex") == 0) {
            ExtractCommandArgs(&argv, &tag, &subfield_code, &compiled_pattern, &field_or_subfield_contents);
            if (subfield_code == CompiledPattern::NO_SUBFIELD_CODE)
                LOG_ERROR("missing subfield code for --add-subfield-if-regex operation!");
            augmentors->emplace_back(
                AugmentorDescriptor::MakeAddSubfieldIfRegexAugmentor(tag, subfield_code, compiled_pattern, field_or_subfield_contents));
        } else
            LOG_ERROR("unknown operation type \"" + std::string(*argv) + "\"!");
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


int Main(int argc, char **argv) {
    ++argv;

    if (argc < 4)
        Usage();

    const std::string input_filename(*argv++);
    const std::string output_filename(*argv++);
    auto marc_reader(MARC::Reader::Factory(input_filename));
    auto marc_writer(MARC::Writer::Factory(output_filename));

    std::vector<AugmentorDescriptor> augmentors;
    if (*(argv + 1) != nullptr and std::strcmp(*(argv + 1), "--config-path") == 0) {
        argv += 2;
        if (*argv == nullptr)
            LOG_ERROR("missing config filename after \"--config-path\"!");
        const std::string config_filename(*argv);
        ++argv;
        if (*argv != nullptr)
            LOG_ERROR("unexpected argument after config filename \"" + std::string(*argv) + "\"!");
        char **file_argv;
        MakeArgumentListFromFile(config_filename, &file_argv);
        ProcessAugmentorArgs(file_argv, &augmentors);
    } else
        ProcessAugmentorArgs(argv, &augmentors);

    Augment(augmentors, marc_reader.get(), marc_writer.get());

    return EXIT_SUCCESS;
}

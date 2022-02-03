/** \brief A MARC-21 filter utility that can remove records or fields based on patterns for MARC subfields.
 *  \author Dr. Johannes Ruscheinski (johannes.ruscheinski@uni-tuebingen.de)
 *
 *  \copyright 2016-2021 Universitätsbibliothek Tübingen.  All rights reserved.
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
    std::cerr << "Usage: " << ::progname << " marc_input marc_output op1 [op2 .. opN]\n"
              << "       where each operation must start with the operation type. Operation-type flags are\n"
              << "           --drop field_or_subfield_specs\n"
              << "               where field_or_subfield_specs is a list of one or more arguments where a field or\n"
              << "               or subfield specifier is followed by a colon and a PCRE regex.  Examples would be\n"
              << "               007:x[ab] or 856u:http:.* etc.\n"
              << "               Any record where a field or subfield matched will be dropped entirely.\n"
              << "           --keep field_or_subfield_specs\n"
              << "               where field_or_subfield_specs is the same as for the --drop operation.\n"
              << "               Only records that have at least one field or subfield that matched will be kept.\n"
              << "           --drop-biblio-level characters\n"
              << "               Drop any records that have a bibliographic level matching any of the specified\n"
              << "               characters.  (Comparison against leader position 07.)\n"
              << "           --keep-biblio-level characters\n"
              << "               Keep only records that have a bibliographic level matching any of the specified\n"
              << "               characters.  (Comparison against leader position 07.)\n"
              << "           --remove-fields field_or_subfield_specs\n"
              << "               Any fields that matched or that have subfields that matched will be dropped.\n"
              << "           --remove-subfields field_or_subfield_specs\n"
              << "               Any subfields that matched will be dropped.\n"
              << "           --filter-chars subfield_specs characters_to_delete\n"
              << "               Drops any characters in characters_to_delete from matching subfields.\n"
              << "           --max-count count\n"
              << "               Quit after we had count records that matched any one of our conditions.\n"
              << "           --translate subfield_specs character_set1 character_set2\n"
              << "               Inspired by tr, this is used to specify a mapping from one set of Unicode\n"
              << "               characters to another.  character_set1 and character_set2 must both must be either\n"
              << "               explicit and equally long lists of individual characters or the sequences [:upper:] or\n"
              << "               [:lower:] where currently [:upper:] and [:lower:] may only be mapped to each other.\n"
              << "           --replace subfield_specs pcre_regex replacement_string\n"
              << "               replacement_string may contain back references like \\3 etc.\n"
              << "             or\n"
              << "           --replace subfield_specs map_file\n"
              << "               every line in \"map_file\" must either start with a hash character in which case it is\n"
              << "               ignored or lines that look like \"regex->replacement\" followed by a newline.\n"
              << "           --replace-strings subfield_specs map_file\n"
              << "               every line in \"map_file\" must either start with a hash character in which case it is\n"
              << "               ignored or lines that look like \"string1|string2|...|stringN->replacement\" followed by a newline.\n"
              << "           --globally-substitute subfield_specs pcre_regex replacement_string\n"
              << "               replacement_string may contain back references like \\3 etc.\n"
              << "               Unlike --replace only the matched parts will be replaced.  This works like se s/.../.../g.\n"
              << "             or\n"
              << "       --filter-chars and --translate character sets may contain any of the following escapes:\n"
              << "         \\n, \\t, \\b, \\r, \\f, \\v, \\a, \\\\, \\uNNNN and \\UNNNNNNNN\n"
              << "       If you don't specify an output format it will be the same as the input format.\n\n";

    std::exit(EXIT_FAILURE);
}


class CompiledPattern {
    std::string tag_;
    char subfield_code_;
    RegexMatcher matcher_;

public:
    static const char NO_SUBFIELD_CODE;

public:
    CompiledPattern(const std::string &tag, const char subfield_code, RegexMatcher &matcher)
        : tag_(tag), subfield_code_(subfield_code), matcher_(matcher) { }
    const std::string &getTag() const { return tag_; }
    bool hasSubfieldCode() const { return subfield_code_ != NO_SUBFIELD_CODE; }
    char getSubfieldCode() const { return subfield_code_; }
    bool fieldMatched(const std::string &field_contents);
    bool subfieldMatched(const std::string &subfield_contents);
};


const char CompiledPattern::NO_SUBFIELD_CODE('\0');


bool CompiledPattern::fieldMatched(const std::string &field_contents) {
    std::string err_msg;
    const bool retval = matcher_.matched(field_contents, &err_msg);
    if (not retval and not err_msg.empty())
        LOG_ERROR("Unexpected error while trying to match a field in CompiledPattern::fieldMatched(): " + err_msg);

    return retval;
}


bool CompiledPattern::subfieldMatched(const std::string &subfield_contents) {
    std::string err_msg;
    const bool retval = matcher_.matched(subfield_contents, &err_msg);
    if (not retval and not err_msg.empty())
        LOG_ERROR("Unexpected error while trying to match a subfield in CompiledPattern::subfieldMatched(): " + err_msg);

    return retval;
}


// Expects "patterns" to contain strings that look like TTTS:REGEX where TTT are 3 characters specifying a field,
// S is a subfield code and REGEX is a PCRE-style regex supporting UTF8 that should match subfield contents.
// Alteratively a pattern can look like TTT:REGEX where TTT is a tag and we have no subfield code.
bool CompilePatterns(const std::vector<std::string> &patterns, std::vector<CompiledPattern *> * const compiled_patterns,
                     std::string * const err_msg) {
    compiled_patterns->clear();
    compiled_patterns->reserve(patterns.size());

    for (const auto &pattern : patterns) {
        std::string tag;
        char subfield_code;
        std::string::size_type first_colon_pos = pattern.find(':');
        if (first_colon_pos == std::string::npos) {
            *err_msg = "missing colon!";
            return false;
        } else if (first_colon_pos == MARC::Record::TAG_LENGTH) {
            tag = pattern.substr(0, 3);
            subfield_code = CompiledPattern::NO_SUBFIELD_CODE;
        } else if (first_colon_pos == MARC::Record::TAG_LENGTH + 1) {
            tag = pattern.substr(0, 3);
            subfield_code = pattern[3];
        } else {
            *err_msg = "colon in wrong position (" + std::to_string(first_colon_pos) + ")! (Tag length must be "
                       + std::to_string(MARC::Record::TAG_LENGTH) + ".)";
            return false;
        }

        const std::string regex_string(pattern.substr(first_colon_pos + 1));
        RegexMatcher * const new_matcher(RegexMatcher::RegexMatcherFactory(regex_string, err_msg));
        if (new_matcher == nullptr) {
            *err_msg = "failed to compile regular expression: \"" + regex_string + "\"! (" + *err_msg + ")";
            return false;
        }

        compiled_patterns->push_back(new CompiledPattern(tag, subfield_code, *new_matcher));
        delete new_matcher;
    }

    return true;
}


/** Returns true if we have at least one match. */
bool Matched(const MARC::Record &record, const std::vector<CompiledPattern *> &compiled_patterns,
             std::vector<size_t> * const matched_field_indices) {
    matched_field_indices->clear();

    for (const auto &compiled_pattern : compiled_patterns) {
        const MARC::Record::ConstantRange range(record.getTagRange(compiled_pattern->getTag()));
        for (auto field(range.begin()); field != range.end(); ++field) {
            if (compiled_pattern->hasSubfieldCode()) {
                const MARC::Subfields subfields(field->getSubfields());
                for (const auto &subfield : subfields) {
                    if (subfield.code_ == compiled_pattern->getSubfieldCode() and compiled_pattern->subfieldMatched(subfield.value_)) {
                        matched_field_indices->emplace_back(field - record.begin());
                        break;
                    }
                }
            } else if (compiled_pattern->fieldMatched(field->getContents()))
                matched_field_indices->emplace_back(field - record.begin());
        }
    }

    return not matched_field_indices->empty();
}


bool MatchedSubfield(const MARC::Record &record, const std::vector<CompiledPattern *> &compiled_patterns,
                     std::vector<std::pair<size_t, char>> * const matched_field_indices_and_subfields) {
    matched_field_indices_and_subfields->clear();

    for (const auto &compiled_pattern : compiled_patterns) {
        if (compiled_pattern->hasSubfieldCode()) {
            const MARC::Record::ConstantRange field_range(record.getTagRange(compiled_pattern->getTag()));
            for (auto field(field_range.begin()); field != field_range.end(); ++field) {
                const MARC::Subfields subfields(field->getSubfields());
                for (const auto &subfield : subfields) {
                    if (subfield.code_ == compiled_pattern->getSubfieldCode() and compiled_pattern->subfieldMatched(subfield.value_))
                        matched_field_indices_and_subfields->emplace_back(field - record.begin(), subfield.code_);
                }
            }
        }
    }

    return not matched_field_indices_and_subfields->empty();
}


enum class OutputFormat { MARC_XML, MARC_21, SAME_AS_INPUT };
enum class FilterType {
    KEEP,
    DROP,
    KEEP_BIBLIOGRAPHIC_LEVEL,
    DROP_BIBLIOGRAPHIC_LEVEL,
    REMOVE_FIELDS,
    REMOVE_SUBFIELDS,
    FILTER_CHARS,
    MAX_COUNT,
    TRANSLATE,
    REPLACE,
    MAP_STRING_TO_STRING,
    GLOBAL_SUBSTITUTION
};


class TranslateMap {
public:
    virtual ~TranslateMap() { }

    virtual TranslateMap *clone() const = 0;

    /** \return True if "*s" was changed, else false. */
    virtual bool map(std::string * const s) const = 0;
};


class CharSetTranslateMap : public TranslateMap {
    std::wstring set1_, set2_;

public:
    CharSetTranslateMap(const std::string &set1, const std::string &set2);
    virtual bool map(std::string * const s) const final;
    virtual CharSetTranslateMap *clone() const final { return new CharSetTranslateMap(set1_, set2_); }

private:
    CharSetTranslateMap(const std::wstring &set1, const std::wstring &set2): set1_(set1), set2_(set2) { }
};


CharSetTranslateMap::CharSetTranslateMap(const std::string &set1, const std::string &set2) {
    if (unlikely(not TextUtil::UTF8ToWCharString(set1, &set1_)))
        LOG_ERROR("set1 \"" + set1 + "\" is not a valid UTF-8 string!");
    if (unlikely(not TextUtil::UTF8ToWCharString(set2, &set2_)))
        LOG_ERROR("set2 \"" + set2 + "\" is not a valid UTF-8 string!");
    if (set1_.size() != set2_.size())
        LOG_ERROR("input sets must contain an equal number of Unicode characters!");
}


bool CharSetTranslateMap::map(std::string * const s) const {
    std::wstring ws;
    if (unlikely(not TextUtil::UTF8ToWCharString(*s, &ws)))
        LOG_ERROR("input \"" + *s + "\" is not a valid UTF-8 string!");

    bool changed(false);
    for (auto wch(ws.begin()); wch != ws.end(); ++wch) {
        const auto pos(set1_.find(*wch));
        if (pos != std::wstring::npos) {
            *wch = set2_[pos];
            changed = true;
        }
    }

    if (unlikely(not TextUtil::WCharToUTF8String(ws, s)))
        LOG_ERROR("TextUtil::WCharToUTF8String() failed! (This should *never* happen!)");

    return changed;
}


class UpperLowerTranslateMap : public TranslateMap {
public:
    enum MapType { UPPER_TO_LOWER, LOWER_TO_UPPER };

private:
    MapType map_type_;

public:
    UpperLowerTranslateMap(const MapType map_type): map_type_(map_type) { }
    virtual bool map(std::string * const s) const final;
    virtual UpperLowerTranslateMap *clone() const final { return new UpperLowerTranslateMap(map_type_); }
};


bool UpperLowerTranslateMap::map(std::string * const s) const {
    std::wstring ws;
    if (unlikely(not TextUtil::UTF8ToWCharString(*s, &ws)))
        LOG_ERROR("input \"" + *s + "\" is not a valid UTF-8 string!");

    bool changed(false);
    if (map_type_ == UPPER_TO_LOWER) {
        for (auto wch(ws.begin()); wch != ws.end(); ++wch) {
            if (std::iswupper(static_cast<wint_t>(*wch))) {
                *wch = std::towlower(*wch);
                changed = true;
            }
        }
    } else { // map_type_ == LOWER_TO_UPPER.
        for (auto wch(ws.begin()); wch != ws.end(); ++wch) {
            if (std::iswlower(static_cast<wint_t>(*wch))) {
                *wch = std::towupper(*wch);
                changed = true;
            }
        }
    }

    if (unlikely(not TextUtil::WCharToUTF8String(ws, s)))
        LOG_ERROR("TextUtil::WCharToUTF8String() failed! (This should *never* happen!)");

    return changed;
}


struct StringFragmentOrBackreference {
    enum Type { STRING_FRAGMENT, BACK_REFERENCE };
    Type type_;
    std::string string_fragment_;
    unsigned back_reference_;

public:
    explicit StringFragmentOrBackreference(const std::string &string_fragment)
        : type_(STRING_FRAGMENT), string_fragment_(string_fragment) { }
    explicit StringFragmentOrBackreference(const unsigned back_reference): type_(BACK_REFERENCE), back_reference_(back_reference) { }
};


class FilterDescriptor {
private:
    FilterType filter_type_;
    std::vector<CompiledPattern *> compiled_patterns_;
    std::vector<std::string> subfield_specs_;
    std::string chars_to_delete_;
    std::string biblio_levels_;
    mutable unsigned count_;
    unsigned max_count_;
    TranslateMap *translate_map_;
    RegexMatcher *regex_matcher_;
    std::unordered_map<std::string, std::string> originals_to_replacements_map_;
    std::vector<StringFragmentOrBackreference> string_fragments_and_back_references_;
    std::string replacement_;

public:
    inline FilterType getFilterType() const { return filter_type_; }
    inline const std::string &getBiblioLevels() const { return biblio_levels_; }
    bool skipRecordDueToExceededRecordCount() const {
        ++count_;
        return count_ > max_count_;
    }

    /** \note Only call this if the filter type is not FILTER_CHARS! */
    inline const std::vector<CompiledPattern *> getCompiledPatterns() const { return compiled_patterns_; }

    /** \note Only call this if the filter type is FILTER_CHARS! */
    inline const std::vector<std::string> &getSubfieldSpecs() const { return subfield_specs_; }

    /** \note Only call this if the filter type is FILTER_CHARS! */
    inline const std::string &getCharsToDelete() const { return chars_to_delete_; }

    /** \note Only call this if the filter type is TRANSLATE! */
    inline const TranslateMap &getTranslateMap() const { return *translate_map_; }

    /** \note Only call this if the filter type is REPLACE! */
    inline RegexMatcher &getRegexMatcher() const { return *regex_matcher_; }

    /** \note Only call this if the filter type is MAP_S! */
    inline const std::unordered_map<std::string, std::string> &getOriginalsToReplacementsMap() const {
        return originals_to_replacements_map_;
    }

    /** \note Only call this if the filter type is REPLACE! */
    inline const std::vector<StringFragmentOrBackreference> &getStringFragmentsAndBackreferences() const {
        return string_fragments_and_back_references_;
    }

    inline const std::string &getReplacement() const { return replacement_; }

    inline static FilterDescriptor MakeDropFilter(const std::vector<CompiledPattern *> &compiled_patterns) {
        return FilterDescriptor(FilterType::DROP, compiled_patterns);
    }

    inline static FilterDescriptor MakeKeepFilter(const std::vector<CompiledPattern *> &compiled_patterns) {
        return FilterDescriptor(FilterType::KEEP, compiled_patterns);
    }

    inline static FilterDescriptor MakeDropBiblioLevelFilter(const std::string &biblio_levels) {
        return FilterDescriptor(FilterType::DROP_BIBLIOGRAPHIC_LEVEL, biblio_levels);
    }

    inline static FilterDescriptor MakeKeepBiblioLevelFilter(const std::string &biblio_levels) {
        return FilterDescriptor(FilterType::KEEP_BIBLIOGRAPHIC_LEVEL, biblio_levels);
    }

    inline static FilterDescriptor MakeRemoveFieldsFilter(const std::vector<CompiledPattern *> &compiled_patterns) {
        return FilterDescriptor(FilterType::REMOVE_FIELDS, compiled_patterns);
    }

    inline static FilterDescriptor MakeRemoveSubfieldsFilter(const std::vector<CompiledPattern *> &compiled_patterns) {
        return FilterDescriptor(FilterType::REMOVE_SUBFIELDS, compiled_patterns);
    }

    inline static FilterDescriptor MakeFilterCharsFilter(const std::vector<std::string> &subfield_specs,
                                                         const std::string &chars_to_delete) {
        return FilterDescriptor(subfield_specs, chars_to_delete);
    }

    inline static FilterDescriptor MakeMaxCountFilter(const unsigned max_count) { return FilterDescriptor(max_count); }

    inline static FilterDescriptor MakeTranslateFilter(const std::vector<std::string> &subfield_specs, const TranslateMap &translate_map) {
        return FilterDescriptor(subfield_specs, translate_map);
    }

    inline static FilterDescriptor MakeReplacementFilter(const std::vector<std::string> &subfield_specs, const std::string &regex,
                                                         const std::string &replacement) {
        return FilterDescriptor(FilterType::REPLACE, subfield_specs, regex, replacement);
    }

    inline static FilterDescriptor MakeStringReplacementFilter(
        const std::vector<std::string> &subfield_specs, const std::unordered_map<std::string, std::string> &originals_to_replacements_map) {
        return FilterDescriptor(subfield_specs, originals_to_replacements_map);
    }

    inline static FilterDescriptor MakeGlobalSubstitutionFilter(const std::vector<std::string> &subfield_specs, const std::string &regex,
                                                                const std::string &replacement) {
        return FilterDescriptor(FilterType::GLOBAL_SUBSTITUTION, subfield_specs, regex, replacement);
    }

private:
    FilterDescriptor(const FilterType filter_type, const std::vector<CompiledPattern *> &compiled_patterns)
        : filter_type_(filter_type), compiled_patterns_(compiled_patterns), translate_map_(nullptr), regex_matcher_(nullptr) { }
    FilterDescriptor(const std::vector<std::string> &subfield_specs, const std::string &chars_to_delete)
        : filter_type_(FilterType::FILTER_CHARS), subfield_specs_(subfield_specs), chars_to_delete_(chars_to_delete),
          translate_map_(nullptr), regex_matcher_(nullptr) { }
    FilterDescriptor(const FilterType filter_type, const std::string &biblio_levels)
        : filter_type_(filter_type), biblio_levels_(biblio_levels), regex_matcher_(nullptr) { }
    FilterDescriptor(const unsigned max_count)
        : filter_type_(FilterType::MAX_COUNT), count_(0), max_count_(max_count), translate_map_(nullptr), regex_matcher_(nullptr) { }
    FilterDescriptor(const std::vector<std::string> &subfield_specs, const TranslateMap &translate_map)
        : filter_type_(FilterType::TRANSLATE), subfield_specs_(subfield_specs), translate_map_(translate_map.clone()),
          regex_matcher_(nullptr) { }
    FilterDescriptor(const FilterType filter_type, const std::vector<std::string> &subfield_specs, const std::string &regex,
                     const std::string &replacement);
    FilterDescriptor(const std::vector<std::string> &subfield_specs,
                     const std::unordered_map<std::string, std::string> &originals_to_replacements_map)
        : filter_type_(FilterType::MAP_STRING_TO_STRING), subfield_specs_(subfield_specs),
          originals_to_replacements_map_(originals_to_replacements_map) { }
};


void ParseReplacementString(const std::string &replacement,
                            std::vector<StringFragmentOrBackreference> * const string_fragments_and_back_references) {
    if (unlikely(replacement.empty())) {
        string_fragments_and_back_references->emplace_back("");
        return;
    }

    std::string string_fragment;
    bool backslash_seen(false);
    for (const char ch : replacement) {
        if (backslash_seen) {
            if (StringUtil::IsDigit(ch)) {
                if (not string_fragment.empty()) {
                    string_fragments_and_back_references->emplace_back(string_fragment);
                    string_fragment.clear();
                }
                const unsigned digit(ch - '0');
                string_fragments_and_back_references->emplace_back(digit);
            } else
                string_fragment += ch;
            backslash_seen = false;
        } else if (ch == '\\')
            backslash_seen = true;
        else
            string_fragment += ch;
    }

    if (not string_fragment.empty())
        string_fragments_and_back_references->emplace_back(string_fragment);
    if (backslash_seen)
        LOG_ERROR("replacement string for --replace ends in a backslash!");
}


FilterDescriptor::FilterDescriptor(const FilterType filter_type, const std::vector<std::string> &subfield_specs, const std::string &regex,
                                   const std::string &replacement)
    : filter_type_(filter_type), subfield_specs_(subfield_specs), translate_map_(nullptr) {
    if (filter_type != FilterType::REPLACE and filter_type != FilterType::GLOBAL_SUBSTITUTION)
        LOG_ERROR("filter_type must be either REPLACEMENT or GLOBAL_SUBSTITUTION!");

    std::string err_msg;
    if ((regex_matcher_ = RegexMatcher::RegexMatcherFactory(regex, &err_msg)) == nullptr)
        LOG_ERROR("failed to compile regex \"" + regex + "\"! (" + err_msg + ")");
    if (filter_type == FilterType::REPLACE)
        ParseReplacementString(replacement, &string_fragments_and_back_references_);
    else if (filter_type == FilterType::GLOBAL_SUBSTITUTION)
        replacement_ = replacement;
    else
        LOG_ERROR("unknown filter type " + std::to_string(static_cast<int>(filter_type)) + "!");
}


std::string GetSubfieldCodes(const MARC::Tag &tag, const std::vector<std::string> &subfield_specs) {
    std::string subfield_codes;

    for (const auto &subfield_spec : subfield_specs) {
        if (tag == subfield_spec.substr(0, MARC::Record::TAG_LENGTH))
            subfield_codes += subfield_spec[MARC::Record::TAG_LENGTH];
    }

    return subfield_codes;
}


/** \brief Removes any of the characters in "chars_to_delete" from all of the subfields specified by "subfield_specs".
 *  \return True if at least one subfield has been modified, else false.
 */
bool FilterCharacters(const std::vector<std::string> &subfield_specs, const std::string &chars_to_delete, MARC::Record * const record) {
    bool modified_at_least_one_field(false);
    for (auto &field : *record) {
        const std::string subfield_codes(GetSubfieldCodes(field.getTag(), subfield_specs));
        if (subfield_codes.empty())
            continue;

        bool modified_at_least_one_subfield(false);
        MARC::Subfields subfields(field.getSubfields());
        for (auto &subfield : subfields) {
            if (subfield_codes.find(subfield.code_) != std::string::npos) {
                const auto old_length(subfield.value_.length());
                StringUtil::RemoveChars(chars_to_delete, &(subfield.value_));
                if (subfield.value_.length() != old_length)
                    modified_at_least_one_subfield = true;
            }
        }

        if (modified_at_least_one_subfield) {
            modified_at_least_one_field = true;
            field.setContents(std::string(1, field.getIndicator1()) + std::string(1, field.getIndicator2()) + subfields.toString());
        }
    }

    return modified_at_least_one_field;
}


/** \brief Maps contents of the subfields specified by "subfield_specs".
 *  \return True if at least one subfield has been modified, else false.
 */
bool TranslateCharacters(const std::vector<std::string> &subfield_specs, const TranslateMap &translate_map, MARC::Record * const record) {
    bool modified_at_least_one_field(false);
    for (auto &field : *record) {
        const std::string subfield_codes(GetSubfieldCodes(field.getTag(), subfield_specs));
        if (subfield_codes.empty())
            continue;

        bool modified_at_least_one_subfield(false);
        MARC::Subfields subfields(field.getSubfields());
        for (auto &subfield : subfields) {
            if (subfield_codes.find(subfield.code_) != std::string::npos and translate_map.map(&(subfield.value_)))
                modified_at_least_one_subfield = true;
        }

        if (modified_at_least_one_subfield) {
            modified_at_least_one_field = true;
            field.setContents(std::string(1, field.getIndicator1()) + std::string(1, field.getIndicator2()) + subfields.toString());
        }
    }

    return modified_at_least_one_field;
}


bool ReplaceSubfields(const std::vector<std::string> &subfield_specs, RegexMatcher &matcher,
                      const std::vector<StringFragmentOrBackreference> &string_fragments_and_back_references, MARC::Record * const record) {
    std::vector<size_t> indices_of_deleted_fields;
    std::set<std::string> tags_of_deleted_fields;
    bool modified_at_least_one_field(false);
    for (auto field(record->begin()); field != record->end(); ++field) {
        const std::string subfield_codes(GetSubfieldCodes(field->getTag(), subfield_specs));
        if (subfield_codes.empty())
            continue;

        bool modified_at_least_one_subfield(false);
        MARC::Subfields subfields(field->getSubfields());
        for (auto &subfield : subfields) {
            if (subfield_codes.find(subfield.code_) == std::string::npos)
                continue;

            if (matcher.matched(subfield.value_)) {
                const unsigned no_of_match_groups(matcher.getNoOfGroups());
                std::string replacement;
                for (const auto &string_fragment_or_back_reference : string_fragments_and_back_references) {
                    if (string_fragment_or_back_reference.type_ == StringFragmentOrBackreference::STRING_FRAGMENT)
                        replacement += string_fragment_or_back_reference.string_fragment_;
                    else { // We're dealing w/ a back-reference.
                        if (unlikely(string_fragment_or_back_reference.back_reference_ > no_of_match_groups))
                            LOG_ERROR("can't satisfy back-reference \\" + std::to_string(string_fragment_or_back_reference.back_reference_)
                                      + "!");
                        replacement += matcher[string_fragment_or_back_reference.back_reference_];
                    }
                }
                subfield.value_ = replacement;
                modified_at_least_one_subfield = true;
            }
        }

        if (modified_at_least_one_subfield) {
            modified_at_least_one_field = true;
            field->setContents(std::string(1, field->getIndicator1()) + std::string(1, field->getIndicator2()) + subfields.toString());
            if (unlikely(field->empty())) {
                indices_of_deleted_fields.emplace_back(field - record->begin());
                tags_of_deleted_fields.insert(field->getTag().toString());
            }
        }
    }

    // Did we generate completely empty fields?
    if (unlikely(not indices_of_deleted_fields.empty())) {
        LOG_WARNING("regex \"" + matcher.getPattern() + "\" led to empty fields in the record w/ control number "
                    + record->getControlNumber() + " and field(s) " + StringUtil::Join(tags_of_deleted_fields, ',') + "!");
        record->deleteFields(indices_of_deleted_fields);
    }

    return modified_at_least_one_field;
}


bool SubstituteWithinSubfields(const std::vector<std::string> &subfield_specs, RegexMatcher &matcher, const std::string &replacement,
                               MARC::Record * const record) {
    bool modified_at_least_one_field(false);
    for (auto &field : *record) {
        const std::string subfield_codes(GetSubfieldCodes(field.getTag(), subfield_specs));
        if (subfield_codes.empty())
            continue;

        bool modified_at_least_one_subfield(false);
        MARC::Subfields subfields(field.getSubfields());
        for (auto &subfield : subfields) {
            if (subfield_codes.find(subfield.code_) == std::string::npos)
                continue;

            auto new_field_contents(matcher.replaceWithBackreferences(subfield.value_, replacement, /* global = */ true));
            if (new_field_contents != subfield.value_) {
                subfield.value_.swap(new_field_contents);
                modified_at_least_one_subfield = true;
            }
        }

        if (modified_at_least_one_subfield) {
            modified_at_least_one_field = true;
            field.setContents(std::string(1, field.getIndicator1()) + std::string(1, field.getIndicator2()) + subfields.toString());
        }
    }

    return modified_at_least_one_field;
}


bool ReplaceStringsWithStrings(const std::vector<std::string> &subfield_specs,
                               const std::unordered_map<std::string, std::string> &originals_to_replacements_map,
                               MARC::Record * const record) {
    bool modified_at_least_one_field(false);
    for (auto &field : *record) {
        const std::string subfield_codes(GetSubfieldCodes(field.getTag(), subfield_specs));
        if (subfield_codes.empty())
            continue;

        bool modified_at_least_one_subfield(false);
        MARC::Subfields subfields(field.getSubfields());
        for (auto &subfield : subfields) {
            if (subfield_codes.find(subfield.code_) == std::string::npos)
                continue;

            const auto original_and_replacement(originals_to_replacements_map.find(subfield.value_));
            if (original_and_replacement != originals_to_replacements_map.cend()) {
                subfield.value_ = original_and_replacement->second;
                modified_at_least_one_subfield = true;
            }
        }

        if (modified_at_least_one_subfield) {
            modified_at_least_one_field = true;
            field.setContents(std::string(1, field.getIndicator1()) + std::string(1, field.getIndicator2()) + subfields.toString());
        }
    }

    return modified_at_least_one_field;
}


void Filter(const std::vector<FilterDescriptor> &filters, MARC::Reader * const marc_reader, MARC::Writer * const marc_writer) {
    unsigned total_count(0), deleted_count(0), modified_count(0);
    while (MARC::Record record = marc_reader->read()) {
        ++total_count;
        bool deleted_record(false), modified_record(false);
        for (const auto &filter : filters) {
            if (filter.getFilterType() == FilterType::MAX_COUNT) {
                if (filter.skipRecordDueToExceededRecordCount()) {
                    --total_count;
                    goto print_counts;
                }
            } else if (filter.getFilterType() == FilterType::FILTER_CHARS) {
                if (FilterCharacters(filter.getSubfieldSpecs(), filter.getCharsToDelete(), &record)) {
                    modified_record = true;
                    continue;
                }
            } else if (filter.getFilterType() == FilterType::DROP_BIBLIOGRAPHIC_LEVEL) {
                if (std::strchr(filter.getBiblioLevels().c_str(), MARC::Record::BibliographicLevelToChar(record.getBibliographicLevel()))
                    != nullptr) {
                    deleted_record = true;
                    continue;
                }
            } else if (filter.getFilterType() == FilterType::KEEP_BIBLIOGRAPHIC_LEVEL) {
                if (std::strchr(filter.getBiblioLevels().c_str(), MARC::Record::BibliographicLevelToChar(record.getBibliographicLevel()))
                    != nullptr) {
                    deleted_record = true;
                    continue;
                }
            } else if (filter.getFilterType() == FilterType::REMOVE_SUBFIELDS) {
                std::vector<std::pair<size_t, char>> matched_field_indices_and_subfields;
                if (MatchedSubfield(record, filter.getCompiledPatterns(), &matched_field_indices_and_subfields)) {
                    std::sort(matched_field_indices_and_subfields.begin(), matched_field_indices_and_subfields.end());
                    for (const auto &field_index_and_subfield : matched_field_indices_and_subfields) {
                        auto field(record.begin() + field_index_and_subfield.first);
                        field->deleteAllSubfieldsWithCode(field_index_and_subfield.second);
                    }
                    modified_record = true;
                    continue;
                }
            } else if (filter.getFilterType() == FilterType::TRANSLATE) {
                if (TranslateCharacters(filter.getSubfieldSpecs(), filter.getTranslateMap(), &record)) {
                    modified_record = true;
                    continue;
                }
            } else if (filter.getFilterType() == FilterType::REPLACE) {
                if (ReplaceSubfields(filter.getSubfieldSpecs(), filter.getRegexMatcher(), filter.getStringFragmentsAndBackreferences(),
                                     &record)) {
                    modified_record = true;
                    continue;
                }
            } else if (filter.getFilterType() == FilterType::GLOBAL_SUBSTITUTION) {
                if (SubstituteWithinSubfields(filter.getSubfieldSpecs(), filter.getRegexMatcher(), filter.getReplacement(), &record)) {
                    modified_record = true;
                    continue;
                }
            } else if (filter.getFilterType() == FilterType::MAP_STRING_TO_STRING) {
                if (ReplaceStringsWithStrings(filter.getSubfieldSpecs(), filter.getOriginalsToReplacementsMap(), &record)) {
                    modified_record = true;
                    continue;
                }
            } else {
                std::vector<size_t> matched_field_indices;
                if (Matched(record, filter.getCompiledPatterns(), &matched_field_indices)) {
                    if (filter.getFilterType() == FilterType::DROP) {
                        deleted_record = true;
                        continue;
                    } else if (filter.getFilterType() == FilterType::REMOVE_FIELDS) {
                        record.deleteFields(matched_field_indices);
                        modified_record = true;
                    }
                } else if (filter.getFilterType() == FilterType::KEEP)
                    continue;
            }
        }

        if (deleted_record)
            ++deleted_count;
        else {
            if (modified_record)
                ++modified_count;
            marc_writer->write(record);
        }
    }

print_counts:
    std::cerr << "Processed a total of " << total_count << " record(s).\n";
    std::cerr << "Kept " << (total_count - deleted_count) << " record(s).\n";
    std::cerr << "Modified " << modified_count << " record(s).\n";
    std::cerr << "Deleted " << deleted_count << " record(s).\n";
}


std::vector<CompiledPattern *> CollectAndCompilePatterns(char ***argvp) {
    const std::string operation_type(**argvp);
    ++*argvp;

    std::vector<std::string> specs_and_pattern;
    while (**argvp != nullptr and not StringUtil::StartsWith(**argvp, "--")) {
        specs_and_pattern.emplace_back(**argvp);
        ++*argvp;
    }

    if (specs_and_pattern.empty())
        LOG_ERROR("expected at least one field or subfield specification after \"" + operation_type + "\"!");

    std::vector<CompiledPattern *> compiled_patterns;
    std::string err_msg;
    if (not CompilePatterns(specs_and_pattern, &compiled_patterns, &err_msg))
        LOG_ERROR("bad field specification and or regular expression (" + err_msg + ")!");

    return compiled_patterns;
}


// Sanity check.
bool ArePlausibleSubfieldSpecs(const std::vector<std::string> &subfield_specs) {
    if (subfield_specs.empty())
        return false;

    for (const auto &subfield_spec : subfield_specs) {
        if (subfield_spec.length() != MARC::Record::TAG_LENGTH + 1)
            return false;
    }

    return true;
}


std::string GetBiblioLevelArgument(char ***argvp) {
    ++*argvp;
    if (*argvp == nullptr)
        LOG_ERROR("missing bibliographic level after --drop-biblio-level or --keep-biblio-level flag!");
    const std::string bibliographic_level_candidate(**argvp);
    ++*argvp;

    if (bibliographic_level_candidate.empty())
        LOG_ERROR("bad empty bibliographic level!");
    return bibliographic_level_candidate;
}


unsigned TestAndConvertCount(char ***argvp) {
    ++*argvp;
    if (*argvp == nullptr)
        LOG_ERROR("missing count value after --max-count flag!");

    unsigned max_count;
    if (not StringUtil::ToUnsigned(**argvp, &max_count))
        LOG_ERROR("\"" + std::string(**argvp) + "\" is not a valid count argument for the --max-count flag!");
    ++*argvp;

    return max_count;
}


// Sort "subfield_specs" by increasing tag and coalesce specs with the same tag.
void NormalizeSubfieldSpecs(std::vector<std::string> * const subfield_specs) {
    std::sort(subfield_specs->begin(), subfield_specs->end());

    std::vector<std::string> coalesced_specs;
    coalesced_specs.reserve(subfield_specs->size());

    std::string current_tag;

    for (const auto &subfield_spec : *subfield_specs) {
        std::string tag(subfield_spec.substr(0, MARC::Record::TAG_LENGTH));
        if (tag != current_tag) {
            coalesced_specs.emplace_back(subfield_spec);
            current_tag.swap(tag);
        } else {
            for (const char subfield_code : subfield_spec.substr(MARC::Record::TAG_LENGTH)) {
                if (coalesced_specs.back().find(subfield_code, MARC::Record::TAG_LENGTH) == std::string::npos)
                    coalesced_specs.back() += subfield_code;
            }
        }
    }

    coalesced_specs.swap(*subfield_specs);
}


void ExtractSubfieldSpecs(const std::string &command, char ***argvp, std::vector<std::string> * const subfield_specs) {
    ++*argvp;
    StringUtil::Split(std::string(**argvp), ':', subfield_specs, /* suppress_empty_components = */ true);
    if (not ArePlausibleSubfieldSpecs(*subfield_specs))
        LOG_ERROR("bad subfield specifications \"" + std::string(**argvp) + "\" for " + command + "!");
    NormalizeSubfieldSpecs(subfield_specs);
    ++*argvp;
}


void LoadReplaceMapFile(const std::string &map_filename, std::unordered_map<std::string, std::string> * const regexes_to_replacements_map) {
    std::unique_ptr<File> input(FileUtil::OpenInputFileOrDie(map_filename));
    unsigned line_no(0);
    while (not input->eof()) {
        ++line_no;

        std::string line;
        input->getline(&line);
        if (line.empty() or line[0] == '#')
            continue;

        const size_t arrow_start(line.find("->"));
        if (unlikely(arrow_start == std::string::npos))
            LOG_ERROR("bad line #" + std::to_string(line_no) + ": missing \"->\" in \"" + map_filename + "\"!");
        if (unlikely(arrow_start == 0))
            LOG_ERROR("bad line #" + std::to_string(line_no) + ": missing regex before \"->\" in \"" + map_filename + "\"!");
        if (unlikely(arrow_start + 1 == line.length()))
            LOG_ERROR("bad line #" + std::to_string(line_no) + ": missing replacement text after \"->\" in \"" + map_filename + "\"!");
        regexes_to_replacements_map->emplace(std::make_pair(line.substr(0, arrow_start), line.substr(arrow_start + 2)));
    }
}


void ProcessReplaceCommand(char ***argvp, std::vector<FilterDescriptor> * const filters) {
    std::vector<std::string> subfield_specs;
    ExtractSubfieldSpecs("--replace", argvp, &subfield_specs);
    if (**argvp == nullptr or StringUtil::StartsWith(**argvp, "--"))
        LOG_ERROR("missing regex or map-filename arg after the subfield specification of --replace!");
    const std::string regex_or_map_filename(**argvp);
    ++*argvp;
    if (**argvp == nullptr or StringUtil::StartsWith(**argvp, "--")) {
        std::unordered_map<std::string, std::string> regexes_to_replacements_map;
        LoadReplaceMapFile(regex_or_map_filename, &regexes_to_replacements_map);
        for (const auto &regex_and_replacement : regexes_to_replacements_map)
            filters->emplace_back(
                FilterDescriptor::MakeReplacementFilter(subfield_specs, regex_and_replacement.first, regex_and_replacement.second));
    } else {
        const std::string replacement(**argvp);
        filters->emplace_back(FilterDescriptor::MakeReplacementFilter(subfield_specs, regex_or_map_filename, replacement));
        ++*argvp;
    }
}


void ProcessGloballySubstituteCommand(char ***argvp, std::vector<FilterDescriptor> * const filters) {
    std::vector<std::string> subfield_specs;
    ExtractSubfieldSpecs("--globally-substitute", argvp, &subfield_specs);
    if (**argvp == nullptr or StringUtil::StartsWith(**argvp, "--"))
        LOG_ERROR("missing regex after the subfield specification of --globally-substitute!");
    const std::string regex(**argvp);
    ++*argvp;
    if (**argvp == nullptr or StringUtil::StartsWith(**argvp, "--"))
        LOG_ERROR("missing replacement after the regex specification of --globally-substitute!");
    const std::string replacement(**argvp);
    ++*argvp;
    filters->emplace_back(FilterDescriptor::MakeGlobalSubstitutionFilter(subfield_specs, regex, replacement));
}


// Split "line" on | supporting backslash escapes.
std::vector<std::string> SplitLine(const std::string &line) {
    std::vector<std::string> parts;

    bool escaped(false);
    std::string part;
    for (const char ch : line) {
        if (escaped) {
            escaped = false;
            part += ch;
        } else if (ch == '\\')
            escaped = true;
        else if (ch == '|') {
            if (not part.empty()) {
                parts.emplace_back(part);
                part.clear();
            }
        } else
            part += ch;
    }
    if (not part.empty())
        parts.emplace_back(part);

    return parts;
}


void LoadStringsToStringsMapFile(const std::string &map_filename,
                                 std::unordered_map<std::string, std::string> * const originals_to_replacements_map) {
    std::unique_ptr<File> input(FileUtil::OpenInputFileOrDie(map_filename));
    unsigned line_no(0);
    while (not input->eof()) {
        ++line_no;

        std::string line;
        input->getline(&line);
        if (line.empty() or line[0] == '#')
            continue;

        const size_t arrow_start(line.find("->"));
        if (unlikely(arrow_start == std::string::npos))
            LOG_ERROR("bad line #" + std::to_string(line_no) + ": missing \"->\" in \"" + map_filename + "\"!");
        if (unlikely(arrow_start == 0))
            LOG_ERROR("bad line #" + std::to_string(line_no) + ": missing regex before \"->\" in \"" + map_filename + "\"!");
        if (unlikely(arrow_start + 1 == line.length()))
            LOG_ERROR("bad line #" + std::to_string(line_no) + ": missing replacement text after \"->\"! in \"" + map_filename + "\"");

        const std::string replacement(line.substr(arrow_start + 2));
        for (const auto &original : SplitLine(line.substr(0, arrow_start)))
            originals_to_replacements_map->emplace(std::make_pair(original, replacement));
    }
}


void ProcessReplaceStringsCommand(char ***argvp, std::vector<FilterDescriptor> * const filters) {
    std::vector<std::string> subfield_specs;
    ExtractSubfieldSpecs("--replace-strings", argvp, &subfield_specs);
    if (**argvp == nullptr or StringUtil::StartsWith(**argvp, "--"))
        LOG_ERROR("missing map-filename arg after --replace-strings!");
    const std::string map_filename(**argvp);
    ++*argvp;

    std::unordered_map<std::string, std::string> originals_to_replacements_map;
    LoadStringsToStringsMapFile(map_filename, &originals_to_replacements_map);
    filters->emplace_back(FilterDescriptor::MakeStringReplacementFilter(subfield_specs, originals_to_replacements_map));
}


void ProcessFilterArgs(char **argv, std::vector<FilterDescriptor> * const filters) {
    while (*argv != nullptr) {
        std::vector<CompiledPattern *> compiled_patterns;
        if (std::strcmp(*argv, "--max-count") == 0)
            filters->emplace_back(FilterDescriptor::MakeMaxCountFilter(TestAndConvertCount(&argv)));
        else if (std::strcmp(*argv, "--drop") == 0)
            filters->emplace_back(FilterDescriptor::MakeDropFilter(CollectAndCompilePatterns(&argv)));
        else if (std::strcmp(*argv, "--keep") == 0)
            filters->emplace_back(FilterDescriptor::MakeKeepFilter(CollectAndCompilePatterns(&argv)));
        else if (std::strcmp(*argv, "--drop-biblio-level") == 0)
            filters->emplace_back(FilterDescriptor::MakeDropBiblioLevelFilter(GetBiblioLevelArgument(&argv)));
        else if (std::strcmp(*argv, "--keep-biblio-level") == 0)
            filters->emplace_back(FilterDescriptor::MakeKeepBiblioLevelFilter(GetBiblioLevelArgument(&argv)));
        else if (std::strcmp(*argv, "--remove-fields") == 0)
            filters->emplace_back(FilterDescriptor::MakeRemoveFieldsFilter(CollectAndCompilePatterns(&argv)));
        else if (std::strcmp(*argv, "--remove-subfields") == 0)
            filters->emplace_back(FilterDescriptor::MakeRemoveSubfieldsFilter(CollectAndCompilePatterns(&argv)));
        else if (std::strcmp(*argv, "--translate") == 0) {
            std::vector<std::string> subfield_specs;
            ExtractSubfieldSpecs("--translate", &argv, &subfield_specs);
            if (argv == nullptr or StringUtil::StartsWith(*argv, "--"))
                LOG_ERROR("missing or bad \"set1\" argument to \"--translate\"!");
            if (argv + 1 == nullptr or StringUtil::StartsWith(*(argv + 1), "--"))
                LOG_ERROR("missing or bad \"set2\" argument to \"--translate\"!");
            TranslateMap *translate_map;
            if (std::strcmp(*argv, "[:upper:]") == 0 or std::strcmp(*argv, "[:lower:]") == 0) {
                if (std::strcmp(*argv, "[:upper:]") == 0) {
                    if (std::strcmp(*(argv + 1), "[:lower:]") != 0)
                        LOG_ERROR("if \"[:upper:]\" was specified as set1 for --translate, \"[:lower:]\" must be specified as set2!");
                    translate_map = new UpperLowerTranslateMap(UpperLowerTranslateMap::UPPER_TO_LOWER);
                } else {
                    if (std::strcmp(*(argv + 1), "[:upper:]") != 0)
                        LOG_ERROR("if \"[:lower:]\" was specified as set1 for --translate, \"[:upper:]\" must be specified as set2!");
                    translate_map = new UpperLowerTranslateMap(UpperLowerTranslateMap::LOWER_TO_UPPER);
                }
            } else
                translate_map = new CharSetTranslateMap(TextUtil::CStyleUnescape(*argv), TextUtil::CStyleUnescape(*(argv + 1)));
            filters->emplace_back(FilterDescriptor::MakeTranslateFilter(subfield_specs, *translate_map));
            argv += 2;
        } else if (std::strcmp(*argv, "--filter-chars") == 0) {
            std::vector<std::string> subfield_specs;
            ExtractSubfieldSpecs("--filter-chars", &argv, &subfield_specs);
            if (argv == nullptr or StringUtil::StartsWith(*argv, "--"))
                LOG_ERROR("missing or bad \"characters_to_delete\" argument to \"--filter-chars\"!");
            filters->emplace_back(FilterDescriptor::MakeFilterCharsFilter(subfield_specs, TextUtil::CStyleUnescape(*argv++)));
        } else if (std::strcmp(*argv, "--replace") == 0)
            ProcessReplaceCommand(&argv, filters);
        else if (std::strcmp(*argv, "--replace-strings") == 0)
            ProcessReplaceStringsCommand(&argv, filters);
        else if (std::strcmp(*argv, "--globally-substitute") == 0)
            ProcessGloballySubstituteCommand(&argv, filters);
        else
            LOG_ERROR("unknown operation type \"" + std::string(*argv) + "\"!");
    }
}


} // unnamed namespace


int Main(int argc, char **argv) {
    ++argv;

    if (argc < 5)
        Usage();

    const std::string input_filename(*argv++);
    const std::string output_filename(*argv++);
    auto marc_reader(MARC::Reader::Factory(input_filename));
    auto marc_writer(MARC::Writer::Factory(output_filename));

    std::vector<FilterDescriptor> filters;
    ProcessFilterArgs(argv, &filters);
    Filter(filters, marc_reader.get(), marc_writer.get());

    return EXIT_SUCCESS;
}

#include "PhaseAddAuthorSynonyms.h"
#include <iostream>
#include <vector>
#include <cstdlib>
#include "Compiler.h"
#include "MarcUtil.h"
#include "MediaTypeUtil.h"
#include "StringUtil.h"
#include "Subfields.h"
#include "util.h"


static unsigned modified_count(0);
static std::map <std::string, std::string> author_to_synonyms_map;
static std::string field_list("100abcd:400abcd");
static std::string primary_author_field("100abcd");
static std::set <std::string> synonyms;
static std::vector <std::string> tags_and_subfield_codes;


void RemoveCommasDuplicatesAndEmptyEntries(std::vector <std::string> *const vector) {
    std::vector <std::string> cleaned_up_vector;
    std::set <std::string> uniqe_entries;

    for (auto &entry : *vector) {
        StringUtil::RemoveChars(",", &entry);

        if (entry.empty())
            continue;

        const bool is_new_entry(uniqe_entries.emplace(entry).second);
        if (is_new_entry)
            cleaned_up_vector.emplace_back(std::move(entry));
    }
    vector->swap(cleaned_up_vector);
}


std::string ExtractNameFromSubfields(const std::string &field_contents, const std::string &subfield_codes) {
    const Subfields subfields(field_contents);
    std::vector <std::string> subfield_values;
    if (subfields.extractSubfields(subfield_codes, &subfield_values) == 0)
        return "";

    std::sort(subfield_values.begin(), subfield_values.end());
    return StringUtil::Join(subfield_values, ' ');
}


const std::string SYNOMYM_FIELD("101"); // This must be an o/w unused field!


PipelinePhaseState PhaseAddAuthorSynonyms::preprocessNormData(const MarcUtil::Record &record, std::string *const) {
    const int primary_name_field_index(record.getFieldIndex(tags_and_subfield_codes[0].substr(0, 3)));
    if (primary_name_field_index == -1)
        return SUCCESS;

    const std::vector <std::string> &fields(record.getFields());
    const std::string primary_name(ExtractNameFromSubfields(fields[primary_name_field_index],
                                                            tags_and_subfield_codes[0].substr(3)));
    if (unlikely(primary_name.empty()))
        return SUCCESS;

    std::vector <std::string> alternatives;
    alternatives.emplace_back(primary_name);
    if (author_to_synonyms_map.find(primary_name) != author_to_synonyms_map.end())
        return SUCCESS;

    const std::vector <DirectoryEntry> &dir_entries(record.getDirEntries());

    for (unsigned i(1); i < tags_and_subfield_codes.size(); ++i) {
        const std::string tag(tags_and_subfield_codes[i].substr(0, 3));
        const std::string secondary_field_subfield_codes(tags_and_subfield_codes[i].substr(3));
        int secondary_name_field_index(record.getFieldIndex(tag));
        while (secondary_name_field_index != -1 and static_cast<size_t>(secondary_name_field_index) < dir_entries.size()
                and dir_entries[secondary_name_field_index].getTag() == tag) {
            const std::string secondary_name(ExtractNameFromSubfields(fields[secondary_name_field_index],
                                                                      secondary_field_subfield_codes
            ));
            if (not secondary_name.empty())
                alternatives.emplace_back(secondary_name);
            ++secondary_name_field_index;
        }
    }

    RemoveCommasDuplicatesAndEmptyEntries(&alternatives);
    if (alternatives.size() <= 1)
        return SUCCESS;

    alternatives.erase(alternatives.begin());
    author_to_synonyms_map.emplace(primary_name, StringUtil::Join(alternatives, ','));
    return SUCCESS;
};


PipelinePhaseState PhaseAddAuthorSynonyms::process(MarcUtil::Record &record, std::string *const error_message) {
    record.setRecordWillBeWrittenAsXml(true);

    if (unlikely(record.getFieldIndex(SYNOMYM_FIELD) != -1))
        return MakeError("field " + SYNOMYM_FIELD + " is apparently already in use in at least some title records!", error_message);

    const int primary_name_field_index(record.getFieldIndex(primary_author_field.substr(0, 3)));
    if (primary_name_field_index == -1)
        return SUCCESS;

    const std::vector <std::string> &fields(record.getFields());
    const std::string primary_name(ExtractNameFromSubfields(fields[primary_name_field_index], primary_author_field.substr(3)));
    if (unlikely(primary_name.empty()))
        return SUCCESS;

    const auto synonyms_iterator = author_to_synonyms_map.find(primary_name);
    if (synonyms_iterator == author_to_synonyms_map.end())
        return SUCCESS;

    const std::string synonyms_data = synonyms_iterator->second;
    Subfields subfields(/* indicator1 = */' ', /* indicator2 = */' ');
    subfields.addSubfield('a', synonyms_data);

    if (not record.insertField(SYNOMYM_FIELD, subfields.toString()))
        return MakeError("Not enough room to add a " + SYNOMYM_FIELD + " field! (Control number: " + fields[0] + ")", error_message);

    ++modified_count;
    return SUCCESS;
};


PhaseAddAuthorSynonyms::PhaseAddAuthorSynonyms() {
    if (unlikely(StringUtil::Split(field_list, ':', &tags_and_subfield_codes) < 2))
        Error("PhaseAddAuthorSynonyms: field_list ('" + field_list + "') need at least two fields!");
}


PhaseAddAuthorSynonyms::~PhaseAddAuthorSynonyms() {
    std::cerr << "Add author synonyms:\n";
    std::cerr << "\tFound synonyms for " << author_to_synonyms_map.size() << " authors\n";
}
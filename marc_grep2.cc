// Grep for field contents in MARC-21 datasets.
//
#include <iostream>
#include <iterator>
#include <memory>
#include <stdexcept>
#include <unordered_map>
#include <utility>
#include <vector>
#include <cstring>
#include "Compiler.h"
#include "DirectoryEntry.h"
#include "Leader.h"
#include "MarcQueryParser.h"
#include "MarcUtil.h"
#include "Subfields.h"
#include "util.h"


char help_text[] =
  "  Query syntax:\n"
  "    query                                    = [ leader_condition ] simple_query\n"
  "    leader_condition                         = \"leader[\" offset_range \"]=\" string_constant\n"
  "    offset_range                             = start_offset [ \"-\" end_offset ]\n"
  "    start_offset                             = unsigned_integer\n"
  "    end_offset                               = unsigned_integer\n"
  "    unsigned_integer                         = digit { digit }\n"
  "    digit                                    = \"0\" | \"1\" | \"2\" | \"3\" | \"4\" | \"5\" | \"6\" | \"7\"\n"
  "                                               | \"8\" | \"9\"\n"
  "    simple_query                             = simple_field_list | conditional_field_or_subfield_references\n"
  "    simple_field_list                        = field_or_subfield_reference\n"
  "                                               { \":\" field_or_subfield_reference }\n"
  "    field_or_subfield_reference              = field_reference | subfield_reference\n"
  "    conditional_field_or_subfield_references = conditional_field_or_subfield_reference\n"
  "                                               { \",\" conditional_field_or_subfield_reference }\n"
  "    conditional_field_or_subfield_reference  = \"if\" condition \"extract\"\n"
  "                                               (field_or_subfield_reference | \"*\")\n"
  "    condition                                = field_or_subfield_reference comp_op reg_ex\n"
  "                                               | field_or_subfield_reference \"exists\"\n"
  "                                               | field_or_subfield_reference \"is_missing\"\n"
  "    reg_ex                                   = string_constant\n"
  "    comp_op                                  = \"==\" | \"!=\" | \"===\" | \"!==\"\n"
  "\n"
  "  String constants start and end with double quotes. Backslashes and double quotes within need to be escaped\n"
  "  with a backslash. The difference between the \"==\" and \"!=\" vs. \"===\" and \"!===\" comparision\n"
  "  operators is that the latter compare subfields within a given field while the former compare against any two\n"
  "  matching fields or subfields.  This becomes relevant when there are mutiple occurrences of a field in a\n"
  "  record. \"*\" matches all fields.\n"
  "\n"
  "  Output label format:\n"
  "    label_format = matched_field_or_subfield | control_number | control_number_and_matched_field_or_subfield\n"
  "                   | no_label\n"
  "\n"
  "  The default output label is the control number followed by a colon followed by matched field or subfield\n"
  "  followed by a colon.\n";


void Usage() {
    std::cerr << "Usage: " << progname << " marc_filename query [output_label_format]\n\n";
    std::cerr << help_text << '\n';
    std::exit(EXIT_FAILURE);
}


enum OutputLabel { MATCHED_FIELD_OR_SUBFIELD_ONLY, CONTROL_NUMBER_ONLY, CONTROL_NUMBER_AND_MATCHED_FIELD_OR_SUBFIELD,
		   TRADITIONAL, NO_LABEL };


OutputLabel ParseOutputLabel(const std::string &label_format_candidate) {
    if (label_format_candidate == "matched_field_or_subfield")
	return MATCHED_FIELD_OR_SUBFIELD_ONLY;
    if (label_format_candidate == "control_number")
	return CONTROL_NUMBER_ONLY;
    if (label_format_candidate == "control_number_and_matched_field_or_subfield")
	return CONTROL_NUMBER_AND_MATCHED_FIELD_OR_SUBFIELD;
    if (label_format_candidate == "traditional")
	return TRADITIONAL;
    if (label_format_candidate == "no_label")
	return NO_LABEL;

    Error("\"" + label_format_candidate + "\" is no valid output label format!");
}


void Emit(const std::string &control_number, const std::string &tag_or_tag_plus_subfield_code,
	  const std::string &contents, const OutputLabel output_format)
{
    switch (output_format) {
    case MATCHED_FIELD_OR_SUBFIELD_ONLY:
	std::cout << tag_or_tag_plus_subfield_code << ':' << contents << '\n';
	return;
    case CONTROL_NUMBER_ONLY:
	std::cout << control_number << ':' << contents << '\n';
	return;
    case CONTROL_NUMBER_AND_MATCHED_FIELD_OR_SUBFIELD:
	std::cout << control_number << ':' << tag_or_tag_plus_subfield_code << ':' << contents << '\n';
	return;
    case TRADITIONAL:
	std::cout << tag_or_tag_plus_subfield_code.substr(0, 3) << ' ' << StringUtil::Map(contents, '\x1F', '$')
		  << '\n';
	return;
    case NO_LABEL:
	std::cout << contents << '\n';
	return;
    }
}


bool EmitSubfields(const std::string &control_number, const std::string &tag, const char subfield_code,
		   const std::string &contents, const OutputLabel output_format)
{
    std::string tag_plus_subfield_code(tag);
    tag_plus_subfield_code += subfield_code;
    const Subfields subfields(contents);
    const auto begin_end(subfields.getIterators(subfield_code));
    bool emitted_at_least_one(false);
    for (auto code_and_subfield(begin_end.first); code_and_subfield != begin_end.second; ++code_and_subfield) {
	Emit(control_number, tag_plus_subfield_code, code_and_subfield->second, output_format);
	emitted_at_least_one = true;
    }

    return emitted_at_least_one;
}


bool ProcessEqualityComp(const ConditionDescriptor &cond_desc,
			 const std::unordered_multimap<std::string, const std::string *> &field_to_content_map)
{
    const FieldOrSubfieldDescriptor comp_field_or_subfield(cond_desc.getFieldOrSubfieldReference());
    const auto begin_end(field_to_content_map.equal_range(comp_field_or_subfield.getTag()));
    const std::string subfield_codes(comp_field_or_subfield.getSubfieldCodes());
    const ConditionDescriptor::CompType comp_type(cond_desc.getCompType());
    std::string err_msg;
    bool matched_at_least_one(false);
    for (auto field(begin_end.first); field != begin_end.second; ++field) {
	const std::string &contents(*(field->second));
	if (subfield_codes.empty()) { // Compare against the entire field. (Does this even make sense?)
	    if (cond_desc.getDataMatcher().matched(contents, &err_msg)) {
		matched_at_least_one = true;
		break;
	    }
	    if (unlikely(not err_msg.empty()))
		Error("ProcessEqualityComp: match failed (" + err_msg + ")! (1)");
	} else  { // We need to match against a subfield's content.
	const Subfields subfields(contents);
	if (not subfields.hasSubfield(subfield_codes[0]))
	    continue;
	const auto sub_begin_end(subfields.getIterators(subfield_codes[0]));
	for (auto subfield_contents(sub_begin_end.first); subfield_contents != sub_begin_end.second;
	     ++subfield_contents)
	    {
		if (cond_desc.getDataMatcher().matched(subfield_contents->second, &err_msg)) {
		    matched_at_least_one = true;
		    break;
		} else if (unlikely(not err_msg.empty()))
		    Error("ProcessEqualityComp: match failed (" + err_msg + ")! (1)");
	    }
	}
    }

    return (comp_type == ConditionDescriptor::EQUAL_EQUAL) ? matched_at_least_one : not matched_at_least_one;
}


bool ProcessExistenceTest(const ConditionDescriptor &cond_desc,
			  const std::unordered_multimap<std::string, const std::string *> &field_to_content_map)
{
    const FieldOrSubfieldDescriptor test_field_or_subfield(cond_desc.getFieldOrSubfieldReference());
    const ConditionDescriptor::CompType comp_type(cond_desc.getCompType());
    const auto begin_end(field_to_content_map.equal_range(test_field_or_subfield.getTag()));
    if (begin_end.first == begin_end.second)
	return comp_type == ConditionDescriptor::IS_MISSING;
    const std::string subfield_codes(test_field_or_subfield.getSubfieldCodes());
    if (subfield_codes.empty())
	return comp_type == ConditionDescriptor::EXISTS;

    bool found_at_least_one(false);
    for (auto field(begin_end.first); field != begin_end.second; ++field) {
	const std::string &contents(*(field->second));
	const Subfields subfields(contents);
	if (subfields.hasSubfield(subfield_codes[0])) {
	    found_at_least_one = true;
	    break;
	}
    }

    return (comp_type == ConditionDescriptor::EXISTS) ? found_at_least_one : not found_at_least_one;
}


bool ProcessConditions(const ConditionDescriptor &cond_desc, const FieldOrSubfieldDescriptor &field_or_subfield_desc,
		       const std::unordered_multimap<std::string, const std::string *> &field_to_content_map,
		       const OutputLabel output_format)
{
    // Determine the control number:
    const auto &control_number_iter(field_to_content_map.find("001"));
    if (unlikely(control_number_iter == field_to_content_map.end()))
	Error("In ProcessConditions: record has no control number!");
    const std::string control_number(*(control_number_iter->second));

    const std::string extraction_tag(field_or_subfield_desc.getTag());
    const auto begin_end(field_to_content_map.equal_range(extraction_tag));
    const bool extraction_tag_found(begin_end.first != begin_end.second or field_or_subfield_desc.isStar());
    if (not extraction_tag_found)
	return false;

    const ConditionDescriptor::CompType comp_type(cond_desc.getCompType());
    if (comp_type == ConditionDescriptor::NO_COMPARISION
	or ((comp_type == ConditionDescriptor::EQUAL_EQUAL or comp_type == ConditionDescriptor::NOT_EQUAL)
	    and ProcessEqualityComp(cond_desc, field_to_content_map))
	or ((comp_type == ConditionDescriptor::EXISTS or comp_type == ConditionDescriptor::IS_MISSING)
	    and ProcessExistenceTest(cond_desc, field_to_content_map)))
    {
	if (field_or_subfield_desc.isStar()) {
	    for (const auto &tag_and_content : field_to_content_map)
		Emit(control_number, tag_and_content.first, *(tag_and_content.second), output_format);
	    return true;
	}

	const std::string subfield_codes(field_or_subfield_desc.getSubfieldCodes());
	bool emitted_at_least_one(false);
	for (auto tag_and_field_contents(begin_end.first); tag_and_field_contents != begin_end.second;
	     ++tag_and_field_contents)
	{
	    if (subfield_codes.empty()) {
		Emit(control_number, extraction_tag, *(tag_and_field_contents->second), output_format);
		emitted_at_least_one = true;
	    } else { // Looking for one or more subfields:
		for (const auto &subfield_code : subfield_codes) {
		    if (EmitSubfields(control_number, extraction_tag, subfield_code, *tag_and_field_contents->second,
				      output_format))
			emitted_at_least_one = true;
		}
	    }
	}

	return emitted_at_least_one;
    } else if (comp_type == ConditionDescriptor::SINGLE_FIELD_EQUAL
	       or comp_type == ConditionDescriptor::SINGLE_FIELD_NOT_EQUAL)
    {
	if (field_or_subfield_desc.isStar()) {
	    for (const auto &tag_and_content : field_to_content_map)
		Emit(control_number, tag_and_content.first, *(tag_and_content.second), output_format);
	    return true;
	}

	bool emitted_at_least_one(false);
	const char test_subfield_code(cond_desc.getFieldOrSubfieldReference()[DirectoryEntry::TAG_LENGTH]);
	const char extract_subfield_code(field_or_subfield_desc.getSubfieldCodes()[0]);
	for (auto tag_and_field_contents(begin_end.first); tag_and_field_contents != begin_end.second;
	     ++tag_and_field_contents)
	{
	    const Subfields subfields(*tag_and_field_contents->second);
	    if (not subfields.hasSubfield(extract_subfield_code))
		continue;

	    if (not subfields.hasSubfield(test_subfield_code)) {
		if (comp_type == ConditionDescriptor::SINGLE_FIELD_NOT_EQUAL) {
		    if (EmitSubfields(control_number, extraction_tag, extract_subfield_code,
				      *tag_and_field_contents->second, output_format))
			emitted_at_least_one = true;
		} else
		    return false;
	    } else {
		bool matched_at_least_one(false);
		const auto sub_begin_end(subfields.getIterators(test_subfield_code));
		std::string err_msg;
		for (auto code_and_value(sub_begin_end.first); code_and_value != sub_begin_end.second;
		     ++code_and_value)
		{
		    if (cond_desc.getDataMatcher().matched(code_and_value->second, &err_msg)) {
			matched_at_least_one = true;
			break;
		    } else if (unlikely(not err_msg.empty()))
			Error("Unexpected: Match failed in ProcessConditions!");
		}

		if ((matched_at_least_one and comp_type == ConditionDescriptor::SINGLE_FIELD_EQUAL)
		    or (not matched_at_least_one and comp_type == ConditionDescriptor::SINGLE_FIELD_NOT_EQUAL))
		{
		    if (EmitSubfields(control_number, extraction_tag, extract_subfield_code,
				      *tag_and_field_contents->second, output_format))
			emitted_at_least_one = true;
		}
	    }
	}

	return emitted_at_least_one;
    } else
	return false;
}


void FieldGrep(const std::string &input_filename, const QueryDescriptor &query_desc,
	       const OutputLabel output_format)
{
    FILE *input = std::fopen(input_filename.c_str(), "rbm");
    if (input == NULL)
	Error("can't open \"" + input_filename + "\" for reading!");

    Leader *raw_leader;
    std::vector<DirectoryEntry> dir_entries;
    std::vector<std::string> field_data;
    std::string err_msg;
    unsigned count(0), matched_count(0);

    while (MarcUtil::ReadNextRecord(input, &raw_leader, &dir_entries, &field_data, &err_msg)) {
	++count;
	std::unique_ptr<Leader> leader(raw_leader);

	if (query_desc.hasLeaderCondition()) {
	    const LeaderCondition &leader_cond(query_desc.getLeaderCondition());
	    if (raw_leader->toString().substr(leader_cond.getStartOffset(),
					      leader_cond.getEndOffset() - leader_cond.getStartOffset() + 1)
		!= leader_cond.getMatch())
		continue;
	}

	std::unordered_multimap<std::string, const std::string *> field_to_content_map;
	for (unsigned i(0); i < dir_entries.size(); ++i)
	    field_to_content_map.insert(std::make_pair(dir_entries[i].getTag(), &field_data[i]));

	bool matched(false);

	// Extract fields and subfields:
	for (const auto &cond_and_field_or_subfield : query_desc.getCondsAndFieldOrSubfieldDescs()) {
	    if (ProcessConditions(cond_and_field_or_subfield.first, cond_and_field_or_subfield.second,
				  field_to_content_map, output_format))
		matched = true;
	}

	if (matched)
	    ++matched_count;
    }

    if (not err_msg.empty())
	Error(err_msg);
    std::cerr << "Matched " << matched_count << (matched_count == 1 ? " record of " :  " records of ") << count
	      << " overall records.\n";

    std::fclose(input);
}


int main(int argc, char *argv[]) {
    progname = argv[0];

    if (argc < 3 or argc > 4)
	Usage();

    QueryDescriptor query_desc;
    std::string err_msg;
    if (not ParseQuery(argv[2], &query_desc, &err_msg))
	Error("Query parsing failed: " + err_msg);

    const OutputLabel output_label = (argc == 4) ? ParseOutputLabel(argv[3])
	                                         : CONTROL_NUMBER_AND_MATCHED_FIELD_OR_SUBFIELD;
    FieldGrep(argv[1], query_desc, output_label);
}

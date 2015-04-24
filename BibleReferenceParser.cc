#include "BibleReferenceParser.h"
#include <cctype>
#include "Locale.h"
#include "StringUtil.h"


namespace {
    

// Checks whether the new reference comes strictly after already existing references.
bool NewReferenceIsCompatibleWithExistingReferences(
	const std::pair<std::string, std::string> &new_ref,
	const std::set<std::pair<std::string, std::string>> &existing_refs)
{
    for (const auto existing_ref : existing_refs) {
	if (new_ref.first <= existing_ref.second)
	    return false;
    }

    return true;
}
	

bool IsNumericString(const std::string &s) {
    for (const char ch : s) {
	if (not isdigit(ch))
	    return false;
    }

    return true;
}


bool ParseRefWithDot(const std::string &bib_ref_candidate, const std::string &book_code,
		     std::set<std::pair<std::string, std::string>> * const start_end)
{
    std::set<std::pair<std::string, std::string>> new_start_end;

    const size_t comma_pos(bib_ref_candidate.find(','));
    if (comma_pos == std::string::npos) // We must have a comma!
	return false;

    const std::string chapter(StringUtil::PadLeading(bib_ref_candidate.substr(0, comma_pos), 3, '0'));
    if (chapter.length() != 3 or not IsNumericString(chapter))
	return false;

    const std::string rest(bib_ref_candidate.substr(comma_pos + 1));
    bool in_verse1(true);
    std::string verse1, verse2;
    for (const char ch : rest) {
	if (isdigit(ch)) {
	    if (in_verse1) {
		verse1 += ch;
		if (verse1.length() > 2)
		    return false;
	    } else {
		verse2 += ch;
		if (verse2.length() > 2)
		    return false;
	    }
	} else if (ch == '.') {
	    if (in_verse1) {
		if (verse1.empty())
		    return false;
		verse1 = StringUtil::PadLeading(verse1, 2, '0');
		const std::pair<std::string, std::string> new_reference(
		    std::make_pair(book_code + chapter + verse1, book_code + chapter + verse1));
		if (not NewReferenceIsCompatibleWithExistingReferences(new_reference, new_start_end))
		    return false;
		new_start_end.insert(new_reference);
		verse1.clear();
	    } else {
		if (verse2.empty())
		    return false;
		verse2 = StringUtil::PadLeading(verse2, 2, '0');
		if (verse2 <= verse1)
		    return false;
		const std::pair<std::string, std::string> new_reference(
                    std::make_pair(book_code + chapter + verse1, book_code + chapter + verse2));
		if (not NewReferenceIsCompatibleWithExistingReferences(new_reference, new_start_end))
		    return false;
		new_start_end.insert(new_reference);
		verse1.clear();
		verse2.clear();
		in_verse1 = true;
	    }
	} else if (ch == '-') {
	    if (not in_verse1 or verse1.empty())
		return false;
	    verse1 = StringUtil::PadLeading(verse1, 2, '0');
	    in_verse1 = false;
	} else if (islower(ch)) {
	    if (in_verse1) {
		if (verse1.empty())
		    return false;
		verse1 = StringUtil::PadLeading(verse1, 2, '0');
	    } else {
		if (verse2.empty())
		    return false;
		verse2 = StringUtil::PadLeading(verse2, 2, '0');
	    }
	} else
	    return false;
    }

    if (in_verse1) {
	if (verse1.empty())
	    return false;
	verse1 = StringUtil::PadLeading(verse1, 2, '0');
	const std::pair<std::string, std::string> new_reference(
            std::make_pair(book_code + chapter + verse1, book_code + chapter + verse1));
	if (not NewReferenceIsCompatibleWithExistingReferences(new_reference, new_start_end))
	    return false;
	new_start_end.insert(new_reference);
    } else {
	if (verse2.empty())
	    return false;
	verse2 = StringUtil::PadLeading(verse2, 2, '0');
	if (verse2 <= verse1)
	    return false;
	const std::pair<std::string, std::string> new_reference(
            std::make_pair(book_code + chapter + verse1, book_code + chapter + verse2));
	if (not NewReferenceIsCompatibleWithExistingReferences(new_reference, new_start_end))
	    return false;
	new_start_end.insert(new_reference);
    }

    start_end->insert(new_start_end.cbegin(), new_start_end.cend());
    return true;
}


enum State { INITIAL, CHAPTER1, CHAPTER2, VERSE1, VERSE2 };
    

} // unnamed namespace


bool ParseBibleReference(const std::string &bib_ref_candidate, const std::string &book_code,
			 std::set<std::pair<std::string, std::string>> * const start_end)
{
    if (bib_ref_candidate.empty()) {
	start_end->insert(std::make_pair(book_code + "00000", book_code + "99999"));
	return true;
    }

    const Locale c_locale("C", LC_ALL); // We don't want islower() to accept characters w/ diacritical marks!

    if (bib_ref_candidate.find('.') != std::string::npos)
	return ParseRefWithDot(bib_ref_candidate, book_code, start_end);

    State state(INITIAL);
    std::string accumulator, chapter1, verse1, chapter2, verse2;
    for (auto ch(bib_ref_candidate.cbegin()); ch != bib_ref_candidate.cend(); ++ch) {
	switch (state) {
	case INITIAL:
	    if (isdigit(*ch)) {
		accumulator += *ch;
		state = CHAPTER1;
	    } else
		return false;
	    break;
	case CHAPTER1:
	    if (isdigit(*ch)) {
		accumulator += *ch;
		if (accumulator.length() > 3)
		    return false;
	    } else if (*ch == '-') {
		chapter1 = StringUtil::PadLeading(accumulator, 3, '0');
		accumulator.clear();
		state = CHAPTER2;
	    } else if (*ch == ',') {
		chapter1 = StringUtil::PadLeading(accumulator, 3, '0');
		accumulator.clear();
		state = VERSE1;
	    } else
		return false;
	    break;
	case VERSE1:
	    if (isdigit(*ch)) {
		accumulator += *ch;
		if (accumulator.length() > 2)
                    return false;
	    } else if (islower(*ch)) {
		if (accumulator.empty())
		    return false;
		accumulator = StringUtil::PadLeading(accumulator, 2, '0');
		// Ignore this non-standardised letter!
	    } else if (*ch == '-') {
		if (accumulator.empty())
		    return false;
		verse1 = StringUtil::PadLeading(accumulator, 2, '0');
		accumulator.clear();

		// We need to differentiate between a verse vs. a chapter-hyphen:
		const std::string remainder(bib_ref_candidate.substr(ch - bib_ref_candidate.cbegin()));
		if (remainder.find(',') == std::string::npos) // => We have a verse hyphen!
		    state = VERSE2;
		else
		    state = CHAPTER2;
	    } else
		return false;
	    break;
	case CHAPTER2:
	    if (isdigit(*ch)) {
		accumulator += *ch;
		if (accumulator.length() > 3)
                    return false;
	    } else if (*ch == ',') {
		if (accumulator.empty())
		    return false;
		chapter2 = StringUtil::PadLeading(accumulator, 3, '0');
		accumulator.clear();
		state = VERSE2;
	    } else
		return false;
	    break;
	case VERSE2:
	    if (isdigit(*ch)) {
		accumulator += *ch;
		if (accumulator.length() > 2)
                    return false;
	    } else if (islower(*ch)) {
		if (accumulator.empty())
		    return false;
		accumulator = StringUtil::PadLeading(accumulator, 2, '0');
		// Ignore this non-standardised letter!
	    } else
		return false;
	    break;
	}
    }

    if (state == CHAPTER1) {
	chapter1 = book_code + StringUtil::PadLeading(accumulator, 3, '0');
	start_end->insert(std::make_pair(chapter1 + "00", chapter1 + "99"));
    } else if (state == CHAPTER2) {
	if (accumulator.empty())
	    return false;
	verse1 = StringUtil::PadLeading(verse1, 2, '0');
	verse2 = verse2.empty() ? "99" : StringUtil::PadLeading(verse2, 2, '0');
	const std::string chapter1_verse1(chapter1 + verse1);
	const std::string chapter2_verse2(StringUtil::PadLeading(accumulator, 3, '0') + verse2);
	if (chapter2_verse2 <= chapter1_verse1)
	    return false;
	start_end->insert(std::make_pair(book_code + chapter1_verse1, book_code + chapter2_verse2));
    } else if (state == VERSE1) {
	verse1 = StringUtil::PadLeading(accumulator, 2, '0');
	accumulator = book_code + chapter1 + verse1;
	start_end->insert(std::make_pair(accumulator, accumulator));
    } else if (state == VERSE2) {
	if (accumulator.empty())
	    return false;
	verse2 = StringUtil::PadLeading(accumulator, 2, '0');
	const std::string start(book_code + chapter1 + verse1);
	const std::string end(book_code + (chapter2.empty() ? chapter1 : chapter2) + verse2);
	if (end <= start)
	    return false;
	start_end->insert(std::make_pair(start, end));
    }

    return true;
}


bool CanParseBibleReference(const std::string &bib_ref_candidate) {
    std::set<std::pair<std::string, std::string>> start_end;
    return ParseBibleReference(bib_ref_candidate, "00", &start_end);
}

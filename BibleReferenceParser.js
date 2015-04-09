var State = {
    INITIAL: 0,
    CHAPTER1: 1,
    CHAPTER2: 2,
    VERSE1: 3,
    VERSE2: 4
};


// Adds up to 3 zeros to the front of "string".
function PadLeadingZeroes(string, max_length) {
    if (string.length > max_length)
	return string;
    else
	return "000".substr(0, max_length - string.length) + string;
}


function IsDigit(digit_candidate) {
    return /\d/.test(digit_candidate);
}


function IsLower(ch) {
    return /[a-z]/.test(ch);
}
	

function ParseRefWithDot(bib_ref_candidate, book_code, start_end) {
    var comma_pos = bib_ref_candidate.indexOf(",");
    if (comma_pos === -1) // We must have a comma!
	return false;

    var chapter = PadLeadingZeroes(bib_ref_candidate.substr(0, comma_pos), 3);
    if (chapter.length !== 3)
	return false;

    var rest = bib_ref_candidate.substr(comma_pos + 1);
    var in_verse1 = true;
    var verse1 = "", verse2 = "";
    for (var i = 0; i < rest.length; ++i) {
	var ch = rest.charAt(i);
	if (IsDigit(ch)) {
	    if (in_verse1) {
		verse1 += ch;
		if (verse1.length > 2)
		    return false;
	    } else {
		verse2 += ch;
		if (verse2.length > 2)
		    return false;
	    }
	} else if (ch === ".") {
	    if (in_verse1) {
		if (verse1.length === 0)
		    return false;
		verse1 = PadLeadingZeroes(verse1, 2);
		start_end[book_code + chapter + verse1 + ":" + book_code + chapter + verse1] = true;
		verse1 = "";
	    } else {
		if (verse2.length === 0)
		    return false;
		verse2 = PadLeadingZeroes(verse2, 2);
		if (verse2 <= verse1)
		    return false;
		start_end[book_code + chapter + verse1 + ":" + book_code + chapter + verse2] = true;
		verse1 = "";
		verse2 = "";
		in_verse1 = true;
	    }
	} else if (ch === "-") {
	    if (!in_verse1 || verse1.length === 0)
		return false;
	    verse1 = PadLeadingZeroes(verse1, 2);
	    in_verse1 = false;
	} else if (IsLower(ch)) {
	    if (in_verse1) {
		if (verse1.length === 0)
		    return false;
		verse1 = PadLeadingZeroes(verse1, 2);
	    } else {
		if (verse2.length === 0)
		    return false;
		verse2 = PadLeadingZeroes(verse2, 2);
	    }
	} else
	    return false;
    }

    if (in_verse1) {
	if (verse1.length === 0)
	    return false;
	verse1 = PadLeadingZeroes(verse1, 2);
	start_end[book_code + chapter + verse1 + ":" + book_code + chapter + verse1] = true;
    } else {
	if (verse2.length === 0)
	    return false;
	verse2 = PadLeadingZeroes(verse2, 2);
	if (verse2 <= verse1)
	    return false;
	start_end[book_code + chapter + verse1 + ":" + book_code + chapter + verse2] = true;
    }

    return true;
}


function ParseBibleReference(bib_ref_candidate, book_code, start_end) {
    if (bib_ref_candidate.length === 0) {
        start_end[book_code + "00000:" + book_code + "00000"] = true;
	return true;
    }

    if (bib_ref_candidate.indexOf(".") !== -1)
	return ParseRefWithDot(bib_ref_candidate, book_code, start_end);

    var state = State.INITIAL;
    var accumulator = "", chapter1 = "", verse1 = "", chapter2 = "", verse2 = "";
    for (var i = 0; i < bib_ref_candidate.length; ++i) {
	var ch = bib_ref_candidate.charAt(i);
	switch (state) {
	case State.INITIAL:
	    if (IsDigit(ch)) {
		accumulator += ch;
		state = State.CHAPTER1;
	    } else
		return false;
	    break;
	case State.CHAPTER1:
	    if (IsDigit(ch)) {
		accumulator += ch;
		if (accumulator.length > 3)
		    return false;
	    } else if (ch === "-") {
		chapter1 = PadLeadingZeroes(accumulator, 3);
		accumulator = "";
		state = State.CHAPTER2;
	    } else if (ch === ",") {
		chapter1 = PadLeadingZeroes(accumulator, 3);
		accumulator = "";
		state = State.VERSE1;
	    } else
		return false;
	    break;
	case State.VERSE1:
	    if (IsDigit(ch)) {
		accumulator += ch;
		if (accumulator.length > 2)
                    return false;
	    } else if (IsLower(ch)) {
		if (accumulator.length === 0)
		    return false;
		accumulator = PadLeadingZeroes(accumulator, 2);
		// Ignore this non-standardised letter!
	    } else if (ch === "-") {
		if (accumulator.length === 0)
		    return false;
		verse1 = PadLeadingZeroes(accumulator, 2);
		accumulator = "";

		// We need to differentiate between a verse vs. a chapter-hyphen:
		var remainder = bib_ref_candidate.substr(i + 1);
		if (remainder.indexOf(",") === -1) // => We have a verse hyphen!
		    state = State.VERSE2;
		else
		    state = State.CHAPTER2;
	    } else
		return false;
	    break;
	case State.CHAPTER2:
	    if (IsDigit(ch)) {
		accumulator += ch;
		if (accumulator.length > 3)
                    return false;
	    } else if (ch === ",") {
		if (accumulator.length === 0)
		    return false;
		chapter2 = PadLeadingZeroes(accumulator, 3);
		accumulator = "";
		state = State.VERSE2;
	    } else
		return false;
	    break;
	case State.VERSE2:
	    if (IsDigit(ch)) {
		accumulator += ch;
		if (accumulator.length > 2)
                    return false;
	    } else if (IsLower(ch)) {
		if (accumulator.length === 0)
		    return false;
		accumulator = PadLeadingZeroes(accumulator, 2);
		// Ignore this non-standardised letter!
	    } else
		return false;
	    break;
	}
    }

    if (state === State.CHAPTER1) {
	chapter1 = book_code + PadLeadingZeroes(accumulator, 3) + "00";
	start_end[chapter1 + ":" + chapter1] = true;
    } else if (state === State.CHAPTER2) {
	if (accumulator.length === 0)
	    return false;
	verse1 = PadLeadingZeroes(verse1, 2);
	verse2 = PadLeadingZeroes(verse2, 2);
	var chapter1_verse1 = chapter1 + verse1;
	var chapter2_verse2 = PadLeadingZeroes(accumulator, 3) + verse2;
	if (chapter2_verse2 <= chapter1_verse1)
	    return false;
	start_end[book_code + chapter1_verse1 + ":" + book_code + chapter2_verse2] = true;
    } else if (state === State.VERSE1) {
	verse1 = PadLeadingZeroes(accumulator, 2);
	accumulator = book_code + chapter1 + verse1;
	start_end[accumulator + ":" + accumulator] = true;
    } else if (state === State.VERSE2) {
	if (accumulator.length === 0)
	    return false;
	verse2 = PadLeadingZeroes(accumulator, 2);
	var start = book_code + chapter1 + verse1;
	var end = book_code + (chapter2.length === 0 ? chapter1 : chapter2) + verse2;
	if (end <= start)
	    return false;
	start_end[start + ":" + end] = true;
    }

    return true;
}

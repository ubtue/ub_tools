// Include "BibleReferenceParser.js" und "BibleBookReferenceParser.js":
var fs = require("fs");
eval(fs.readFileSync("BibleReferenceParser.js").toString());
eval(fs.readFileSync("BibleBookReferenceParser.js").toString());


function IsLetter(ch) {
    return /[a-zäöü]/.test(ch);
}


var RefState = {
    INITIAL: 0,
    BOOK_DIGIT: 1,
    BOOK_LETTERS: 2,
    VERS_OR_CHAPTER: 3
};


/* \brief If successful, sets "bible_book" and "chapters_and_verses" properties on "reference_parts"
 *        and returns true.  O/w returns false.
 */
function SplitAndNormaliseReference(reference_candidate, reference_parts) {
    reference_candidate = reference_candidate.toLowerCase();
    reference_parts.bible_book = "";
    reference_parts.chapters_and_verses = "";
    var dot_seen = false;
    var state = RefState.INITIAL;
    for (var i = 0; i < reference_candidate.length; ++i) {
        var ch = reference_candidate.charAt(i);
	if (ch === " ")
	    continue;

	switch (state) {
	    case RefState.INITIAL:
	        if (IsDigit(ch)) {
		    reference_parts.bible_book += ch;
	            state = RefState.BOOK_DIGIT;
                } else if (IsLetter(ch)) {
		    reference_parts.bible_book += ch;
	            state = RefState.BOOK_LETTERS;
		} else
		    return false;
	        break;
	    case RefState.BOOK_DIGIT:
	        if (ch === ".") {
		    reference_parts.bible_book += ch;
		    state = RefState.BOOK_LETTERS;
		} else if (IsLetter(ch)) {
		    reference_parts.bible_book += ch;
		    state = RefState.BOOK_LETTERS;
                } else
		    return false;
	        break;
	    case RefState.BOOK_LETTERS:
	        if (IsLetter(ch) || ch == "-")
		    reference_parts.bible_book += ch;
	        else if (IsDigit(ch)) {
		    reference_parts.chapters_and_verses += ch;
		    state = RefState.VERS_OR_CHAPTER;
                } else
		    return false;
	        break;
	    case RefState.VERS_OR_CHAPTER:
                if (ch === ":" || ch === ",")
                    reference_parts.chapters_and_verses += ",";
	        else if (IsDigit(ch) || IsLetter(ch) || ch === "-" || ch === ".")
		    reference_parts.chapters_and_verses += ch;
	        else
		    return false;
	        break;
        }
    }

    return true;
}


var progname;


function Usage() {
    console.log("usage: " + progname
		+ " bible_reference_candidate [expected_pair1 expected_pair2 ... expected_pairN]");
    console.log("       When the expected pairs, where start and end have to be separated with a colon, are");
    console.log("       provided, the program returns a non-zero exit code if not all pairs have been matched!");
    process.exit(1);
}


function main(argv) {
    var last_slash_index = argv[1].lastIndexOf("/");
    if (last_slash_index === -1)
	progname = argv[1];
    else
	progname = argv[1].substr(last_slash_index + 1);

    if (argv.length < 3)
	Usage();

    var reference_parts = {};
    if (!SplitAndNormaliseReference(argv[2], reference_parts)) {
	if (argv.length === 3)
            console.log("Bad bible reference: " + argv[2]);
	process.exit(1);
    }

    var book_code;
    if ((book_code = ParseBibleBookReference(reference_parts.bible_book)) == null) {
	if (argv.length === 3)
            console.log("Bad bible book reference: " + reference_parts.bible_book);
	process.exit(1);
    }

    var start_end = {};
    if (!ParseBibleReference(reference_parts.chapters_and_verses, book_code, start_end)) {
	if (argv.length === 3)
            console.log("Bad bible reference: " + argv[2]);
	process.exit(1);
    }

    if (argv.length === 3) {
	for (var prop in start_end) {
	    if (start_end.hasOwnProperty(prop))
		console.log(prop);
	}    
	process.exit(0);
    }

    var matched_count = 0;
    for (var arg_no = 3; arg_no < argv.length; ++arg_no) {
	if (!start_end.hasOwnProperty(argv[arg_no]))
	    process.exit(1);
	++matched_count;
    }

    process.exit(matched_count === Object.keys(start_end).length ? 0 : 1);
}


main(process.argv);

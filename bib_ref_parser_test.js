// Include "BibleReferenceParser.js":
var fs = require("fs");
eval(fs.readFileSync("BibleReferenceParser.js").toString());


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

    var book_code = "01";
    var start_end = {};
    if (!ParseBibleReference(argv[2], book_code, start_end)) {
	if (argv.length === 3)
            console.log("Bad bible reference: " + argv[2]);
	process.exit(1);
    }

    if (argv.length === 3)
	process.exit(0);

    var matched_count = 0;
    for (var arg_no = 3; arg_no < argv.length; ++arg_no) {
	if (!start_end.hasOwnProperty(argv[arg_no]))
	    process.exit(1);
	++matched_count;
    }

    process.exit(matched_count === Object.keys(start_end).length ? 0 : 1);
}


main(process.argv);

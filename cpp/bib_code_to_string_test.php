<?php declare(strict_types=1); // strict mode
$bible_range = $argv[1];
$language_code = $argv[2];


function HasChapter(int $code) {
    return (intdiv($code, 1000) % 1000) != 0;
}


function HasVerse(int $code) {
    return ($code % 1000) != 0;
}


function GetBookCode(int $code) {
    return intdiv($code, 1000000);
}


function GetChapter(int $code) {
    return intdiv($code, 1000) % 1000;
}


function GetVerse(int $code) {
    return $code % 1000;
}


$codes_to_book_abbrevs = array(
     1 => "Mt",
     2 => "Mk",
     3 => "Lk",
     4 => "Jn",
     5 => "Acts",
     6 => "Rom",
     7 => "1 Cor",
     8 => "2 Cor",
     9 => "Gal",
    10 => "Eph",
    11 => "Phil",
    12 => "Col",
    13 => "1 Thess",
    14 => "2 Thess",
    15 => "1 Tim",
    16 => "2 Tim",
    17 => "Titus",
    18 => "Philemon",
    19 => "Heb",
    20 => "Jas",
    21 => "1 Pet",
    22 => "2 Pet",
    23 => "1 Jn",
    24 => "2 Jn",
    25 => "3 Jn",
    26 => "Jude",
    27 => "Rev",
    28 => "Gen",
    29 => "Ex",
    30 => "Lev",
    31 => "Num",
    32 => "Deut",
    33 => "Josh",
    34 => "Judg",
    35 => "Ruth",
    36 => "1 Sam",
    37 => "2 Sam",
    38 => "1 Kings",
    39 => "2 Kings",
    40 => "1 Chr",
    41 => "2 Chr",
    42 => "Ezra",
    43 => "Neh",
    44 => "Eth1",
    45 => "Job",
    46 => "Ps",
    47 => "Prov",
    48 => "Ecc1",
    49 => "Song",
    50 => "Isa",
    51 => "Jer",
    52 => "Lam",
    53 => "Ezek",
    54 => "Dan",
    55 => "Hos",
    56 => "Joel",
    57 => "Am",
    58 => "Obadiah",
    59 => "Jon",
    60 => "Mic",
    61 => "Nah",
    62 => "Hab",
    63 => "Zeph",
    64 => "Hag",
    65 => "Zech",
    66 => "Mal",
    67 => "3 Ezra",
    68 => "4 Ezra",
    69 => "1 Macc",
    70 => "2 Macc",
    71 => "3 Macc",
    72 => "4 Macc",
    73 => "Tob",
    74 => "Jdt",
    75 => "Bar",
    77 => "Sir",
    78 => "Wis",
    81 => "6 Macc",
    82 => "5 Ezra",
    83 => "6 Ezra",
    84 => "",
    85 => "",
);


function DecodeBookCode(int $book_code, string $separator) {
    global $codes_to_book_abbrevs;
    
    $book_code_as_string = $codes_to_book_abbrevs[GetBookCode($book_code)];
    if (!HasChapter($book_code))
        return $book_code_as_string;
    $book_code_as_string .= " " . strval(GetChapter($book_code));
    if (!HasVerse($book_code))
        return $book_code_as_string;
    return $book_code_as_string . $separator . strval(GetVerse($book_code));
}


function BibleRangeToDisplayString(string $bible_range, string $language_code) {
    global $codes_to_book_abbrevs;

    $separator = (substr($language_code, 0, 2) == "de") ? "." : ":"; 
    $code1 = (int)substr($bible_range, 0, 8);
    $code2 = (int)substr($bible_range, 9, 8);

    if ($code1 == $code2)
        return DecodeBookCode($code1, $separator);
    if (GetBookCode($code1) != GetBookCode($code2))
        return DecodeBookCode($code1, $separator) . " – " . DecodeBookCode($code2, $separator);

    $codes_as_string = $codes_to_book_abbrevs[GetBookCode($code1)] . " ";
    $chapter1 = GetChapter($code1);
    $chapter2 = GetChapter($code2);
    if ($chapter1 == $chapter2) {
        $codes_as_string .= strval($chapter1) . $separator;
        return $codes_as_string . strval(GetVerse($code1)) . "–" . strval(GetVerse($code2));
    }
    return $codes_as_string . strval($chapter1) . "–" . strval($chapter2);
}


echo BibleRangeToDisplayString($bible_range, $language_code) . "\n";
?>

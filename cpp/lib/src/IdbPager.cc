#include "IdbPager.h"
#include <stdexcept>
#include <vector>
#include "Downloader.h"
#include "HtmlUtil.h"
#include "StringUtil.h"


const char *roman_numerals[][10] = {
    { "", "i", "ii", "iii", "iv", "v", "vi", "vii", "viii", "ix" },
    { "", "x", "xx", "xxx", "xl", "l", "lx", "lxx", "lxxx", "xc" },
    { "", "c", "cc", "ccc", "cd", "d", "dc", "dcc", "dccc", "cm" },
    { "", "m", "mm", "mmm" }
};


int ipow(int base, int exp) {
    int result(1);
    while (exp) {
        if (exp & 1)
            result *= base;
        exp >>= 1;
        base *= base;
    }

    return result;
}


std::string ToRoman(unsigned u) {
    if (u == 0 or u > 3999)
        throw std::runtime_error("in ToRoman: number too large to convert!");

    std::string roman_numeral;
    for (int power(3); power >= 0; --power) {
        const unsigned base_multiple(ipow(10, power));
        const unsigned digit(u / base_multiple);
        roman_numeral += roman_numerals[power][digit];
        u %= base_multiple;
    }

    return roman_numeral;
}


std::string RomanPageNumberGenerator::getNextPageNumber() {
    return ToRoman(++page_no_);
}


bool IdbPager::getNextPage(const TimeLimit &time_limit, std::string * const ocr_text) {
    if (not Download(base_url_ + number_generator_->getNextPageNumber() + "/ocr",
                 time_limit, ocr_text))
        return false;

    const std::string page_no_start_string("Seite:&nbsp;");
    size_t page_no_start_pos(ocr_text->find(page_no_start_string));
    if (page_no_start_pos == std::string::npos)
        return false;
    if (std::isspace((*ocr_text)[page_no_start_pos + page_no_start_string.length()]))
        return false; // No page number => we've probably gone too far!

    const std::string start_text("<div class=\"viewbox\">");
    size_t start_pos(ocr_text->find(start_text));
    if (start_pos == std::string::npos)
        return false;
    start_pos += start_text.length();

    const std::string end_text("</div>");
    size_t end_pos(ocr_text->find(end_text, start_pos));
    if (end_pos == std::string::npos)
        return false;

    *ocr_text = StringUtil::ReplaceString("<br>", "\n", ocr_text->substr(start_pos, end_pos - start_pos));
    HtmlUtil::ReplaceEntities(ocr_text);

    return true;
}

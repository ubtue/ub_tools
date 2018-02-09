#ifndef IDB_PAGER_H
#define IDB_PAGER_H


#include <string>
#include "TimeLimit.h"


class PageNumberGenerator {
protected:
    unsigned page_no_;
public:
    PageNumberGenerator(): page_no_(0) { }
    virtual ~PageNumberGenerator() { }
    virtual std::string getNextPageNumber() = 0;
};


class ArabicPageNumberGenerator: public PageNumberGenerator {
public:
    virtual std::string getNextPageNumber() { ++page_no_; return std::to_string(page_no_); }
};


class RomanPageNumberGenerator: public PageNumberGenerator {
public:
    virtual std::string getNextPageNumber();
};


class IdbPager {
    PageNumberGenerator * const number_generator_;
    std::string base_url_;
public:
    IdbPager(const std::string &id, PageNumberGenerator * const number_generator)
        : number_generator_(number_generator), base_url_("http://idb.ub.uni-tuebingen.de/diglit/" +id + "/") { }
    bool getNextPage(const TimeLimit &time_limit, std::string * const ocr_text);
};


#endif // ifndef IDB_PAGER_H

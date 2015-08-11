#include "Stemmer.h"
#include <stdexcept>


Stemmer::Stemmer(const std::string &language_name_or_code) {
    if ((stemmer_ = ::sb_stemmer_new(language_name_or_code.c_str(), "UTF_8")) == NULL)
	throw std::runtime_error("in Stemmer::Stemmer: unsuported language or language code \""
				 + language_name_or_code + "\"!");
}


std::string Stemmer::stem(const std::string &word) {
    return std::string((const char *)::sb_stemmer_stem(stemmer_, (const sb_symbol *)word.c_str(), word.size()));
}

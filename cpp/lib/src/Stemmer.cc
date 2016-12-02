#include "Stemmer.h"
#include <stdexcept>
#include <unordered_map>


Stemmer::Stemmer(const std::string &language_name_or_code) {
    if ((stemmer_ = ::sb_stemmer_new(language_name_or_code.c_str(), "UTF_8")) == nullptr)
        throw std::runtime_error("in Stemmer::Stemmer: unsuported language or language code \""
                                 + language_name_or_code + "\"!");
}


std::string Stemmer::stem(const std::string &word) const {
    return std::string((const char *)::sb_stemmer_stem(stemmer_, (const sb_symbol *)word.c_str(), word.size()));
}


const Stemmer *Stemmer::StemmerFactory(const std::string &language_name_or_code) {
    static std::unordered_map<std::string, const Stemmer *> code_to_stemmer_map;

    // First see if we already have a cached Stemmer for the given language name of code:
    const auto code_and_stemmer_iter(code_to_stemmer_map.find(language_name_or_code));
    if (code_and_stemmer_iter != code_to_stemmer_map.end())
        return code_and_stemmer_iter->second;

    Stemmer *new_stemmer(nullptr);
    try {
        new_stemmer = new Stemmer(language_name_or_code);
    } catch (const std::exception &x) {
    }
    code_to_stemmer_map[language_name_or_code] = new_stemmer;

    return new_stemmer;
}

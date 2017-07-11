#ifndef STEMMER_H
#define STEMMER_H


#include <string>
#ifndef LIBSTEMMER_H
#       include "libstemmer.h"
#       define LIBSTEMMER_H
#endif


/** \warning This class is not thread-safe! */
class Stemmer {
    sb_stemmer *stemmer_;

    explicit Stemmer(const std::string &language_name_or_code);
public:
    virtual ~Stemmer() { if (stemmer_) ::sb_stemmer_delete(stemmer_); }

    std::string stem(const std::string &word) const;

    /** \brief Constructs a new Stemmer instance.
     *  \param language_name_or_code  An English name of a language or an international 2- or 3-digit code.
     *  \throws std::runtime_error if an unsupported language name or code was passed in.
     *
     *  Supported languages are Hungarian, Russian, Norwegian, Spanish, Italian, Dutch, English, Swedish,
     *  Portugese, Danish, French, Turkish, Romanian, German, and Finnish.
     *
     * \return A pointer to a stemmer if we were able to construct one, o/w nullptr.
     * \note Caches the returned Stemmer so that it is cheap to repeatedly call this factory with
     *       the same language name or code.
     * \warning  Never delete Stemmers returned by this factory!!
     */
    static const Stemmer *StemmerFactory(const std::string &language_name_or_code);
};


#endif // ifndef STEMMER_H

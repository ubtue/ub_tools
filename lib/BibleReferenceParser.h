#ifndef BIBLE_REFERENCE_PARSER_H
#define BIBLE_REFERENCE_PARSER_H


#include <set>
#include <string>
#include <utility>


/** \brief Parses bible references into ranges.
 *  \param bib_ref_candidate  The hopefully syntactically correct bible chapter(s)/verse(s) reference(s).
 *  \param book_code          A two-digit code indicating the book of the bible.  Will be prepended to any
 *                            recognised chapter/verse references returned in "start_end".
 *  \param start_end          The successfully extracted bible ranges.
 *  \return If the parse succeded or not.
 */
bool ParseBibleReference(const std::string &bib_ref_candidate, const std::string &book_code,
			 std::set<std::pair<std::string, std::string>> * const start_end);


/** \brief Tests the validity of a possible chapter/verse reference. */
bool CanParseBibleReference(const std::string &bib_ref_candidate);


#endif // ifndef BIBLE_REFERENCE_PARSER_H

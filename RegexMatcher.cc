#include "RegexMatcher.h"
#include "util.h"


bool RegexMatcher::utf8_configured_;


bool CompileRegex(const std::string &pattern, const bool enable_utf8, ::pcre **pcre, ::pcre_extra **pcre_extra,
		  std::string * const err_msg)
{
    if (err_msg != NULL)
	err_msg->clear();

    const char *errptr;
    int erroffset;
    *pcre = ::pcre_compile(pattern.c_str(), enable_utf8 ? PCRE_UTF8 : 0, &errptr, &erroffset, NULL);
    if (*pcre == NULL) {
	*pcre = NULL;
	*pcre_extra = NULL;
	if (err_msg != NULL)
	    *err_msg = "failed to compile invalid regular expression: \"" + pattern + "\"! ("
		+ std::string(errptr) + ")";
	return false;
    }

    *pcre_extra = ::pcre_study(*pcre, PCRE_STUDY_JIT_COMPILE, &errptr);
    if (*pcre_extra == NULL and errptr != NULL) {
	::pcre_free(*pcre);
	*pcre = NULL;
	*pcre_extra = NULL;
	if (err_msg != NULL)
	    *err_msg = "failed to \"study\" the compiled pattern \"" + pattern + "\"! (" + std::string(errptr) + ")";
	return false;
    }

    return true;
}


RegexMatcher *RegexMatcher::RegexMatcherFactory(const std::string &pattern, std::string * const err_msg,
						const bool enable_utf8)
{
    // Make sure the PCRE library supports UTF8:
    if (enable_utf8 and not RegexMatcher::utf8_configured_) {
	int utf8_available;
	if (::pcre_config(PCRE_CONFIG_UTF8, reinterpret_cast<void *>(&utf8_available)) == PCRE_ERROR_BADOPTION) {
	    if (err_msg != NULL)
		*err_msg = "PCRE library does not know PCRE_CONFIG_UTF8!";
	    return NULL;
	}

	if (utf8_available != 1) {
	    if (err_msg != NULL)
		*err_msg = "This version of the PCRE library does not support UTF8!";
	    return NULL;
	}

	RegexMatcher::utf8_configured_ = true;
    }

    ::pcre *pcre;
    ::pcre_extra *pcre_extra;
    if (not CompileRegex(pattern, enable_utf8, &pcre, &pcre_extra, err_msg))
	return NULL;

    return new RegexMatcher(pattern, enable_utf8, pcre, pcre_extra);
}


RegexMatcher::RegexMatcher(const RegexMatcher &that): pattern_(that.pattern_) {
    if (this == &that)
	return;

    if (that.pcre_ == NULL) {
	pcre_ = NULL;
	pcre_extra_ = NULL;
    } else {
	std::string err_msg;
	if (not CompileRegex(pattern_, that.utf8_enabled_, &pcre_, &pcre_extra_, &err_msg))
	    Error("In RegexMatcher copy constructor: unexpected error: " + err_msg);
    }
}


RegexMatcher::RegexMatcher(RegexMatcher &&that)
    : pattern_(std::move(that.pattern_)), utf8_enabled_(that.utf8_enabled_), pcre_(that.pcre_),
      pcre_extra_(that.pcre_extra_)
{
    that.pcre_       = NULL;
    that.pcre_extra_ = NULL;
}


bool RegexMatcher::matched(const std::string &s, std::string * const err_msg,
			   std::string::size_type * const start_pos) const
{
    err_msg->clear();

    const int substr_vector_size(60); // must be a multiple of 3
    int substr_vector[substr_vector_size];
    const int retcode = ::pcre_exec(pcre_, pcre_extra_, s.data(), s.length(), 0, 0,
				    substr_vector, substr_vector_size);
    if (retcode > 0) {
	if (start_pos != NULL)
	    *start_pos = retcode;
	return true;
    }

    if (retcode != PCRE_ERROR_NOMATCH) {
	if (retcode == PCRE_ERROR_BADUTF8)
	    *err_msg = "A \"subject\" with invalid UTF-8 was passed into RegexMatcher::matched()!";
	else
	    *err_msg = "Unknown error!";
    }

    return false;
}

/** Regression tests for the PCRE2 regex wrappers. */
#include <atomic>
#include <thread>
#include <vector>
#include "RegexMatcher.h"
#include "UnitTest.h"


TEST(OptionsAndCaptures) {
    static_assert(RegexMatcher::ENABLE_UTF8 == 1 and RegexMatcher::CASE_INSENSITIVE == 2
                  and RegexMatcher::MULTILINE == 4 and RegexMatcher::ENABLE_UCP == 8);
    const ThreadSafeRegexMatcher matcher("(foo)-(bar)?(baz)", ThreadSafeRegexMatcher::CASE_INSENSITIVE);
    size_t start = 0, end = 0;
    const auto result = matcher.match("xx FOO-baz", 0, &start, &end);
    CHECK_TRUE(result);
    CHECK_EQ(start, 3u);
    CHECK_EQ(end, 10u);
    CHECK_EQ(result.size(), 4u);
    CHECK_EQ(result[2], "");
    CHECK_EQ(result[3], "baz");
    const ThreadSafeRegexMatcher many_groups(std::string(150, '(') + "x" + std::string(150, ')'));
    CHECK_EQ(many_groups.match("x")[150], "x");
    std::string error;
    CHECK_TRUE(RegexMatcher::RegexMatcherFactory("(", &error) == nullptr);
    CHECK_FALSE(error.empty());
    CHECK_TRUE(RegexMatcher::RegexMatcherFactory("x", &error, 16) == nullptr);
    CHECK_FALSE(error.empty());
}


TEST(UnicodeAndErrors) {
    const ThreadSafeRegexMatcher matcher("(.)");
    CHECK_EQ(matcher.match("é")[1], "é");
    const auto invalid = matcher.match(std::string(1, '\xff'));
    CHECK_FALSE(invalid);
    CHECK_FALSE(invalid.getErrorMessage().empty());
    CHECK_FALSE(matcher.match("é", 1).getErrorMessage().empty());
    CHECK_TRUE(matcher.match("").getErrorMessage().empty());
    const ThreadSafeRegexMatcher unicode("^\\w+$", ThreadSafeRegexMatcher::ENABLE_UTF8 | ThreadSafeRegexMatcher::ENABLE_UCP);
    CHECK_TRUE(unicode.match("é"));
    const ThreadSafeRegexMatcher multiline("^foo$", ThreadSafeRegexMatcher::MULTILINE);
    CHECK_TRUE(multiline.match("other\nfoo\nlast"));
}


TEST(Replacements) {
    CHECK_EQ(ThreadSafeRegexMatcher("^").replaceAll("abc", "X"), "Xabc");
    CHECK_EQ(ThreadSafeRegexMatcher("$").replaceAll("abc", "X"), "abcX");
    CHECK_EQ(ThreadSafeRegexMatcher("").replaceAll("", "X"), "X");
    CHECK_EQ(ThreadSafeRegexMatcher("").replaceAll("é", "X"), "XéX");
    CHECK_EQ(ThreadSafeRegexMatcher("|a").replaceAll("a", "X"), "XXX");
    CHECK_EQ(ThreadSafeRegexMatcher("a*?").replaceAll("a", "X"), "XXX");
    CHECK_EQ(ThreadSafeRegexMatcher("(*CRLF)").replaceAll("\r\n", "X"), "X\r\nX");
    CHECK_EQ(ThreadSafeRegexMatcher("x").replaceAll("x", "$\\1"), "$\\1");
    const ThreadSafeRegexMatcher lookahead("(?=(.))");
    CHECK_EQ(lookahead.replaceWithBackreferences("éx", "\\1", true), "ééxx");
    CHECK_EQ(lookahead.replaceWithBackreferences("éx", "\\1"), "ééx");
    const ThreadSafeRegexMatcher optional("(foo)-(bar)?(baz)");
    CHECK_EQ(optional.replaceWithBackreferences("foo-baz", "\\3/\\2/\\1"), "baz//foo");
    CHECK_EQ(RegexMatcher::ReplaceAll("^", "abc", "X"), "Xabc");
    CHECK_EQ(RegexMatcher::ReplaceAll("$", "abc", "X"), "abcX");
    CHECK_EQ(RegexMatcher::ReplaceAll("", "", "X"), "X");
    CHECK_EQ(RegexMatcher::ReplaceAll("", "é", "X", RegexMatcher::ENABLE_UTF8), "XéX");
    CHECK_EQ(RegexMatcher::ReplaceAll("|a", "a", "X"), "XXX");
}


TEST(ConcurrentMatching) {
    const ThreadSafeRegexMatcher matcher("(foo)-(bar)?(baz)", ThreadSafeRegexMatcher::CASE_INSENSITIVE);
    std::atomic<bool> success(true);
    std::vector<std::thread> workers;
    for (unsigned i = 0; i < 8; ++i) {
        workers.emplace_back([&matcher, &success] {
            const ThreadSafeRegexMatcher copy(matcher);
            for (unsigned j = 0; j < 200; ++j) {
                if (copy.match("FOO-baz")[3] != "baz"
                    or not RegexMatcher::Matched("foo", "FOO", RegexMatcher::CASE_INSENSITIVE)
                    or RegexMatcher::Matched("foo", "FOO", 0))
                    success = false;
            }
        });
    }
    for (auto &worker : workers)
        worker.join();
    CHECK_TRUE(success.load());
}


TEST_MAIN(RegexMatcher)

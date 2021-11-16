/** \file    MiscUtil.cc
 *  \brief   Implementation of miscellaneous utility functions.
 *  \author  Dr. Johannes Ruscheinski
 *  \author  Dr. Gordon W. Paynter
 */

/*
 *  Copyright 2002-2008 Project iVia.
 *  Copyright 2002-2008 The Regents of The University of California.
 *  Copyright 2016-2021 Universitätsbibliothek Tübingen
 *
 *  This file is part of the libiViaCore package.
 *
 *  The libiViaCore package is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public License as
 *  published by the Free Software Foundation; either version 2 of the License,
 *  or (at your option) any later version.
 *
 *  libiViaCore is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public License
 *  along with libiViaCore; if not, write to the Free Software Foundation, Inc.,
 *  59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "MiscUtil.h"
#include <algorithm>
#include <map>
#include <queue>
#include <set>
#include <stack>
#include <stdexcept>
#include <unordered_set>
#include <cctype>
#include <cxxabi.h>
#include <execinfo.h>
#include <unistd.h>
#include "BSZUtil.h"
#include "Compiler.h"
#include "FileUtil.h"
#include "RegexMatcher.h"
#include "StringUtil.h"
#include "TimeUtil.h"
#include "util.h"


namespace MiscUtil {


char HexDigit(const unsigned value) {
    switch (value) {
    case 0:
        return '0';
    case 1:
        return '1';
    case 2:
        return '2';
    case 3:
        return '3';
    case 4:
        return '4';
    case 5:
        return '5';
    case 6:
        return '6';
    case 7:
        return '7';
    case 8:
        return '8';
    case 9:
        return '9';
    case 0xA:
        return 'A';
    case 0xB:
        return 'B';
    case 0xC:
        return 'C';
    case 0xD:
        return 'D';
    case 0xE:
        return 'E';
    case 0xF:
        return 'F';
    default:
        LOG_ERROR("invalid value " + std::to_string(value) + "!");
    }
}


static char HEX_DIGITS[] = "0123456789aAbBcCdDeEfF";


bool IsHexDigit(const char ch) {
    return std::strchr(HEX_DIGITS, ch) != nullptr;
}


char GeneratePPNChecksumDigit(const std::string &ppn_without_checksum) {
    const unsigned ppn_without_checksum_length(ppn_without_checksum.length());
    if (unlikely(ppn_without_checksum_length != 8 and ppn_without_checksum_length != 9))
        throw std::runtime_error("in MiscUtil::GeneratePPNChecksumDigit: argument's length is neither 8 nor 9!");

    unsigned checksum(0);
    // c.f. https://wiki.dnb.de/pages/viewpage.action?pageId=48139522
    for (unsigned i(0); i < ppn_without_checksum_length; ++i)
        checksum += (ppn_without_checksum_length + 1 - i) * (ppn_without_checksum[i] - '0');
    checksum = (11 - (checksum % 11)) % 11;
    return checksum == 10 ? 'X' : '0' + checksum;
}



bool IsValidPPN(const std::string &ppn_candidate) {
    if (ppn_candidate.length() != BSZUtil::PPN_LENGTH_OLD and ppn_candidate.length() != BSZUtil::PPN_LENGTH_NEW)
        return false;

    for (unsigned i(0); i < ppn_candidate.length() - 1; ++i) {
        if (not StringUtil::IsDigit(ppn_candidate[i]))
            return false;
    }

    return ppn_candidate[ppn_candidate.length() - 1]
           == GeneratePPNChecksumDigit(ppn_candidate.substr(0, ppn_candidate.length() - 1));
}


std::string GetEnv(const char * const name) {
    const char * const value(::getenv(name));
    if (value == nullptr)
        throw std::runtime_error("in MiscUtil::GetEnv: ::getenv(\"" + std::string(name) + "\") failed!");

    return value;
}


std::string SafeGetEnv(const char * const name) {
    const char * const value(::getenv(name));
    return value == nullptr ? "" : value;
}


std::string GetPassword(const std::string &prompt) {
    errno = 0;
    const std::string password(::getpass((prompt + " > ").c_str()));
    if (errno != 0)
        LOG_ERROR("failed to read the password from the terminal!");

    return password;
}


void SetEnv(const std::string &name, const std::string &value, const bool overwrite) {
    if (unlikely(::setenv(name.c_str(), value.c_str(), overwrite ? 1 : 0) != 0))
        throw std::runtime_error("in MiscUtil::SetEnv: setenv(3) failed!");
}


void SetEnvFromFile(const std::string &file, const bool overwrite) {
    for (const auto line : FileUtil::ReadLines(file)) {
        std::string key, value;
        if (StringUtil::SplitOnString(line, "=", &key, &value))
            MiscUtil::SetEnv(key, value, overwrite);
        else
            LOG_WARNING("could not set environment variable in " + file + ": " + line);
    }
}


enum EscapeState { NOT_ESCAPED, SINGLE_QUOTED, DOUBLE_QUOTED };


// Helper for ParseLine(),
static std::string ExtractBourneString(std::string::const_iterator &ch, const std::string::const_iterator &end, const char barrier) {
    std::string value;

    bool backslash_seen(false);
    EscapeState state(NOT_ESCAPED);
    for (;;) {
        if (ch == end)
            return value;

        if (state == NOT_ESCAPED) {
            if (backslash_seen) {
                value += *ch;
                backslash_seen = false;
            } else if (*ch == '\'')
                state = SINGLE_QUOTED;
            else if (*ch == '"')
                state = DOUBLE_QUOTED;
            else if (*ch == '\\')
                backslash_seen = true;
            else if (*ch == barrier)
                return value;
            else
                value += *ch;
        } else if (state == SINGLE_QUOTED) {
            if (*ch == '\'')
                state = NOT_ESCAPED;
            else
                value += *ch;
        } else if (state == DOUBLE_QUOTED) {
            if (backslash_seen) {
                if (*ch != '"' and *ch != '\\')
                    value += '\\';
                value += *ch;
                backslash_seen = false;
            } else if (*ch == '\\')
                backslash_seen = true;
            else if (*ch == '"')
                state = NOT_ESCAPED;
            else
                value += *ch;
        }

        ++ch;
    }

    return value;
}


static bool ExportsParseLine(std::string line, std::string * const key, std::string * const value) {
    key->clear(), value->clear();

    StringUtil::TrimWhite(&line);
    if (not StringUtil::StartsWith(line, "export"))
        return true;

    // Remove leading "export":
    line = line.substr(__builtin_strlen("export"));

    StringUtil::TrimWhite(&line);
    auto ch(line.cbegin());
    *key = ExtractBourneString(ch, line.cend(), '=');
    if (key->empty())
        return false;
    ++ch;
    *value = ExtractBourneString(ch, line.cend(), '#');
    return true;
}


void LoadExports(const std::string &path, const bool overwrite) {
    std::unique_ptr<File> input(FileUtil::OpenInputFileOrDie(path));
    unsigned line_no(0);
    while (not input->eof()) {
        ++line_no;

        std::string line;
        input->getline(&line);
        std::string key, value;
        if (not ExportsParseLine(line, &key, &value))
            LOG_ERROR("failed to parse an export statement on line #" + std::to_string(line_no) + " in \"" + input->getPath()
                  + "\"!");
        if (not key.empty())
            SetEnv(key, value, overwrite);
    }
}



bool EnvironmentVariableExists(const std::string &name) {
    const char * const value(::getenv(name.c_str()));
    return value != nullptr;
}


std::string GetUserName() {
    char username[200];
    if (unlikely(::getlogin_r(username, sizeof username) != 0))
        return "*unknown user* [" + std::string(::strerror(errno)) + "]";
    return username;
}


const ThreadSafeRegexMatcher DOI_MATCHER("^(?:http[s]?://[^/]+/|doi:)?(10[.][0-9]{4,}(?:[.][0-9]+)*/(?:(?![\"&\\'<>])\\S)+)$");


bool IsDOI(const std::string &doi_candidate) {
    return DOI_MATCHER.match(doi_candidate);
}


bool NormaliseDOI(const std::string &doi_candidate, std::string * const normalised_doi) {
    if (not DOI_MATCHER.match(doi_candidate))
        return false;

    *normalised_doi = StringUtil::ASCIIToLower(DOI_MATCHER.match(doi_candidate)[1]);
    return true;
}


const ThreadSafeRegexMatcher CONTAINS_DOI_MATCHER("(10.\\d{4,9}/[-._;()/:A-Za-z0-9]+)");


bool ContainsDOI(const std::string &contains_doi_candidate) {
    return CONTAINS_DOI_MATCHER.match(contains_doi_candidate);
}

std::string extractDOI(const std::string &extract_doi_candidate) {
     const auto match(CONTAINS_DOI_MATCHER.match(extract_doi_candidate));
     return match ? match[1] : "";
}

bool IsPossibleISSN(std::string issn_candidate) {
    if (issn_candidate.length() != 8 and issn_candidate.length() != 9)
        return false;
    if (issn_candidate.length() == 9) {
        if (issn_candidate[4] != '-')
            return false;
        issn_candidate = issn_candidate.substr(0, 4) + issn_candidate.substr(5, 4); // Remove hyphen.
    }

    //
    // For an explanation of how to determine the checksum digit, have a look at
    // https://en.wikipedia.org/wiki/International_Standard_Serial_Number#Code_format&oldid=767018094
    //

    unsigned sum(0), position(8);
    for (unsigned i(0); i < 7; ++i) {
        const char ch(issn_candidate[i]);
        if (not StringUtil::IsDigit(ch))
            return false;
        sum += position * (ch - '0');
        --position;
    }
    const unsigned modulus(sum % 11);

    char check_digit;
    if (modulus == 0)
        check_digit = '0';
    else {
        unsigned digit(11 - modulus);
        if (digit == 10)
            check_digit = 'X';
        else
            check_digit = '0' + digit;
    }

    return std::toupper(issn_candidate[7]) == check_digit;
}


bool NormaliseISSN(const std::string &issn_candidate, std::string * const normalised_issn) {
    if (issn_candidate.length() == 9) {
        *normalised_issn = issn_candidate;
        return true;
    }

    if (issn_candidate.length() == 8) {
        *normalised_issn = issn_candidate.substr(0, 4) + '-' + issn_candidate.substr(4, 4);
        return true;
    }

    return false;
}


bool NormaliseISBN(const std::string &isbn_candidate, std::string * const normalised_isbn) {
    for (auto ch : isbn_candidate) {
        if (ch != '-')
            *normalised_isbn += ch;
    }
    return (normalised_isbn->length() == 10) or (normalised_isbn->length() == 13);
}


std::string StringMapToString(const std::map<std::string, std::string> &map) {
    std::string map_as_string;
    for (const auto &key_and_value : map)
        map_as_string += key_and_value.first + "=" + key_and_value.second + ", ";

    return map_as_string.empty() ? "[]" : "[" + map_as_string.substr(0, map_as_string.length() - 2) + "]";
}


static unsigned GetLogSuffix(const std::string &log_file_prefix, const std::string &filename) {
    if (filename == log_file_prefix)
        return 0;

    unsigned generation;
    if (unlikely(not StringUtil::ToUnsigned(filename.substr(log_file_prefix.length() + 1), &generation)))
        LOG_ERROR("in GetLogSuffix: bad conversion, filename = \"" + filename + "\"!");

    return generation;
}


class LogCompare {
    const std::string log_file_prefix_;
public:
    explicit LogCompare(const std::string &log_file_prefix) : log_file_prefix_(log_file_prefix) { }
    bool operator()(const std::string &filename1, const std::string &filename2)
        { return GetLogSuffix(log_file_prefix_, filename1) < GetLogSuffix(log_file_prefix_, filename2); }
};


static std::string IncrementFile(const std::string &log_file_prefix, const std::string &filename) {
    if (filename == log_file_prefix)
        return log_file_prefix + ".1";

    return log_file_prefix + "." + std::to_string(GetLogSuffix(log_file_prefix, filename) + 1);
}


void LogRotate(const std::string &log_file_prefix, const unsigned max_count) {
    std::string dirname, basename;
    FileUtil::DirnameAndBasename(log_file_prefix, &dirname, &basename);
    if (basename.empty()) {
        basename = dirname;
        dirname  = ".";
    }
    if (dirname.empty())
        dirname = ".";

    std::vector<std::string> filenames;
    FileUtil::Directory directory(dirname, "^" + basename + "(\\.[0-9]+)?$");
    for (const auto entry : directory) {
        if (entry.getType() == DT_REG)
            filenames.emplace_back(entry.getName());;
    }

    std::sort(filenames.begin(), filenames.end(), LogCompare(basename));

    if (max_count > 0) {
        while (filenames.size() > max_count) {
            const std::string path_to_delete(dirname + "/" + filenames.back());
            if (unlikely(not FileUtil::DeleteFile(path_to_delete)))
                LOG_ERROR("failed to delete \"" + path_to_delete + "\"!");
            filenames.pop_back();
        }
    }

    for (auto filename(filenames.rbegin()); filename != filenames.rend(); ++filename) {
        if (unlikely(not FileUtil::RenameFile(dirname + "/" + *filename,
                                              dirname + "/" + IncrementFile(basename, *filename))))
            LOG_ERROR("failed to rename \"" + dirname + "/" + *filename + "\" to \"" + dirname + "/"
                      + IncrementFile(basename, *filename) + "\"!");
    }
}


// Returns false if the edges are not labelled from 0 to N-1 where N is the number of vertices, o/w returns true.
static bool NodeNumberingIsCorrect(const std::vector<std::pair<unsigned, unsigned>> &edges, std::unordered_set<unsigned> * const nodes) {
    int max_node(-1);
    for (const auto &edge : edges) {
        nodes->emplace(edge.first);
        if (static_cast<int>(edge.first) > max_node)
            max_node = edge.first;
        nodes->emplace(edge.second);
        if (static_cast<int>(edge.second) > max_node)
            max_node = edge.second;
    }

    return max_node == static_cast<int>(nodes->size() - 1);
}


static void ConstructShortestPath(const std::vector<int> &parents, unsigned s, std::vector<unsigned> * const cycle) {
    while (parents[s] != -1)  {
        cycle->emplace_back(s);
        s = parents[s];
    }

    cycle->emplace_back(s);
    std::reverse(cycle->begin(), cycle->end());
}


bool FindACycleInGraph(const std::vector<std::pair<unsigned, unsigned>> &edges, std::vector<unsigned> * const cycle) {
    cycle->clear();
    if (edges.empty())
        return false;

    std::unordered_set<unsigned> nodes;
    if (not NodeNumberingIsCorrect(edges, &nodes))
        LOG_ERROR("we don't have the required 0..N-1 labelling of nodes!");

    std::vector<std::vector<unsigned>> neighbours(nodes.size());
    for (const auto &edge : edges)
        neighbours[edge.first].emplace_back(edge.second);

    std::vector<bool> visited(nodes.size(), false);

    // Iterative DFS (https://en.wikipedia.org/wiki/Depth-first_search#Pseudocode):
    unsigned v(0); // NB., "0" is a valid vertex!
    std::stack<unsigned> stack;
    stack.push(v);
    while (not stack.empty()) {
        v = stack.top();
        stack.pop();
        if (visited[v])
            goto found_a_cycle;
        visited[v] = true;
        for (const auto neighbour : neighbours[v])
            stack.push(neighbour);
    }
    return false; // No cycle was found.

found_a_cycle:
    // Use BFS (https://www.geeksforgeeks.org/shortest-path-weighted-graph-weight-edge-1-2/) to find the nodes in the cycle:
    visited = std::vector<bool>(nodes.size(), false); // Reset to all false entries.
    std::vector<int> parents(nodes.size(), -1);
    std::queue<unsigned> queue;
    for (unsigned n : neighbours[v])
        queue.push(n);
    while (not queue.empty()) {
        unsigned s(queue.front());
        if (s == v) {
            ConstructShortestPath(parents, s, cycle);
            return true;
        }

        for (const auto neighbour : neighbours[s]) {
            if (not visited[neighbour]) {
                visited[neighbour] = true;
                queue.push(neighbour);
                parents[neighbour] = static_cast<int>(s);
            }
        }

        queue.pop();
    }

    return true;
}


bool TopologicalSort(const std::vector<std::pair<unsigned, unsigned>> &edges, std::vector<unsigned> * const node_order,
                     std::vector<unsigned> * const cycle)
{
    std::unordered_set<unsigned> nodes;
    if (not NodeNumberingIsCorrect(edges, &nodes))
        LOG_ERROR("we don't have the required 0..N-1 labelling of nodes!");

    std::vector<std::vector<unsigned>> neighbours(nodes.size());
    std::vector<unsigned> indegrees(nodes.size());
    for (const auto &edge : edges) {
        neighbours[edge.first].emplace_back(edge.second);
        ++indegrees[edge.second];
    }

    // Enqueue all nodes with no incident edges:
    std::queue<unsigned> queue;
    for (unsigned node(0); node < nodes.size(); ++node) {
        if (indegrees[node] == 0)
            queue.push(node);
    }

    unsigned visited_node_count(0);
    while (not queue.empty()) {
        const unsigned current_node(queue.front());
        node_order->emplace_back(current_node);
        queue.pop();

        for (auto neighbour : neighbours[current_node]) {
            if (--indegrees[neighbour] == 0)
                queue.push(neighbour);
        }

        ++visited_node_count;
    }

    if (visited_node_count == nodes.size())
        return true;

    // If we get here we have at least one cycle!
    if (cycle != nullptr)
        FindACycleInGraph(edges, cycle);

    return false;
}


std::vector<std::string> GetCallStack() {
    std::vector<std::string> call_stack;

    const int MAX_ADDR_COUNT(100);
    char buffer[MAX_ADDR_COUNT * sizeof(void *)];
    const int address_count(::backtrace((void **)buffer, MAX_ADDR_COUNT));
    char **symbols(backtrace_symbols(reinterpret_cast<void **>(buffer), address_count));
    for (int addr_no(0); addr_no < address_count; ++addr_no) {
        char *symbol_start(std::strchr(symbols[addr_no], '('));
        if (symbol_start == nullptr)
            continue;
        ++symbol_start; // Skip over the opening parenthesis.
        // Symbols end at a plus sign which is followed by an address.
        char *plus(std::strchr(symbol_start, '+'));
        if (plus == nullptr)
            continue;
        *plus = '\0';
        int status;
        char *demangled_name(abi::__cxa_demangle(symbol_start, nullptr, nullptr, &status));
        if (status == 0)
            call_stack.emplace_back(demangled_name);
        ::free(reinterpret_cast<void *>(demangled_name));
    }
    ::free(reinterpret_cast<void *>(symbols));

    return call_stack;
}


std::string MakeOrdinal(const unsigned number) {
    if (number % 10 == 1)
        return StringUtil::ToString(number) + "st";
    if (number % 10 == 2)
        return StringUtil::ToString(number) + "nd";
    if (number % 10 == 3)
        return StringUtil::ToString(number) + "rd";
    return StringUtil::ToString(number) + "th";
}


// Implementation taken from https://en.wikibooks.org/wiki/Algorithm_Implementation/Strings/Levenshtein_distance#C++
template <typename StringType> unsigned LevenshteinDistance(const StringType &s1, const StringType &s2) {
    const std::size_t len1(s1.size()), len2(s2.size());
    std::vector<unsigned int> column(len2 + 1), previous_column(len2 + 1);

    for (unsigned i(0); i < previous_column.size(); ++i)
        previous_column[i] = i;
    for (unsigned i(0); i < len1; ++i) {
        column[0] = i + 1;
        for (unsigned j(0); j < len2; ++j)
            // note that std::min({arg1, arg2, arg3}) works only in C++11,
            // for C++98 use std::min(std::min(arg1, arg2), arg3)
            column[j + 1] = std::min({ previous_column[1 + j] + 1, column[j] + 1, previous_column[j] + (s1[i] == s2[j] ? 0 : 1) });
        column.swap(previous_column);
    }
    return previous_column[len2];
}


unsigned LevenshteinDistance(const std::string &s1, const std::string &s2) {
    return LevenshteinDistance<std::string>(s1, s2);
}


unsigned LevenshteinDistance(const std::wstring &s1, const std::wstring &s2) {
    return LevenshteinDistance<std::wstring>(s1, s2);
}


bool AddToPATH(const std::string &new_directory_path, const PreferredPathLocation preferred_path_location) {
    if (new_directory_path.empty() or new_directory_path[0] != '/')
        LOG_ERROR("we only support adding absolute paths to $PATH!");

    const auto PATH(SafeGetEnv("PATH"));

    std::vector<std::string> path_components;
    StringUtil::Split(PATH, ':', &path_components);
    if (std::find(path_components.cbegin(), path_components.cend(), new_directory_path) != path_components.cend())
        return false;

    if (preferred_path_location == PreferredPathLocation::LEADING)
        SetEnv("PATH", new_directory_path + ":" + PATH);
    else
        SetEnv("PATH", PATH + ":" + new_directory_path);

    return true;
}


std::string GenerateAddress(const std::string &optional_first_name, const std::string &optional_last_name,
                            const std::string &fallback)
{
    if (optional_first_name.empty())
        return optional_last_name.empty() ? fallback : optional_last_name;
    else if (optional_last_name.empty())
        return optional_first_name.empty() ? fallback : optional_first_name;
    return optional_first_name + " " + optional_last_name;
}


// The files in the proc filesystem all have a file size of 0 even if they are not empty.
std::string ReadProcEntry(const std::string &path) {
    std::string file_contents;
    File input(path, "r");
    for (;;) {
        char byte;
        if (input.read(&byte, 1) == 1)
            file_contents += byte;
        else
            return file_contents;
    }
}


std::string NormalizeName(const std::string &name) {
    static const auto name_with_trailing_initials_matcher(
        RegexMatcher::RegexMatcherFactoryOrDie("([\\p{L}-]+, ?)(\\p{L}\\.){2,}(.)"));
    if (not name_with_trailing_initials_matcher->matched(name))
        return name;

    std::string modified_name;
    modified_name.reserve(name.size() + 3);
    bool insert_spaces_after_periods(false);
    for (auto ch(name.cbegin()); ch != name.cend(); ++ch) {
        if (*ch == ',')
            insert_spaces_after_periods = true;
        modified_name += *ch;
        if (insert_spaces_after_periods and *ch == '.' and ch != name.cend() - 1 and *(ch + 1) != ' ')
            modified_name += ' ';
    }

    return modified_name;
}


} // namespace MiscUtil

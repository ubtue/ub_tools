/** \file    MiscUtil.h
 *  \brief   Declarations of miscellaneous utility functions.
 *  \author  Dr. Johannes Ruscheinski
 */

/*
 *  Copyright 2002-2009 Project iVia.
 *  Copyright 2002-2009 The Regents of The University of California.
 *  Copyright 2016-2021 Universitätsbibliothek Tübingen.
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
#pragma once


#include <algorithm>
#include <map>
#include <set>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>


namespace MiscUtil {


/** \brief  Convert a numeric value between 0 and 15 to a hex digit.
 *  \param  value  A number between 0 and 15.
 *  \return The character representing the hexadecimal value.
 */
char HexDigit(const unsigned value);


bool IsHexDigit(const char ch);


/** Given a Pica Production Number w/o the checksum digit, this function will return said digit. */
char GeneratePPNChecksumDigit(const std::string &ppn_without_checksum_digit);


/** \return True if "ppn_candidate" is a possible Pica Production Number, else false. */
bool IsValidPPN(const std::string &ppn_candidate);


/** \brief  A safe (i.e. throws on error) wrapper around getenv(3).
 *  \param  name  The name of an environment variable.
 *  \return The value of the environment variable if set (else throws an exception).
 */
std::string GetEnv(const char * const name);
inline std::string GetEnv(const std::string &name) {
    return GetEnv(name.c_str());
}


/** \brief  A safe wrapper around getenv(3).
 *  \param  name  The name of an environment variable.
 *  \return The value of the environment variable if set otherwise the empty string.
 */
std::string SafeGetEnv(const char * const name);
inline std::string SafeGetEnv(const std::string &name) {
    return SafeGetEnv(name.c_str());
}


std::string GetPassword(const std::string &prompt);


/** \brief Checks for existence of an environment variable
 *  \param  name  The name of an environment variable.
 *  \return true of the variable exists, false otherwise.
 */
bool EnvironmentVariableExists(const std::string &name);


/** \brief A wrapper around setenv(3).
 *  \param name      The name of the environment variable.
 *  \param value     The value of the environment variable.
 *  \param overwrite Whether or not the current value for the given environment variable may be overwritten.
 *  \return          True if the addition of "name=value" is successful.
 */
void SetEnv(const std::string &name, const std::string &value, const bool overwrite = true);

/** \brief Parse environment file and set all contained variables.
 *  \note  File structure: KEY=VALUE, e.g. "ZTS_PROXY=http://localhost:3128"
 *  \param file      The path of the file
 *  \param overwrite Whether or not the given environment variables may be overwritten.
 */
void SetEnvFromFile(const std::string &file, const bool overwrite = true);


/** \brief Expects Bourne Shell "export" statments in "path" and calls setenv(3) on any export found.
 *  \param overwrite Whether or not the current values of environment variables should be overwritten.
 *  \note Aborts if we fail to parse any line starting w/ "export.
 */
void LoadExports(const std::string &path, const bool overwrite = true);


// \return The POSIX login name. */
std::string GetUserName();


/** \brief Check if the string is a DOI, i.e. a Digital Object Identifier, e.g. 10.1109/5.771073
 *  \see   https://en.wikipedia.org/wiki/Digital_Object_Identifier
 */
bool IsDOI(const std::string &doi_candidate);


/** \brief Converts a DOI to its canonical format.
 *  \return True if the input was a valid DOI.
 */
bool NormaliseDOI(const std::string &doi_candidate, std::string * const normalised_doi);

/** \brief Check if a string contains a DOI pattern
 *  \return True if the input contains a valid DOI.
 */

bool ContainsDOI(const std::string &contains_doi_candidate);


/** \brief Extract DOI pattern from string
 *  \return The DOI pattern
 */

std::string extractDOI(const std::string &extract_doi_candidate);


bool IsPossibleISSN(std::string issn_candidate);


/** \brief Converts an ISSN to the XXXXYYYY format w/o hyphens.
 *  \return True if the input had a length of 8 or 9 and false o/w.
 */
bool NormaliseISSN(const std::string &issn_candidate, std::string * const normalised_issn);


/** \brief Strips hyphens from ISBN's
 *  \return True if the stripped input had a length of 10 or 13 and false o/w.
 */
bool NormaliseISBN(const std::string &isbn_candidate, std::string * const normalised_isbn);


// \return a string representation of "map".
std::string StringMapToString(const std::map<std::string, std::string> &map);


// \return The number of bits that are set in "mask".
inline unsigned HammingWeight(unsigned mask) {
    return static_cast<unsigned>(__builtin_popcount(mask));
}
inline unsigned HammingWeight(unsigned long mask) {
    return static_cast<unsigned>(__builtin_popcountl(mask));
}
inline unsigned HammingWeight(unsigned long long mask) {
    return static_cast<unsigned>(__builtin_popcountll(mask));
}


/** \brief Renames a set of log files with a common basename.
 *  \param log_file_prefix  We're looking for this followed by no suffix, ".1", ".2" etc.
 *  \param max_count        If positive, we remove older logs that would cause more than "max_count" log files
 *                          to exist.
 *
 *  \note If there are access permission problems we abort. OTOH, a non-existent logfile is will be ignored.
 */
void LogRotate(const std::string &log_file_prefix, const unsigned max_count = 0);


/** \brief  Performs a topological sort.
 *  \param  edges       The edges defining our graph.
 *  \param  node_order  The sorted list of nodes will be stored here if the sort succeeded.
 *  \param  cycle       If not NULL and if at least one cycle exists in the graph, a node order of a cycle will be returned here.
 *  \return True if no cycle exists in the input graph and false o/w.
 *  \note   Nodes must be labelled 0 to N-1 (in any order) where N is the number of nodes.
 */
bool TopologicalSort(const std::vector<std::pair<unsigned, unsigned>> &edges, std::vector<unsigned> * const node_order,
                     std::vector<unsigned> * const cycle = nullptr);


// \return the list of functions that were called at the point of invocation of this function.
std::vector<std::string> GetCallStack();


/** \return "1st" for 1, "2nd" for 2 etc. */
std::string MakeOrdinal(const unsigned number);


unsigned LevenshteinDistance(const std::string &s1, const std::string &s2);
unsigned LevenshteinDistance(const std::wstring &s1, const std::wstring &s2);


template <typename ElementType>
std::set<ElementType> Intersect(const std::set<ElementType> &set1, const std::set<ElementType> &set2) {
    std::set<ElementType> result;
    std::set_intersection(set1.cbegin(), set1.cend(), set2.cbegin(), set2.cend(), std::inserter(result, result.begin()));
    return result;
}


template <typename ElementType>
std::vector<ElementType> Intersect(const std::vector<ElementType> &set1, const std::vector<ElementType> &set2) {
    std::vector<ElementType> result;
    std::set_intersection(set1.cbegin(), set1.cend(), set2.cbegin(), set2.cend(), std::inserter(result, result.begin()));
    return result;
}


template <typename ElementType>
std::set<ElementType> Intersect(const std::set<ElementType> &set1, const std::unordered_set<ElementType> &set2) {
    std::vector<ElementType> sorted_set2;
    sorted_set2.reserve(set2.size());
    std::copy(set2.cbegin(), set2.cend(), std::back_inserter(sorted_set2));
    std::sort(sorted_set2.begin(), sorted_set2.end());

    std::set<ElementType> result;
    std::set_intersection(set1.cbegin(), set1.cend(), sorted_set2.cbegin(), sorted_set2.cend(), std::inserter(result, result.begin()));
    return result;
}


template <typename ElementType>
std::set<ElementType> Intersect(const std::unordered_set<ElementType> &set1, const std::unordered_set<ElementType> &set2) {
    std::vector<ElementType> sorted_set1;
    sorted_set1.reserve(set1.size());
    std::copy(set1.cbegin(), set1.cend(), std::back_inserter(sorted_set1));
    std::sort(sorted_set1.begin(), sorted_set1.end());

    std::vector<ElementType> sorted_set2;
    sorted_set2.reserve(set2.size());
    std::copy(set2.cbegin(), set2.cend(), std::back_inserter(sorted_set2));
    std::sort(sorted_set2.begin(), sorted_set2.end());

    std::set<ElementType> result;
    std::set_intersection(sorted_set1.cbegin(), sorted_set1.cend(), sorted_set2.cbegin(), sorted_set2.cend(),
                          std::inserter(result, result.begin()));
    return result;
}


/** \return of "container1" ⟍  "container2" */
template <typename ContainerType>
ContainerType AbsoluteComplement(const ContainerType &container1, const ContainerType &container2) {
    ContainerType result;
    for (const auto &element : container1) {
        if (container2.find(element) == container2.end())
            result.emplace(element);
    }
    return result;
}


enum class PreferredPathLocation { LEADING, TRAILING };


/** \brief Add a new component to $PATH.
 *  \note  The new path component will not be added if it is already a part of $PATH.
 *  \return True if we added the new path componnent and false if it was already part of $PATH.
 */
bool AddToPATH(const std::string &new_directory_path, const PreferredPathLocation preferred_path_location);


/** Search for key in container and return its value, or default value if not found. */
template <typename ContainerType, typename KeyType, typename ValueType>
ValueType GetContainerValueOrDefault(const ContainerType &container, const KeyType &key, const ValueType &default_value,
                                     bool * const default_returned = nullptr) {
    const auto key_and_value(container.find(key));
    if (key_and_value != container.cend()) {
        if (default_returned != nullptr)
            *default_returned = false;
        return key_and_value->second;
    } else {
        if (default_returned != nullptr)
            *default_returned = true;
        return default_value;
    }
}


/** \returns optional_first_name + " " + optional_last_name if both are non-empty,
 *           optional_last_name or optional_first_name if one of the two is empty and
 *           fallback, if both are empty.
 */
std::string GenerateAddress(const std::string &optional_first_name, const std::string &optional_last_name, const std::string &fallback);


// Entries in /proc require special handling to read them.
// This is what this function provides.  "path" must be an absolute path.
std::string ReadProcEntry(const std::string &path);


// Currently only cleans up names w/ trailing initials, e.g.
//   Müller, Fritz A.B. => Müller, Fritz A. B.
// Doesn't change anything if no comma is found or there are no initials.
// Also doesn't add a space after the final initial.
std::string NormalizeName(const std::string &name);


} // namespace MiscUtil

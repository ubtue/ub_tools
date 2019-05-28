/** \file    iViaCore-mkdep.cc
 *  \brief   Creates makefile dependencies.
 *  \author  Dr. Johannes Ruscheinski
 *  \author  Anthony Moralez
 */

/*
 *  Copyright 2002-2008 Project iVia.
 *  Copyright 2002-2008 The Regents of The University of California.
 *  Copyright 2019 Universitätsbibliothek Tübingen.
 *
 *  This file is part of the libiViaCore package.
 *
 *  The libiViaCore package is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License as published
 *  by the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  libiViaCore is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with libiViaCore; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <iostream>
#include <list>
#include <map>
#include <stack>
#include <stdexcept>
#include <string>
#include <fstream>
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <cstddef>
#include <dirent.h>
#include <fnmatch.h>
#include <getopt.h>
#include <sys/stat.h>
#include <unistd.h>


/** \brief   Remove all occurences of a character from the end of a string. */
inline char *StringUtil_strrtrim(char *s, char trim_char=' ') {
    size_t len = std::strlen(s);
    char *cp = s + len;
    while (--cp >= s && *cp == trim_char)
	/* intentionally empty */;
    *(cp + 1) = '\0';

    return s;
}


/** \brief  Split a string around a delimiter string
 *  \param  source            The string to split
 *  \param  delimiter_string  The string to split around
 *  \param  fields            A list to return the resulting fields in
 *
 *  Splits "source" around the characters in "field_separators" and return
 *  the resulting list of fields in "fields."  Empty fields are returned in the list.
 *
 *  \return The number of extracted "fields".
 */
template<typename BackInsertContainer> unsigned StringUtil_Split(const std::string &source, const std::string &delimiter_string,
								 BackInsertContainer * const fields)
{
    fields->clear();
    if (source.empty())
	return 0;

    std::string::size_type start = 0;
    std::string::size_type next_delimiter = 0;
    unsigned count = 0;

    while (next_delimiter != std::string::npos) {
	// Search for first occurence of delimiter that appears after start
	next_delimiter = source.find(delimiter_string, start);

	// Add the field starting at start and ending at next_delimiter
	if (next_delimiter == std::string::npos) {
	    fields->push_back(source.substr(start));
	    ++count;
	} else if (next_delimiter > start) {
	    fields->push_back(source.substr(start, next_delimiter - start));
	    ++count;
	}

	// Move the start pointer along the array
	if (next_delimiter != std::string::npos)
	    start = next_delimiter + delimiter_string.length();
	if (start >= source.length())
	    next_delimiter = std::string::npos;
    }

    return count;
}


void FileUtil_MakeCanonicalPathList(const char * const path, std::list<std::string> * const canonical_path_list) {
    const char *cp = path;
    if (*cp == '/') {
	canonical_path_list->push_back("/");
	++cp;
    }
    while (*cp != '\0') {
	std::string directory;
	while (*cp != '\0' and *cp != '/')
	    directory += *cp++;
	if (*cp == '/')
	    ++cp;

	if (directory.length() == 0 or directory == ".")
	    continue;

	if (directory == ".." and !canonical_path_list->empty()) {
	    if (canonical_path_list->size() != 1 or canonical_path_list->front() != "/")
		canonical_path_list->pop_back();
	}
	else
	    canonical_path_list->push_back(directory);
    }
}


// FileUtil_DirnameAndBasename -- Split a path into a directory name part and filename part.
//
void FileUtil_DirnameAndBasename(const std::string &path, std::string * const dirname, std::string * const basename) {
    if (path.length() == 0) {
	*dirname = *basename = "";
	return;
    }

    std::string::size_type last_slash_pos = path.rfind('/');
    if (last_slash_pos == std::string::npos) {
	*dirname  = "";
	*basename = path;
    }
    else {
	*dirname  = path.substr(0, last_slash_pos);
	*basename = path.substr(last_slash_pos+1);
    }
}


std::string FileUtil_MakeAbsolutePath(const std::string &reference_path, const std::string &relative_path) {
    if (relative_path[0] == '/' or reference_path.length() == 0)
	return relative_path;

    std::string reference_dirname, reference_basename;
    FileUtil_DirnameAndBasename(reference_path, &reference_dirname, &reference_basename);
    std::list<std::string> resultant_dirname_components;
    FileUtil_MakeCanonicalPathList(reference_dirname.c_str(), &resultant_dirname_components);

    std::string relative_dirname, relative_basename;
    FileUtil_DirnameAndBasename(relative_path, &relative_dirname, &relative_basename);
    std::list<std::string> relative_dirname_components;
    FileUtil_MakeCanonicalPathList(relative_dirname.c_str(), &relative_dirname_components);

    // Now merge the two canonical path lists.
    for (std::list<std::string>::const_iterator component(relative_dirname_components.begin());
	 component != relative_dirname_components.end(); ++component)
	{
	    if (*component == ".." and (resultant_dirname_components.size() > 1 or
					resultant_dirname_components.front() != "/"))
		resultant_dirname_components.pop_back();
	    else
		resultant_dirname_components.push_back(*component);
	}

    // Build the final path:
    std::string canonized_path;
    std::list<std::string>::const_iterator dir(resultant_dirname_components.begin());
    if (dir != resultant_dirname_components.end() and *dir == "/") {
	canonized_path = "/";
	++dir;
    }
    for (/* empty */; dir != resultant_dirname_components.end(); ++dir)
	canonized_path += *dir + "/";
    canonized_path += relative_basename;

    return canonized_path;
}


std::string FileUtil_MakeRelativePath(const std::string &reference_path, const std::string &path) {
    if (reference_path.empty() or reference_path[0] != '/')
	throw std::runtime_error("in FileUtil::MakeRelativePath: the reference path must be non-empty and "
				 "absolute!");
    if (path.empty() or path[0] != '/')
	throw std::runtime_error("in FileUtil::MakeRelativePath: the path must be non-empty and absolute!");

    std::list<std::string> reference_path_components;
    StringUtil_Split(reference_path, "/", &reference_path_components);
    if (reference_path[reference_path.length() - 1] != '/')
	reference_path_components.pop_back();

    std::list<std::string> path_components;
    StringUtil_Split(path, "/", &path_components);
    std::string filename_component;
    if (not path_components.empty() and path[path.length() - 1] != '/') {
	filename_component = path_components.back();
	path_components.pop_back();
    }

    std::list<std::string>::const_iterator reference_path_iter(reference_path_components.begin());
    for (/* Empty! */; reference_path_iter != reference_path_components.end() and not path_components.empty();
		     ++reference_path_iter)
    {
	if (*reference_path_iter != path_components.front())
	    break;
	path_components.pop_front();
    }

    std::string relative_path;
    for (/* Empty! */; reference_path_iter != reference_path_components.end(); ++reference_path_iter)
	relative_path += "../";

    while (not path_components.empty()) {
	relative_path += path_components.front() + "/";
	path_components.pop_front();
    }

    relative_path += filename_component;

    return relative_path.empty() ? "." : relative_path;
}


// FileUtil_IsDirectory -- returns false if "dir_name" either doesn't exist, we don't have sufficient
//                         priviledges to stat it or it exists but is not a directory.
//
bool FileUtil_IsDirectory(const std::string &dir_name) {
    struct stat statbuf;
    if (::stat(dir_name.c_str(), &statbuf) != 0)
	return false;

    return S_ISDIR(statbuf.st_mode);
}


class Directory {
    std::string path_;
    std::list<std::string> patterns_;
public:
    struct Entry {
	std::string filename_;
    public:
	explicit Entry(const std::string &filename = "")
	    : filename_(filename) { }
	bool empty() const { return filename_.empty(); }
	bool operator==(const Entry &rhs) const
	{ return filename_ == rhs.filename_; }
    };

    class const_iterator {
	friend class Directory;
	DIR *dir_;
	std::string path_;
	std::list<std::string> patterns_;
	Entry previous_entry_, current_entry_;
    public:
	const_iterator(const const_iterator &rhs);
	~const_iterator() { ::closedir(dir_); }
	const Entry *operator->() const { return &current_entry_; }
	const Entry &operator*() const { return current_entry_; }
	const Entry *operator++();
	const Entry *operator++(int);
	bool operator==(const const_iterator &rhs) const;
	bool operator!=(const const_iterator &rhs) const { return !operator==(rhs); }
    private:
	void advance();
	bool match(const char * const filename) const;
	const_iterator(const std::string &path, const std::list<std::string> &patterns);
	const const_iterator operator=(const const_iterator &rhs); // Intentionally unimplemented!
    };
public:
    /* \brief  Constructs a Directory object.
     * \param  path            A relative or absolute search path referring to a directory.
     * \param  filter_pattern  A shell-style pattern specifying which file names should be selected.
     */
    explicit Directory(const std::string &path, const std::string &filter_pattern = "*")
	: path_(path) {  patterns_.push_back(filter_pattern); }
    Directory(const std::string &path, const std::list<std::string> &filter_patterns)
	: path_(path), patterns_(filter_patterns) { }
    const_iterator begin() const { return const_iterator(path_, patterns_); }
    const_iterator end() const { return const_iterator("", std::list<std::string>()); }
};


void Directory::const_iterator::advance() {
    previous_entry_ = current_entry_;

    struct dirent *dirent;
    do {
	errno = 0;
	dirent = ::readdir(dir_);
	if (dirent == nullptr and errno == EBADF)
	    throw std::runtime_error("in Directory::const_iterator::advance: readdir(3) returned "
				     "an error (" + std::string(std::strerror(errno)) + ")!");
    } while (dirent != nullptr and not match(dirent->d_name));

    if (dirent != nullptr)
	current_entry_ = Entry(dirent->d_name);
    else
	current_entry_ = Entry();
}


bool Directory::const_iterator::match(const char * const filename) const {
    for (std::list<std::string>::const_iterator pattern(patterns_.begin()); pattern != patterns_.end(); ++pattern)
	if (::fnmatch(pattern->c_str(), filename, FNM_PATHNAME | FNM_PERIOD) == 0)
	    return true;

    return false;
}


Directory::const_iterator::const_iterator(const std::string &path, const std::list<std::string> &patterns)
    : dir_(nullptr), path_(path), patterns_(patterns)
{
    if (path_.empty())
	return;

    if ((dir_ = ::opendir(path_.c_str())) == nullptr)
	throw std::runtime_error("In const_iterator::const_iterator(1): opendir(3) on \""
				 + path_ + "\" failed (" + std::string(std::strerror(errno)) + ")!");

    advance();
}


Directory::const_iterator::const_iterator(const const_iterator &rhs)
    : dir_(nullptr), path_(rhs.path_), patterns_(rhs.patterns_)
{
    if (path_.empty())
	return;

    if ((dir_ = ::opendir(path_.c_str())) == nullptr)
	throw std::runtime_error("In const_iterator::const_iterator(2): opendir(3) on \""
				 + path_ + "\" failed (" + std::string(std::strerror(errno)) + ")!");

    advance();
}


// Note: this function may be more paranoid than strictly necessary (but that shouldn't harm
//       anything).
//
bool Directory::const_iterator::operator==(const const_iterator &rhs) const {
    if (previous_entry_ == rhs.previous_entry_ and current_entry_ == rhs.current_entry_)
	return true;

    // At the end?
    if (current_entry_.empty() and rhs.current_entry_.empty())
	return true;

    return false;
}


const Directory::Entry *Directory::const_iterator::operator++() {
    advance();

    return &current_entry_;
}


const Directory::Entry *Directory::const_iterator::operator++(int) {
    advance();

    return &previous_entry_;
}


typedef std::map<std::string, std::string> StrMap;


class ExternalIncludes: public StrMap {
public:
    void insert(const std::string &include_filename, const std::string &path)
        { StrMap::insert(std::make_pair(include_filename, path)); }
    bool find(const std::string &include_filename, std::string * const path) const;
    friend std::ostream &operator<<(std::ostream &stream, const ExternalIncludes &external_includes);
};


bool ExternalIncludes::find(const std::string &include_filename, std::string * const path) const {
    const_iterator iter = StrMap::find(include_filename);
    if (iter == end())
	return false;

    *path = iter->second;

    return true;
}


std::ostream &operator<<(std::ostream &stream, const ExternalIncludes &external_includes) {
    for (ExternalIncludes::const_iterator include(external_includes.begin());
	 include != external_includes.end(); ++include)
	stream << include->first << ' ' << include->second << '\n';

    return stream;
}


// ProcessFile -- recursively processes a file.  The top level call is typically to a source file.
//                Recursive calls usually process header files.
//
void ProcessFile(const std::string &source_filename, const ExternalIncludes &external_includes,
		 const bool ignore_nostdinc, StrMap * const already_processed)
{
    std::ifstream source(source_filename.c_str());
    unsigned lineno = 0;
    while (source.good() and !source.eof()) {
	char line[500+1];
	source.getline(line, sizeof line);
	++lineno;

	// Look for preprocessor directives:
	char *cp = line;
	while (*cp == ' ' or *cp == '\t')
	    ++cp;
	if (*cp != '#')
	    continue;
	++cp;
	while (*cp == ' ' or *cp == '\t')
	    ++cp;
	if (std::strncmp(cp, "include", 7) != 0)
	    continue;
	cp += 7;
	while (*cp == ' ' or *cp == '\t')
	    ++cp;
	if (*cp != '<' and *cp != '"') {
	    std::cerr << "iViaCore-mkdep: warning, weird include directive in file \""<< source_filename
		      << "\" on line " << lineno << "!\n";
	    continue;
	}

	const char end_include = (*cp == '<') ? '>' : '"';
	++cp;
	const char * const include_filename = cp;
	while (*cp != '\0' and *cp != end_include)
	    ++cp;
	if (*cp == '\0') {
	    std::cerr << "iViaCore-mkdep: warning: nonterminated include directive in file \""
		      << source_filename << "\" on line " << lineno << "!\n";
	    continue;
	}
	*cp = '\0';

	if (end_include == '"' and ignore_nostdinc)
	    continue;

	std::stack<std::string> includes_to_process;
	std::string external_include_path;
	if (external_includes.find(include_filename, &external_include_path)) {
	    if (already_processed->find(include_filename) != already_processed->end())
		continue;

	    already_processed->insert(std::make_pair(include_filename, include_filename));
	    includes_to_process.push(include_filename);
	}

	while (not includes_to_process.empty()) {
	    std::string top_include_filename(includes_to_process.top());
	    std::string external_include_path1;
	    if (external_includes.find(top_include_filename, &external_include_path1))
		top_include_filename = external_include_path1 + "/" + top_include_filename;
	    ProcessFile(top_include_filename, external_includes, ignore_nostdinc, already_processed);
	    includes_to_process.pop();
	}
    }
}


// ProcessDirectory -- scans a directory for files ending in .h and records them in "include_map".
//
void ProcessDirectory(const std::string &directory_name, ExternalIncludes * const external_includes) {
    std::list<std::string> patterns;
    patterns.push_back("*.h");
    patterns.push_back("*.H");
    patterns.push_back("*.hh");
    patterns.push_back("*.hpp");
    Directory directory(directory_name, patterns);
    for (Directory::const_iterator entry(directory.begin()); entry != directory.end(); ++entry)
	external_includes->insert(entry->filename_, directory_name);
}


std::string RemoveFileSuffix(std::string * const filename) {
    std::string::size_type suffix_start_pos;
    if ((suffix_start_pos = filename->rfind(".c")) == filename->length()-2)
	*filename = filename->substr(0, filename->length()-2);
    else if ((suffix_start_pos = filename->rfind(".C")) == filename->length()-2)
	*filename = filename->substr(0, filename->length()-2);
    else if ((suffix_start_pos = filename->rfind(".cc")) == filename->length()-3)
	*filename = filename->substr(0, filename->length()-3);
    else if ((suffix_start_pos = filename->rfind(".c++")) == filename->length()-4)
	*filename = filename->substr(0, filename->length()-4);
    else if ((suffix_start_pos = filename->rfind(".cpp")) == filename->length()-4)
	*filename = filename->substr(0, filename->length()-4);
    else {
	std::cerr << "iViaCore-mkdep: error: In RemoveFileSuffix: none of .cc, .c++, .cpp, .C or .c found "
		  << "(filename = \"" << *filename << "\")!\n";
	std::exit(EXIT_FAILURE);
    }

    return *filename;
}


std::string ProcessUtil_GetWorkingDirectory() {
    char buf[20 * PATH_MAX];
    if (::getcwd(buf, sizeof buf) != nullptr)
	return buf;
    throw std::runtime_error("in ProcessUtil_GetWorkingDirectory: getcwd(3) failed ("
			     + std::string(std::strerror(errno)) + ")!");
}


void PrintUsage() {
    std::cerr << "usage: iViaCore-mkdep [ ( --include | -I ) path ] [ --ignore-nonstdinc | -g ] [--static-objects | -S] "
	"[ ( --use-relative-paths | -r ) [ reference_directory ] ] file1 file2 ... fileN\n";
    std::cerr << "\tThe --include option may be specified multiple times.\n";
    std::cerr << "\tIf the --ignore-nostdinc option has been specified includes "
	"not using angle brackets are ignored.\n";
    std::cerr << "\tIf the --use-relative-paths option has been specified the generated .deps files will only use "
	      << "relative paths.  If the reference directory has not been specified, the current directory will be "
	      << "used instead.\n";
    std::exit(EXIT_FAILURE);
}


static struct option options[] = {
    { "include",              required_argument, nullptr, 'I' },
    { "ignore-nostdinc",      no_argument,       nullptr, 'g' },
    { "static-objects",       no_argument,       nullptr, 'S' },
    { "use-relative-paths",   optional_argument, nullptr, 'r' },
    { "output-program-rules", optional_argument, nullptr, 'p' },
    { nullptr,                no_argument,       nullptr,  0  }
};


int main(int argc, char *argv[]) {
    if (argc == 1)
	PrintUsage();

    // Process command line options
    bool ignore_nostdinc(false);
    bool use_relative_paths(false);
    bool output_program_rules(false);
    bool static_objects(false);
    ExternalIncludes external_includes;
    std::string cwd;
    for (;;) {
	int option_index = 0;
	int option = ::getopt_long(argc, argv, "iI:gr::", options, &option_index);
	if (option == -1)
	    break;
	switch (option) {
	case 'i':
	case 'I':
	    if (not FileUtil_IsDirectory(optarg)) {
		std::cerr << "iViaCore-mkdep: can't find directory \"" << optarg << "\"!\n";
		return EXIT_FAILURE;
	    }

	    ProcessDirectory(optarg, &external_includes);
	    break;
	case 'S':
	    static_objects = true;
	    break;
	case 'g':
	    ignore_nostdinc = true;
	    break;
	case 'r':
	    use_relative_paths = true;
	    cwd = optarg == nullptr ? ProcessUtil_GetWorkingDirectory() : std::string(optarg);
	    break;
	case 'p':
	    output_program_rules = true;
	    break;
	default:
	    PrintUsage();
	}
    }

    try {
	const unsigned MAX_LINE_LENGTH(130);

	std::ofstream deps(".deps");
	for (int arg_no(optind); arg_no < argc; ++arg_no) {

	    // 1. Extract the include file names.
	    StrMap includes;
	    ProcessFile(argv[arg_no], external_includes, ignore_nostdinc, &includes);

	    // 2. Print out the nicely formatted dependency information.
	    std::string dirname, basename;
	    FileUtil_DirnameAndBasename(argv[arg_no], &dirname, &basename);

	    if (output_program_rules)
		deps << RemoveFileSuffix(&basename) << ": " << argv[arg_no];
	    else
		deps << "$(OBJ)/" << RemoveFileSuffix(&basename) << (static_objects ? "_static.o: " : ".o: ") << argv[arg_no];

	    const unsigned OFFSET = 7 + basename.length() + 4 + std::strlen(argv[arg_no]);
	    unsigned current_line_length = OFFSET;
	    for (StrMap::const_iterator include(includes.begin()); include != includes.end(); ++include) {
		std::string include_path(include->second);
		std::string path;
		if (external_includes.find(include_path, &path))
		    include_path = path + "/" + include_path;
		else
		    include_path = FileUtil_MakeAbsolutePath(dirname, include_path);
		if (current_line_length + 1 + include_path.length() + 2 > MAX_LINE_LENGTH) {
		    deps << " \\\n";
		    for (unsigned i = 0; i < OFFSET; ++i)
			deps << ' ';
		    current_line_length = OFFSET;
		}
		if (use_relative_paths)
		    include_path = FileUtil_MakeRelativePath(cwd, include_path);
		deps << ' ' << include_path;
		current_line_length += 1 + include_path.length();
	    }
	    deps << '\n';
	}
    } catch (const std::runtime_error &x) {
	std::cerr << "iViaCore-mkdep: error: caught exception: " << x.what() << '\n';
	return EXIT_FAILURE;
    }
}

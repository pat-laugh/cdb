//Copyright 2018-2019 Patrick Laughrea
#include "cdb.hpp"

#include <cassert>
#include <cstdlib>
#include <climits>
#include <cstring>
#include <exception>
#include <iostream>
#include <memory>
#include <queue>
#include <regex>
#include <string>
#include <vector>

#include <boost/filesystem/fstream.hpp>

using namespace cdb;
using namespace std;
namespace fs = boost::filesystem;

static const char* PATH_BMKS = ".cdb";
static const char* FILE_BMKS = "bmks";

static string* errMsg = nullptr;
static char* lastBmk;

static bool resolvePathWildcard(fs::path& p, PathPart pathPart, char* ptr1, char* ptr2, const bool BASH_COMPLETION);
static void updatePathsWildcard(list<fs::path>& paths, PathPart pathPart, const char* item, bool hasWildcard);
static void setToPathBmks(fs::path& p);
static fs::path getFileBmks(const fs::path& p);
static int getToBmkPath(fs::ifstream& in, const char* bmk);
static bool getBmk(fs::path& p, const char* bmk);
static bool resolveBmkPath(fs::path& p, fs::ifstream& in);
static bool appendPath(fs::path& p, const char* path);
static bool getItem(fs::path& p, PathPart pathPart, const char* item);
static void getPaths(const fs::path& p, PathPart pathPart, const regex& r, list<fs::path>* morePaths, vector<string>* names = nullptr);
static string makeWildcardRegex(const char* item);
static void setErrMsg(string msg);
static void nulToDir(char* ptr1, char* ptr2);
static bool bmkNameValid(const char* name);
static bool isBmkNameStart(char c);
static bool isBmkNameBody(char c);
static bool isAbsolute(const char* path);

#ifndef NDEBUG
#define debug(msg) std::cerr << msg << std::endl
#else
#define debug(msg)
#endif

//p starts at "current path"
bool cdb::resolvePath(fs::path& p, char* path, const bool BASH_COMPLETION) {
	assert(fs::is_directory(p) && path != nullptr);
	PathPart pathPart;
	char* ptr1 = path;
	if (*ptr1 == CHAR_ROOT_LOCAL)
		goto charBmkDir;
	else if (*ptr1 == CHAR_CURR_DIR) {
		pathPart = PathPart::DIR;
		goto pathEnd;
	} else if (*ptr1 == CHAR_ROOT_USER) {
		++ptr1;
		if (*ptr1 == CHAR_SEP_DIR || *ptr1 == CHAR_SEP_BMK) {
			p = getPathHome();
			goto charBmkDir;
		} else {
			throw runtime_error("functionality not yet developed");
		}
	} else if (*ptr1 == CHAR_ROOT_SYSTEM) {
		p = fs::current_path().root_path();
		goto charBmkDir;
	}
	p = getPathHome();
	pathPart = PathPart::BMK;
	goto pathEnd;
charBmkDir:
	pathPart = static_cast<PathPart>(*ptr1);
	*ptr1++ = '\0';
pathEnd:
	if (BASH_COMPLETION)
		lastBmk = pathPart == PathPart::BMK ? ptr1 : path;
	
	char* ptr2 = ptr1;
	for (;; ++ptr2) {
		char c = *ptr2;
		if (c == CHAR_SEP_DIR || c == CHAR_SEP_BMK) {
			*ptr2 = '\0';
			if (!getItem(p, pathPart, ptr1))
				return false;
			pathPart = static_cast<PathPart>(c);
			ptr1 = ptr2 + 1;
			if (BASH_COMPLETION && pathPart == PathPart::BMK)
				lastBmk = ptr1;
		} else if (c == '\0') {
			if (BASH_COMPLETION) {
				if (pathPart == PathPart::DIR)
					nulToDir(lastBmk, ptr1 - 1);
				printBashCompletion(p, pathPart, ptr1);
				exit(EXIT_SUCCESS);
			}
			return getItem(p, pathPart, ptr1);
		} else if (c == CHAR_WILDCARD)
			return resolvePathWildcard(p, pathPart, ptr1, ++ptr2, BASH_COMPLETION);
	}
}

fs::path cdb::getPathHome() {
	const char* home = getenv("HOME");
	if (home == nullptr)
		throw runtime_error("no environment variable HOME");
	return fs::path(home);
}

void cdb::printBashCompletion(const fs::path& p, PathPart pathPart, const char* item) {
	assert(fs::is_directory(p) && item != nullptr);
	vector<string> names;
	string itemPlusWildcard(item);
	itemPlusWildcard += CHAR_WILDCARD;
	regex r(makeWildcardRegex(itemPlusWildcard.c_str()));
	getPaths(p, pathPart, r, nullptr, &names);
	for (const auto& name : names) {
		if (pathPart == PathPart::DIR)
			cout << lastBmk << CHAR_SEP_DIR;
		cout << name << '\n';
	}
	cout.flush();
}

std::string& cdb::getErrMsg() {
	assert(errMsg != nullptr);
	return *errMsg;
}

//this checks if path ends with "/.", like "/parent/dir-name/.", and does not count that in the path length
static string::size_type getPathLenMinusSlashDotAtEnd(const string& dir) {
	assert(fs::is_directory(fs::path(dir)));
	auto len = dir.length();
	if (dir.back() == '.') {
		if (len > 2) {
			if (dir[len - 2] == '/')
				len -= 2;
		} else if (len == 2)
			len = 1; //can only be "/."
	}
	return len;
}

/* gets rid of redundant parts in a path.
significant chars are '/', ':', '.', and ".."
At start:
- ./ -> as is
- .: -> :
- ../ -> as is
- ..: -> as is
- /: -> as is
- /./ -> /
- /../ -> /
- /..: -> /:
- :/ -> ./
In mid:
- // -> /
- /./ -> /
- /../ -> go to parent dir
- /: -> :
- :: -> :
- :.: -> :
- :..: -> as is
- :/ -> /
At end:
- /. -> remove
- /.. -> go to parent dir
- :/ -> remove
- /: -> remove
*/
//static void trimPath(char* path) {
	
//}

void cdb::addBmk(const boost::filesystem::path& basePath, const char* name, const char* pathVal) {
	debug("addBmk"); debug(basePath); debug(name); debug(pathVal);
	assert(fs::is_directory(basePath) && name != nullptr && pathVal != nullptr);
	if (!bmkNameValid(name))
		throw runtime_error("name is not valid");
	auto pathValLength = strlen(pathVal);
	if (pathValLength >= PATH_MAX)
		throw runtime_error("path value is too long");
	
	//TODO: PATH_MAX should be the max length of the newBmkPath path (like <bmk name>=<newBmkPath path>)
	
	fs::path newBmkPath = fs::current_path();
	{
		unique_ptr<char> pathValCopy(new char[pathValLength]);
		strcpy(pathValCopy.get(), pathVal);
		if (!resolvePath(newBmkPath, pathValCopy.get()))
			throw runtime_error(move(getErrMsg()));
	}
	
	char bufPath[PATH_MAX]; //bmkPath may use an existing string, otherwise it uses this instead of creating a new strjng
	const char *bmkPath;
	if (isAbsolute(pathVal))
		bmkPath = pathVal;
	else {
		/* TODO:
		If the pathVal is relative, then we must get the basePath and the
		newBmkPath. If basePath is not an ancestor of newBmkPath, then an
		absolute full path must be put before pathVal. Otherwise, whatever from
		newBmkPath that is between basePath and pathVal must be put. In other
		words, the final path should be as if the following was done:
			cd basePath; cd intermediatePath; cd pathVal
		If basePath is not an ancestor of newBmkPath or pathVal starts right at
		basePath, then intermediatePath would be equivalent to "./".
		*/
		if (*pathVal != CHAR_CURR_DIR && *pathVal != CHAR_ROOT_LOCAL)
			throw runtime_error("unknown path value");
		
		//TODO: get rid of redundant things, like /../. Using the member function lexically_normal in fs::path does not work for whatever reason
		auto basePathCleaned = basePath;
		auto newBmkPathCleaned = newBmkPath;
		
		const auto& basePathStr = basePathCleaned.native(), newBmkPathCleanedStr = newBmkPathCleaned.native();
		auto basePathLength = getPathLenMinusSlashDotAtEnd(basePathStr);
		
		//if basePath is not an ancestor of newBmkPath, then newBmkPath's absolute path must be put
		if (newBmkPathCleanedStr.length() < basePathLength
				|| (newBmkPathCleanedStr.length() > basePathLength && newBmkPathCleanedStr[basePathLength] != '/')
				|| memcmp(basePathStr.data(), newBmkPathCleanedStr.data(), basePathLength) != 0) {
			auto smallestLength = getPathLenMinusSlashDotAtEnd(newBmkPathCleanedStr);
			if (smallestLength >= PATH_MAX)
				throw runtime_error("new bookmark path is too long");
			if (smallestLength < newBmkPathCleanedStr.length()) {
				auto s = newBmkPathCleanedStr;
				do
					s.pop_back();
				while (smallestLength < s.length());
				newBmkPath = fs::path(s);
			}
			bmkPath = newBmkPath.c_str();
		} else {
			debug(newBmkPathCleanedStr);
			debug(basePathLength);
			auto numCharsFromNewBmkPath = getPathLenMinusSlashDotAtEnd(newBmkPathCleanedStr) - basePathLength;
			if (numCharsFromNewBmkPath == 0)
				bmkPath = ".";
			else {
				if (*pathVal == CHAR_CURR_DIR && (*(pathVal + 1) == CHAR_SEP_DIR || *(pathVal + 1) == '\0')) {
					++pathVal;
					--pathValLength;
				}
				if (1 + numCharsFromNewBmkPath + pathValLength >= PATH_MAX)
					throw runtime_error("new bookmark path is too long");
				char* ptr = bufPath;
				*ptr++ = '.';
				std::memcpy(ptr, newBmkPathCleanedStr.data() + basePathLength, numCharsFromNewBmkPath);
				ptr += numCharsFromNewBmkPath;
				//std::memcpy(ptr, pathVal, pathValLength);
				//ptr += pathValLength;
				*ptr = '\0';
				bmkPath = bufPath;
			}
		}
	}
	
	auto fileBmks = getFileBmks(basePath);
	if (fs::is_regular_file(fileBmks)) {
		fs::ifstream in(fileBmks, std::ios::in);
		if (!in)
			throw runtime_error("error reading bookmarks file");
		if (getToBmkPath(in, name) != -1)
			throw runtime_error("bookmark already exists");
	} else {
		auto bmksDir = basePath;
		setToPathBmks(bmksDir);
		fs::create_directory(bmksDir);
		fs::ofstream out(fileBmks, std::ios::out);
		if (!out)
			throw runtime_error("bookmark was not added: could not create bookmarks file");
	}
	fs::path tempPath(fileBmks.native() + "temp");
	fs::copy_file(fileBmks, tempPath);
	fs::ofstream out(tempPath, std::ios::app);
	if (!out)
		throw runtime_error("bookmark was not added: could not open temporary file to write");
	out << name << '=' << bmkPath << '\n';
	if (out.fail()) {
		out.close();
		fs::remove(tempPath);
		throw runtime_error("bookmark was not added: an I/O error occurred");
	}
	out.close();
	fs::rename(tempPath, fileBmks);
}

void cdb::rmBmk(const boost::filesystem::path& basePath, const char* name) {
	auto fileBmks = getFileBmks(basePath);
	fs::ifstream in(fileBmks, std::ios::in);
	if (!in)
		throw runtime_error("error reading bookmarks file");
	int line = getToBmkPath(in, name);
	if (line == -1)
		throw runtime_error("no such bookmark");
	fs::path tempPath(fileBmks.native() + "temp");
	fs::ofstream out(tempPath, std::ios::out);
	if (!out)
		throw runtime_error("bookmark was not removed: could not open a temporary file");
	in.seekg(0);
	char c;
	if (line != 0) {
		int inLine = 0;
		for (c = in.get(); in.good(); c = in.get()) {
			out.put(c);
			if (c == '\n' && ++inLine  == line)
				break;
		}
	}
	for (c = in.get(); in.good() && c != '\n'; c = in.get())
		;
	for (c = in.get(); in.good(); c = in.get())
		out.put(c);
	if (!in.eof() || out.fail()) {
		out.close();
		fs::remove(tempPath);
		throw runtime_error("bookmark was not removed: an I/O error occurred");
	}
	in.close();
	out.close();
	fs::rename(tempPath, fileBmks);
}

static bool resolvePathWildcard(fs::path& p, PathPart pathPart, char* ptr1, char* ptr2, const bool BASH_COMPLETION) {
	assert(fs::is_directory(p) && ptr1 != nullptr && ptr2 != nullptr);
	list<fs::path> paths;
	paths.push_back(move(p));
	bool hasWildcard = true;
	for (;; ++ptr2) {
		char c = *ptr2;
		if (c == CHAR_SEP_DIR || c == CHAR_SEP_BMK) {
			*ptr2 = '\0';
			updatePathsWildcard(paths, pathPart, ptr1, hasWildcard);
			hasWildcard = false;
			pathPart = static_cast<PathPart>(c);
			ptr1 = ptr2 + 1;
			if (BASH_COMPLETION && pathPart == PathPart::BMK)
				lastBmk = ptr1;
		} else if (c == '\0') {
			if (BASH_COMPLETION) {
				if (pathPart == PathPart::DIR)
					nulToDir(lastBmk, ptr1 - 1);
				for (const auto& path : paths)
					printBashCompletion(path, pathPart, ptr1);
				exit(EXIT_SUCCESS);
			}
			updatePathsWildcard(paths, pathPart, ptr1, hasWildcard);
			break;
		} else if (c == CHAR_WILDCARD)
			hasWildcard = true;
	}
	if (paths.empty()) {
		setErrMsg("no matches found");
		return false;
	}
	p = move(paths.front());
	return true;
}

static void updatePathsWildcard(list<fs::path>& paths, PathPart pathPart, const char* item, bool hasWildcard) {
	assert(item != nullptr);
	if (*item == '\0')
		return;
	list<fs::path> morePaths;
	unique_ptr<regex> r(hasWildcard ? new regex(makeWildcardRegex(item)) : nullptr);
	for (auto it = paths.begin(); it != paths.end();)
		if (!hasWildcard)
			it = getItem(*it, pathPart, item) ? ++it : paths.erase(it);
		else {
			getPaths(*it, pathPart, *r, &morePaths);
			it = paths.erase(it);
		}
	if (!morePaths.empty())
		paths.splice(paths.end(), morePaths);
}

static void setToPathBmks(fs::path& p) {
	assert(fs::is_directory(p));
	p /= PATH_BMKS;
}

static fs::path getFileBmks(const fs::path& p) {
	assert(fs::is_directory(p));
	fs::path pCopy(p);
	setToPathBmks(pCopy);
	pCopy /= FILE_BMKS;
	return pCopy;
}

static int getToBmkPath(fs::ifstream& in, const char* bmk) {
	assert(in.good() && bmk != nullptr);
	char c;
	int line = 0;
getToEqual:
	for (const char* bmkPtr = bmk;; ++bmkPtr) {
		c = in.get();
		if (!in.good())
			goto eofException;
		if (c != *bmkPtr) {
			if (c == '=' && *bmkPtr == '\0')
				break;
			goto skipLine;
		} else if (c == '\0')
			goto skipLine;
	}
	return line;
skipLine:
	do {
		if (c == '\n') {
			++line;
			goto getToEqual;
		}
		c = in.get();
	} while (in.good());
eofException:
	setErrMsg(string("failed to get bookmark \"") + bmk + "\": " + (in.eof() ? "no such bookmark" : "an I/O error occurred"));
	return -1;
}

static bool getBmk(fs::path& p, const char* bmk) {
	assert(fs::is_directory(p) && bmk != nullptr);
	fs::ifstream in(getFileBmks(p), std::ios::in);
	if (!in) {
		setErrMsg("error reading bookmarks file");
		return false;
	}
	return getToBmkPath(in, bmk) == -1 ? false : resolveBmkPath(p, in);
}

static bool resolveBmkPath(fs::path& p, fs::ifstream& in) {
	assert(fs::is_directory(p) && in.good());
	char path[PATH_MAX];
	char* ptr = path;
	for (char c = in.get(); in.good() && c != '\n' && ptr < path + PATH_MAX - 1; c = in.get(), ++ptr)
		*ptr = c;
	if (!in && !in.eof()) {
		setErrMsg("failed to get bookmark: an I/O error occurred");
		return false;
	}
	*ptr = '\0';
	return resolvePath(p, path);
}

static bool appendPath(fs::path& p, const char* path) {
	assert(fs::is_directory(p) && path != nullptr);
	if (path[0] == '.') {
		if (path[1] == '\0')
			return true;
		if (path[1] == '.' && path[2] == '\0') {
			auto parent = p.parent_path();
			if (!parent.empty())
				p = parent;
			return true;
		}
	}
	p /= path;
	if (fs::is_directory(p))
		return true;
	setErrMsg(string("not a directory: ") + path);
	return false;
}

static bool getItem(fs::path& p, PathPart pathPart, const char* item) {
	assert(fs::is_directory(p) && item != nullptr);
	return *item == '\0' || (pathPart == PathPart::DIR ? appendPath(p, item) : getBmk(p, item));
}

static void getPaths(const fs::path& p, PathPart pathPart, const regex& r, list<fs::path>* morePaths, vector<string>* names) {
	assert(fs::is_directory(p) && (morePaths != nullptr || names != nullptr));
	if (pathPart == PathPart::DIR) {
		for (const auto& dirPath : fs::directory_iterator(p)) {
			const auto& dirName = dirPath.path().filename().native();
#ifdef __APPLE__
			string cpy = dirName; //work around bug
			if (regex_match(cpy, r)) {
#else
			if (regex_match(dirName, r)) {
#endif
				if (names != nullptr)
					names->push_back(dirName);
				else {
					auto otherPath = p;
					appendPath(otherPath, dirName.c_str());
					morePaths->push_back(move(otherPath));
				}
			}
		}
	} else {
		fs::ifstream in(getFileBmks(p), std::ios::in);
		if (!in)
			throw runtime_error("error reading bookmarks file");
		char c, bmk[FILENAME_MAX];
		char* MAX_PTR = bmk + FILENAME_MAX;
getToEqual:
		char* ptr = bmk;
		for (c = in.get(); in.good(); c = in.get()) {
			if (c == '=') {
				*ptr = '\0';
#ifdef __APPLE__
				string cpy = bmk; //work around bug
				if (regex_match(cpy, r)) {
#else
				if (regex_match(bmk, r)) {
#endif
					if (names != nullptr)
						names->push_back(bmk);
					else {
						auto otherPath = p;
						resolveBmkPath(otherPath, in);
						morePaths->push_back(move(otherPath));
						goto getToEqual;
					}
				}
			} else {
				*ptr = c;
				if (++ptr < MAX_PTR)
					continue;
			}
			do { //skip line
				if (c == '\n')
					goto getToEqual;
				c = in.get();
			} while (in.good());
			break;
		}
		if (!in.eof())
			throw runtime_error("reading bookmarks file failed: an I/O error occurred");
	}
}

//TODO: using brackets really is a hack to avoid special chars. I need to do something better
static string makeWildcardRegex(const char* item) {
	assert(item != nullptr && *item != '\0');
	string regexStr;
	regexStr += '^';
	const char* ptr = item;
	bool wasWildcard;
	if (*ptr != CHAR_WILDCARD)
		goto addRegularChar;
	regexStr += "[^.]*";
	goto addWildcard;
	for (; *ptr != '\0'; ++ptr)
		if (*ptr == CHAR_WILDCARD) {
			if (wasWildcard)
				continue;
			regexStr += ".*";
addWildcard:
			wasWildcard = true;
		} else {
addRegularChar:
			regexStr += '[';
			if (*ptr == '\\' || *ptr == ']')
				regexStr += '\\';
			regexStr += *ptr;
			regexStr += ']';
			wasWildcard = false;
		}
	regexStr += '$';
	return regexStr;
}

static void setErrMsg(string msg) {
	if (errMsg == nullptr)
		errMsg = new string();
	*errMsg = move(msg);
}

static void nulToDir(char* ptr1, char* ptr2) {
	assert(ptr1 != nullptr && ptr2 != nullptr);
	for (; ptr1 < ptr2; ++ptr1)
		if (*ptr1 == '\0')
			*ptr1 = CHAR_SEP_DIR;
}

static bool bmkNameValid(const char* name) {
	assert(name != nullptr);
	const char* ptr = name;
	if (!isBmkNameStart(*ptr))
		return false;
	while (*++ptr != '\0')
		if (!isBmkNameBody(*ptr) && (*ptr != '-' || !isBmkNameBody(*++ptr)))
			return false;
	return true;
}

static bool isBmkNameStart(char c) {
	assert(sizeof(char) == 1);
	if ((unsigned char)c >= 'a')
		return (signed char)c <= 'z';
	if (c <= 'Z')
		return c >= 'A';
	return c == '_';
}

static bool isBmkNameBody(char c) {
	return isBmkNameStart(c) || (c >= '0' && c <= '9');
}

static bool isAbsolute(const char* path) {
	assert(path != nullptr);
	char c = path[0];
	return isBmkNameStart(c) || c == CHAR_ROOT_SYSTEM || c == CHAR_ROOT_USER || c == CHAR_WILDCARD;
}
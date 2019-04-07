//Copyright 2018-2019 Patrick Laughrea

#include <boost/filesystem.hpp>

namespace cdb {
	const char CHAR_SEP_BMK = ':';
	const char CHAR_SEP_DIR = '/';
	const char CHAR_WILDCARD = '%';

	const char CHAR_CURR_DIR = '.';
	const char CHAR_ROOT_LOCAL = CHAR_SEP_BMK;
	const char CHAR_ROOT_USER = '~';
	const char CHAR_ROOT_SYSTEM = CHAR_SEP_DIR;

	enum class PathPart { BMK = CHAR_SEP_BMK, DIR = CHAR_SEP_DIR };

	//puts in p the new path derived from path using p as the base dir
	//NOTE: path might change! Make a copy if you wish to keep the original
	bool resolvePath(boost::filesystem::path& p, char* path, const bool BASH_COMPLETION = false);
	
	boost::filesystem::path getPathHome();
	void printBashCompletion(const boost::filesystem::path& p, PathPart pathPart, const char* item);
	std::string& getErrMsg();
	void addBmk(const boost::filesystem::path& basePath, const char* name, const char* pathVal);
	void rmBmk(const boost::filesystem::path& basePath, const char* name);
}
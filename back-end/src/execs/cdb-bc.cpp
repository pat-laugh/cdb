//Copyright 2018 Patrick Laughrea

#include <exception>

#include <boost/filesystem.hpp>

#include "cdb/cdb.hpp"

using namespace cdb;
using namespace std;
namespace fs = boost::filesystem;

int main(int argc, char** argv) {
	try {
		if (argc != 2) {
			if (argc != 1)
				return 1;
			auto pathHome = getPathHome();
			printBashCompletion(pathHome, PathPart::BMK, "");
			return 0;
		}
		fs::path p = fs::current_path();
		resolvePath(p, argv[1], true);
		return 0;
	} catch (const exception& e) {
		return 1;
	}
}
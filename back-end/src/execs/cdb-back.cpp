//Copyright 2018-2019 Patrick Laughrea

#include <exception>
#include <iostream>

#include <boost/filesystem.hpp>

#include "cdb/cdb.hpp"

using namespace cdb;
using namespace std;
namespace fs = boost::filesystem;

#define APP_NAME "cdb"
const char* USAGE = "Usage: " APP_NAME " [path=~] [-a name [path=.]|-l|-p|-r name]";
#define isOption(arg) (arg[0] == '-')

#define EXIT_CD 0
#define EXIT_ERROR 1
#define EXIT_DO_NOTHING 2
#define EXIT_ECHO 3

[[noreturn]] void exitUsage();
void resolveOption(const fs::path& basePath, int argc, char** argv, int indexOption);
void checkArgCount(int argc, int indexOption, int min, int max);

int main(int argc, char** argv) {
	try {
		if (argc <= 1) {
			cout << getPathHome().c_str() << endl;
			return EXIT_CD;
		}
		fs::path p;
		if (isOption(argv[1])) {
			p = getPathHome();
			resolveOption(p, argc, argv, 1);
		} else {
			p = fs::current_path();
			if (!resolvePath(p, argv[1]))
				throw runtime_error(move(getErrMsg()));
			if (argc == 2) {
				cout << p.c_str() << endl;
				return EXIT_CD;
			}
			resolveOption(p, argc, argv, 2);
		}
	} catch (const exception& e) {
		cerr << APP_NAME << ": error: " << e.what() << endl;
		return EXIT_ERROR;
	}
}

[[noreturn]] void exitUsage() {
	cerr << USAGE << endl;
	exit(EXIT_ERROR);
}

void resolveOption(const fs::path& basePath, int argc, char** argv, int indexOption) {
	char* opt = argv[indexOption];
	char optChar = opt[1];
	if (optChar == '\0' || opt[2] != '\0')
		goto invalidOpt;
	if (optChar == 'a') {
		//cdb <bmk> -a name <bmk>
		checkArgCount(argc, indexOption, 2, 3);
		char* pathVal;
		if (argc == indexOption + 3)
			pathVal = argv[indexOption + 2];
		else { //does not have arg for path to add
			pathVal = opt + 1;
			*pathVal = '.'; //"-a\0" to "-.\0"; this is an optimization
		}
		addBmk(basePath, argv[indexOption + 1], pathVal);
		exit(EXIT_DO_NOTHING);
	} else if (optChar == 'l') {
		checkArgCount(argc, indexOption, 1, 1);
		printBashCompletion(basePath, PathPart::BMK, "");
		exit(EXIT_ECHO);
	} else if (optChar == 'p') {
		checkArgCount(argc, indexOption, 1, 1);
		cout << basePath.c_str() << endl;
		exit(EXIT_ECHO);
	} else if (optChar == 'r') {
		checkArgCount(argc, indexOption, 2, 2);
		rmBmk(basePath, argv[indexOption + 1]);
		exit(EXIT_DO_NOTHING);
	}
invalidOpt:
	cerr << APP_NAME << ": invalid option: " << opt << endl;
	exitUsage();
}

void checkArgCount(int argc, int indexOption, int min, int max) {
	if (argc < indexOption + min || argc > indexOption + max) {
		cerr << APP_NAME << ": too " << (argc < indexOption + min ? "few" : "many") << " arguments" << endl;
		exitUsage();
	}
}
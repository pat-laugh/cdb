# Copyright 2018-2019 Patrick Laughrea

DIR_PROJECT = ..
DIR_CDB = $(DIR_PROJECT)/cdb

IS_MAC = $$(test "$$(uname -s)" = 'Darwin' && echo 1 || echo 0)

OPTIONS = -std=c++11 -Wall -Wextra -Wno-missing-field-initializers -I $(DIR_PROJECT)
CPP_FILES = $(wildcard *.cpp)
OBJ_FILES = $(notdir $(CPP_FILES:.cpp=.o))
EXEC_CDB = cdb-back.out
EXEC_BASH_COMPLETION = cdb-bc.out

all: cdb bc
cdb: compile_libs $(EXEC_CDB)
bc: compile_libs $(EXEC_BASH_COMPLETION)

compile_libs:
	cd $(DIR_CDB); make -s

$(EXEC_CDB): cdb-back.o
	$(CXX) $(OPTIONS) -o $@ $^ -L$(DIR_CDB) -lcdb $$( \
		if [ $(IS_MAC) -eq 1 ]; then \
			echo '-L/opt/local/lib -lboost_filesystem-mt -lboost_system-mt'; \
		else \
			echo '-lboost_filesystem -lboost_system'; \
		fi \
	)

$(EXEC_BASH_COMPLETION): cdb-bc.o
	$(CXX) $(OPTIONS) -o $@ $^ -L$(DIR_CDB) -lcdb $$( \
		if [ $(IS_MAC) -eq 1 ]; then \
			echo '-L/opt/local/lib -lboost_filesystem-mt -lboost_system-mt'; \
		else \
			echo '-lboost_filesystem -lboost_system'; \
		fi \
	)

%.o: %.cpp
	$(CXX) $(OPTIONS) -o $@ -c $<

clean:
	cd $(DIR_CDB); make clean -s
	rm -rf $(EXEC_CDB) $(EXEC_BASH_COMPLETION) *.o

release: OPTIONS += -DNDEBUG -O3
release: compile_libs_release $(EXEC_CDB) $(EXEC_BASH_COMPLETION)

debug: OPTIONS += -g3
debug: compile_libs_debug $(EXEC_CDB) $(EXEC_BASH_COMPLETION)

compile_libs_release:
	cd $(DIR_CDB); make release -s

compile_libs_debug:
	cd $(DIR_CDB); make debug -s

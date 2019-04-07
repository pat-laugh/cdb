# Copyright 2018-2019 Patrick Laughrea

DIR_PROJECT = ..

OPTIONS = -std=c++11 -Wall -Wextra -Wno-missing-field-initializers -I $(DIR_PROJECT)
LIBRARY = libcdb.a
CPP_FILES = $(wildcard *.cpp)
OBJ_FILES = $(notdir $(CPP_FILES:.cpp=.o))

all: $(LIBRARY)

$(LIBRARY): $(OBJ_FILES)
	ar -rcs $@ $^

%.o: %.cpp
	$(CXX) $(OPTIONS) -o $@ -c $<

clean:
	rm -rf $(LIBRARY) *.o

release: OPTIONS += -DNDEBUG -O3
release: all

debug: OPTIONS += -g3
debug: all

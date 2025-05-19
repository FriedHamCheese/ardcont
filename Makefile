SRC_FILES := $(wildcard ./src/*.cpp)
HEADER_FILES := $(wildcard ./src/*.hpp)
OBJ_FILES := $(patsubst ./src/%.cpp,./bin/%.o,$(SRC_FILES))

NTRB_DIR := ../naturau-base
NTRB_OBJFILES := $(wildcard $(NTRB_DIR)/bin/*.o)

#imports NTRB_DEPENDENCY_INCLUDES, NTRB_LINKING_DEPENDENCIES and NTRB_COMPILING_SYMBOLS
include $(NTRB_DIR)/makeconfig.make

include makeconfig.make

CXXFLAGS := -Wall -Wextra -g3 $(NTRB_DEPENDENCY_INCLUDES) -I$(NTRB_DIR)/include $(SERIAL_INCLUDE) $(NTRB_COMPILING_SYMBOLS) $(AUTOMATED_TEST_MACRO) -DNCURSES_STATIC
LDLIBS := $(NTRB_LINKING_DEPENDENCIES) $(SERIAL_LINKING_FLAGS) -lncurses

build.exe: $(NTRB_OBJFILES) $(OBJ_FILES)
	$(CXX) -o $@ $^ $(LDLIBS)

$(OBJ_FILES): ./bin/%.o: ./src/%.cpp $(HEADER_FILES) | ./bin
	$(CXX) $< -c $(CXXFLAGS) -o $@

./bin:
	mkdir $@

.PHONY: clean
clean: clean_build clean_test
	
.PHONY: clean_build
clean_build:
	rm ./bin/*.o
	rm ./build.exe
	
.PHONY: clean_test
clean_test:
	rm ./tests/test_main.o
	rm ./test.exe

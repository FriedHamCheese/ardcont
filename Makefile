SRC_FILES := $(wildcard ./src/*.cpp)
HEADER_FILES := $(wildcard ./src/*.hpp)
OBJ_FILES := $(patsubst ./src/%.cpp,./bin/%.o,$(SRC_FILES))

NTRB_DIR := ../naturau-base
NTRB_OBJFILES := $(wildcard $(NTRB_DIR)/bin/*.o)

#imports NTRB_DEPENDENCY_INCLUDES, NTRB_LINKING_DEPENDENCIES and NTRB_COMPILING_SYMBOLS
include $(NTRB_DIR)/makeconfig.make

AUTOMATED_TEST := YES
ifeq ($(AUTOMATED_TEST),YES)
	AUTOMATED_TEST_MACRO := -DARDCONT_AUTOMATED_TEST
else
	AUTOMATED_TEST_MACRO :=
endif

CXXFLAGS := -Wall -Wextra -g3 $(NTRB_DEPENDENCY_INCLUDES) -I$(NTRB_DIR)/include -I../serial-main/include -I./src $(NTRB_COMPILING_SYMBOLS) $(AUTOMATED_TEST_MACRO)
LDLIBS := $(NTRB_LINKING_DEPENDENCIES) -L../serial-main/build -lserial -lsetupAPI

build.exe: $(NTRB_OBJFILES) $(OBJ_FILES)
	$(CXX) -o $@ $^ $(LDLIBS)

$(OBJ_FILES): ./bin/%.o: ./src/%.cpp $(HEADER_FILES) | ./bin
	$(CXX) $< -c $(CXXFLAGS) -o $@

#GOOGLE_TEST_ includes
include makeconfig.make
OBJ_FILES_NO_MAIN := $(filter-out ./bin/main.o,$(OBJ_FILES))
TESTING_HEADER_FILES := $(wildcard ./tests/*.hpp)

./test.exe: $(NTRB_OBJFILES) $(OBJ_FILES_NO_MAIN) ./tests/test_main.o
	$(CXX) -o $@ $^ $(GOOGLE_TEST_OBJ) $(LDLIBS)

./tests/test_main.o: ./tests/test_main.cpp $(HEADER_FILES) $(TESTING_HEADER_FILES)
	$(CXX) $< -c $(CXXFLAGS) $(GOOGLE_TEST_INCLUDE) -o $@	


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
	rm ./test.exe
	rm ./tests/test_main.o

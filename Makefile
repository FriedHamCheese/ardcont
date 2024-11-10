SRC_FILES := $(wildcard ./src/*.cpp)
HEADER_FILES := $(wildcard ./src/*.hpp)
OBJ_FILES := $(patsubst ./src/%.cpp,./bin/%.o,$(SRC_FILES))

NTRB_DIR := ../naturau-base
NTRB_OBJFILES := $(wildcard $(NTRB_DIR)/bin/*.o)

AUTOMATED_TEST := NO
ifeq ($(AUTOMATED_TEST),YES)
	AUTOMATED_TEST_MACRO := -DARDCONT_AUTOMATED_TEST
	TEST_EXECUTABLE := ./test.exe
else
	AUTOMATED_TEST_MACRO :=
	TEST_EXECUTABLE :=
endif

CXXFLAGS := -Wall -Wextra -g3 -I../naturau-base/include -I../wjwwood-serial/include -I../portaudio/include -I../flac-1.4.3/include -I./src -DFLAC__NO_DLL -DNTRB_MEMDEBUG $(AUTOMATED_TEST_MACRO)
LDLIBS := -L../flac-build/src/libFLAC -L../portaudio/build -L../wjwwood-serial/bin -lFLAC -lportaudio -lserial -lsetupAPI

.PHONY: all
all: build.exe $(TEST_EXECUTABLE)

build.exe: $(NTRB_OBJFILES) $(OBJ_FILES)
	$(CXX) -o $@ $^ $(LDLIBS)

$(OBJ_FILES): ./bin/%.o: ./src/%.cpp $(HEADER_FILES) | ./bin
	$(CXX) $< -c $(CXXFLAGS) -o $@


OBJ_FILES_NO_MAIN := $(filter-out ./bin/main.o,$(OBJ_FILES))
TESTING_HEADER_FILES := $(wildcard ./tests/*.hpp)
GOOGLE_TEST_INCLUDE := -I../googletest/googletest/include
GOOGLE_TEST_OBJ := ..\googletest\build\googletest\CMakeFiles\gtest.dir\src\gtest-all.cc.obj

$(TEST_EXECUTABLE): $(NTRB_OBJFILES) $(OBJ_FILES_NO_MAIN) ./tests/test_main.o
	$(CXX) -o $@ $^ $(GOOGLE_TEST_OBJ) $(LDLIBS)

./tests/test_main.o: ./tests/test_main.cpp $(HEADER_FILES) $(TESTING_HEADER_FILES)
	$(CXX) $< -c $(CXXFLAGS) $(GOOGLE_TEST_INCLUDE) -o $@	


./bin:
	mkdir $@

.PHONY: clean
clean: clean_build clean_test
	
.PHONY: clean_build
clean_build:
	rm ./build.exe
	rm ./bin/*.o
	
.PHONY: clean_test
clean_test:
	rm ./test.exe
	rm ./tests/test_main.o
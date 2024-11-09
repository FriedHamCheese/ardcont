SRC_FILES := $(wildcard ./src/*.cpp)
HEADER_FILES := $(wildcard ./src/*.hpp)
OBJ_FILES := $(patsubst ./src/%.cpp,./bin/%.o,$(SRC_FILES))

NTRB_DIR := ../naturau-base
NTRB_OBJFILES := $(wildcard $(NTRB_DIR)/bin/*.o)

CXXFLAGS := -Wall -Wextra -I../naturau-base/include -I../wjwwood-serial/include -I../portaudio/include -I../flac-1.4.3/include -I./src -DFLAC__NO_DLL -DNTRB_MEMDEBUG -g3
LDLIBS := -L../flac-build/src/libFLAC -L../portaudio/build -L../wjwwood-serial/bin -lFLAC -lportaudio -lserial -lsetupAPI

.PHONY: all
all: build.exe test.exe

build.exe: $(NTRB_OBJFILES) $(OBJ_FILES)
	$(CXX) -o $@ $^ $(LDLIBS)

$(OBJ_FILES): ./bin/%.o: ./src/%.cpp $(HEADER_FILES) | ./bin
	$(CXX) $< -c $(CXXFLAGS) -o $@


OBJ_FILES_NO_MAIN := $(filter-out ./bin/main.o,$(OBJ_FILES))
TESTING_HEADER_FILES := $(wildcard ./tests/*.hpp)

./test.exe: $(NTRB_OBJFILES) $(OBJ_FILES_NO_MAIN) ./tests/test_main.o
	$(CXX) -o $@ $^ ..\googletest\build\googletest\CMakeFiles\gtest.dir\src\gtest-all.cc.obj $(LDLIBS)

./tests/test_main.o: ./tests/test_main.cpp $(HEADER_FILES) $(TESTING_HEADER_FILES)
	$(CXX) $< -c $(CXXFLAGS) -I../googletest/googletest/include -o $@	

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
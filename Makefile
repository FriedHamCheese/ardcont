SRC_FILES := $(wildcard ./src/*.cpp)
OBJ_FILES := $(patsubst ./src/%.cpp,./bin/%.o,$(SRC_FILES))
HEADER_FILES := $(wildcard ./src/*.hpp)

NTRB_DIR := ../naturau-base
NTRB_OBJFILES := $(wildcard $(NTRB_DIR)/bin/*.o)

CXXFLAGS := -Wall -Wextra -I../naturau-base/include -I../wjwwood-serial/include -I../portaudio/include -I../flac-1.4.3/include -DFLAC__NO_DLL -DNTRB_MEMDEBUG -g3
LDLIBS := -L../flac-build/src/libFLAC -L../portaudio/build -L../wjwwood-serial/bin -lFLAC -lportaudio -lserial -lsetupAPI

build.exe: $(NTRB_OBJFILES) $(OBJ_FILES)
	$(CXX) -o $@ $^ $(LDLIBS)

$(OBJ_FILES): ./bin/%.o: ./src/%.cpp $(HEADER_FILES) | ./bin
	$(CXX) $< -c $(CXXFLAGS) -o $@

./bin:
	mkdir $@
	
.PHONY: clean
clean:
	rm ./build.exe
	rm ./bin/*.o
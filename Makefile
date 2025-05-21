SRC_FILES := $(wildcard ./src/*.cpp)
HEADER_FILES := $(wildcard ./src/*.hpp)
OBJ_FILES := $(patsubst ./src/%.cpp,./bin/%.o,$(SRC_FILES))

NTRB_DIR := ../naturau-base
NTRB_OBJFILES := 

NTRB_DLL := ./libntrb.dll

include $(NTRB_DIR)/makeconfig.make

include makeconfig.make

CXXFLAGS := -Wall -Wextra -g3 -I$(NTRB_DIR)/$(NTRB_PORTAUDIO_INCLUDE) -I$(NTRB_DIR)/$(NTRB_FLAC_INCLUDE) -I$(NTRB_DIR)/include $(SERIAL_INCLUDE) $(NTRB_COMPILING_SYMBOLS) -DNCURSES_STATIC -DNTRB_DLL_IMPORT
LDLIBS := $(SERIAL_LINKING_FLAGS) -L$(NTRB_DIR)/$(NTRB_PORTAUDIO_LIBDIR) -L$(NTRB_DIR)/$(NTRB_FLAC_LIBDIR) -L$(NTRB_DIR)/bin -lntrb -lncurses -lportaudio -lflac.dll

build.exe: $(OBJ_FILES) $(NTRB_DLL) Makefile
	$(CXX) -o $@ $(OBJ_FILES) $(LDLIBS)
	
./libntrb.dll: Makefile
	cp ../naturau-base/bin/libntrb.dll ./

$(OBJ_FILES): ./bin/%.o: ./src/%.cpp $(HEADER_FILES) Makefile | ./bin
	$(CXX) $< -c $(CXXFLAGS) -o $@

./bin:
	-mkdir $@

.PHONY: clean
clean: clean_build
	
.PHONY: clean_build
clean_build:
	-rm ./bin/*.o
	-rm ./build.exe
CXXFLAGS := -g3 -I./include
LD := $(CXX)

./bin/libserial.a: ./bin/serial.o ./bin/win.o ./bin/list_ports_win.o
	$(AR) r $@ $^

./bin/serial.o: ./src/serial.cc | ./bin
	$(CXX) $(CXXFLAGS) -c $^ -o $@

./bin/win.o: ./src/impl/win.cc | ./bin
	$(CXX) $(CXXFLAGS) -c $^ -o $@

./bin/list_ports_win.o: ./src/impl/list_ports/list_ports_win.cc | ./bin
	$(CXX) $(CXXFLAGS) -c $^ -o $@
./bin:
	-mkdir ./bin

.PHONY: clean
clean:
	-rm -rf ./bin

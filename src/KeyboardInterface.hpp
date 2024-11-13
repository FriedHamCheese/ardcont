#ifndef keyboardinterface_hpp
#define keyboardinterface_hpp

#include "GlobalStates.hpp"

#include <string>
#include <iostream>

class KeyboardInterface{
	public:
	virtual void getline(std::string& buffer) = 0;
};

class KeyboardCin : public KeyboardInterface{
	public:
	void getline(std::string& buffer) override{
		std::getline(std::cin, buffer);
	}
};

class KeyboardMock : public KeyboardInterface{
	public:
	void getline(std::string& buffer) override{
		buffer = this->getline_mockstr;
	}
	void setline(std::string&& str){
		this->getline_mockstr = str;
	}
	
	protected:
	std::string getline_mockstr;
};

void keyboard_listener(KeyboardInterface& keyboard_input, GlobalStates& global_states);

#endif
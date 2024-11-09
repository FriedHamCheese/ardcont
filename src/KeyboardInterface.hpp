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

struct KeyboardListenerTestingFlags{
	bool l_command_not_enough_arguments = false;
	bool l_command_ntrbAudioBuffer_error = false;
	bool l_command_id_not_a_number = false;
	bool l_command_id_out_of_range = false;	
	bool invalid_command = false;
	
	void clear_all(){
		this->l_command_not_enough_arguments = false;
		this->l_command_ntrbAudioBuffer_error = false;
		this->l_command_id_not_a_number = false;
		this->l_command_id_out_of_range = false;	
		this->invalid_command = false;		
	}
};

void keyboard_listener(KeyboardInterface& keyboard_input, GlobalStates& global_states, 
						KeyboardListenerTestingFlags* const testing_flags = nullptr);

#endif
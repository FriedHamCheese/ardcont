#ifndef keyboardinterface_hpp
#define keyboardinterface_hpp

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

#ifndef ARDCONT_AUTOMATED_TEST
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

void keyboard_listener(KeyboardInterface& keyboard_input);

namespace keyboard_listener_test_flags{
	inline bool l_command_not_enough_arguments = false;
	inline bool l_command_ntrbAudioBuffer_error = false;
	inline bool l_command_id_not_a_number = false;
	inline bool l_command_id_out_of_range = false;	
}

inline void clear_keyboard_listener_test_flags(){
	keyboard_listener_test_flags::l_command_not_enough_arguments = false;
	keyboard_listener_test_flags::l_command_ntrbAudioBuffer_error = false;
	keyboard_listener_test_flags::l_command_id_not_a_number = false;
	keyboard_listener_test_flags::l_command_id_out_of_range = false;
}
#endif
#endif
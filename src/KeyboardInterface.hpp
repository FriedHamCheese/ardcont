/**
\file KeyboardInterface.hpp
*/

#ifndef KEYBOARDINTERFACE_HPP
#define KEYBOARDINTERFACE_HPP

#include "GlobalStates.hpp"

#include <string>
#include <iostream>

///An abstract class for keyboard input.
class KeyboardInterface{
	public:
	/**
	This function clears and fills *buffer* with the contents from a line of keyboard input.
	Implementations may halt the flow of the caller until an input is given.
	*/
	virtual void getline(std::string& buffer) const = 0;
};

///Keyboard input read from std::cin.
class KeyboardCin : public KeyboardInterface{
	public:
	///This function blocks the flow of the caller until a new line is present in std::cin.
	void getline(std::string& buffer) const override{
		std::getline(std::cin, buffer);
	}
};

///A mock for injecting strings as keyboard inputs.
class KeyboardMock : public KeyboardInterface{
	public:
	///Clears and fills *buffer* with the input set with KeyboardMock::setline().
	void getline(std::string& buffer) const override{
		buffer = this->getline_mockstr;
	}
	///Sets the next mock input to be used in KeyboardMock::getline().
	void setline(const std::string& str){
		this->getline_mockstr = str;
	}

	private:
	std::string getline_mockstr;
};

/**
A function to interpret lines from keyboard inputs and dispatch actions accordingly.

This should be used as a thread.
*/
void keyboard_listener(KeyboardInterface& keyboard_input, GlobalStates& global_states);

#endif
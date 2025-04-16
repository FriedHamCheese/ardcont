/**
\file KeyboardInterface.hpp
*/

#ifndef KEYBOARDINTERFACE_HPP
#define KEYBOARDINTERFACE_HPP

#include "GlobalStates.hpp"

/**
A function to interpret lines from keyboard inputs and dispatch actions accordingly.

This should be used as a thread.
*/
void keyboard_listener(GlobalStates& global_states);

#endif
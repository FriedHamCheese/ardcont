/**
\file command_interpreter.hpp
*/

#ifndef COMMAND_INTERPRETER_HPP
#define COMMAND_INTERPRETER_HPP

#include <ncursesw/ncurses.h>

#include "GlobalStates.hpp"

/**
A function to interpret lines from keyboard inputs and dispatch actions accordingly.
*/
void interpret_command(GlobalStates& global_states, const std::string& input_text) noexcept;

#endif
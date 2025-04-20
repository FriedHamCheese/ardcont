#include "KeyboardInterface.hpp"
#include "AudioTrack.hpp"
#include "GlobalStates.hpp"

#include "ntrb/utils.h"

#include <thread>
#include <cfloat>
#include <chrono>
#include <string>
#include <iostream>

void load_command(const std::string& args_str, GlobalStates& global_states){
	const std::size_t first_arg_separator_index = args_str.find(' ');
	if(first_arg_separator_index == std::string::npos){
		std::cerr << "KeyboardInterface: load_command(): Not enough arguments for the load command."
					<< "\n\tForm of the l command: l [track] [filename]\n";
		return;
	}

	try{
		const std::uint8_t track_id = std::stoi(args_str.substr(0, first_arg_separator_index));
		const std::string aud_filename = args_str.substr(first_arg_separator_index+1);
		const std::unique_ptr<AudioTrack>& deck_ptr = global_states.audio_tracks.at(track_id);
		const ntrb_AudioBufferNew_Error new_aud_err = deck_ptr->set_file_to_load_from(aud_filename.c_str(), global_states.get_frames_per_callback());
		if(new_aud_err){
			std::cerr << "KeyboardInterface: load_command(): Error setting a new track."
						<< "\n\t(ntrb_AudioBufferNew_Error " << new_aud_err << ").\n";
		}else 
			deck_ptr->load_audio_info(aud_filename);
		deck_ptr->display_deck_info();
	}
	catch(const std::invalid_argument& stoi_fmt_err){
		std::cerr << "KeyboardInterface: load_command(): track ID is not a number.\n";
	}
	catch(const std::out_of_range& stoi_out_of_range){
		std::cerr << "KeyboardInterface: load_command(): track ID too large, too small, or does not refer to an existing AudioTrack.\n";
	}
}

void play_toggle_command(const std::string& args_str, GlobalStates& global_states){
	if(args_str.size() == 0){
		std::cerr << "KeyboardInterface: play_toggle_command(): Deck index not specified." << std::endl;
		return;
	}
	
	try{
		const int deck_index = std::stoi(args_str);
		global_states.audio_tracks.at(deck_index)->toggle_play_pause();
	}
	catch(const std::invalid_argument& stoi_fmt_err){
		std::cerr << "KeyboardInterface: play_toggle_command(): track ID is not a number.\n";
	}
	catch(const std::out_of_range& stoi_out_of_range){
		std::cerr << "KeyboardInterface: play_toggle_command(): track ID too large, too small, or does not refer to an existing AudioTrack.\n";
	}
}

void effect_command(const std::string& args_str, GlobalStates& global_states){
	const std::size_t first_arg_separator_index = args_str.find(' ');
	if(first_arg_separator_index == std::string::npos){
		std::cerr << "KeyboardInterface: effect_command(): Not enough arguments for the effect command."
					<< "\n\tForm of the e command: e [track] [effect_id] [effect_strength] [effect_parameter]\n";
		return;
	}
	const std::size_t second_arg_index = first_arg_separator_index+1;
	
	const std::size_t second_arg_separator_index = args_str.find(' ', second_arg_index);
	if(second_arg_separator_index == std::string::npos or second_arg_index >= args_str.size()){
		std::cerr << "KeyboardInterface: effect_command(): Not enough arguments for the effect command."
					<< "\n\tForm of the e command: e [track] [effect_id] [effect_strength] [effect_parameter]\n";
		return;		
	}
	const std::size_t third_arg_index = second_arg_separator_index + 1;
	
	const std::size_t third_argument_separator = args_str.find(' ', third_arg_index);
	if(third_argument_separator == std::string::npos or third_arg_index >= args_str.size()){
		std::cerr << "KeyboardInterface: effect_command(): Not enough arguments for the effect command."
					<< "\n\tForm of the e command: e [track] [effect_id] [effect_strength] [effect_parameter]\n";
		return;		
	}
	
	const std::size_t fourth_arg_index = third_argument_separator + 1;
	if(fourth_arg_index >= args_str.size()){
		std::cerr << "KeyboardInterface: effect_command(): Not enough arguments for the effect command."
					<< "\n\tForm of the e command: e [track] [effect_id] [effect_strength] [effect_parameter]\n";
		return;		
	}

	int deck_index, effect_id = 0;
	float effect_parameter, effect_strength = 0;
	try{
		deck_index = std::stoi(args_str.substr(0, first_arg_separator_index));
		effect_id = std::stoi(args_str.substr(second_arg_index, second_arg_separator_index - second_arg_index));
		effect_strength = std::stof(args_str.substr(third_arg_index, third_argument_separator - third_arg_index));
		effect_parameter = std::stof(args_str.substr(fourth_arg_index, args_str.size() - fourth_arg_index));
	}
	catch(const std::invalid_argument& stoi_fmt_err){
		std::cerr << "KeyboardInterface: effect_command(): one of the command arguments is not a number.\n";
		return;
	}
	catch(const std::out_of_range& stoi_out_of_range){
		std::cerr << "KeyboardInterface: effect_command(): one of the command arguments overflows.\n";
	}
	
	try{
		const std::unique_ptr<AudioTrack>& deck = global_states.audio_tracks.at(deck_index);
		deck->effect_type = EffectType(effect_id);
		
		EffectContainer& effect_container = deck->get_effect_container();
		effect_container.effect_mix_ratio = ntrb_clamp_float(effect_strength, 0, 1.0);
		effect_container.param_value = ntrb_clamp_float(effect_parameter, 0, FLT_MAX);
	}
	catch(const std::out_of_range& stoi_out_of_range){
		std::cerr << "KeyboardInterface: effect_command(): invalid deck index.\n";
	}
}

void toggle_monitor_command(const std::string& args_str, GlobalStates& global_states){
	try{
		const std::uint8_t track_id = std::stoi(args_str);
		const std::unique_ptr<AudioTrack>& deck = global_states.audio_tracks.at(track_id);
		deck->output_to_monitor = not deck->output_to_monitor.load();
		std::cout << "Deck " << int16_t(track_id) << " monitor output";
		if(deck->output_to_monitor.load()) 
			std::cout << " on." << std::endl;
		else
			std::cout << " off." << std::endl;
	}
	catch(const std::invalid_argument& stoi_fmt_err){
		std::cerr << "KeyboardInterface: toggle_monitor_command(): track ID is not a number.\n";
	}
	catch(const std::out_of_range& stoi_out_of_range){
		std::cerr << "KeyboardInterface: toggle_monitor_command(): track ID too large, too small, or does not refer to an existing AudioTrack.\n";
	}
}

void keyboard_listener(GlobalStates& global_states) noexcept{
	try{
		//Wait for arduino 2 second wait before its outputs are sent.
		std::this_thread::sleep_for(std::chrono::seconds(3));
		std::cout << "Keyboard input is ready.\n";
		
		std::string user_input, command_str, args_str;

		while(not global_states.requested_exit.load()){
			user_input.clear();
			command_str.clear();
			args_str.clear();
			
			std::cout << ": " << std::flush;
			std::getline(std::cin, user_input);
			
			const std::size_t command_arg_separator_index = user_input.find(' ');
			const bool entire_line_is_command = command_arg_separator_index == std::string::npos;
			if(entire_line_is_command)
				command_str = user_input;
			else{
				command_str = user_input.substr(0, command_arg_separator_index);
				args_str = user_input.substr(command_arg_separator_index+1);
			}
			
			if(command_str == "q"){
				global_states.requested_exit = true;
				std::cout << "To quit the program, interact with any sensors on the Arduino..." << std::endl;
			}else if(command_str == "l")
				load_command(args_str, global_states);
			else if(command_str == "p")
				play_toggle_command(args_str, global_states);
			else if(command_str == "e")
				effect_command(args_str, global_states);
			else if(command_str == "tm")
				toggle_monitor_command(args_str, global_states);
			else std::cerr << "Invalid command " << command_str << '\n';
		}
	}
	catch(const std::exception& excp){
		std::cerr << "KeyboardInterface: keyboard_listener(): Uncaught exception: " << excp.what() << std::endl;
	}catch(...){
		std::cerr << "KeyboardInterface: keyboard_listener(): Uncaught throw." << std::endl;		
	}
}
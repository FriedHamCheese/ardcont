#include "KeyboardInterface.hpp"
#include "AudioTrack.hpp"
#include "GlobalStates.hpp"

#include <thread>
#include <chrono>
#include <string>
#include <iostream>

void keyboard_listener(KeyboardInterface& keyboard_input, GlobalStates& global_states){
	//Wait for arduino 2 second wait before its outputs are sent.
	std::this_thread::sleep_for(std::chrono::seconds(5));
	std::cout << "Keyboard input is ready.\n";
	
	std::string user_input, command_str, args_str;

	while(not global_states.requested_exit.load()){
		user_input.clear();
		command_str.clear();
		args_str.clear();
		
		std::cout << ": " << std::flush;
		keyboard_input.getline(user_input);
		
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
		}else if(command_str == "l"){
			const std::size_t first_arg_separator_index = args_str.find(' ');
			if(first_arg_separator_index == std::string::npos){
				std::cerr << "Not enough arguments for the load command."
						<< "\n\tForm of the l command: l [track] [filename]\n";
				continue;
			}

			try{
				const std::uint8_t track_id = std::stoi(args_str.substr(0, first_arg_separator_index));
				const std::string aud_filename = args_str.substr(first_arg_separator_index+1);
				const std::unique_ptr<AudioTrack>& deck_ptr = global_states.audio_tracks.at(track_id);
				const ntrb_AudioBufferNew_Error new_aud_err = deck_ptr->set_file_to_load_from(aud_filename.c_str(), global_states.get_frames_per_callback());
				if(new_aud_err){
					std::cerr << "keyboard_listener(): Error setting a new track."
								<< "\n\t(ntrb_AudioBufferNew_Error " << new_aud_err << ").\n";
				}else 
					deck_ptr->load_audio_info(aud_filename);
				deck_ptr->display_deck_info();
			}
			catch(const std::invalid_argument& stoi_fmt_err){
				std::cerr << "[Error]: keyboard_listener(): track ID is not a number.\n";
			}
			catch(const std::out_of_range& stoi_out_of_range){
				std::cerr << "[Error]: keyboard_listener(): track ID too large, too small, or does not refer to an existing AudioTrack.\n";
			}
		}
		else std::cerr << "Invalid command " << command_str << '\n';
	}
}
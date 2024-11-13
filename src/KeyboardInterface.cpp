#include "KeyboardInterface.hpp"
#include "AudioTrack.hpp"
#include "GlobalStates.hpp"

#include <thread>
#include <chrono>
#include <string>
#include <iostream>

void keyboard_listener(KeyboardInterface& keyboard_input, GlobalStates& global_states, 
						KeyboardListenerTestingFlags* const testing_flags){
	#ifndef ARDCONT_AUTOMATED_TEST
	std::this_thread::sleep_for(std::chrono::seconds(5));
	std::cout << "Keyboard input is ready.\n";
	#endif
	
	std::string user_input, command_str, arg_str;

	//we want to run this loop once per keyboard input mock
	#ifndef ARDCONT_AUTOMATED_TEST
	while(true){
	#endif
		if(global_states.requested_exit.load())
			return;
		
		user_input.clear();
		command_str.clear();
		arg_str.clear();
		
		#ifndef ARDCONT_AUTOMATED_TEST
		std::cout << ": " << std::flush;
		#endif		
		keyboard_input.getline(user_input);
		
		const std::size_t command_arg_separator_index = user_input.find(' ');
		if(command_arg_separator_index == std::string::npos)
			command_str = user_input;
		else{
			command_str = user_input.substr(0, command_arg_separator_index);
			arg_str = user_input.substr(command_arg_separator_index+1);
		}
		
		if(command_str == "q"){
			global_states.requested_exit = true;
			#ifndef ARDCONT_AUTOMATED_TEST
			std::cout << "To quit the program, interact with any sensors on the Arduino..." << std::endl;
			#endif
		}else if(command_str == "l"){
			const std::size_t first_arg_separator_index = arg_str.find(' ');			
			if(first_arg_separator_index == std::string::npos){
				#ifndef ARDCONT_AUTOMATED_TEST
				std::cerr << "Not enough arguments for the load command."
						<< "\n\tForm of the l command: l [track] [filename]\n";
				continue;
				#else
				testing_flags->l_command_not_enough_arguments = true;
				return;
				#endif
			}

			try{
				const std::uint8_t track_id = std::stoi(arg_str.substr(0, first_arg_separator_index));
				const std::string aud_filename = arg_str.substr(first_arg_separator_index+1);
				const std::unique_ptr<AudioTrack>& deck_ptr = global_states.audio_tracks.at(track_id);
				const ntrb_AudioBufferNew_Error new_aud_err = deck_ptr->set_file_to_load_from(aud_filename.c_str(), global_states.get_frames_per_callback());
				if(new_aud_err){
					#ifndef ARDCONT_AUTOMATED_TEST						
					std::cerr << "[Error]: keyboard_listener(): Error setting a new track."
								<< "\n\t(ntrb_AudioBufferNew_Error " << new_aud_err << ").\n";
					#else
					testing_flags->l_command_ntrbAudioBuffer_error = true;
					#endif
				}else{
					#ifndef ARDCONT_AUTOMATED_TEST
					deck_ptr->load_audio_info(aud_filename);
					#endif
				}
				#ifndef ARDCONT_AUTOMATED_TEST
				deck_ptr->display_deck_info();
				#endif
			}
			catch(const std::invalid_argument& stoi_fmt_err){
				#ifndef ARDCONT_AUTOMATED_TEST
				std::cerr << "[Error]: keyboard_listener(): track ID is not a number.\n";
				#else
				testing_flags->l_command_id_not_a_number = true;				
				#endif
			}
			catch(const std::out_of_range& stoi_out_of_range){
				#ifndef ARDCONT_AUTOMATED_TEST
				std::cerr << "[Error]: keyboard_listener(): track ID too large, too small, or does not refer to an existing AudioTrack.\n";
				#else
				testing_flags->l_command_id_out_of_range = true;
				#endif
			}
		}
		else{
			#ifndef ARDCONT_AUTOMATED_TEST
			std::cerr << "Invalid command " << command_str << '\n';
			#else
			testing_flags->invalid_command = true;
			#endif
		}
	#ifndef ARDCONT_AUTOMATED_TEST
	//...while(true){
	}
	#endif
}
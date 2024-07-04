#include "AudioTrack.hpp"
#include "audeng_state.hpp"
#include "audeng_wrapper.hpp"

#include "ntrb/alloc.h"
#include "ntrb/utils.h"
#include "serial/serial.h"

#include <cmath>
#include <vector>
#include <string>
#include <thread>
#include <cstdint>
#include <iostream>

static void serial_listener(serial::Serial& arduino_serial){
	std::string buffer;
	
	while(true){
		if(audeng_state::requested_exit.load())
			return;
		
		arduino_serial.readline(buffer);
		
		const size_t comma_index = buffer.find(',');
		if(comma_index == std::string::npos){
			std::cerr << "\n[Warn]: serial_listener(): No comma in line.";
			std::cerr << "\n\tRead text: " << buffer << std::flush;
			std::cout << ": " << std::flush;
			buffer.clear();
			continue;
		}
		
		try{
			const int sensor_id = std::stoi(buffer.substr(0, comma_index));
			const int sensor_value = std::stoi(buffer.substr(comma_index+1));
		
			switch(sensor_id){
				case 0:
					if(sensor_value){
						if(!audeng_state::audio_tracks[1].toggle_play_pause()){
							std::cerr << "[Error]: serial_listener(): Unable to toggle play-pause due to rwlock acquisition error.\n";
							std::cout << ": " << std::flush;
						}
					}
					break;
				case 1:
					if(sensor_value){
						if(!audeng_state::audio_tracks[1].set_loop()){
							std::cerr << "[Error]: serial_listener(): Unable to set loop cue due to rwlock acquisition error.\n";
							std::cout << ": " << std::flush;
						}
					}
					break;					
				case 2:
					if(sensor_value) audeng_state::audio_tracks[1].cancel_loop();
					break;
				case 3:{
					const float speed_multiplier = 0.5 + ((float)sensor_value / 1023.0);
					audeng_state::audio_tracks[1].speed_multiplier = speed_multiplier;
					std::cout << "speed: " << audeng_state::audio_tracks[1].speed_multiplier.load() << "x.\n";
					
					break;
				}
				case 4:
					audeng_state::audio_tracks[1].set_beats_per_loop(std::pow(2, ntrb_clamp_i64(sensor_value, -5, 5)));
					break;
			}
		}catch(const std::invalid_argument& not_a_number){
			std::cerr << "\n[Warn]: serial_listener(): recieved text instead of number.";
			std::cerr << "\n\tRead text: " << buffer << std::flush;
			std::cout << ": " << std::flush;
		}
		catch(const std::out_of_range& stoi_out_of_range){
			std::cerr << "\n[Error]: serial_listener(): Either the sensor ID or the value from the sensor is either too large or too small.\n";
			std::cout << ": " << std::flush;
		}
		
		buffer.clear();
	}
}

static void keyboard_listener(){
	std::this_thread::sleep_for(std::chrono::seconds(5));
	std::cout << "Keyboard input is ready.\n";
	
	std::string user_input, command_str, arg_str;

	while(true){
		if(audeng_state::requested_exit.load())
			return;
		
		user_input.clear();
		command_str.clear();
		arg_str.clear();

		std::cout << ": " << std::flush;
		std::getline(std::cin, user_input);
		
		const std::size_t command_arg_separator_index = user_input.find(' ');
		if(command_arg_separator_index == std::string::npos)
			command_str = user_input;
		else{
			command_str = user_input.substr(0, command_arg_separator_index);
			arg_str = user_input.substr(command_arg_separator_index+1);
		}
		
		if(command_str == "q"){
			audeng_state::requested_exit = true;
			std::cout << "To quit the program, interact with any sensors on the Arduino..." << std::endl;
		}else if(command_str == "l"){
			const std::size_t first_arg_separator_index = arg_str.find(' ');
			if(first_arg_separator_index == std::string::npos){
				std::cerr << "Not enough arguments for the load command."
						<< "\n\tForm of the l command: l [track] [filename]\n";
				continue;
			}
			
			try{
				const std::uint8_t track_id = std::stoi(arg_str.substr(0, first_arg_separator_index));
				const ntrb_AudioBufferNew_Error new_aud_err = audeng_state::audio_tracks[track_id].set_file_to_load_from(arg_str.substr(first_arg_separator_index+1).c_str());
				if(new_aud_err){
				std::cerr << "[Error]: keyboard_listener(): Error setting a new track."
							<< "\n\t(ntrb_AudioBufferNew_Error " << new_aud_err << ").\n";
				}
			}
			catch(const std::invalid_argument& stoi_fmt_err){
				std::cerr << "[Error]: keyboard_listener(): track ID is not a number.\n";
			}
			catch(const std::out_of_range& stoi_out_of_range){
				std::cerr << "[Error]: keyboard_listener(): track ID too large or too small.\n";				
			}
		}
		else if(command_str == "lc"){
			try{
				AudioTracks_loop_cue_step = std::stof(arg_str);
				std::cout << "Set global loop cue step to " << AudioTracks_loop_cue_step.load() << " beats." << std::endl;
			}
			catch(const std::invalid_argument& stof_fmt_err){
				std::cerr << "[Error]: keyboard_listener(): Loop cue step is not a number.\n";
			}
			catch(const std::out_of_range& stof_out_of_range){
				std::cerr << "[Error]: keyboard_listener(): Loop cue step too large or too small.\n";
			}
		}
		else std::cerr << "Invalid command " << command_str << '\n';
	}
}

int main(){
	#ifdef NTRB_MEMDEBUG
	ntrb_memdebug_init_with_return_value();
	#endif
	
	serial::Serial arduino_serial("", 9600);
	constexpr bool user_is_choosing_serial = true;
	
	while(user_is_choosing_serial){
		const std::vector<serial::PortInfo> serial_ports = serial::list_ports();
		for(const auto& port : serial_ports)
			std::cout << "Port name: " << port.port << "\n\tdesc: " << port.description << "\n\tID: " << port.hardware_id << '\n';
		std::cout << std::flush;
		
		std::string selected_port_name;
		std::cout << "Arduino port name: " << std::flush;
		std::getline(std::cin, selected_port_name);
		
		try{
			arduino_serial.setPort(selected_port_name);
			arduino_serial.open();
			
			if(!arduino_serial.isOpen())
				std::cerr << "Port is not opened.\n\n.";
			else
				break;
		}catch(const std::exception& e){
			std::cerr << "[Error]: Exception caught while trying to open a serial port."
						<< "\n\tstd::exception::what(): " << e.what() << "\n\n";
		}
	}
	
	arduino_serial.flush();
	std::cout << "Messages from the Arduino will be displayed to indicate the Arduino is ready.\n";
	std::cout << "First time serial connection may result in garbage being in the message.\n\n" << std::flush;
	
	std::thread serial_thread(serial_listener, std::ref(arduino_serial));
	std::thread keyboard_thread(keyboard_listener);
	std::thread audeng_thread(run_audio_engine);
	
	serial_thread.join();
	keyboard_thread.join();
	audeng_thread.join();
	
	audeng_state::audio_tracks[0].~AudioTrack();
	audeng_state::audio_tracks[1].~AudioTrack();	
	
	#ifdef NTRB_MEMDEBUG
	ntrb_memdebug_uninit(true);
	#endif
	
	return 0;
}
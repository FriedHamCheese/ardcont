#include "SerialInterface.hpp"
#include "audeng_state.hpp"
#include "sensor.hpp"

#include "serial/serial.h"

#include <string>
#include <iostream>

void serial_listener(SerialInterface& arduino_serial){
	std::string buffer;
	
	while(true){
		if(audeng_state::requested_exit.load()) return;
		
		arduino_serial.readline(buffer);
		const size_t comma_index = buffer.find(',');
		if(comma_index == std::string::npos){
			std::cerr << "\n[Warn]: serial_listener(): No comma in \"" << buffer << "\". Line ignored.\n";
			std::cout << ": " << std::flush;
			buffer.clear();
			continue;
		}
		
		try{
			const int sensor_id = std::stoi(buffer.substr(0, comma_index));
			const int sensor_value = std::stoi(buffer.substr(comma_index+1));
			
			switch(sensor_id){
				case SensorID_left_playpause_button:
					if(sensor_value == ButtonState_Released){
						if(!audeng_state::audio_tracks[0]->toggle_play_pause()){
							std::cerr << "[Error]: serial_listener(): Unable to toggle play-pause due to rwlock acquisition error.\n";
							std::cout << ": " << std::flush;
						}
					}
					break;
				case SensorID_left_cue_button:
					if(sensor_value == ButtonState_Released)
						audeng_state::audio_tracks[0]->cue_to_nearest_cue_point();
					break;
				case SensorID_left_tempo_poten:{
					const float speed_multiplier = 1.0 + ((float)(sensor_value - potentiometer_centre_value) / potentiometer_value_for_max_range);
					audeng_state::audio_tracks[0]->set_destination_speed_multiplier(speed_multiplier);
					std::cout << "Track 0 bpm: " << speed_multiplier * audeng_state::audio_tracks[0]->get_bpm() << "x.\n";
					std::cout << ": " << std::flush;
					break;
				}	
				case SensorID_left_loop_duration_rotaryenc:
					if(sensor_value == 1)
						audeng_state::audio_tracks[0]->fine_step_forward();
					else if(sensor_value == -1)
						audeng_state::audio_tracks[0]->fine_step_backward();						
					break;
				case SensorID_left_loop_in_button:
					if(sensor_value == ButtonState_Released){
						if(!audeng_state::audio_tracks[0]->set_loop()){
							std::cerr << "[Error]: serial_listener(): Unable to set loop cue due to rwlock acquisition error.\n";
							std::cout << ": " << std::flush;
						}
					}
					break;					
				case SensorID_left_loop_out_button:
					if(sensor_value == ButtonState_Released) audeng_state::audio_tracks[0]->cancel_loop();
					break;
					
				case SensorID_right_playpause_button:
					if(sensor_value == ButtonState_Released){
						if(!audeng_state::audio_tracks[1]->toggle_play_pause()){
							std::cerr << "[Error]: serial_listener(): Unable to toggle play-pause due to rwlock acquisition error.\n";
							std::cout << ": " << std::flush;
						}
					}
					break;
				case SensorID_right_cue_button:
					if(sensor_value)
						audeng_state::audio_tracks[1]->cue_to_nearest_cue_point();
					break;
				case SensorID_right_tempo_poten:{	
					const float speed_multiplier = 1.0 + ((float)(sensor_value - potentiometer_centre_value) / potentiometer_value_for_max_range);
					audeng_state::audio_tracks[1]->set_destination_speed_multiplier(speed_multiplier);
					std::cout << "Track 1 bpm: " << speed_multiplier * audeng_state::audio_tracks[1]->get_bpm() << "x.\n";
					std::cout << ": " << std::flush;
					break;
				}
				case SensorID_right_loop_duration_rotaryenc:
					if(sensor_value == 1)
						audeng_state::audio_tracks[1]->fine_step_forward();
					else if(sensor_value == -1)
						audeng_state::audio_tracks[1]->fine_step_backward();	
					break;				
				case SensorID_right_loop_in_button:
					if(sensor_value == 3){
						if(!audeng_state::audio_tracks[1]->set_loop()){
							std::cerr << "[Error]: serial_listener(): Unable to set loop cue due to rwlock acquisition error.\n";
							std::cout << ": " << std::flush;
						}
					}
					break;					
				case SensorID_right_loop_out_button:
					if(sensor_value) audeng_state::audio_tracks[1]->cancel_loop();
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
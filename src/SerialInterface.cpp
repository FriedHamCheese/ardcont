#include "SerialInterface.hpp"
#include "GlobalStates.hpp"
#include "sensor.hpp"

#include "../ardcont/SensorID.hpp"

#include "serial/serial.h"

#include <cstdint>
#include <string>
#include <thread>
#include <iostream>

void serial_listener(serial::Serial& arduino_serial, GlobalStates& global_states){
	std::vector<std::unique_ptr<Sensor>> sensors;
	sensors.reserve(12);
	sensors.emplace_back(std::make_unique<Button>(SensorID_left_playpause_button));
	sensors.emplace_back(std::make_unique<Button>(SensorID_left_cue_button));
	sensors.emplace_back(std::make_unique<Sensor>(SensorID_left_tempo_poten));
	sensors.emplace_back(std::make_unique<RotaryEncoder>(SensorID_left_jogdial_rotaryenc));
	sensors.emplace_back(std::make_unique<Button>(SensorID_left_loop_in_button));
	sensors.emplace_back(std::make_unique<Button>(SensorID_left_loop_out_button));
	
	sensors.emplace_back(std::make_unique<Button>(SensorID_right_playpause_button));
	sensors.emplace_back(std::make_unique<Button>(SensorID_right_cue_button));
	sensors.emplace_back(std::make_unique<Sensor>(SensorID_right_tempo_poten));
	sensors.emplace_back(std::make_unique<RotaryEncoder>(SensorID_right_jogdial_rotaryenc));
	sensors.emplace_back(std::make_unique<Button>(SensorID_right_loop_in_button));
	sensors.emplace_back(std::make_unique<Button>(SensorID_right_loop_out_button));
	
	std::string buffer;
	std::thread sensor_translating_thread(_translate_sensor_changes, std::ref(sensors), std::ref(global_states));
	
	while(not global_states.requested_exit.load()){	
		arduino_serial.readline(buffer);
		const size_t comma_index = buffer.find(',');
		if(comma_index == std::string::npos){
			std::cerr << "\nserial_listener(): No comma in \"" << buffer << "\". Line ignored.\n";
			std::cout << ": " << std::flush;
			buffer.clear();
			continue;
		}
		
		try{
			const int sensor_id = std::stoi(buffer.substr(0, comma_index));
			const int sensor_value = std::stoi(buffer.substr(comma_index+1));
			_write_to_sensor(sensors, sensor_id, sensor_value);

		}catch(const std::invalid_argument& not_a_number){
			std::cerr << "\nserial_listener(): recieved text instead of number.";
			std::cerr << "\n\tRead text: " << buffer << std::flush;
			std::cout << ": " << std::flush;
		}
		catch(const std::out_of_range& stoi_out_of_range){
			std::cerr << "\nserial_listener(): Either the sensor ID or the value from the sensor is either too large or too small.\n";
			std::cout << ": " << std::flush;
		}
		
		buffer.clear();
	}
	sensor_translating_thread.join();
}


bool _write_to_sensor(std::vector<std::unique_ptr<Sensor>>& sensors, 
						const std::int16_t sensor_id, const std::int16_t sensor_value)
{
	for(auto& sensor : sensors){
		if(sensor->sensor_id == sensor_id){
			std::lock_guard<std::mutex> current_sensor_access(sensor->access_mutex);
			sensor->write(sensor_value);
			return true;
		}
	}
	return false;
}

std::optional<std::int16_t> _read_sensor(std::vector<std::unique_ptr<Sensor>>& sensors, const::std::int16_t sensor_id){
	for(const auto& sensor : sensors){
		if(sensor->sensor_id == sensor_id){
			std::lock_guard<std::mutex> current_sensor_access(sensor->access_mutex);
			return sensor->value;
		}
	}
	return std::nullopt;
}

void _translate_sensor_changes(std::vector<std::unique_ptr<Sensor>>& sensors, GlobalStates& global_states){
	const std::unique_ptr<AudioTrack>& left_deck = global_states.audio_tracks[0];
	const std::unique_ptr<AudioTrack>& right_deck = global_states.audio_tracks[1];
	
	while(not global_states.requested_exit.load()){
		for(auto& sensor : sensors){
			std::lock_guard<std::mutex> current_sensor_access(sensor->access_mutex);
			if(not sensor->value_changed()) continue;
			
			switch(sensor->sensor_id){
				case SensorID_left_playpause_button:
					if(sensor->value == ButtonState_Released){
						if(!global_states.audio_tracks[0]->toggle_play_pause()){
							std::cerr << "SerialInterface: _translate_sensor_changes(): Unable to toggle play-pause due to rwlock acquisition error.\n";
							std::cout << ": " << std::flush;
						}
					}
					break;
				case SensorID_left_cue_button:{
					const AudioTrack_PlayMode play_mode = left_deck->get_play_mode();
					const bool initiate_return_to_nearest_cue = (sensor->value == ButtonState_Released) 
																and ((play_mode == AudioTrack_regular_play) or (play_mode == AudioTrack_slowdown_to_halt));
					const bool initiate_cue_play = (sensor->value == ButtonState_Pressed) and (play_mode == AudioTrack_no_playback);
					const bool stop_cue_play = (sensor->value == ButtonState_Released) and (play_mode == AudioTrack_cue_play);
					
					if(initiate_return_to_nearest_cue)
						left_deck->cue_to_nearest_cue_point();
					else if(initiate_cue_play)
						left_deck->initiate_cue_play();
					else if(stop_cue_play)
						left_deck->stop_cue_play();
					break;
				}
				case SensorID_left_tempo_poten:{
					const float speed_multiplier = 1.0 + ((float)(sensor->value - potentiometer_centre_value) / potentiometer_value_for_max_range);
					left_deck->set_destination_speed_multiplier(speed_multiplier);
					std::cout << "Track 0 bpm: " << speed_multiplier * left_deck->get_bpm() << "x.\n";
					std::cout << ": " << std::flush;
					break;
				}	
				case SensorID_left_jogdial_rotaryenc:{
					const std::optional<std::int16_t> sensor_value_opt = _read_sensor(sensors, SensorID_left_loop_in_button);
					if(not sensor_value_opt.has_value()){
						std::cerr << "SerialInterface: _translate_sensor_changes(): no sensor with ID " << SensorID_left_loop_in_button << '\n';
						std::cout << ": " << std::flush;
					}
					const bool holding_loop_in_button = sensor_value_opt.value() == ButtonState_Held;
					if(holding_loop_in_button){
						float loop_step = 0;
						if(sensor->value == 1)
							loop_step = left_deck->increment_loop_step();
						else if(sensor->value == -1)
							loop_step = left_deck->decrement_loop_step();
						std::cout << "Track 0 beats per loop: " << loop_step << '\n';
						std::cout << ": " << std::flush;
					}else{
						if(left_deck->get_play_mode() == AudioTrack_no_playback or left_deck->get_play_mode() == AudioTrack_beat_preview){
							if(sensor->value == 1)
								left_deck->play_only_next_beat();
							else if(sensor->value == -1)
								left_deck->play_only_prev_beat();							
						}else{
							if(sensor->value == 1)
								left_deck->fine_step_forward();
							else if(sensor->value == -1)
								left_deck->fine_step_backward();
						}
					}
					break;
				}
				case SensorID_left_loop_in_button:
					if(sensor->value == ButtonState_Released){
						if(!left_deck->set_loop()){
							std::cerr << "SerialInterface: _translate_sensor_changes(): Unable to set loop cue due to rwlock acquisition error.\n";
							std::cout << ": " << std::flush;
						}
					}
					break;					
				case SensorID_left_loop_out_button:
					if(sensor->value == ButtonState_Released) left_deck->cancel_loop();
					break;
					
				case SensorID_right_playpause_button:
					if(sensor->value == ButtonState_Released){
						if(!right_deck->toggle_play_pause()){
							std::cerr << "SerialInterface: _translate_sensor_changes(): Unable to toggle play-pause due to rwlock acquisition error.\n";
							std::cout << ": " << std::flush;
						}
					}
					break;
				case SensorID_right_cue_button:{
					const AudioTrack_PlayMode play_mode = right_deck->get_play_mode();
					const bool initiate_return_to_nearest_cue = (sensor->value == ButtonState_Released) 
																and ((play_mode == AudioTrack_regular_play) or (play_mode == AudioTrack_slowdown_to_halt));
					const bool initiate_cue_play = (sensor->value == ButtonState_Pressed) and (play_mode == AudioTrack_no_playback);
					const bool stop_cue_play = (sensor->value == ButtonState_Released) and (play_mode == AudioTrack_cue_play);
					
					if(initiate_return_to_nearest_cue)
						right_deck->cue_to_nearest_cue_point();
					else if(initiate_cue_play)
						right_deck->initiate_cue_play();
					else if(stop_cue_play)
						right_deck->stop_cue_play();
					break;
				}
				case SensorID_right_tempo_poten:{	
					const float speed_multiplier = 1.0 + ((float)(sensor->value - potentiometer_centre_value) / potentiometer_value_for_max_range);
					right_deck->set_destination_speed_multiplier(speed_multiplier);
					std::cout << "Track 1 bpm: " << speed_multiplier * right_deck->get_bpm() << "x.\n";
					std::cout << ": " << std::flush;
					break;
				}
				case SensorID_right_jogdial_rotaryenc:{
					const std::optional<std::int16_t> sensor_value_opt = _read_sensor(sensors, SensorID_right_loop_in_button);
					if(not sensor_value_opt.has_value()){
						std::cerr << "SerialInterface: _translate_sensor_changes(): no sensor with ID " << SensorID_right_loop_in_button << '\n';
						std::cout << ": " << std::flush;
					}
					const bool holding_loop_in_button = sensor_value_opt.value() == ButtonState_Held;
					if(holding_loop_in_button){
						float loop_step = 0;
						if(sensor->value == 1)
							loop_step = right_deck->increment_loop_step();
						else if(sensor->value == -1)
							loop_step = right_deck->decrement_loop_step();
						std::cout << "Track 1 beats per loop: " << loop_step << '\n';
						std::cout << ": " << std::flush;
					}else{
						if(right_deck->get_play_mode() == AudioTrack_no_playback or right_deck->get_play_mode() == AudioTrack_beat_preview){
							if(sensor->value == 1)
								right_deck->play_only_next_beat();
							else if(sensor->value == -1)
								right_deck->play_only_prev_beat();							
						}else{
							if(sensor->value == 1)
								right_deck->fine_step_forward();
							else if(sensor->value == -1)
								right_deck->fine_step_backward();
						}
					}
					break;	
				}					
				case SensorID_right_loop_in_button:{
					if(sensor->value == ButtonState_Released){
						if(!right_deck->set_loop()){
							std::cerr << "SerialInterface: _translate_sensor_changes(): Unable to set loop cue due to rwlock acquisition error.\n";
							std::cout << ": " << std::flush;
						}
					}
					break;
				}
				case SensorID_right_loop_out_button:
					if(sensor->value == ButtonState_Released) right_deck->cancel_loop();
					break;
			}
		}
	}
}
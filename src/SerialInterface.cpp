#include "SerialInterface.hpp"
#include "GlobalStates.hpp"
#include "sensor.hpp"
#include "ui.hpp"

#include "../ardcont/SensorID.hpp"

#include "serial/serial.h"

#include <cstdint>
#include <string>
#include <thread>
#include <iostream>

void serial_listener(serial::Serial& arduino_serial, GlobalStates& global_states) noexcept{
	while(not global_states.requested_exit.load()){
		try{
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
					buffer.clear();
					continue;
				}
				
				try{
					const int sensor_id = std::stoi(buffer.substr(0, comma_index));
					const int sensor_value = std::stoi(buffer.substr(comma_index+1));
					_write_to_sensor(sensors, sensor_id, sensor_value);

				}catch(const std::invalid_argument& not_a_number){
					const std::string msg = std::string("Serial: ") + buffer + std::string(" are not numbers.");
					ui::print_to_infobar(msg, UIColorPair_Warning);
				}
				catch(const std::out_of_range& stoi_out_of_range){
					const std::string msg = std::string("Serial: ") + buffer + std::string(" value is out of range.");
					ui::print_to_infobar(msg, UIColorPair_Warning);
				}
				
				buffer.clear();
			}
			sensor_translating_thread.join();
		}
		catch(const std::exception& excp){
			const std::string msg = std::string("SerialInterface: serial_listener(): ") + excp.what();
			ui::print_to_infobar(msg, UIColorPair_Error);
		}
		catch(...){
			ui::print_to_infobar("SerialInterface: serial_listener(): Uncaught throw.", UIColorPair_Error);		
		}
	}
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
						if(!global_states.audio_tracks[0]->toggle_play_pause())
							ui::print_to_infobar("SerialInterface: _translate_sensor_changes(): mutex error.", UIColorPair_Error);
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
					break;
				}	
				case SensorID_left_jogdial_rotaryenc:{
					const std::optional<std::int16_t> sensor_value_opt = _read_sensor(sensors, SensorID_left_loop_in_button);
					if(not sensor_value_opt.has_value()){
						const std::string msg = std::string("Serial: No sensor with ID ") + std::to_string(SensorID_left_loop_in_button);
						ui::print_to_infobar(msg, UIColorPair_Error);
					}
					const bool holding_loop_in_button = sensor_value_opt.value() == ButtonState_Held;
					if(holding_loop_in_button){
						if(sensor->value == 1)
							left_deck->increment_loop_step();
						else if(sensor->value == -1)
							left_deck->decrement_loop_step();
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
						if(!left_deck->set_loop())
							ui::print_to_infobar("SerialInterface: _translate_sensor_changes(): mutex error.", UIColorPair_Error);
					}
					break;					
				case SensorID_left_loop_out_button:
					if(sensor->value == ButtonState_Released) left_deck->cancel_loop();
					break;
					
				case SensorID_right_playpause_button:
					if(sensor->value == ButtonState_Released){
						if(!right_deck->toggle_play_pause())
							ui::print_to_infobar("SerialInterface: _translate_sensor_changes(): mutex error.", UIColorPair_Error);
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
					break;
				}
				case SensorID_right_jogdial_rotaryenc:{
					const std::optional<std::int16_t> sensor_value_opt = _read_sensor(sensors, SensorID_right_loop_in_button);
					if(not sensor_value_opt.has_value()){			
						const std::string msg = std::string("Serial: No sensor with ID ") + std::to_string(SensorID_right_loop_in_button);
						ui::print_to_infobar(msg, UIColorPair_Error);
					}
					const bool holding_loop_in_button = sensor_value_opt.value() == ButtonState_Held;
					if(holding_loop_in_button){
						if(sensor->value == 1)
							right_deck->increment_loop_step();
						else if(sensor->value == -1)
							right_deck->decrement_loop_step();
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
						if(!right_deck->set_loop())
							ui::print_to_infobar("SerialInterface: _translate_sensor_changes(): mutex error.", UIColorPair_Error);
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
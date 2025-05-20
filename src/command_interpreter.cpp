#include "command_interpreter.hpp"
#include "AudioTrack.hpp"
#include "GlobalStates.hpp"

#include "ui.hpp"
#include "ntrb/utils.h"

#include <ncursesw/ncurses.h>

#include <thread>
#include <cfloat>
#include <chrono>
#include <string>
#include <iostream>

void load_command(const std::string& args_str, GlobalStates& global_states){	
	const std::size_t first_arg_separator_index = args_str.find(' ');
	if(first_arg_separator_index == std::string::npos){
		ui::print_to_infobar("l(oad) command format: l track_id filename", UIColorPair_Error);
		return;
	}

	try{
		const std::uint8_t track_id = std::stoi(args_str.substr(0, first_arg_separator_index));
		const std::string aud_filename = args_str.substr(first_arg_separator_index+1);
		const std::unique_ptr<AudioTrack>& deck_ptr = global_states.audio_tracks.at(track_id);
		const ntrb_AudioBufferNew_Error new_aud_err = deck_ptr->set_file_to_load_from(aud_filename.c_str(), global_states.get_frames_per_callback());
		if(new_aud_err){
			const std::string msg = std::string("Error setting the track (ntrb_AudioBufferNew_Error ") + std::to_string(new_aud_err) + std::string(")");
			ui::print_to_infobar(msg, UIColorPair_Error);
		}else 
			deck_ptr->load_audio_info(aud_filename);
	}
	catch(const std::invalid_argument& stoi_fmt_err){
		ui::print_to_infobar("Track ID not a number.", UIColorPair_Error);
	}
	catch(const std::out_of_range& stoi_out_of_range){
		ui::print_to_infobar("Track ID not in range.", UIColorPair_Error);
	}
}

void play_toggle_command(const std::string& args_str, GlobalStates& global_states){
	if(args_str.size() == 0){
		ui::print_to_infobar("p(lay/pause) command format: p track_id", UIColorPair_Error);
		return;
	}
	
	try{
		const int deck_index = std::stoi(args_str);
		global_states.audio_tracks.at(deck_index)->toggle_play_pause();
	}
	catch(const std::invalid_argument& stoi_fmt_err){
		ui::print_to_infobar("Track ID not a number.", UIColorPair_Error);
	}
	catch(const std::out_of_range& stoi_out_of_range){
		ui::print_to_infobar("Track ID not in range.", UIColorPair_Error);
	}
}

void effect_command(const std::string& args_str, GlobalStates& global_states){
	const std::size_t first_arg_separator_index = args_str.find(' ');
	if(first_arg_separator_index == std::string::npos){
		ui::print_to_infobar("e(ffect) command format: track_id effect_id effect_strength effect_parameter", UIColorPair_Error);
		return;
	}
	const std::size_t second_arg_index = first_arg_separator_index+1;
	
	const std::size_t second_arg_separator_index = args_str.find(' ', second_arg_index);
	if(second_arg_separator_index == std::string::npos or second_arg_index >= args_str.size()){
		ui::print_to_infobar("e(ffect) command format: track_id effect_id effect_strength effect_parameter", UIColorPair_Error);
		return;		
	}
	const std::size_t third_arg_index = second_arg_separator_index + 1;
	
	const std::size_t third_argument_separator = args_str.find(' ', third_arg_index);
	if(third_argument_separator == std::string::npos or third_arg_index >= args_str.size()){
		ui::print_to_infobar("e(ffect) command format: track_id effect_id effect_strength effect_parameter", UIColorPair_Error);
		return;		
	}
	
	const std::size_t fourth_arg_index = third_argument_separator + 1;
	if(fourth_arg_index >= args_str.size()){
		ui::print_to_infobar("e(ffect) command format: track_id effect_id effect_strength effect_parameter", UIColorPair_Error);
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
		ui::print_to_infobar("All command arguments must be numbers.", UIColorPair_Error);
		return;
	}
	catch(const std::out_of_range& stoi_out_of_range){
		ui::print_to_infobar("One of the command arguments is not in range.", UIColorPair_Error);
		return;
	}
	
	try{
		const std::unique_ptr<AudioTrack>& deck = global_states.audio_tracks.at(deck_index);
		deck->effect_type = EffectType(effect_id);
		
		EffectContainer& effect_container = deck->get_effect_container();
		effect_container.effect_mix_ratio = ntrb_clamp_float(effect_strength, 0, 1.0);
		effect_container.param_value = ntrb_clamp_float(effect_parameter, 0, FLT_MAX);
	}
	catch(const std::out_of_range& stoi_out_of_range){
		ui::print_to_infobar("Invalid deck index.", UIColorPair_Error);
	}
}

void toggle_monitor_command(const std::string& args_str, GlobalStates& global_states){
	try{
		const std::uint8_t track_id = std::stoi(args_str);
		const std::unique_ptr<AudioTrack>& deck = global_states.audio_tracks.at(track_id);
		deck->output_to_monitor = not deck->output_to_monitor.load();
		if(deck->output_to_monitor.load()){
			const std::string msg = std::string("Track ") + std::to_string(track_id) + std::string(" monitoring on");
			ui::print_to_infobar(msg, UIColorPair_Error);
		}else{
			const std::string msg = std::string("Track ") + std::to_string(track_id) + std::string(" monitoring off");
			ui::print_to_infobar(msg, UIColorPair_Error);
		}
	}
	catch(const std::invalid_argument& stoi_fmt_err){
		ui::print_to_infobar("Track ID not a number.", UIColorPair_Error);
	}
	catch(const std::out_of_range& stoi_out_of_range){
		ui::print_to_infobar("Track ID not in range.", UIColorPair_Error);
	}
}

void interpret_command(GlobalStates& global_states, const std::string& input_text) noexcept{
	try{
		std::string command_str, args_str;
		command_str.clear();
		args_str.clear();
		
		const std::size_t command_arg_separator_index = input_text.find(' ');
		const bool entire_line_is_command = command_arg_separator_index == std::string::npos;
		if(entire_line_is_command)
			command_str = input_text;
		else{
			command_str = input_text.substr(0, command_arg_separator_index);
			args_str = input_text.substr(command_arg_separator_index+1);
		}
		ui::print_to_infobar("interpret_command()", UIColorPair_Info);
		
		if(command_str == "q"){
			global_states.requested_exit = true;
			ui::print_to_infobar("Waiting for a sensor update on the Arudino...", UIColorPair_Info);
		}else if(command_str == "l")
			load_command(args_str, global_states);
		else if(command_str == "p")
			play_toggle_command(args_str, global_states);
		else if(command_str == "e")
			effect_command(args_str, global_states);
		else if(command_str == "tm")
			toggle_monitor_command(args_str, global_states);
		else ui::print_to_infobar("Invalid command", UIColorPair_Error);
	}
	catch(const std::exception& excp){
		ui::print_to_infobar(std::string("command_interpreter: interpret_command(): ") + excp.what(), UIColorPair_Error);
	}catch(...){
		ui::print_to_infobar("command_interpreter: interpret_command(): Uncaught throw.", UIColorPair_Error);
	}
}
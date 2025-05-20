#include "ui.hpp"
#include "command_interpreter.hpp"

#include <thread>
#include <string>
#include <cstring>

std::string ui::filename_from_filepath(const std::string& filepath){
	const std::string::size_type last_directory_separator_index = filepath.rfind('/');
	const bool no_directory_separator = last_directory_separator_index == std::string::npos;
	if(no_directory_separator) return filepath;
	
	const bool no_characters_after_directory_separator = filepath[last_directory_separator_index] == filepath.back();
	if(no_characters_after_directory_separator) return filepath;
	return filepath.substr(last_directory_separator_index+1);
}

std::string ui::ms_to_mm_ss_mss_str(std::uint32_t ms){
	constexpr int ms_per_minute = 60 * 1000;
	const int minutes = ms / ms_per_minute;
	const int seconds = (ms % ms_per_minute) / 1000;
	const int millisecs = ms % 1000;
	
	std::string return_str;
	return_str.reserve(2+1+2+1+3);
	
	if(minutes > 99) return_str += "99";
	else if (minutes >= 10) return_str += std::to_string(minutes);
	else return_str += ("0" + std::to_string(minutes));
	return_str += ':';
	
	if(seconds >= 10) return_str += std::to_string(seconds);
	else return_str += ("0" + std::to_string(seconds));
	
	return_str += ':';
	if(millisecs >= 100) return_str += std::to_string(millisecs);
	else if(millisecs >= 10) return_str += ("0" + std::to_string(millisecs));
	else return_str += ("00" + std::to_string(millisecs));
	
	return return_str;
}

static void draw_audiotrack_info_to_deck_window(WINDOW* const window, const std::unique_ptr<AudioTrack>& audiotrack){
	const std::uint32_t current_ms = ui::stdaud_frames_to_ms(audiotrack->get_current_stdaud_frame());
	
	mvwprintw(window, 1, 1, ui::filename_from_filepath(audiotrack->get_filename()).c_str());
	mvwprintw(window, 2, 1, "%s", ui::ms_to_mm_ss_mss_str(current_ms).c_str());
	mvwprintw(window, 3, 1, "BPM: %.2f (%.2fx)", audiotrack->get_bpm() * audiotrack->destination_speed_multiplier.load(), audiotrack->destination_speed_multiplier.load());
	if(audiotrack->is_loop_queued()){
		const std::uint32_t loop_begin_ms = ui::stdaud_frames_to_ms(audiotrack->get_loop_frame_begin());
		const std::uint32_t loop_end_ms = ui::stdaud_frames_to_ms(audiotrack->get_loop_frame_end());

		mvwprintw(window, 4, 1, "Loop %s - %s (%.2f beats)", ui::ms_to_mm_ss_mss_str(loop_begin_ms).c_str(), ui::ms_to_mm_ss_mss_str(loop_end_ms).c_str(), audiotrack->get_beats_per_loop());
	}
	
	const EffectType effect_type = audiotrack->effect_type;
	const EffectContainer& effect_container = audiotrack->get_effect_container();
	mvwprintw(window, 6, 1, "Effect: %s", effect_names[effect_type]);
	mvwprintw(window, 7, 1, "\tMix ratio: %.2f", effect_container.effect_mix_ratio);
	
	switch(effect_type){
		case EffectType_None:
		mvwprintw(window, 8, 1, "\tParameter value: %.2f", effect_container.param_value);
		break;
		case EffectType_Echo:
		mvwprintw(window, 8, 1, "\tDelay: %.2f", effect_container.param_value);
		break;
		case EffectType_LowerSamplerate:
		mvwprintw(window, 8, 1, "\t1/%.2f", effect_container.param_value);
		break;
		case EffectType_Bitcrush:
		mvwprintw(window, 8, 1, "\t %d-bit", (int)effect_container.param_value);
		break;
	}
	
	if(audiotrack->output_to_monitor.load())
		mvwprintw(window, 9, 1, "Monitored");
}

void ui::print_to_infobar(const std::string& msg, UIColorPairIndex message_color){
	ui::last_infobar_message = std::make_tuple(msg, message_color, std::chrono::steady_clock::now());
}
void ui::print_to_infobar(const std::string&& msg, UIColorPairIndex message_color){
	ui::last_infobar_message = std::make_tuple(msg, message_color, std::chrono::steady_clock::now());
}

void ui::render_ui(GlobalStates& global_states) noexcept{
	static std::string typing_text;
	
	while(not global_states.requested_exit.load()){
		try{
			werase(ui::left_deck_info_window);
			box(ui::left_deck_info_window, 0, 0);
			ui::draw_center_text(ui::left_deck_info_window, "Left Deck", 0);
			draw_audiotrack_info_to_deck_window(ui::left_deck_info_window, global_states.audio_tracks[0]);
			wrefresh(ui::left_deck_info_window);
			
			werase(ui::right_deck_info_window);
			box(ui::right_deck_info_window, 0, 0);
			ui::draw_center_text(ui::right_deck_info_window, "Right Deck", 0);
			draw_audiotrack_info_to_deck_window(ui::right_deck_info_window, global_states.audio_tracks[1]);
			wrefresh(ui::right_deck_info_window);
			
			werase(ui::stdout_window);
			const bool last_message_timed_out = std::chrono::steady_clock::now() - std::get<2>(ui::last_infobar_message) > ui::infobar_timeout;
			if(not last_message_timed_out){
				attron(A_REVERSE);
				if(ui::has_colors) attron(COLOR_PAIR(std::get<1>(ui::last_infobar_message)));
				
				mvwprintw(ui::stdout_window, 0, 3, std::get<0>(ui::last_infobar_message).c_str());			
				
				if(ui::has_colors) attroff(COLOR_PAIR(std::get<1>(ui::last_infobar_message)));	
				attroff(A_REVERSE);
			}else
				mvwprintw(ui::stdout_window, 0, 3, " ");					
			
			wrefresh(ui::stdout_window);
			
			werase(ui::keyboard_input_window);
			box(ui::keyboard_input_window, 0, 0);
			
			const std::chrono::milliseconds total_wait_for_user_keystroke(5);
			const std::uint8_t wait_attempts = 8;
			const std::chrono::microseconds wait_per_attempt = total_wait_for_user_keystroke / wait_attempts;
			for(std::uint8_t i = 0; i < wait_attempts; i++){
				std::this_thread::sleep_for(wait_per_attempt);
				const int key_pressed = getch();
				const bool key_is_normal_ASCII = key_pressed < 128;
				
				if(key_pressed == ERR){
					continue;
				}
				else if(key_is_normal_ASCII and key_pressed != '\n'){
					typing_text += key_pressed;
				}
				else if(key_pressed == '\n' or key_pressed == KEY_ENTER){
					interpret_command(global_states, typing_text);
					typing_text.clear();
				}
				else if(key_pressed == KEY_BACKSPACE or key_pressed == KEY_DC){
					if(typing_text.size() > 0) typing_text.pop_back();
				}
				break;
			}
			mvwprintw(ui::keyboard_input_window, 1, 1, ": %s", typing_text.c_str());
			wrefresh(ui::keyboard_input_window);
			
			std::this_thread::sleep_for(std::chrono::milliseconds(5));
		}
		catch(const std::exception& excp){
			ui::print_to_infobar(std::string("ui::render_ui(): ") + excp.what(), UIColorPair_Error);
		}
		catch(...){
			ui::print_to_infobar(std::string("ui::render_ui(): uncaught throw."), UIColorPair_Error);
		}
	}
}
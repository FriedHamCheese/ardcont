#ifndef UI_HPP
#define UI_HPP

#include "GlobalStates.hpp"

#include <ncursesw/ncurses.h>

#include <cstring>
#include <chrono>
#include <utility>

enum UIColorPairIndex: uint8_t{
	UIColorPair_Default,
	UIColorPair_Error,
	UIColorPair_Warning,
	UIColorPair_Info
};

namespace ui{
	inline WINDOW* left_deck_info_window;
	inline WINDOW* right_deck_info_window;
	inline WINDOW* keyboard_input_window;
	inline WINDOW* stdout_window;
	
	inline std::tuple<std::string, UIColorPairIndex, std::chrono::time_point<std::chrono::steady_clock>> 
	last_infobar_message("", UIColorPair_Default, std::chrono::time_point<std::chrono::steady_clock>());

	inline constexpr std::uint16_t input_window_height = 3;
	inline constexpr std::uint16_t stdout_window_height = 2;
	inline constexpr std::uint16_t deck_info_height = 20;
	inline std::uint16_t deck_info_window_width = 20;
	inline bool has_colors = false;
	
	inline std::chrono::seconds infobar_timeout(5);

	inline std::uint16_t get_window_width(WINDOW* const window){
		[[maybe_unused]] std::uint16_t width, height;
		getmaxyx(window, height, width);
		return width;
	}
	inline std::uint16_t get_window_height(WINDOW* const window){
		[[maybe_unused]] std::uint16_t width, height;
		getmaxyx(window, height, width);
		return height;		
	}	

	inline std::int32_t text_center_xoffset(const std::uint16_t width, const std::uint16_t string_length){
		return (width - string_length) / 2;
	}
	inline std::int32_t text_center_xoffset(WINDOW* const window, const std::uint16_t string_length){
		return (get_window_width(window) - string_length) / 2;
	}
	inline bool draw_center_text(WINDOW* const window, const char* const str, const std::uint16_t window_ypos){
		return mvwprintw(window, window_ypos, text_center_xoffset(window, strlen(str)), str) != ERR;
	}
	
	std::string filename_from_filepath(const std::string& filepath);
	std::string ms_to_mm_ss_mss_str(std::uint32_t ms);
	inline constexpr float stdaud_frames_to_ms(const std::uint32_t frames) noexcept{
		return (float)frames * 1000.0 / (float)ntrb_std_samplerate;
	}	

	void print_to_infobar(const std::string& msg, UIColorPairIndex message_color);
	void print_to_infobar(const std::string&& msg, UIColorPairIndex message_color);
	
	void render_ui(GlobalStates& global_states) noexcept;
}

#endif
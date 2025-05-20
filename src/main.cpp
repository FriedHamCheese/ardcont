#include "SerialInterface.hpp"

#include "ui.hpp"
#include "AudioTrack.hpp"
#include "GlobalStates.hpp"
#include "OutputDevicesInterface.hpp"

#include "ntrb/alloc.h"

#include "serial/serial.h"
#include "portaudio.h"

#include <ncursesw/ncurses.h>

#include <vector>
#include <string>
#include <thread>
#include <iostream>
#include <algorithm>

/*
features:
- slip mode
- hot cues
issues:
- play/pause and effect having callback delay D:
*/

std::pair<PaDeviceIndex, PaDeviceIndex> user_select_output_devices(){
	const PaHostApiIndex host_api_index = Pa_GetDefaultHostApi();
	
	std::cout << "\nAvailable output devices\n";
	
	const PaDeviceIndex device_count = Pa_GetDeviceCount();
	std::cout << "ID" << "\tDevice name\n" ;
	
	std::vector<PaDeviceIndex> valid_output_devices;
	
	for(PaDeviceIndex i = 0; i < device_count; i++){
		const PaDeviceInfo* const device_info = Pa_GetDeviceInfo(i);
		const bool is_output_device = device_info->maxOutputChannels > 0;
		if(is_output_device and device_info->hostApi == host_api_index){
			std::cout << i << '\t' << device_info->name << '\n';
			valid_output_devices.push_back(i);
		}
	}
	std::cout << std::flush;
	
	bool valid_device_id = false;
	while(not valid_device_id){
		try{
			std::string audience_output_device_id_str;
			std::string monitor_output_device_id_str;
			
			std::cout << "Audience output device ID: " << std::flush;
			std::getline(std::cin, audience_output_device_id_str);
			const PaDeviceIndex audience_output_device_id = std::stoi(audience_output_device_id_str);

			std::cout << "Monitor output device ID: " << std::flush;
			std::getline(std::cin, monitor_output_device_id_str);
			const PaDeviceIndex monitor_output_device_id = std::stoi(monitor_output_device_id_str);

			const bool invalid_audience_output_device 
				= std::find(valid_output_devices.begin(), valid_output_devices.end(), audience_output_device_id) == valid_output_devices.end();
			const bool invalid_monitor_output_device 
				= std::find(valid_output_devices.begin(), valid_output_devices.end(), monitor_output_device_id) == valid_output_devices.end();
			
			if(invalid_audience_output_device){
				std::cerr << "Invalid device ID for audience output device.\n";
				continue;
			}
			if(invalid_monitor_output_device){
				std::cerr << "Invalid device ID for monitor output device.\n";
				continue;
			}
			std::cout << "Device IDs verified." << std::endl;
			return std::make_pair(audience_output_device_id, monitor_output_device_id);
		}
		catch(const std::invalid_argument& stoi_fmt_err){
			std::cerr << "Device ID is not a number.\n";
		}
		catch(const std::out_of_range& stoi_out_of_range){
			std::cerr << "Device ID out of range.\n";
		}
	}
	std::cin.ignore();
	return std::make_pair(-255, -255);
}

int main(){
	#ifdef NTRB_MEMDEBUG
	ntrb_memdebug_init_with_return_value();
	#endif
	
	bool use_serial = true;
	serial::Serial arduino_serial;
	constexpr bool user_is_choosing_serial = true;
	
	while(user_is_choosing_serial){
		const std::vector<serial::PortInfo> serial_ports = serial::list_ports();
		std::cout << "Port name: " << "null" << "\n\tdesc: " << "keyboard only" << '\n';
		
		for(const auto& port : serial_ports)
			std::cout << "Port name: " << port.port << "\n\tdesc: " << port.description << "\n\tID: " << port.hardware_id << '\n';
		std::cout << std::flush;
		
		std::string selected_port_name;
		std::cout << "Arduino port name | (q)uit |(r)efresh: " << std::flush;
		std::getline(std::cin, selected_port_name);
		
		if(selected_port_name == "q"){
			#ifdef NTRB_MEMDEBUG
			ntrb_memdebug_uninit(true);
			#endif
			return 0;
		}else if(selected_port_name == "r"){
			continue;
		}else if(selected_port_name == "null"){
			use_serial = false;
			break;
		}

		try{
			arduino_serial.setPort(selected_port_name);
			arduino_serial.open();
			
			if(!arduino_serial.isOpen())
				std::cerr << "Port is not opened.\n\n.";
			else
				break;
		}catch(const std::exception& e){
			std::cerr << "Exception caught while trying to open a serial port."
						<< "\n\tstd::exception::what(): " << e.what() << "\n\n";
		}
	}
	
	if(use_serial){
		std::cout << "Messages from the Arduino will be displayed to indicate the Arduino is ready.\n";
		std::cout << "First time serial connection may result in garbage being in the message.\n\n" << std::flush;
	}else{
		std::cout << "Using keyboard only mode." << std::endl;
	}
	
	const PaError portaudio_init_error = Pa_Initialize();
	if(portaudio_init_error != paNoError){
		std::cerr << "main(): failed to initialise PortAudio: PaError: " << portaudio_init_error << '\n';;
		return 0;
	}

	GlobalStates global_states;
	global_states.audio_tracks.emplace_back(std::make_unique<AudioTrack>(global_states.get_frames_per_callback(), 0));
	global_states.audio_tracks.emplace_back(std::make_unique<AudioTrack>(global_states.get_frames_per_callback(), 1));
	
	const auto [audience_output_device_id, monitor_output_device_id] = user_select_output_devices();
	OutputDevicesInterface devices_interface(audience_output_device_id, monitor_output_device_id, global_states);
	
	WINDOW* main_window = initscr();
	if(not main_window){
		std::cerr << "main(): initscr() failed.\n\tExitting...\n";
		return 0;
	}
	box(main_window, 0, 0);

	if(cbreak() == ERR or noecho() == ERR)
		ui::print_to_infobar("main: cbreak() or noecho() error.", UIColorPair_Error);
	timeout(0);
	
	ui::has_colors = has_colors();
	if(ui::has_colors){
		if(start_color() != ERR){
			init_pair((int)UIColorPair_Error, COLOR_RED, COLOR_WHITE);
			init_pair((int)UIColorPair_Warning, COLOR_YELLOW, COLOR_WHITE);
			init_pair((int)UIColorPair_Info, COLOR_BLUE, COLOR_WHITE);
		}else{
			ui::has_colors = false;
			ui::print_to_infobar("main: start_color() failure. No colors D:", UIColorPair_Default);
		}
	}
	else ui::print_to_infobar("No color support D:", UIColorPair_Default);
	
	///\todo: add minimum window size check
	const std::uint16_t main_window_width = ui::get_window_width(main_window);
	ui::draw_center_text(main_window, "Ardcont", 0);
	refresh();

	const std::uint16_t deck_info_window_width = (main_window_width/2) - 1;
	ui::left_deck_info_window = newwin(ui::deck_info_height, deck_info_window_width, 1, 1);
	ui::right_deck_info_window = newwin(ui::deck_info_height, deck_info_window_width, 1, 1 + deck_info_window_width);
	ui::keyboard_input_window = newwin(ui::input_window_height, main_window_width-2, 1 + ui::deck_info_height, 1);
	ui::stdout_window = newwin(ui::stdout_window_height, main_window_width-2, 1 + ui::deck_info_height + ui::input_window_height, 1);
	
	if(use_serial){
		std::thread serial_thread(serial_listener, std::ref(arduino_serial), std::ref(global_states));
		serial_thread.join();
	}
	
	//global_states.audio_tracks[0]->set_file_to_load_from("../mixaud/bolo.flac", global_states.get_frames_per_callback());
	
	std::thread output_devices_interface_thread(devices_interface.run, &devices_interface);
	std::thread ui_renderer_thread(ui::render_ui, std::ref(global_states));
	
	output_devices_interface_thread.join();
	ui_renderer_thread.join();
	
	global_states.audio_tracks[0].reset(nullptr);
	global_states.audio_tracks[1].reset(nullptr);
	
	const PaError portaudio_terminate_error = Pa_Terminate();
	if(portaudio_terminate_error != paNoError){
		std::cerr << "main(): failed to terminate PortAudio instance: PaError: " << portaudio_terminate_error << '\n';;
		return 0;
	}
	
	#ifdef NTRB_MEMDEBUG
	ntrb_memdebug_uninit(true);
	#endif
	
	delwin(ui::left_deck_info_window);
	delwin(ui::right_deck_info_window);	
	delwin(ui::keyboard_input_window);
	delwin(ui::stdout_window);
	endwin();
	
	return 0;
}

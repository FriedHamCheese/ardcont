#include "SerialInterface.hpp"
#include "KeyboardInterface.hpp"

#include "AudioTrack.hpp"
#include "GlobalStates.hpp"
#include "audeng_wrapper.hpp"

#include "ntrb/alloc.h"

#include "serial/serial.h"

#include <vector>
#include <string>
#include <thread>
#include <iostream>

int main(){
	#ifdef NTRB_MEMDEBUG
	ntrb_memdebug_init_with_return_value();
	#endif
	
	bool use_serial = true;
	serial::Serial arduino_serial;
	constexpr bool user_is_choosing_serial = true;
	
	while(user_is_choosing_serial){
		const std::vector<serial::PortInfo> serial_ports = serial::list_ports();
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

	GlobalStates global_states;
	global_states.audio_tracks.emplace_back(std::make_unique<AudioTrack>(global_states.get_frames_per_callback(), 0));
	
	if(use_serial){
		std::thread serial_thread(serial_listener, std::ref(arduino_serial), std::ref(global_states));
		serial_thread.join();
	}
	std::thread keyboard_thread(keyboard_listener, std::ref(global_states));
	std::thread audeng_thread(run_audio_engine, std::ref(global_states));

	keyboard_thread.join();
	audeng_thread.join();
	
	global_states.audio_tracks[0].reset(nullptr);
	global_states.audio_tracks[1].reset(nullptr);
	
	#ifdef NTRB_MEMDEBUG
	ntrb_memdebug_uninit(true);
	#endif
	
	return 0;
}

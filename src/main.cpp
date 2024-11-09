#include "SerialInterface.hpp"
#include "KeyboardInterface.hpp"

#include "AudioTrack.hpp"
#include "GlobalStates.hpp"
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

int main(){
	#ifdef NTRB_MEMDEBUG
	ntrb_memdebug_init_with_return_value();
	#endif
	
	KeyboardCin keyboard_cin;
	
	ArdcontSerial arduino_serial;
	constexpr bool user_is_choosing_serial = true;
	
	while(user_is_choosing_serial){
		const std::vector<serial::PortInfo> serial_ports = serial::list_ports();
		for(const auto& port : serial_ports)
			std::cout << "Port name: " << port.port << "\n\tdesc: " << port.description << "\n\tID: " << port.hardware_id << '\n';
		std::cout << std::flush;
		
		std::string selected_port_name;
		std::cout << "Arduino port name: " << std::flush;
		keyboard_cin.getline(selected_port_name);
		
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
	
	std::cout << "Messages from the Arduino will be displayed to indicate the Arduino is ready.\n";
	std::cout << "First time serial connection may result in garbage being in the message.\n\n" << std::flush;

	GlobalStates global_states;

	global_states.audio_tracks.push_back(std::make_unique<AudioTrackImpl>(global_states.get_frames_per_callback(), 0));
	global_states.audio_tracks.push_back(std::make_unique<AudioTrackImpl>(global_states.get_frames_per_callback(), 1));
	
	std::thread serial_thread(serial_listener, std::ref(arduino_serial), std::ref(global_states));
	std::thread keyboard_thread(keyboard_listener, std::ref(keyboard_cin), std::ref(global_states), nullptr);
	std::thread audeng_thread(run_audio_engine, std::ref(global_states));

	serial_thread.join();
	keyboard_thread.join();
	audeng_thread.join();
	
	global_states.audio_tracks[0].reset(nullptr);
	global_states.audio_tracks[1].reset(nullptr);
	
	#ifdef NTRB_MEMDEBUG
	ntrb_memdebug_uninit(true);
	#endif
	
	return 0;
}

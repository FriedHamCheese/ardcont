#ifndef OutputDevicesInterface_hpp
#define OutputDevicesInterface_hpp

#include "GlobalStates.hpp"
#include "portaudio.h"

#include <thread>

class OutputDevicesInterface{
	public:
	OutputDevicesInterface(
		const PaDeviceIndex audience_output_device_index, 
		const PaDeviceIndex monitor_output_device_index,
		GlobalStates& global_states
	);
	void run() noexcept;
	
	private:
	std::thread monitor_output_device_thread;
	std::thread audience_output_device_thread;
	GlobalStates& global_states;
};

#endif
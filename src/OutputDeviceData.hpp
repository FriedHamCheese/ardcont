#ifndef OutputDeviceData_hpp
#define OutputDeviceData_hpp

#include "GlobalStates.hpp"
#include "portaudio.h"

struct OutputDeviceData{
	OutputDeviceData(GlobalStates& global_states, const PaDeviceIndex device_index, const PaTime output_latency, bool is_monitor_device)
	: 	global_states(global_states), 
		device_index(device_index), 
		is_monitor_device(is_monitor_device)
	{
		this->stream_parameters.device = device_index;
		this->stream_parameters.suggestedLatency = output_latency;
		this->stream_parameters.sampleFormat = ntrb_std_sample_fmt;
		this->stream_parameters.channelCount = ntrb_std_audchannels;
		this->stream_parameters.hostApiSpecificStreamInfo = NULL;
	}

	GlobalStates& global_states;
	PaDeviceIndex device_index;
	PaStreamParameters stream_parameters;
	bool is_monitor_device;
};

#endif
#include "output_device.hpp"
#include "OutputDeviceData.hpp"
#include "ui.hpp"

#include "ntrb/audeng_wrapper.h"
#include "ntrb/aud_std_fmt.h"

#include "portaudio.h"

#include <vector>
#include <thread>
#include <cstring>
#include <chrono>
#include <iostream>

static int stream_audio(const void *, void *output_void, unsigned long frameCount, 
						const PaStreamCallbackTimeInfo*, PaStreamCallbackFlags, 
						void* OutputDeviceData_ptr) noexcept
{
	OutputDeviceData* const device_data = (OutputDeviceData*)OutputDeviceData_ptr;
	try{
		float* mixed_output = (float*)output_void;
		const unsigned long stdaud_sample_count = frameCount * ntrb_std_audchannels;
		std::memset(mixed_output, 0, stdaud_sample_count * sizeof(float));

		GlobalStates& global_states = device_data->global_states;
		
		std::atomic_uint8_t* status;
		if(device_data->is_monitor_device)
			status = &(global_states.monitor_output_device_status);
		else status = &(global_states.audience_output_device_status);
		
		if(global_states.requested_exit.load()) return paComplete;
		
		while(true){
			const std::uint8_t loaded_status = status->load();
			const bool can_read_audiotracks 
				= loaded_status == AudioTrackAccess_ReadyForReading 
				or loaded_status == AudioTrackAccess_BeingRead;
				
			if(can_read_audiotracks or global_states.requested_exit.load()) break;
			std::this_thread::sleep_for(std::chrono::microseconds(500));
		}

		if(status->load() == AudioTrackAccess_ReadyForReading)
			status->store(AudioTrackAccess_BeingRead);
		
		for(const std::unique_ptr<AudioTrack>& track : global_states.audio_tracks){
			if(device_data->is_monitor_device and (not track->output_to_monitor.load()))
				continue;
			const std::vector<float>& track_samples = track->get_samples();
			for(size_t i = 0; i < stdaud_sample_count; i++)
				mixed_output[i] += track_samples[i];
		}
		
		status->store(AudioTrackAccess_FinishedReading);
		return paContinue;
	}
	catch(const std::exception& excp){
		ui::print_to_infobar(std::string("output device callback: ") + excp.what(), UIColorPair_Error);
	}
	catch(...){
		ui::print_to_infobar(std::string("output device callback: uncaught throw"), UIColorPair_Error);
	}
	return paContinue;
}

void run_output_device(OutputDeviceData device_data) noexcept{
	PaError pa_error = paNoError;
	try{
		//output_stream Alloc
		PaStream* output_stream;
		pa_error = Pa_OpenStream(&output_stream, NULL, &(device_data.stream_parameters), 
									ntrb_std_samplerate, device_data.global_states.get_frames_per_callback(), paNoFlag,
									stream_audio, &device_data);
		if(pa_error) goto print_err;
		
		pa_error = Pa_StartStream(output_stream);
		if(pa_error) goto close_stream;

		while(device_data.global_states.requested_exit.load() == false){
			std::this_thread::sleep_for(std::chrono::milliseconds(device_data.global_states.msecs_per_callback) / 10);
		}
		Pa_StopStream(output_stream);
		
		close_stream:
		Pa_CloseStream(output_stream);
		
		print_err:
		if(pa_error != paNoError)
			ui::print_to_infobar(std::string("output device thread: PaError ") + std::to_string(pa_error), UIColorPair_Error);
	}
	catch(const std::exception& excp){
		ui::print_to_infobar(std::string("output device thread: ") + excp.what(), UIColorPair_Error);
	}
	catch(...){
		ui::print_to_infobar(std::string("output device thread: uncaught throw"), UIColorPair_Error);
	}
}
#include "audeng_wrapper.hpp"
#include "OutputDeviceData.hpp"

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
	try{
		float* mixed_output = (float*)output_void;
		const unsigned long stdaud_sample_count = frameCount * ntrb_std_audchannels;
		std::memset(mixed_output, 0, stdaud_sample_count * sizeof(float));

		OutputDeviceData* const device_data = (OutputDeviceData*)OutputDeviceData_ptr;
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
			const std::vector<float>& track_samples = track->get_samples();
			for(size_t i = 0; i < stdaud_sample_count; i++)
				mixed_output[i] += track_samples[i];
		}
		
		status->store(AudioTrackAccess_FinishedReading);
		return paContinue;
	}
	catch(const std::exception& excp){
		std::cerr << "audeng_wrapper: stream_audio(): Uncaught exception: " << excp.what() << '\n';
		std::cerr << "Audio chunk missed.\n";
		std::cout << ": " << std::endl;
	}
	catch(...){
		std::cerr << "audeng_wrapper: stream_audio(): Uncaught throw." << std::endl;
		std::cerr << "Audio chunk missed.\n";
		std::cout << ": " << std::endl;
	}
	return paContinue;
}

void run_audio_engine(OutputDeviceData device_data) noexcept{
	PaError pa_error = paNoError;
	PaError pa_uninit_error = paNoError;
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
			Pa_Sleep(GlobalStates::msecs_per_callback);
		}

		Pa_StopStream(output_stream);
		//it's a lot more messier to error handle the cleanup procedures. So we only check at Pa_Terminate().
		
		close_stream:
		Pa_CloseStream(output_stream);
		
		print_err:
		if(pa_error != paNoError){
			fprintf(stderr, "PaError %i caught.\n", pa_error);
			fprintf(stderr,  "(%s)\n", Pa_GetErrorText(pa_error));
		}
		if(pa_uninit_error != paNoError){
			fprintf(stderr, "Error %i caught while uninitializing audio system.\n", pa_uninit_error);
			fprintf(stderr,  "(%s)\n", Pa_GetErrorText(pa_uninit_error));
		}
	}
	catch(const std::exception& excp){
		std::cerr << "audeng_wrapper: run_audio_engine(): Uncaught exception: " << excp.what() << '\n';
		std::cerr << "Output device stopped.\n";
		std::cout << ": " << std::endl;
	}
	catch(...){
		std::cerr << "audeng_wrapper: run_audio_engine(): Uncaught throw." << std::endl;
		std::cerr << "Output device stopped.\n";
		std::cout << ": " << std::endl;
	}
}
#include "audeng_state.hpp"

#include "ntrb/audeng_wrapper.h"
#include "ntrb/aud_std_fmt.h"

#include "portaudio.h"

#include <vector>
#include <thread>
#include <cstring>
#include <iostream>

static int stream_audio(const void *, void *output_void, unsigned long frameCount, 
						const PaStreamCallbackTimeInfo*, PaStreamCallbackFlags, 
						void *)
{
	float* mixed_output = (float*)output_void;
	const unsigned long stdaud_sample_count = frameCount * ntrb_std_audchannels;
	std::memset(mixed_output, 0, stdaud_sample_count * sizeof(float));
	
	if(audeng_state::requested_exit.load()) return paComplete;
	
	for(AudioTrack& track : audeng_state::audio_tracks){
		const bool got_mutex = track.get_sample_access_mutex().try_lock();
		if(!got_mutex){
			std::cerr << "\n[Warn]: audeng_wrapper: stream_audio(): Unable to acquire the sample access mutex of track " << (std::uint16_t)track.get_track_id()  << ".\n";
			std::cout << ": " << std::flush;
			continue;
		}

		const std::vector<float>& track_samples = track.get_samples();
		for(size_t i = 0; i < stdaud_sample_count; i++)
			mixed_output[i] += track_samples[i];
		
		track.get_sample_access_mutex().unlock();
		
		std::thread track_load_thread(AudioTrack::load_track, &track);
		track_load_thread.detach();
	}
	
	return paContinue;
}

void run_audio_engine(){
	PaError pa_error = paNoError;
	PaError pa_uninit_error = paNoError;
	
	//Pa_Initialize Alloc
	pa_error = Pa_Initialize();
	if(pa_error) goto print_err;
		
	PaStreamParameters output_stream_params;
	pa_error = ntrb_get_output_stream_params(&output_stream_params);
	if(pa_error) goto uninit_pa;
	
	//output_stream Alloc
	PaStream* output_stream;
	pa_error = Pa_OpenStream(&output_stream, NULL, &output_stream_params, 
								ntrb_std_samplerate, audeng_state::frames_per_callback, paNoFlag,
								stream_audio, NULL);
	if(pa_error) goto uninit_pa;
	
	pa_error = Pa_StartStream(output_stream);
	if(pa_error) goto close_stream;

	while(audeng_state::requested_exit.load() == false){
		Pa_Sleep(audeng_state::msecs_per_callback);
	}

	Pa_StopStream(output_stream);
	//it's a lot more messier to error handle the cleanup procedures. So we only check at Pa_Terminate().
	
	close_stream:
	Pa_CloseStream(output_stream);
		
	uninit_pa:
	pa_uninit_error = Pa_Terminate();
	
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
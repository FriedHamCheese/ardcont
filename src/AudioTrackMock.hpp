#ifndef AUDIOTRACK_MOCK_HPP
#define AUDIOTRACK_MOCK_HPP

#include "ntrb/AudioBuffer.h"
#include "ntrb/aud_std_fmt.h"

#include <mutex>
#include <atomic>
#include <vector>
#include <optional>
#include <iostream>

/*
class AudioTrackMock : public AudioTrack{
	public:
	AudioTrackMock(const std::uint32_t minimum_frames_in_buffer, const uint8_t track_id) 
	: AudioTrack(minimum_frames_in_buffer, track_id){
		
	}
	
	void load_track(){
		this->samples.clear();
		this->samples.insert(this->samples.end(), this->minimum_frames_in_buffer * ntrb_std_audchannels, 0.0);		
	}
	ntrb_AudioBufferNew_Error set_file_to_load_from(const char* const filename, const std::uint32_t frames_per_callback){
		return ntrb_AudioBufferNew_OK;
	}
	bool toggle_play_pause(){
		return true;
	}
	const std::vector<float>& get_samples() const{
		return this->samples;
	}
	
	void fine_step_backward(){
		
	}
	void fine_step_forward(){
		
	}
	void set_destination_speed_multiplier(const float dest_speed_multiplier){
		
	}
	
	bool set_loop(){
		
	}
	void set_beats_per_loop(const float beats_per_loop){
		
	}
	void cancel_loop(){
		
	}
	
	bool cue_to_nearest_cue_point(){
		return true;
	}

	private:
	bool load_single_frame(){
		return true;
	}
	bool fill_sample_buffer_while_in_loop(const std::uint32_t minimum_samples_in_sample_buffer){
		return true;
	}
	bool fill_sample_buffer(const std::uint32_t minimum_samples_in_sample_buffer){
		return true;
	}
	
	std::optional<std::uint32_t> find_nearest_loop_cue_point(){
		return 1;
	}
	std::optional<std::uint32_t> find_eariler_cue_point(){
		return 1;
	}
	void adjust_speed_multiplier(){
		
	}
};
*/

#endif
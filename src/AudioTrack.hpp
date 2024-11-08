#ifndef AudioTrack_hpp
#define AudioTrack_hpp

#include "ntrb/AudioBuffer.h"
#include "ntrb/aud_std_fmt.h"

#include <mutex>
#include <atomic>
#include <vector>
#include <optional>
#include <iostream>

class AudioTrack{
	public:
	AudioTrack(const std::uint32_t minimum_frames_in_buffer, const uint8_t track_id);
	~AudioTrack();
	
	void load_track();
	ntrb_AudioBufferNew_Error set_file_to_load_from(const char* const filename);
	bool toggle_play_pause();
	std::mutex& get_sample_access_mutex();
	const std::vector<float>& get_samples() const;
	
	void fine_step_backward();
	void fine_step_forward();
	void set_destination_speed_multiplier(const float dest_speed_multiplier);
	
	bool set_loop();
	void set_beats_per_loop(const float beats_per_loop);
	void cancel_loop();
	
	bool cue_to_nearest_cue_point();
	
	std::uint8_t get_track_id() const;
	float get_bpm() const;
	
	private:
	bool load_single_frame();
	bool fill_sample_buffer_while_in_loop(const std::uint32_t minimum_samples_in_sample_buffer);
	bool fill_sample_buffer(const std::uint32_t minimum_samples_in_sample_buffer);
	
	std::optional<std::uint32_t> find_nearest_loop_cue_point();
	std::optional<std::uint32_t> find_eariler_cue_point();
	void adjust_speed_multiplier();
	
		
	std::mutex sample_access_mutex;
	std::vector<float> samples;
	std::uint32_t minimum_frames_in_buffer;
	
	std::atomic<bool> in_pause_state = true;
		
	std::atomic<double> regular_speed_stdaud_frames_pos = 0;
	
	ntrb_AudioBuffer stdaud_from_file;
	bool initialised_stdaud_from_file = false;
	
	std::atomic<double> speed_multiplier = 1.0;
	std::atomic<double> destination_speed_multiplier = 1.0;

	static constexpr float speed_multiplier_recovering_seconds = 0.5;
	static constexpr float speed_multiplier_recovering_frames = speed_multiplier_recovering_seconds * 48000.0;
	
	std::atomic<float> speed_multiplier_recovering_per_frame = 1.0;
	static constexpr float fine_step_speed_multiplier_delta = 0.05;

	//Looping
	std::atomic<std::uint32_t> loop_frame_begin = 0;
	std::atomic<std::uint32_t> loop_frame_end = 0;
	std::atomic<float> beats_per_loop = 4;
	std::atomic<bool> loop_queued = false;
	std::atomic<std::uint32_t> first_beat_stdaud_frame = 0;

	//Track data
	std::string audfile_name = "Track not loaded.";
	std::atomic<float> bpm = 0.0;		
	std::uint8_t track_id;
};

#endif
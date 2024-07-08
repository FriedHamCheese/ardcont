#ifndef AudioTrack_hpp
#define AudioTrack_hpp

#include "ntrb/AudioBuffer.h"
#include "ntrb/aud_std_fmt.h"

#include <mutex>
#include <atomic>
#include <vector>
#include <optional>
#include <iostream>

inline std::atomic<float> AudioTracks_loop_cue_step = 1.0;

class AudioTrack{
	public:
	AudioTrack(const std::uint32_t minimum_frames_in_buffer, const uint8_t track_id);
	~AudioTrack();
	
	void load_track();
	ntrb_AudioBufferNew_Error set_file_to_load_from(const char* const filename);
	
	bool toggle_play_pause();
	
	void volume(const float volume_knob_ratio);
	
	std::mutex& get_sample_access_mutex();
	const std::vector<float>& get_samples() const;
	
	bool set_loop();
	void set_beats_per_loop(const float beats_per_loop);
	void cancel_loop();
	
	std::uint8_t get_track_id() const;

	std::atomic<double> speed_multiplier = 1.0;
	
	float get_bpm() const;

	private:
	bool load_single_frame();
	
	std::optional<std::uint32_t> find_nearest_loop_cue_point();
	
	std::mutex sample_access_mutex;
	std::vector<float> samples;
	std::uint32_t minimum_frames_in_buffer;
	
	ntrb_AudioBuffer stdaud_from_file;
	bool initialised_stdaud_from_file = false;
	
	std::atomic<float> amplitude_multiplier = 1.0;
	
	std::atomic<bool> in_pause_state = true;
	
	std::atomic<bool> loop_queued = false;
	std::atomic<std::uint32_t> loop_frame_begin = 0;
	std::atomic<std::uint32_t> loop_frame_end = 0;
	std::atomic<float> beats_per_loop = 4;
		
	std::atomic<double> regular_speed_stdaud_frames_pos = 0;

	std::string audfile_name = "Track not loaded.";
	std::atomic<float> bpm = 0.0;
	std::atomic<std::uint32_t> first_beat_stdaud_frame = 0;
	
	std::uint8_t track_id;
};

#endif
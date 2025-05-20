/**
\file AudioTrack.hpp
A file containing the AudioTrack interface and its implementation AudioTrackImpl.
*/

#ifndef AudioTrack_hpp
#define AudioTrack_hpp

#include "EffectContainer.hpp"

#include "ntrb/AudioBuffer.h"
#include "ntrb/aud_std_fmt.h"

#include <mutex>
#include <atomic>
#include <vector>
#include <optional>
#include <iostream>

///How the AudioTrack should manipulate its playback duration and speed.
///This does not include looping.
enum AudioTrack_PlayMode{
	///No audio loading or playback, 0 filled AudioTrack::samples.
	AudioTrack_no_playback,
	///Keep playing until AudioTrack reaches EOF.
	AudioTrack_regular_play,
	///Keep playing until AudioTrack reaches EOF or the cue button is released from being held.
	AudioTrack_cue_play,
	///Play for only one beat.
	AudioTrack_beat_preview,
	///Keep playing while slowing down the playback speed to a halt.
	AudioTrack_slowdown_to_halt
};

/**
A representation of a turntable deck.
\todo
- float/double inconsistency
*/
class AudioTrack{
	public:
	/**
	Initialises AudioTrack::sample_access_mutex, AudioTrack::samples; 
	sets AudioTrack::minimum_frames_in_buffer and AudioTrack::track_id.
	
	\param[in] minimum_frames_in_buffer The exact amount of stdaud frames which the audio engine reads per callback.
	\param[in] track_id track id which the interface represents.
	*/
	AudioTrack(const std::uint32_t minimum_frames_in_buffer, const uint8_t track_id);
	
	///Frees AudioTrack::stdaud_from_file if AudioTrack::initialised_stdaud_from_file is true.
	~AudioTrack();
	
	/**
	Loads the final audio of the deck to be played in an audio engine callback to AudioTrack::samples.
	
	This function is used as a detached thread, any errors are reported through standard streams.
	
	Errors and information are reported through standard streams such as:
	- the underlying pure stdaud read from ntrb_AudioBuffer failing to read its audio file, reported through std::cerr
	- AudioTrack::stdaud_from_file reaching EOF, reported through std::cout
	*/
	void load_samples() noexcept;
	/**	
	Sets the file which the deck will play (frees the previous audio file if needed).
	Error from initialising an ntrb_AudioBuffer for the file is returned.
	
	///\todo cant have incorrect audio loaded
	*/
	ntrb_AudioBufferNew_Error set_file_to_load_from(const char* const filename, const std::uint32_t frames_per_callback) noexcept;
	/**
	Loads audio info file from aud_filename, usually by reading from a file which has the extension of aud_filename replaced with .txt.
	
	Errors are displayed through std::cerr.
	*/
	void load_audio_info(const std::string& aud_filename);
	///Displays the details of the deck through std::cout.
	void display_deck_info();
	
	/**
	If play, pauses; if pause, plays :D
	
	Returns false if errors occurred while toggling.
	*/
	bool toggle_play_pause() noexcept;
	
	AudioTrack_PlayMode get_play_mode() const noexcept;
	
	/**
	Sets AudioTrack::current_stdaud_frame to the nearest earlier cue point 
	and set AudioTrack::play_mode to AudioTrack_cue_play.
	
	This function should be called when the deck is in AudioTrack_no_playback.
	
	Returns false if could not find the nearest cue point due to rwlock error.
	*/
	bool initiate_cue_play() noexcept;
	
	/**
	Returns AudioTrack::current_stdaud_frame back to the starting beat of AudioTrack::initiate_cue_play()
	and brings AudioTrack::play_mode to AudioTrack_no_playback.
	
	This should be called when AudioTrack::play_mode is AudioTrack_cue_play.
	*/	
	void stop_cue_play() noexcept;
	
	/**
	Sets AudioTrack::current_stdaud_frame to the beat after it and sets AudioTrack::play_mode to AudioTrack_beat_preview.
	
	Returns false if could not find the next beat due to rwlock error.
	*/
	bool play_only_next_beat() noexcept;
	
	/**
	Sets AudioTrack::current_stdaud_frame to the beat before it and sets AudioTrack::play_mode to AudioTrack_beat_preview.
	
	Returns false if could not find the previous beat due to rwlock error.
	*/
	bool play_only_prev_beat() noexcept;

	///Get a const reference to AudioTrack::samples.
	const std::vector<float>& get_samples() const noexcept{
		return this->samples;
	}
	
	///Jogs the deck to play at a slightly slower speed temporarily.
	void fine_step_backward() noexcept;
	///Jogs the deck to play at a slightly faster speed temporarily.
	void fine_step_forward() noexcept;
	///Set the destination playback speed.
	void set_destination_speed_multiplier(const float dest_speed_multiplier) noexcept;
	
	/**
	Increases AudioTrack::beats_per_loop by 2x and adjust AudioTrack::loop_frame_end according to it.
	Returns AudioTrack::beats_per_loop.
	*/
	float increment_loop_step() noexcept;
	/**
	Halves AudioTrack::beats_per_loop by 2 and adjust AudioTrack::loop_frame_end according to it.
	Returns AudioTrack::beats_per_loop.
	*/
	float decrement_loop_step() noexcept;
	
	/**
	Set the nearest cue point as the loop begin point and enters the loop with AudioTrack::beats_per_loop as the loop length. 
	
	This implies setting AudioTrack::loop_frame_begin, AudioTrack::loop_frame_end, 
	with the latter being AudioTrack::beats_per_loop after the former.
	
	Returns false if could not find nearest cue point to start from or bpm data is unavailable.
	*/
	bool set_loop();
	void cancel_loop();
	
	/**
	Moves the next loading frame to the nearest earlier cue point. Returns false if couldn't find a nearby cue point.
	
	Makes more sense to move to earlier cue point because the user will press cue after hearing the beat of the cue.
	*/
	bool cue_to_nearest_cue_point() noexcept;
	
	std::uint8_t get_track_id() const noexcept{
		return this->track_id;		
	}
	float get_bpm() const noexcept{
		return this->bpm.load();		
	}
	double get_current_stdaud_frame() const noexcept{
		return this->current_stdaud_frame.load();
	}
	std::uint32_t get_loop_frame_begin() const noexcept{
		return this->loop_frame_begin.load();
	}
	std::uint32_t get_loop_frame_end() const noexcept{
		return this->loop_frame_end.load();
	}
	float get_beats_per_loop() const noexcept{
		return this->beats_per_loop.load();
	}
	bool is_loop_queued() const noexcept{
		return this->loop_queued.load();
	}
	const std::string& get_filename() const noexcept{
		return this->audfile_name;
	}
	
	enum EffectType effect_type;
	
	EffectContainer& get_effect_container() noexcept{
		return this->effect_container;
	}

	///Mutex for accessing AudioTrack::samples.
	std::mutex sample_access_mutex;
	std::atomic_bool output_to_monitor = false;
	std::atomic<double> destination_speed_multiplier = 1.0;
	
	private:
	/**
	Append a single frame (both left and right samples) to AudioTrack::samples while taking playback speed into account,
	increment AudioTrack::current_stdaud_frame by AudioTrack::speed_multiplier 
	and call AudioTrack::adjust_speed_multiplier.
	
	If the frame to load is not within the boundary of the underlying AudioTrack::ntrb_AudioBuffer, the function returns false, notifying to the caller to load the ntrb_AudioBuffer and call this function again to append to AudioTrack::samples.
	
	This function assumes the caller has acquired at least a read lock of AudioTrack::stdaud_from_file.
	*/
	bool load_single_frame();
	/**
	Fills AudioTrack::samples for looping, 
	and guaranteeing no garbage is in AudioTrack::samples by 0 filling AudioTrack::samples
	if AudioTrack::in_pause_state is true or the underlying AudioTrack::stdaud_from_file encounters any errors.
	
	The function keeps AudioTrack::current_stdaud_frame to be within 
	AudioTrack::loop_frame_begin and AudioTrack::loop_frame_end at all times to create an audio loop.
	
	\return false for any errors from loading AudioTrack::stdaud_from_file but not for ntrb_AudioBufferLoad_EOF.
	\return true if no errors occurred or AudioTrack::stdaud_from_file returns ntrb_AudioBufferLoad_EOF.
	*/
	bool fill_sample_buffer_while_in_loop(const std::uint32_t minimum_samples_in_sample_buffer);
	
	/**
	Fills AudioTrack::samples for regular playback, 
	and guaranteeing no garbage is in AudioTrack::samples by 0 filling AudioTrack::samples
	if AudioTrack::in_pause_state is true or the underlying AudioTrack::stdaud_from_file encounters any errors.
	
	\return false for any errors from loading AudioTrack::stdaud_from_file but not for ntrb_AudioBufferLoad_EOF.
	\return true if no errors occurred or AudioTrack::stdaud_from_file returns ntrb_AudioBufferLoad_EOF.	
	*/
	bool fill_sample_buffer(const std::uint32_t minimum_samples_in_sample_buffer);
	
	/**
	Return a stdaud frame number representing the nearest beat (or cue point) to AudioTrack::current_stdaud_frame. 
	Returns a std::nullopt if couldn't acquire a rdlock of AudioTrack::stdaud_from_file.
	*/
	std::optional<std::uint32_t> find_nearest_loop_cue_point() noexcept;
	
	/**
	Return a stdaud frame number representing the beat (or cue point) prior to AudioTrack::current_stdaud_frame.
	Returns a std::nullopt if couldn't acquire a rdlock of AudioTrack::stdaud_from_file.
	*/
	std::optional<std::uint32_t> find_eariler_cue_point() noexcept;
	
	///Adjust AudioTrack::speed_multiplier to approach AudioTrack::destination_speed_multiplier in between frames.
	void adjust_speed_multiplier() noexcept;
	
	///A vector containing the final stdaud frames of the deck for an audio engine callback.
	std::vector<float> samples;
	///The number of frames to be in AudioTrack::samples which an AudioTrack must provide for the audio engine callback.
	const std::uint32_t minimum_frames_in_buffer;
	
	std::atomic<AudioTrack_PlayMode> play_mode = AudioTrack_no_playback;
	
	/**
	 * The stdaud frame which an AudioTrack is at. 
	 * This has to be incremented by AudioTrack::speed_multiplier.
	 * This can be used to set the start of AudioTrack::stdaud_from_file.
	 * */
	std::atomic<double> current_stdaud_frame;
	/**
	 * The underlying object which contains a buffer of stdaud frames of an audio file to play from.  
	 */
	ntrb_AudioBuffer stdaud_from_file;
	///Used to determine whether to ntrb_AudioBuffer_free AudioTrack::stdaud_from_file or not,
	///since the function does not allow for uninitialised objects to be freed.
	bool initialised_stdaud_from_file = false;
	
	/**
	 The ratio of speed at which the AudioTrack plays at.
	 
	 This can directly be used to increment AudioTrack::current_stdaud_frame,
	 as 1x means incrementing one frame after another, 0.5 is play the same frame twice 
	 and 2x means skip every other frame, etc.

	 This value differs from AudioTrack::destination_speed_multiplier which can only be changed by the tempo knob and changes instantly; while speed_multiplier needs time to catch up with the former to simulate turntable rotational speed acceleration/deceleration. It can be changed from the tempo knob or jogging, and should be changed for every frame applied to AudioTrack::samples to simulate smooth turntable rotational acceleration. 
	 */
	std::atomic<double> speed_multiplier = 1.0;
	///The playback speed ratio to theoretically play at.

	private:
	/**
	 * The amount of time for AudioTrack::speed_multiplier to reach AudioTrack::destination_speed_multiplier, regardless of the difference between the two.
	 */
	static constexpr float speed_multiplier_recovering_seconds = 0.5;
	///AudioTrack::speed_multiplier_recovering_seconds but as stdaud frame count actual calculations in AudioTrack::adjust_speed_multiplier.
	static constexpr float speed_multiplier_recovering_frames = speed_multiplier_recovering_seconds * 48000.0;
	///The speed multiplier change for a jog click.
	static constexpr float fine_step_speed_multiplier_delta = 0.025;
	
	///The frame which cue play started from, used for returning back after cue play has stopped.
	std::atomic<std::uint32_t> cue_play_begin_frame;
	///The frame to end playback if the deck is previewing a beat.
	std::atomic<std::uint32_t> end_beat_preview_at_frame;

	//Looping
	std::atomic<std::uint32_t> loop_frame_begin;
	std::atomic<std::uint32_t> loop_frame_end;
	std::atomic<float> beats_per_loop = 4;
	std::atomic<bool> loop_queued = false;
	///The stdaud frame at which the first beat is at.
	///This is used in calculating loops and cue points.
	std::atomic<std::uint32_t> first_beat_stdaud_frame;

	//Track data
	std::string audfile_name = "Track not loaded.";
	std::atomic<float> bpm = 0.0;		
	std::uint8_t track_id;

	bool fill_sample_buffer_while_in_beat_preview(const std::uint32_t minimum_samples_in_sample_buffer);
	
	static float get_seconds_per_beat(const float bpm) noexcept{
		return 60.0 / bpm;
	}
	static float get_frames_per_beat(const float bpm) noexcept{
		return (float)ntrb_std_samplerate * get_seconds_per_beat(bpm);
	}
	
	EffectContainer effect_container;
};

#endif

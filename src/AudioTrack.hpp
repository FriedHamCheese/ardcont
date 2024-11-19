/**
\file AudioTrack.hpp
A file containing the AudioTrack interface and its implementation AudioTrackImpl.
*/

#ifndef AudioTrack_hpp
#define AudioTrack_hpp

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
An interface for a turntable deck. Use AudioTrackImpl for actual usage in production code.
\todo 
- hard to test stuff should not be tested and be in implementation,
  easy to test can be in interface.
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
	
	///Loads the final audio of the deck to be played in an audio engine callback to AudioTrack::samples.
	virtual void load_samples() = 0;
	/**	
	Sets the file which the deck will play (frees the previous audio file if needed).
	Error from initialising an ntrb_AudioBuffer for the file is returned.
	*/
	virtual ntrb_AudioBufferNew_Error set_file_to_load_from(const char* const filename, const std::uint32_t frames_per_callback) = 0;
	/**
	Loads audio info file from aud_filename, usually by reading from a file which has the extension of aud_filename replaced with .txt.
	
	Errors are displayed through std::cerr.
	*/
	virtual void load_audio_info(const std::string& aud_filename) = 0;
	///Displays the details of the deck through std::cout.
	virtual void display_deck_info() = 0;
	
	/**
	If play, pauses; if pause, plays :D
	
	Returns false if errors occurred while toggling.
	*/
	virtual bool toggle_play_pause() = 0;
	
	virtual AudioTrack_PlayMode get_play_mode() const = 0;
	
	/**
	Sets AudioTrack::current_stdaud_frame to the nearest earlier cue point 
	and set AudioTrack::play_mode to AudioTrack_cue_play.
	
	This function should be called when the deck is in AudioTrack_no_playback.
	
	Returns false if could not find the nearest cue point due to rwlock error.
	*/
	virtual bool initiate_cue_play() = 0;
	
	/**
	Returns AudioTrack::current_stdaud_frame back to the starting beat of AudioTrack::initiate_cue_play()
	and brings AudioTrack::play_mode to AudioTrack_no_playback.
	
	This should be called when AudioTrack::play_mode is AudioTrack_cue_play.
	*/	
	virtual void stop_cue_play() = 0;
	
	/**
	Sets AudioTrack::current_stdaud_frame to the beat after it and sets AudioTrack::play_mode to AudioTrack_beat_preview.
	
	Returns false if could not find the next beat due to rwlock error.
	*/
	virtual bool play_only_next_beat() = 0;
	
	/**
	Sets AudioTrack::current_stdaud_frame to the beat before it and sets AudioTrack::play_mode to AudioTrack_beat_preview.
	
	Returns false if could not find the previous beat due to rwlock error.
	*/
	virtual bool play_only_prev_beat() = 0;
	
	///Get the mutex for accessing AudioTrack::samples.
	std::mutex& get_sample_access_mutex(){
		return this->sample_access_mutex;
	}
	///Get a const reference to AudioTrack::samples.
	const std::vector<float>& get_samples() const{
		return this->samples;
	}
	
	///Jogs the deck to play at a slightly slower speed temporarily.
	virtual void fine_step_backward() = 0;
	///Jogs the deck to play at a slightly faster speed temporarily.
	virtual void fine_step_forward() = 0;
	///Set the destination playback speed.
	virtual void set_destination_speed_multiplier(const float dest_speed_multiplier) = 0;
	
	/**
	Increases AudioTrack::beats_per_loop by 2x and adjust AudioTrack::loop_frame_end according to it.
	Returns AudioTrack::beats_per_loop.
	*/
	virtual float increment_loop_step() = 0;
	/**
	Halves AudioTrack::beats_per_loop by 2 and adjust AudioTrack::loop_frame_end according to it.
	Returns AudioTrack::beats_per_loop.
	*/
	virtual float decrement_loop_step() = 0;
	
	/**
	Set the nearest cue point as the loop begin point and enters the loop with AudioTrack::beats_per_loop as the loop length. 
	
	This implies setting AudioTrack::loop_frame_begin, AudioTrack::loop_frame_end, 
	with the latter being AudioTrack::beats_per_loop after the former.
	
	Returns false if could not.
	*/
	virtual bool set_loop() = 0;
	virtual void cancel_loop() = 0;
	
	/**
	Moves the next loading frame to the nearest earlier cue point. Returns false if couldn't find a nearby cue point.
	
	Makes more sense to move to earlier cue point because the user will press cue after hearing the beat of the cue.
	*/
	virtual bool cue_to_nearest_cue_point() = 0;
	
	std::uint8_t get_track_id() const{
		return this->track_id;		
	}
	float get_bpm() const{
		return this->bpm.load();		
	}
	
	protected:
	/**
	Append a single frame (both left and right samples) to AudioTrack::samples while taking playback speed into account,
	increment AudioTrack::current_stdaud_frame by AudioTrack::speed_multiplier 
	and call AudioTrack::adjust_speed_multiplier.
	
	If the frame to load is not within the boundary of the underlying AudioTrack::ntrb_AudioBuffer, the function returns false, notifying to the caller to load the ntrb_AudioBuffer and call this function again to append to AudioTrack::samples.
	*/
	virtual bool load_single_frame() = 0;
	/**
	Fills AudioTrack::samples for looping, 
	and guaranteeing no garbage is in AudioTrack::samples by 0 filling AudioTrack::samples
	if AudioTrack::in_pause_state is true or the underlying AudioTrack::stdaud_from_file encounters any errors.
	
	The function keeps AudioTrack::current_stdaud_frame to be within 
	AudioTrack::loop_frame_begin and AudioTrack::loop_frame_end at all times to create an audio loop.
	
	\return false for any errors from loading AudioTrack::stdaud_from_file but not for ntrb_AudioBufferLoad_EOF.
	\return true if no errors occurred or AudioTrack::stdaud_from_file returns ntrb_AudioBufferLoad_EOF.
	*/
	virtual bool fill_sample_buffer_while_in_loop(const std::uint32_t minimum_samples_in_sample_buffer) = 0;
	
	/**
	Fills AudioTrack::samples for regular playback, 
	and guaranteeing no garbage is in AudioTrack::samples by 0 filling AudioTrack::samples
	if AudioTrack::in_pause_state is true or the underlying AudioTrack::stdaud_from_file encounters any errors.
	
	\return false for any errors from loading AudioTrack::stdaud_from_file but not for ntrb_AudioBufferLoad_EOF.
	\return true if no errors occurred or AudioTrack::stdaud_from_file returns ntrb_AudioBufferLoad_EOF.	
	*/
	virtual bool fill_sample_buffer(const std::uint32_t minimum_samples_in_sample_buffer) = 0;
	
	/**
	Return a stdaud frame number representing the nearest beat (or cue point) to AudioTrack::current_stdaud_frame. 
	Returns a std::nullopt if couldn't acquire a rdlock of AudioTrack::stdaud_from_file.
	*/
	virtual std::optional<std::uint32_t> find_nearest_loop_cue_point() = 0;
	
	/**
	Return a stdaud frame number representing the beat (or cue point) prior to AudioTrack::current_stdaud_frame.
	Returns a std::nullopt if couldn't acquire a rdlock of AudioTrack::stdaud_from_file.
	*/
	virtual std::optional<std::uint32_t> find_eariler_cue_point() = 0;
	
	///Adjust AudioTrack::speed_multiplier to approach AudioTrack::destination_speed_multiplier in between frames.
	virtual void adjust_speed_multiplier() = 0;
	
	///Mutex for accessing AudioTrack::samples.
	std::mutex sample_access_mutex;
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
	std::atomic<double> current_stdaud_frame = 0;
	
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
	std::atomic<double> destination_speed_multiplier = 1.0;
	
	/**
	 * The amount of time for AudioTrack::speed_multiplier to reach AudioTrack::destination_speed_multiplier, regardless of the difference between the two.
	 */
	static constexpr float speed_multiplier_recovering_seconds = 0.5;
	///AudioTrack::speed_multiplier_recovering_seconds but as stdaud frame count actual calculations in AudioTrack::adjust_speed_multiplier.
	static constexpr float speed_multiplier_recovering_frames = speed_multiplier_recovering_seconds * 48000.0;
	///The speed multiplier change for a jog click.
	static constexpr float fine_step_speed_multiplier_delta = 0.025;
	
	///The frame which cue play started from, used for returning back after cue play has stopped.
	std::atomic<std::uint32_t> cue_play_begin_frame = 0;
	///The frame to end playback if the deck is previewing a beat.
	std::atomic<std::uint32_t> end_beat_preview_at_frame = 0;

	//Looping
	std::atomic<std::uint32_t> loop_frame_begin = 0;
	std::atomic<std::uint32_t> loop_frame_end = 0;
	std::atomic<float> beats_per_loop = 4;
	std::atomic<bool> loop_queued = false;
	///The stdaud frame at which the first beat is at.
	///This is used in calculating loops and cue points.
	std::atomic<std::uint32_t> first_beat_stdaud_frame = 0;

	//Track data
	std::string audfile_name = "Track not loaded.";
	std::atomic<float> bpm = 0.0;		
	std::uint8_t track_id;
};

/**
An implementation of AudioTrack to use for production code.
*/
class AudioTrackImpl : public AudioTrack{
	public:
	AudioTrackImpl(const std::uint32_t minimum_frames_in_buffer, const uint8_t track_id)
	: AudioTrack(minimum_frames_in_buffer, track_id)
	{
	}
	
	/**
	Loads the final audio of the deck to be played in a an audio engine callback to AudioTrack::samples.
	
	Errors and information are reported through standard streams such as:
	- the underlying pure stdaud read from ntrb_AudioBuffer failing to read its audio file, reported through std::cerr
	- AudioTrack::stdaud_from_file reaching EOF, reported through std::cout
	*/
	void load_samples();
	
	/**
	Sets the file which the deck will play (frees the previous audio file if needed), loads the info of the audio file,
	and displays the deck info.
	
	Error from initialising an ntrb_AudioBuffer for the file is returned; but the ones related to loading the audio info is reported through std::cerr.
	*/
	ntrb_AudioBufferNew_Error set_file_to_load_from(const char* const filename, const std::uint32_t frames_per_callback);
	void load_audio_info(const std::string& aud_filename);
	
	void display_deck_info();

	bool toggle_play_pause();
	AudioTrack_PlayMode get_play_mode() const noexcept override;

	bool initiate_cue_play() override;
	void stop_cue_play() noexcept override;

	bool play_only_next_beat() override;
	bool play_only_prev_beat() override;
	
	void fine_step_backward() noexcept override;
	void fine_step_forward()  noexcept override;
	void set_destination_speed_multiplier(const float dest_speed_multiplier);

	float increment_loop_step();
	float decrement_loop_step();
	
	bool set_loop();
	void set_beats_per_loop(const float beats_per_loop);
	void cancel_loop();
	
	bool cue_to_nearest_cue_point();
	
	private:
	bool load_single_frame();
	bool fill_sample_buffer_while_in_loop(const std::uint32_t minimum_samples_in_sample_buffer);
	bool fill_sample_buffer(const std::uint32_t minimum_samples_in_sample_buffer);
	bool fill_sample_buffer_while_in_beat_preview(const std::uint32_t minimum_samples_in_sample_buffer);
	
	std::optional<std::uint32_t> find_nearest_loop_cue_point();
	std::optional<std::uint32_t> find_eariler_cue_point();
	void adjust_speed_multiplier();
	
	float get_seconds_per_beat() const;
	float get_frames_per_beat(const float seconds_per_beat) const;
	float get_frames_per_beat() const{
		return get_frames_per_beat(get_seconds_per_beat());
	}
};

#endif

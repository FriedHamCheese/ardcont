#include "AudioTrack.hpp"
#include "ui.hpp"
#include "GlobalStates.hpp"

#include "ntrb/aud_std_fmt.h"
#include "ntrb/utils.h"

#include <pthread.h>

#include <mutex>
#include <cmath>
#include <vector>
#include <chrono>
#include <fstream>
#include <cstring>
#include <optional>
#include <iostream>

AudioTrack::AudioTrack(const std::uint32_t minimum_frames_in_buffer, const uint8_t track_id)
:	effect_type(EffectType_None),
	sample_access_mutex(), 
	samples(minimum_frames_in_buffer * ntrb_std_audchannels), 
	minimum_frames_in_buffer(minimum_frames_in_buffer),
	track_id(track_id),
	effect_container()
{
}

AudioTrack::~AudioTrack(){
	if(this->initialised_stdaud_from_file)
		ntrb_AudioBuffer_free(&(this->stdaud_from_file));
}

void AudioTrack::load_samples() noexcept{
	try{
		std::lock_guard<std::mutex> stdaud_samples_access(this->sample_access_mutex);
		
		this->samples.clear();

		const bool not_loading_audio = (not this->initialised_stdaud_from_file) 
										or (this->play_mode.load() == AudioTrack_no_playback)
										or (this->stdaud_from_file.load_err == ntrb_AudioBufferLoad_EOF);
		if(not_loading_audio){
			this->samples.insert(this->samples.end(), this->minimum_frames_in_buffer * ntrb_std_audchannels, 0.0);
			if(this->stdaud_from_file.load_err) this->play_mode = AudioTrack_no_playback;
		}else{
			const std::uint32_t minimum_samples_in_sample_buffer = this->minimum_frames_in_buffer * ntrb_std_audchannels;
			bool sample_buffer_loaded = true;
			
			if(this->loop_queued.load())
				sample_buffer_loaded = this->fill_sample_buffer_while_in_loop(minimum_samples_in_sample_buffer);
			else if(this->play_mode.load() == AudioTrack_beat_preview)
				sample_buffer_loaded = this->fill_sample_buffer_while_in_beat_preview(minimum_samples_in_sample_buffer);
			else 
				sample_buffer_loaded = this->fill_sample_buffer(minimum_samples_in_sample_buffer);
			
			if(not sample_buffer_loaded){
				this->samples.insert(this->samples.end(), this->minimum_frames_in_buffer * ntrb_std_audchannels, 0.0);
				const std::string msg = std::string("Error loading samples to deck") + std::to_string(this->track_id) + std::string("(ntrb_AudioBufferLoad_Error ") + std::to_string(this->stdaud_from_file.load_err) + std::string(").");
				ui::print_to_infobar(msg, UIColorPair_Error);
			}
			
			if(this->stdaud_from_file.load_err == ntrb_AudioBufferLoad_EOF){
				const std::string msg = std::string("Track ") + std::to_string(this->track_id) + "finished.";
				ui::print_to_infobar(msg, UIColorPair_Info);
				this->play_mode = AudioTrack_no_playback;
			}
		}
		this->effect_container.apply_effect(this->samples, this->effect_type);		
	}
	catch(const std::system_error& e){
		const std::string msg = std::string("AudioTrack::load_samples(): ") + std::to_string(this->track_id) + std::string(" mutex error.");
		ui::print_to_infobar(msg, UIColorPair_Error);
		return;
	}
	catch(const std::exception& e){
		const std::string msg = std::string("AudioTrack::load_samples(): ") + std::to_string(this->track_id) + e.what();
		ui::print_to_infobar(msg, UIColorPair_Error);
		return;
	}
	catch(...){
		const std::string msg = std::string("AudioTrack::load_samples(): ") + std::to_string(this->track_id) + std::string(" uncaught throw.");
		ui::print_to_infobar(msg, UIColorPair_Error);
		return;
	}
}


ntrb_AudioBufferNew_Error AudioTrack::set_file_to_load_from(const char* const filename, const std::uint32_t frames_per_callback) noexcept{
	try{
		std::lock_guard<std::mutex> _(this->sample_access_mutex);
		
		if(this->initialised_stdaud_from_file)
			ntrb_AudioBuffer_free(&(this->stdaud_from_file));
		
		const ntrb_AudioBufferNew_Error new_file_aud_err = ntrb_AudioBuffer_new(&(this->stdaud_from_file), filename, frames_per_callback);
		if(new_file_aud_err) return new_file_aud_err;

		this->initialised_stdaud_from_file = true;
		this->audfile_name = filename;
		this->loop_queued = false;
		this->current_stdaud_frame = 0.0;
		return new_file_aud_err;
	}
	catch(const std::system_error& e){
		return ntrb_AudioBufferNew_RwlockInitError;
	}
}


void AudioTrack::load_audio_info(const std::string& aud_filename){
	this->bpm = 0.0;
	this->first_beat_stdaud_frame = 0;
	
	std::string aud_info_filename;
	const std::size_t filetype_separator_index = aud_filename.rfind('.');
	if(filetype_separator_index == std::string::npos)
		aud_info_filename =  aud_filename + ".txt";
	else{
		const std::string before_separator = aud_filename.substr(0, filetype_separator_index);
		aud_info_filename = before_separator + ".txt";
	}
	
	std::ifstream metadata_file(aud_info_filename);
	if(!metadata_file){
		ui::print_to_infobar("Audio info file not found. Functionalities limited.", UIColorPair_Warning);
		return;
	}
	
	std::string keyword, value;
	while(metadata_file >> keyword >> value){
		try{
			if(keyword == "bpm") this->bpm = std::stof(value);
			else if(keyword == "first_beat"){
				float seconds = 0.0;
				const std::string::size_type minute_second_separator = value.find(':');
				const std::string::size_type second_millisecond_separator = value.find('.');
				
				const bool has_minute_second_separator = minute_second_separator != std::string::npos;
				const bool has_second_millisecond_separator = second_millisecond_separator != std::string::npos;

				if(has_minute_second_separator){
					const std::uint8_t minutes = std::stoi(value.substr(0, minute_second_separator));
					seconds += (float)minutes * 60.0;
					
					if(has_second_millisecond_separator)
						seconds += std::stoi(value.substr(minute_second_separator, second_millisecond_separator));
					else
						seconds += std::stoi(value.substr(minute_second_separator));					
				}else{
					if(has_second_millisecond_separator)
						seconds += std::stoi(value.substr(0, second_millisecond_separator));
					else
						seconds += std::stoi(value);						
				}
				if(has_second_millisecond_separator){
					const std::uint16_t milliseconds = std::stoi(value.substr(second_millisecond_separator+1));
					seconds += (float)milliseconds / 1000.0;
				}

				this->first_beat_stdaud_frame = seconds * ntrb_std_samplerate;
			}
		}
		catch(const std::invalid_argument& stox_not_a_number){
			const std::string msg = "Audio info file: data for " + keyword + " is not a number.";
			ui::print_to_infobar(msg, UIColorPair_Warning);
		}
		catch(const std::out_of_range& stox_out_of_range){
			const std::string msg = "Audio info file: data for " + keyword + " not in range.";
			ui::print_to_infobar(msg, UIColorPair_Warning);
		}
	}
}

bool AudioTrack::toggle_play_pause() noexcept{
	if(this->initialised_stdaud_from_file){
		const int stdaud_rwlock_acq_err = pthread_rwlock_rdlock(&(this->stdaud_from_file.buffer_access));
		if(stdaud_rwlock_acq_err) return false;
	}
	
	const bool user_requests_replay = (this->play_mode.load() == AudioTrack_no_playback) 
										and (this->stdaud_from_file.load_err == ntrb_AudioBufferLoad_EOF);
	if(user_requests_replay){
		this->current_stdaud_frame = 0;
		this->stdaud_from_file.load_err = ntrb_AudioBufferLoad_OK;
	}
	
	if(this->initialised_stdaud_from_file)
		pthread_rwlock_unlock(&(this->stdaud_from_file.buffer_access));
	
	if (this->play_mode.load() != AudioTrack_regular_play)
		this->play_mode = AudioTrack_regular_play;
	else
		this->play_mode = AudioTrack_no_playback;
	return true;
}

AudioTrack_PlayMode AudioTrack::get_play_mode() const noexcept{
	return this->play_mode.load();
}

bool AudioTrack::initiate_cue_play() noexcept{
	const std::optional<std::uint32_t> earlier_cue_point = this->find_eariler_cue_point();
	if(not earlier_cue_point.has_value()) return false;
	
	const std::uint32_t current_beat = earlier_cue_point.value() + this->get_frames_per_beat(this->bpm.load());
	this->cue_play_begin_frame = current_beat;
	this->current_stdaud_frame = current_beat;
	this->play_mode = AudioTrack_cue_play;
	return true;
}

void AudioTrack::stop_cue_play() noexcept{
	if(this->play_mode.load() != AudioTrack_cue_play)
		return;
	
	this->play_mode = AudioTrack_no_playback;
	this->current_stdaud_frame = this->cue_play_begin_frame.load();
}

bool AudioTrack::play_only_next_beat() noexcept{
	const AudioTrack_PlayMode current_play_mode = this->play_mode.load();
	
	if((current_play_mode == AudioTrack_no_playback) or (current_play_mode == AudioTrack_cue_play)){
		const float frames_per_beat = this->get_frames_per_beat(this->bpm.load());
		
		const std::optional<std::uint32_t> earlier_cue_point = this->find_eariler_cue_point();
		if(not earlier_cue_point.has_value()) return false;
		
		const std::uint32_t next_beat_at_frame = earlier_cue_point.value() + (frames_per_beat*2);
		this->current_stdaud_frame = next_beat_at_frame;
		this->end_beat_preview_at_frame = next_beat_at_frame + frames_per_beat;
		this->play_mode = AudioTrack_beat_preview;
	}
	
	return true;
}

bool AudioTrack::play_only_prev_beat() noexcept{
	const AudioTrack_PlayMode current_play_mode = this->play_mode.load();
	
	if((current_play_mode == AudioTrack_no_playback) or (current_play_mode == AudioTrack_cue_play)){
		const std::optional<std::uint32_t> earlier_cue_point = this->find_eariler_cue_point();
		if(not earlier_cue_point.has_value()) return false;
		
		const std::uint32_t previous_beat_frame = earlier_cue_point.value();
		const float frames_per_beat = this->get_frames_per_beat(this->bpm.load());
		
		this->current_stdaud_frame = previous_beat_frame;
		this->end_beat_preview_at_frame = previous_beat_frame + frames_per_beat;
		this->play_mode = AudioTrack_beat_preview;
	}
	return true;
}

void AudioTrack::fine_step_backward() noexcept{
	this->speed_multiplier = this->speed_multiplier.load() - this->fine_step_speed_multiplier_delta;
}

void AudioTrack::fine_step_forward() noexcept{
	this->speed_multiplier = this->speed_multiplier.load() + this->fine_step_speed_multiplier_delta;
}

void AudioTrack::set_destination_speed_multiplier(const float dest_speed_multiplier) noexcept{
	this->destination_speed_multiplier = dest_speed_multiplier;
}

bool AudioTrack::set_loop(){
	if(this->bpm.load() == 0.0) return false;
	
	const std::optional<std::uint32_t> nearest_loop_cue_point = this->find_nearest_loop_cue_point();
	if(!nearest_loop_cue_point.has_value())
		return false;
	
	this->loop_frame_begin = nearest_loop_cue_point.value();
	
	const float frames_per_loop = get_frames_per_beat(this->bpm.load()) * this->beats_per_loop.load();
	this->loop_frame_end = this->loop_frame_begin + frames_per_loop;
	this->loop_queued = true;
	return true;
}

float AudioTrack::increment_loop_step() noexcept{
	this->beats_per_loop = this->beats_per_loop.load() * 2;

	const float frames_per_loop = get_frames_per_beat(this->bpm.load()) * this->beats_per_loop.load();
	this->loop_frame_end = this->loop_frame_begin + frames_per_loop;
	return this->beats_per_loop.load();
}

float AudioTrack::decrement_loop_step() noexcept{
	this->beats_per_loop = this->beats_per_loop.load() / 2;
	
	const float frames_per_loop = get_frames_per_beat(this->bpm.load()) * this->beats_per_loop.load();
	this->loop_frame_end = this->loop_frame_begin + frames_per_loop;
	return this->beats_per_loop.load();
}

void AudioTrack::cancel_loop(){
	if(this->loop_queued.load()) this->loop_queued = false;
}

bool AudioTrack::cue_to_nearest_cue_point() noexcept{
	const std::optional<std::uint32_t> cue_point_frames = this->find_eariler_cue_point();
	if(not cue_point_frames.has_value())
		return false;
	
	this->current_stdaud_frame = cue_point_frames.value();
	return true;
}

//private methods
bool AudioTrack::load_single_frame(){
	const float current_frame = this->current_stdaud_frame.load();
	const std::uint32_t current_frame_floored = std::floor(current_frame);
	const std::uint32_t current_frame_ceiled = std::ceil(current_frame);

	const std::uint32_t file_stdaud_buffer_end_frame = this->stdaud_from_file.stdaud_buffer_first_frame + this->stdaud_from_file.monochannel_samples;
	const bool frame_outside_file_stdaud = current_frame_ceiled >= file_stdaud_buffer_end_frame;
	if(frame_outside_file_stdaud) return false;
	
	const std::uint32_t last_sample_index_in_file_stdaud = (this->stdaud_from_file.monochannel_samples * ntrb_std_audchannels) - 1;
	const std::uint32_t last_left_channel_index_in_file_stdaud = last_sample_index_in_file_stdaud - 1;
	const std::uint32_t last_right_channel_index_in_file_stdaud = last_sample_index_in_file_stdaud;
	
	/*
	Substracting the current frame by the first frame which the file audio buffer is at,
	yields the index to access the current frame in the file audio buffer.
	*/
	const std::uint32_t file_frame_index_floored = current_frame_floored - this->stdaud_from_file.stdaud_buffer_first_frame;
	const std::uint32_t file_frame_index_ceiled = current_frame_ceiled - this->stdaud_from_file.stdaud_buffer_first_frame;
	
	const std::uint32_t floored_file_sample_index = file_frame_index_floored * ntrb_std_audchannels;
	const std::uint32_t ceiled_file_sample_index = file_frame_index_ceiled * ntrb_std_audchannels;

	const std::uint32_t floored_left_channel_index = ntrb_clamp_u64(floored_file_sample_index, 0, last_left_channel_index_in_file_stdaud);
	const std::uint32_t ceiled_left_channel_index = ntrb_clamp_u64(ceiled_file_sample_index, 0, last_left_channel_index_in_file_stdaud);
	
	const std::uint32_t floored_right_channel_index = ntrb_clamp_u64(floored_file_sample_index + 1, 0, last_right_channel_index_in_file_stdaud);
	const std::uint32_t ceiled_right_channel_index = ntrb_clamp_u64(ceiled_file_sample_index + 1, 0, last_right_channel_index_in_file_stdaud);
	
	const float left_channel_value = this->stdaud_from_file.datapoints[floored_left_channel_index] 
									+ ((current_frame - current_frame_floored) * (this->stdaud_from_file.datapoints[ceiled_left_channel_index] - this->stdaud_from_file.datapoints[floored_left_channel_index]));
	
	const float right_channel_value = this->stdaud_from_file.datapoints[floored_right_channel_index] 
									+ ((current_frame - current_frame_floored) * (this->stdaud_from_file.datapoints[ceiled_right_channel_index] - this->stdaud_from_file.datapoints[floored_right_channel_index]));
	
	this->samples.push_back(left_channel_value);
	this->samples.push_back(right_channel_value);	
	this->current_stdaud_frame = current_frame + this->speed_multiplier.load();
	
	this->adjust_speed_multiplier();
	return true;
}

bool AudioTrack::fill_sample_buffer_while_in_loop(const std::uint32_t minimum_samples_in_sample_buffer){
	const std::uint32_t loop_frame_end_copy = this->loop_frame_end.load();
	if(this->current_stdaud_frame.load() >= loop_frame_end_copy){
		this->stdaud_from_file.stdaud_next_buffer_first_frame = this->loop_frame_begin;
		this->current_stdaud_frame = this->loop_frame_begin;
	}else
		this->stdaud_from_file.stdaud_next_buffer_first_frame = this->current_stdaud_frame.load();
	
	while(this->samples.size() < minimum_samples_in_sample_buffer){
		this->stdaud_from_file.load_buffer_callback(&(this->stdaud_from_file));	
		const ntrb_AudioBufferLoad_Error load_err = this->stdaud_from_file.load_err;
		
		if(load_err != ntrb_AudioBufferLoad_OK && load_err != ntrb_AudioBufferLoad_EOF)
			return false;
		if(load_err == ntrb_AudioBufferLoad_EOF) 
			//ntrb will 0 fill its stdaud buffer if an eof has reached.
			break;	
				
		while(this->samples.size() < minimum_samples_in_sample_buffer){
			const bool has_next_frame_in_file_aud_buffer = this->load_single_frame();
			if(!has_next_frame_in_file_aud_buffer || this->current_stdaud_frame.load() >= loop_frame_end_copy) 
				break;
		}
	}
	return true;
}

bool AudioTrack::fill_sample_buffer(const std::uint32_t minimum_samples_in_sample_buffer){
	while(this->samples.size() < minimum_samples_in_sample_buffer){
		this->stdaud_from_file.stdaud_next_buffer_first_frame = this->current_stdaud_frame.load();
		
		this->stdaud_from_file.load_buffer_callback(&(this->stdaud_from_file));
		const ntrb_AudioBufferLoad_Error load_err = this->stdaud_from_file.load_err;
		
		if(load_err != ntrb_AudioBufferLoad_OK && load_err != ntrb_AudioBufferLoad_EOF)
			return false;
		if(load_err == ntrb_AudioBufferLoad_EOF) 
			//ntrb will 0 fill its stdaud buffer if an eof has reached.
			break;
		
		while(this->samples.size() < minimum_samples_in_sample_buffer){
			const bool has_next_frame_in_file_aud_buffer = this->load_single_frame();
			if(!has_next_frame_in_file_aud_buffer) break;
		}		
	}
	return true;
}

bool AudioTrack::fill_sample_buffer_while_in_beat_preview(const std::uint32_t minimum_samples_in_sample_buffer){
	while(this->samples.size() < minimum_samples_in_sample_buffer){
		this->stdaud_from_file.stdaud_next_buffer_first_frame = this->current_stdaud_frame.load();
		this->stdaud_from_file.load_buffer_callback(&(this->stdaud_from_file));	
		const ntrb_AudioBufferLoad_Error load_err = this->stdaud_from_file.load_err;
		
		if(load_err != ntrb_AudioBufferLoad_OK && load_err != ntrb_AudioBufferLoad_EOF)
			return false;
		if(load_err == ntrb_AudioBufferLoad_EOF) 
			//ntrb will 0 fill its stdaud buffer if an eof has reached.
			break;
		
		while(this->samples.size() < minimum_samples_in_sample_buffer){
			const bool has_next_frame_in_file_aud_buffer = this->load_single_frame();
			if(!has_next_frame_in_file_aud_buffer) break;
			if(this->current_stdaud_frame >= this->end_beat_preview_at_frame){
				const std::uint32_t remaining_frames_for_zero_fill = minimum_samples_in_sample_buffer - this->samples.size();
				this->samples.insert(this->samples.end(), remaining_frames_for_zero_fill, 0.0);
				
				this->current_stdaud_frame = this->end_beat_preview_at_frame - this->get_frames_per_beat(this->bpm.load());
				this->play_mode = AudioTrack_no_playback;
				return true;
			}
		}
	}
	return true;
}

std::optional<std::uint32_t> AudioTrack::find_nearest_loop_cue_point() noexcept{
	const int stdaud_rwlock_acq_err = pthread_rwlock_rdlock(&(this->stdaud_from_file.buffer_access));
	if(stdaud_rwlock_acq_err) return std::nullopt;
	
	const std::uint32_t frames_per_beat = this->get_frames_per_beat(this->bpm.load());
	const std::uint32_t current_frame = this->current_stdaud_frame.load();

	std::uint32_t later_nearest_beat_in_frames = this->first_beat_stdaud_frame.load();
	//keep adding the frame for the next beat until exceeding current frame
	while(later_nearest_beat_in_frames <= current_frame)
		later_nearest_beat_in_frames += frames_per_beat;
	
	const std::int32_t earlier_nearest_beat_in_frames_unclamped = later_nearest_beat_in_frames - frames_per_beat;
	//Clamp using signed integer because the substraction may result in a negative number, then clamp to a positive number.
	const std::uint32_t earlier_nearest_beat_in_frames = ntrb_clamp_i64(earlier_nearest_beat_in_frames_unclamped, this->first_beat_stdaud_frame, INT32_MAX);
	
	const std::uint32_t delta_frames_earlier_nearest_beat = current_frame - earlier_nearest_beat_in_frames;
	const std::uint32_t delta_frames_later_nearest_beat = later_nearest_beat_in_frames - current_frame;
	
	pthread_rwlock_unlock(&(this->stdaud_from_file.buffer_access));
	
	if(delta_frames_earlier_nearest_beat <= delta_frames_later_nearest_beat)
		return earlier_nearest_beat_in_frames;
	else
		return later_nearest_beat_in_frames;
}

std::optional<std::uint32_t> AudioTrack::find_eariler_cue_point() noexcept{
	const int stdaud_rwlock_acq_err = pthread_rwlock_rdlock(&(this->stdaud_from_file.buffer_access));
	if(stdaud_rwlock_acq_err) return std::nullopt;
	
	const std::uint32_t frames_per_beat = this->get_frames_per_beat(this->bpm.load());
	const std::uint32_t current_frame = this->current_stdaud_frame.load();

	std::uint32_t later_nearest_beat_in_frames = this->first_beat_stdaud_frame.load();	
	//keep adding the frame for the next beat until exceeding current frame
	while(later_nearest_beat_in_frames < current_frame)
		later_nearest_beat_in_frames += frames_per_beat;
	
	const std::int32_t earlier_nearest_beat_in_frames_unclamped = later_nearest_beat_in_frames - frames_per_beat;
	//Clamp using signed integer because the substraction may result in a negative number, then clamp to a positive number.
	const std::uint32_t earlier_nearest_beat_in_frames = ntrb_clamp_i64(earlier_nearest_beat_in_frames_unclamped, this->first_beat_stdaud_frame, INT32_MAX);
	
	pthread_rwlock_unlock(&(this->stdaud_from_file.buffer_access));
	return earlier_nearest_beat_in_frames;
}

void AudioTrack::adjust_speed_multiplier() noexcept{
	const float current_speed_multiplier = this->speed_multiplier.load();
	const float current_destination_speed_multiplier = this->destination_speed_multiplier.load();
	
	const float speed_multiplier_delta = current_speed_multiplier - current_destination_speed_multiplier;
	const float speed_multiplier_recovering_per_frame = std::fabs(speed_multiplier_delta) / this->speed_multiplier_recovering_frames;
	
	if(ntrb_float_equal(current_speed_multiplier, current_destination_speed_multiplier, 0.01))
		this->speed_multiplier = current_destination_speed_multiplier;
	else if(this->speed_multiplier.load() > this->destination_speed_multiplier.load())
		this->speed_multiplier = current_speed_multiplier - speed_multiplier_recovering_per_frame;
	else
		this->speed_multiplier = current_speed_multiplier + speed_multiplier_recovering_per_frame;
}
#include "AudioTrack.hpp"
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
:	sample_access_mutex(), 
	samples(minimum_frames_in_buffer * ntrb_std_audchannels), 
	minimum_frames_in_buffer(minimum_frames_in_buffer),
	track_id(track_id)
{
	
}

AudioTrack::~AudioTrack(){
	if(this->initialised_stdaud_from_file)
		ntrb_AudioBuffer_free(&(this->stdaud_from_file));
}

void AudioTrackImpl::load_samples(){
	std::lock_guard<std::mutex> stdaud_samples_access(this->sample_access_mutex);
	this->samples.clear();

	const bool not_loading_audio = (not this->initialised_stdaud_from_file) 
									or (this->play_mode.load() == AudioTrack_no_playback)
									or (this->stdaud_from_file.load_err == ntrb_AudioBufferLoad_EOF);
	if(not_loading_audio){
		this->samples.insert(this->samples.end(), this->minimum_frames_in_buffer * ntrb_std_audchannels, 0.0);
		if(this->stdaud_from_file.load_err) this->play_mode = AudioTrack_no_playback;
		return;
	}

	const std::uint32_t minimum_samples_in_sample_buffer = this->minimum_frames_in_buffer * ntrb_std_audchannels;
	bool sample_buffer_loaded = true;
	
	if(this->loop_queued.load())
		sample_buffer_loaded = this->fill_sample_buffer_while_in_loop(minimum_samples_in_sample_buffer);
	else 
		sample_buffer_loaded = this->fill_sample_buffer(minimum_samples_in_sample_buffer);
	
	if(not sample_buffer_loaded){
		this->samples.insert(this->samples.end(), this->minimum_frames_in_buffer * ntrb_std_audchannels, 0.0);
		std::cerr << "[Error]: AudioTrackImpl::load_samples(): Error loading " << this->audfile_name << "(ntrb_AudioBufferLoad_Error: " << this->stdaud_from_file.load_err << ")."
			<< "\n\tSkipping audio loading callback...\n";
		std::cout << ": " << std::flush;		
	}
	
	if(this->stdaud_from_file.load_err == ntrb_AudioBufferLoad_EOF){
		std::cout << "Track " << (std::uint16_t)this->track_id << ": " << this->audfile_name << " finished."
					<< "\n\tTrack now in pause." << std::endl;
		std::cout << ": " << std::flush;
		this->play_mode = AudioTrack_no_playback;
	}
}


ntrb_AudioBufferNew_Error AudioTrackImpl::set_file_to_load_from(const char* const filename, const std::uint32_t frames_per_callback){
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


void AudioTrackImpl::load_audio_info(const std::string& aud_filename){
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
		std::cerr << "[Warn]: metadata for audio file not found."
					<< "\n\t bpm is set to 0.0, cueing and looping is unavaliable.\n";
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

				if(minute_second_separator != std::string::npos){
					const std::uint8_t minutes = std::stoi(value.substr(0, minute_second_separator));
					seconds += (float)minutes * 60.0;
				}
				if(second_millisecond_separator != std::string::npos){
					const std::uint16_t milliseconds = std::stoi(value.substr(second_millisecond_separator+1));
					seconds += (float)milliseconds / 1000.0;
				}
				
				this->first_beat_stdaud_frame = seconds * ntrb_std_samplerate;
			}
		}
		catch(const std::invalid_argument& stox_not_a_number){
			std::cerr << "[Error]: AudioTrackImpl::set_file_to_load_from(): Data in " << keyword << " field is not a number.\n";
		}
		catch(const std::out_of_range& stox_out_of_range){
			std::cerr << "[Error]: AudioTrackImpl::set_file_to_load_from(): Data in " << keyword << " field is either too small or too large.\n";
		}
	}
}

void AudioTrackImpl::display_deck_info(){
	std::cout << "\nTrack " << (std::uint16_t)this->track_id
				<< "\n\tAudio filename: " << this->audfile_name
				<< "\n\tTrack paused: " <<  (this->play_mode.load() == AudioTrack_no_playback)
				<< "\n\tbpm: " << this->bpm.load()
				<< "\n\tFirst beat at " << (float)this->first_beat_stdaud_frame / (float)ntrb_std_samplerate << " seconds." << std::endl;
}

bool AudioTrackImpl::toggle_play_pause(){
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

AudioTrack_PlayMode AudioTrackImpl::get_play_mode() const noexcept{
	return this->play_mode.load();
}

void AudioTrackImpl::initiate_cue_play(){
	this->cue_play_begin_frame = this->find_eariler_cue_point().value();
	this->current_stdaud_frame = this->cue_play_begin_frame.load();
	this->play_mode = AudioTrack_cue_play;
}

void AudioTrackImpl::stop_cue_play(){
	if(this->play_mode.load() != AudioTrack_cue_play)
		return;
	
	this->play_mode = AudioTrack_no_playback;
	this->current_stdaud_frame = this->cue_play_begin_frame.load();
}

void AudioTrackImpl::fine_step_backward(){
	this->speed_multiplier = this->speed_multiplier.load() - this->fine_step_speed_multiplier_delta;
}

void AudioTrackImpl::fine_step_forward(){
	this->speed_multiplier = this->speed_multiplier.load() + this->fine_step_speed_multiplier_delta;
}

void AudioTrackImpl::set_destination_speed_multiplier(const float dest_speed_multiplier){
	this->destination_speed_multiplier = dest_speed_multiplier;
}

float AudioTrackImpl::get_seconds_per_beat() const{
	return  60.0 / this->bpm.load();
}
float AudioTrackImpl::get_frames_per_beat(const float seconds_per_beat) const{
	return (float)ntrb_std_samplerate * seconds_per_beat;
}

bool AudioTrackImpl::set_loop(){
	if(this->bpm.load() == 0.0) return true;
	
	const std::optional<std::uint32_t> nearest_loop_cue_point = this->find_nearest_loop_cue_point();
	if(!nearest_loop_cue_point.has_value())
		return false;
	
	this->loop_frame_begin = nearest_loop_cue_point.value();
	std::cout << "Track " << (std::uint16_t)this->track_id << ": Nearest beat at: " << (float)this->loop_frame_begin / ntrb_std_samplerate << " seconds" 
				<< "\n\t(" << this->loop_frame_begin << " stdaud frames).\n";
	std::cout << ": " << std::flush;
	
	const float frames_per_loop = get_frames_per_beat(get_seconds_per_beat()) * this->beats_per_loop.load();
	this->loop_frame_end = this->loop_frame_begin + frames_per_loop;
	this->loop_queued = true;
	return true;
}

float AudioTrackImpl::increment_loop_step(){
	this->beats_per_loop = this->beats_per_loop.load() * 2;

	const float frames_per_loop = get_frames_per_beat(get_seconds_per_beat()) * this->beats_per_loop.load();
	this->loop_frame_end = this->loop_frame_begin + frames_per_loop;
	return this->beats_per_loop.load();
}

float AudioTrackImpl::decrement_loop_step(){
	this->beats_per_loop = this->beats_per_loop.load() / 2;
	
	const float frames_per_loop = get_frames_per_beat(get_seconds_per_beat()) * this->beats_per_loop.load();
	this->loop_frame_end = this->loop_frame_begin + frames_per_loop;
	return this->beats_per_loop.load();
}

void AudioTrackImpl::cancel_loop(){
	if(this->loop_queued.load() == true){
		this->loop_queued = false;
		std::cout << "Track " << (std::uint16_t)this->track_id << " loop cancelled.\n";
		std::cout << ": " << std::flush;
	}
}

bool AudioTrackImpl::cue_to_nearest_cue_point(){
	const std::optional<std::uint32_t> cue_point_frames = this->find_eariler_cue_point();
	if(not cue_point_frames.has_value())
		return false;
	
	this->current_stdaud_frame = cue_point_frames.value();
	return true;
}


//private methods
bool AudioTrackImpl::load_single_frame(){
	const double this_frame_pos = this->current_stdaud_frame.load();
	const std::uint32_t this_frame_pos_floored = std::floor(this_frame_pos);
	const std::uint32_t this_frame_pos_ceiled = std::ceil(this_frame_pos);

	const std::uint32_t stdaud_from_file_frame_end = this->stdaud_from_file.stdaud_buffer_first_frame + this->stdaud_from_file.monochannel_samples;
	if(this_frame_pos_ceiled >= stdaud_from_file_frame_end) return false;
	
	const std::uint32_t max_i_in_stdaud_from_file = (this->stdaud_from_file.monochannel_samples * 2) - 1;
	
	const std::uint32_t stdaud_frame_i_frame_floored = this_frame_pos_floored - this->stdaud_from_file.stdaud_buffer_first_frame;
	const std::uint32_t stdaud_frame_i_frame_ceiled = this_frame_pos_ceiled - this->stdaud_from_file.stdaud_buffer_first_frame;
	
	const std::uint32_t stdaud_frame_lch_i_floored = ntrb_clamp_u64(stdaud_frame_i_frame_floored * 2, 0, max_i_in_stdaud_from_file - 1);
	const std::uint32_t stdaud_frame_lch_i_ceiled = ntrb_clamp_u64(stdaud_frame_i_frame_ceiled * 2, 0, max_i_in_stdaud_from_file - 1);
	
	const std::uint32_t stdaud_frame_rch_i_floored = ntrb_clamp_u64((stdaud_frame_i_frame_floored * 2) + 1, 0, max_i_in_stdaud_from_file);
	const std::uint32_t stdaud_frame_rch_i_ceiled = ntrb_clamp_u64((stdaud_frame_i_frame_ceiled * 2) + 1, 0, max_i_in_stdaud_from_file);
	
	const float left_channel_value = this->stdaud_from_file.datapoints[stdaud_frame_lch_i_floored] + ((this_frame_pos - this_frame_pos_floored) * (this->stdaud_from_file.datapoints[stdaud_frame_lch_i_ceiled] - this->stdaud_from_file.datapoints[stdaud_frame_lch_i_floored]));
	
	const float right_channel_value = this->stdaud_from_file.datapoints[stdaud_frame_rch_i_floored] + ((this_frame_pos - this_frame_pos_floored) * (this->stdaud_from_file.datapoints[stdaud_frame_rch_i_ceiled] - this->stdaud_from_file.datapoints[stdaud_frame_rch_i_floored]));
	
	this->samples.push_back(left_channel_value);
	this->samples.push_back(right_channel_value);	
	this->current_stdaud_frame = this_frame_pos + this->speed_multiplier.load();
	
	this->adjust_speed_multiplier();
	return true;
}

bool AudioTrackImpl::fill_sample_buffer_while_in_loop(const std::uint32_t minimum_samples_in_sample_buffer){
	while(this->samples.size() < minimum_samples_in_sample_buffer){
		this->stdaud_from_file.load_buffer_callback(&(this->stdaud_from_file));	
		const ntrb_AudioBufferLoad_Error load_err = this->stdaud_from_file.load_err;
		
		if(load_err != ntrb_AudioBufferLoad_OK && load_err != ntrb_AudioBufferLoad_EOF)
			return false;
		if(load_err == ntrb_AudioBufferLoad_EOF) 
			//ntrb will 0 fill its stdaud buffer if an eof has reached.
			break;	
		
		const std::uint32_t loop_frame_end_copy = this->loop_frame_end.load();
		
		while(this->samples.size() < minimum_samples_in_sample_buffer){
			const bool has_next_frame_in_file_aud_buffer = this->load_single_frame();
			if(!has_next_frame_in_file_aud_buffer || this->current_stdaud_frame.load() >= loop_frame_end_copy) break;
		}
		
		if(this->current_stdaud_frame.load() >= loop_frame_end_copy){
			this->stdaud_from_file.stdaud_next_buffer_first_frame = this->loop_frame_begin;
			this->current_stdaud_frame = this->loop_frame_begin;
		}else
			this->stdaud_from_file.stdaud_next_buffer_first_frame = this->current_stdaud_frame.load();
	}
	return true;
}

bool AudioTrackImpl::fill_sample_buffer(const std::uint32_t minimum_samples_in_sample_buffer){
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
			if(!has_next_frame_in_file_aud_buffer) break;
		}
		
		this->stdaud_from_file.stdaud_next_buffer_first_frame = this->current_stdaud_frame.load();
	}
	return true;
}

std::optional<std::uint32_t> AudioTrackImpl::find_nearest_loop_cue_point(){
	const int stdaud_rwlock_acq_err = pthread_rwlock_rdlock(&(this->stdaud_from_file.buffer_access));
	if(stdaud_rwlock_acq_err) return std::nullopt;
	
	const std::uint32_t frames_per_cue_step = (60.0 / this->bpm.load()) * float(ntrb_std_samplerate);

	std::uint32_t later_nearest_beat_in_frames = this->first_beat_stdaud_frame.load();
	//keep adding the frame for the next beat until exceeding current frame
	while(later_nearest_beat_in_frames <= this->current_stdaud_frame.load())
		later_nearest_beat_in_frames += frames_per_cue_step;
	
	const std::uint32_t earlier_nearest_beat_in_frames = ntrb_clamp_i64((std::int32_t)later_nearest_beat_in_frames - (std::int32_t)frames_per_cue_step, this->first_beat_stdaud_frame, INT64_MAX);
	
	const std::uint32_t delta_frames_earlier_nearest_beat = this->stdaud_from_file.stdaud_buffer_first_frame - earlier_nearest_beat_in_frames;
	const std::uint32_t delta_frames_later_nearest_beat = later_nearest_beat_in_frames - this->stdaud_from_file.stdaud_buffer_first_frame;
	
	pthread_rwlock_unlock(&(this->stdaud_from_file.buffer_access));
	
	if(delta_frames_earlier_nearest_beat <= delta_frames_later_nearest_beat)
		return earlier_nearest_beat_in_frames;
	else
		return later_nearest_beat_in_frames;
}

std::optional<std::uint32_t> AudioTrackImpl::find_eariler_cue_point(){
	const int stdaud_rwlock_acq_err = pthread_rwlock_rdlock(&(this->stdaud_from_file.buffer_access));
	if(stdaud_rwlock_acq_err) return std::nullopt;
	
	const std::uint32_t frames_per_cue_step = (60.0 / this->bpm.load()) * float(ntrb_std_samplerate);
	std::uint32_t later_nearest_beat_in_frames = this->first_beat_stdaud_frame.load();
	
	//keep adding the frame for the next beat until exceeding current frame
	while(later_nearest_beat_in_frames <= this->current_stdaud_frame.load())
		later_nearest_beat_in_frames += frames_per_cue_step;
	
	const std::uint32_t earlier_nearest_beat_in_frames = ntrb_clamp_i64((std::int32_t)later_nearest_beat_in_frames - (std::int32_t)frames_per_cue_step, this->first_beat_stdaud_frame, INT64_MAX);
	
	pthread_rwlock_unlock(&(this->stdaud_from_file.buffer_access));
	return earlier_nearest_beat_in_frames;
}

void AudioTrackImpl::adjust_speed_multiplier(){
	const float current_speed_multiplier = this->speed_multiplier.load();
	const float current_destination_speed_multiplier = this->destination_speed_multiplier.load();
	
	const float speed_multiplier_recovering_per_frame = std::fabs((current_speed_multiplier - current_destination_speed_multiplier) / this->speed_multiplier_recovering_frames);
	
	if(ntrb_float_equal(current_speed_multiplier, current_destination_speed_multiplier, 0.01))
		this->speed_multiplier = current_destination_speed_multiplier;
	else if(this->speed_multiplier.load() > this->destination_speed_multiplier.load())
		this->speed_multiplier = current_speed_multiplier - speed_multiplier_recovering_per_frame;
	else
		this->speed_multiplier = current_speed_multiplier + speed_multiplier_recovering_per_frame;
}
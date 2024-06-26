#include "AudioTrack.hpp"
#include "audeng_state.hpp"

#include "ntrb/aud_std_fmt.h"
#include "ntrb/utils.h"

#include <pthread.h>

#include <mutex>
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


void AudioTrack::load_track(){	
	std::lock_guard<std::mutex> stdaud_samples_access(this->sample_access_mutex);
	this->samples.clear();
	
	if(!(this->initialised_stdaud_from_file) || this->in_pause_state.load() || this->stdaud_from_file.load_err == ntrb_AudioBufferLoad_EOF){
		this->samples.insert(this->samples.end(), this->minimum_frames_in_buffer * ntrb_std_audchannels, 0.0);
		if(this->stdaud_from_file.load_err) this->in_pause_state = true;
		return;
	}
		
	if(this->loop_queued.load()){
		const std::uint32_t samples_in_sample_buffer = this->minimum_frames_in_buffer * ntrb_std_audchannels;

		while(this->samples.size() < samples_in_sample_buffer){
			if(this->stdaud_from_file.stdaud_next_buffer_first_frame >= this->loop_frame_end.load())
				this->stdaud_from_file.stdaud_next_buffer_first_frame = this->loop_frame_begin.load();

			this->stdaud_from_file.load_buffer_callback(&(this->stdaud_from_file));
			
			const ntrb_AudioBufferLoad_Error load_err = this->stdaud_from_file.load_err;

			if(load_err != ntrb_AudioBufferLoad_OK && load_err != ntrb_AudioBufferLoad_EOF){
				this->samples.insert(this->samples.end(), this->minimum_frames_in_buffer * ntrb_std_audchannels, 0.0);
				std::cerr << "[Error]: AudioTrack::load_track(): Error loading " << this->audfile_name << "(ntrb_AudioBufferLoad_Error: " << load_err << ")."
						<< "\n\tSkipping audio loading callback...\n";
				std::cout << ": " << std::flush;
				continue;
			}
			
			const std::uint32_t frames_to_be_read_from_file = ntrb_clamp_u64(this->loop_frame_end - this->loop_frame_begin, 0, this->stdaud_from_file.monochannel_samples);
			const std::uint32_t samples_to_be_read_from_file = frames_to_be_read_from_file * ntrb_std_audchannels;
			
			for(std::uint32_t i = 0; i < samples_to_be_read_from_file; i++)
				this->samples.push_back(this->stdaud_from_file.datapoints[i]);
		}
	}else{		
		this->stdaud_from_file.load_buffer_callback(&(this->stdaud_from_file));	
		const ntrb_AudioBufferLoad_Error load_err = this->stdaud_from_file.load_err;
		
		if(load_err != ntrb_AudioBufferLoad_OK && load_err != ntrb_AudioBufferLoad_EOF){
			this->samples.insert(this->samples.end(), this->minimum_frames_in_buffer * ntrb_std_audchannels, 0.0);
			std::cerr << "[Error]: AudioTrack::load_track(): Error loading " << this->audfile_name << "(ntrb_AudioBufferLoad_Error: " << load_err << ")."
				<< "\n\tSkipping audio loading callback...\n";
			std::cout << ": " << std::flush;
			return;
		}
		
		const std::uint32_t file_stdaud_sample_count = this->stdaud_from_file.monochannel_samples * ntrb_std_audchannels;
		
		for(std::uint32_t i = 0; i < file_stdaud_sample_count; i++)
			this->samples.push_back(this->stdaud_from_file.datapoints[i]);
	}
	
	const std::uint32_t sample_count = samples.size();
	for(std::uint32_t i = 0; i < sample_count; i++)
		samples[i] *= this->amplitude_multiplier.load();
	
	if(this->stdaud_from_file.load_err == ntrb_AudioBufferLoad_EOF){
		std::cout << "Track " << (std::uint16_t)this->track_id << ": " << this->audfile_name << " finished."
					<< "\n\tTrack now in pause." << std::endl;
		std::cout << ": " << std::flush;
		this->in_pause_state = true;
	}
}

static std::string get_audio_info_filename_from_audio_filename(const std::string&& aud_filename){
	const std::size_t filetype_separator_index = aud_filename.rfind('.');
	if(filetype_separator_index == std::string::npos)
		return std::string();
	else{
		const std::string before_separator = aud_filename.substr(0, filetype_separator_index);
		return before_separator + ".txt";
	}
}

ntrb_AudioBufferNew_Error AudioTrack::set_file_to_load_from(const char* const filename){
	std::lock_guard<std::mutex> _(this->sample_access_mutex);

	if(this->initialised_stdaud_from_file)
		ntrb_AudioBuffer_free(&(this->stdaud_from_file));
	
	const ntrb_AudioBufferNew_Error new_file_aud_err = ntrb_AudioBuffer_new(&(this->stdaud_from_file), filename, audeng_state::frames_per_callback);
	if(new_file_aud_err) return new_file_aud_err;

	this->initialised_stdaud_from_file = true;
	this->audfile_name = filename;
	
	this->loop_queued = false;
	this->bpm = 0.0;
	this->first_beat_stdaud_frame = 0;
	
	const std::string aud_info_filename = get_audio_info_filename_from_audio_filename(filename);
	if(aud_info_filename.empty()){
		std::cerr << "[Warn]: Unable to form path to the audio info file."
					<< "\n\t bpm is set to 0.0 and looping is unavaliable.\n";
		return new_file_aud_err;		
	}
	
	std::ifstream metadata_file(aud_info_filename);
	if(!metadata_file){
		std::cerr << "[Warn]: metadata for audio file not found."
					<< "\n\t bpm is set to 0.0 and looping is unavaliable.\n";
		return new_file_aud_err;
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
			std::cerr << "[Error]: AudioTrack::set_file_to_load_from(): Data in " << keyword << " field is not a number.\n";
		}
		catch(const std::out_of_range& stox_out_of_range){
			std::cerr << "[Error]: AudioTrack::set_file_to_load_from(): Data in " << keyword << " field is either too small or too large.\n";
		}
	}
	
	std::cout << "\nTrack " << (std::uint16_t)this->track_id
			<< "\n\tAudio filename: " << this->audfile_name
			<< "\n\tTrack paused: " <<  this->in_pause_state.load()
			<< "\n\tbpm: " << this->bpm.load()
			<< "\n\tFirst beat at " << (float)this->first_beat_stdaud_frame / (float)ntrb_std_samplerate << " seconds." << std::endl;
	
	return new_file_aud_err;
}


bool AudioTrack::toggle_play_pause(){
	if(this->initialised_stdaud_from_file){
		const int stdaud_rwlock_acq_err = pthread_rwlock_rdlock(&(this->stdaud_from_file.buffer_access));
		if(stdaud_rwlock_acq_err) return false;
	}
	
	if(this->in_pause_state && this->stdaud_from_file.load_err == ntrb_AudioBufferLoad_EOF){
		this->stdaud_from_file.stdaud_next_buffer_first_frame = 0;
		this->stdaud_from_file.load_err = ntrb_AudioBufferLoad_OK;
	}
	
	if(this->initialised_stdaud_from_file)
		pthread_rwlock_unlock(&(this->stdaud_from_file.buffer_access));

	this->in_pause_state = !(this->in_pause_state.load());
	return true;
}


void AudioTrack::volume(const float volume_knob_ratio){
	this->amplitude_multiplier = volume_knob_ratio;
}


std::mutex& AudioTrack::get_sample_access_mutex(){
	return this->sample_access_mutex;
}

const std::vector<float>& AudioTrack::get_samples() const{
	return this->samples;
}

std::optional<std::uint32_t> AudioTrack::find_nearest_loop_cue_point(){
	const int stdaud_rwlock_acq_err = pthread_rwlock_rdlock(&(this->stdaud_from_file.buffer_access));
	if(stdaud_rwlock_acq_err) return std::nullopt;
	
	const std::uint32_t frames_per_cue_step = AudioTracks_loop_cue_step.load() * (60.0 / this->bpm.load()) * float(ntrb_std_samplerate);

	std::uint32_t later_nearest_beat_in_frames = this->first_beat_stdaud_frame.load();
	while(later_nearest_beat_in_frames <= this->stdaud_from_file.stdaud_buffer_first_frame)
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

bool AudioTrack::set_loop(){
	if(this->bpm.load() == 0.0) return true;
	
	const std::optional<std::uint32_t> nearest_loop_cue_point = this->find_nearest_loop_cue_point();
	if(!nearest_loop_cue_point.has_value())
		return false;
	
	this->loop_frame_begin = nearest_loop_cue_point.value();
	std::cout << "Track " << (std::uint16_t)this->track_id << ": Nearest beat at: " << (float)this->loop_frame_begin / ntrb_std_samplerate << " seconds" 
				<< "\n\t(" << this->loop_frame_begin << " stdaud frames).\n";
	std::cout << ": " << std::flush;
	
	this->loop_frame_end = this->loop_frame_begin + (float)ntrb_std_samplerate * (60.0/(this->bpm.load())) * this->beats_per_loop.load();
	this->loop_queued = true;
	return true;
}

void AudioTrack::set_beats_per_loop(const float beats_per_loop){
	if(beats_per_loop != this->beats_per_loop.load()){
		this->beats_per_loop = beats_per_loop;
		std::cout << "Track " << std::uint16_t(this->track_id) << " beats per loop: " << beats_per_loop << '\n';
		std::cout << ": " << std::flush;
	}
	if(this->loop_queued)
		this->loop_frame_end = this->loop_frame_begin.load() + (float)ntrb_std_samplerate * (60.0/(this->bpm.load())) * this->beats_per_loop.load();
}

void AudioTrack::cancel_loop(){
	if(this->loop_queued.load() == true){
		this->loop_queued = false;
		std::cout << "Track " << (std::uint16_t)this->track_id << " loop cancelled.\n";
		std::cout << ": " << std::flush;
	}
}

std::uint8_t AudioTrack::get_track_id() const{
	return this->track_id;
}
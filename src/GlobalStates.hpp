/**
\file GlobalStates.hpp
*/

#ifndef GLOBALSTATES_HPP
#define GLOBALSTATES_HPP

#include "AudioTrack.hpp"

#include "ntrb/aud_std_fmt.h"

#include <memory>
#include <vector>
#include <atomic>
#include <cstdint>

enum AudioTrackAccess_Status : std::uint8_t{
	AudioTrackAccess_ReadingBlocked,
	AudioTrackAccess_ReadyForReading,
	AudioTrackAccess_BeingRead,
	AudioTrackAccess_FinishedReading,
};

/**
A struct containing states and constants shared between different threads of this program,
passed by reference to the threads.
*/

struct GlobalStates{
	GlobalStates(){
		this->requested_exit = false;	
	}

	static constexpr std::uint16_t msecs_per_callback = 100;
	std::vector<std::unique_ptr<AudioTrack>> audio_tracks;
	
	//A flag used by any thread to notify the other threads to prepare for exiting as soon as possible.
	std::atomic_bool requested_exit;
	
	///A const function for accessing the GlobalStates::frames_per_callback,
	///since C extern const is not considered as C++ const.
	std::uint32_t get_frames_per_callback() const{
		return this->frames_per_callback;
	}

	std::atomic_uint8_t audience_output_device_status = AudioTrackAccess_FinishedReading;
	std::atomic_uint8_t monitor_output_device_status = AudioTrackAccess_FinishedReading;
	
	private:
	std::uint32_t frames_per_callback = (ntrb_std_samplerate * msecs_per_callback) / 1000;
};

#endif
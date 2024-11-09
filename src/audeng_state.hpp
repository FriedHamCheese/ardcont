#ifndef audeng_state_hpp
#define audeng_state_hpp

#include "AudioTrack.hpp"

#include "ntrb/aud_std_fmt.h"

#include <memory>
#include <vector>
#include <atomic>
#include <cstdint>

namespace audeng_state{
	constexpr inline std::uint16_t msecs_per_callback = 50;
	const inline std::uint32_t frames_per_callback = (ntrb_std_samplerate * msecs_per_callback) / 1000;

	inline std::vector<std::unique_ptr<AudioTrack>> audio_tracks;
	inline std::atomic_bool requested_exit = false;
}

#endif
#ifndef audeng_state_hpp
#define audeng_state_hpp

#include "AudioTrack.hpp"

#include "ntrb/aud_std_fmt.h"

#include <array>
#include <atomic>
#include <cstdint>

namespace audeng_state{
	constexpr inline std::uint8_t msecs_per_callback = 50;
	const inline std::uint32_t frames_per_callback = ntrb_std_samplerate * msecs_per_callback / 1000;

	inline std::array<AudioTrack, 2> audio_tracks = {AudioTrack(frames_per_callback, 0), AudioTrack(frames_per_callback, 1)};
	inline std::atomic_bool requested_exit = false;
}

#endif
#ifndef EffectContainer_hpp
#define EffectContainer_hpp

#include "ntrb/aud_std_fmt.h"
#include <vector>
#include <memory>

enum EffectType : uint8_t{
	EffectType_None,
	EffectType_Echo,
	EffectType_LowerSamplerate,
	EffectType_Bitcrush
};

class EffectContainer{
	public:
	EffectContainer();
	void clear_buffer();
	void apply_effect(std::vector<float>& samples, EffectType effect_type);

	float effect_mix_ratio = 0.25;
	float param_value = 1.00;

	private:
	void apply_echo_effect(std::vector<float>& samples);
	void apply_low_samplerate_effect(std::vector<float>& samples);
	void apply_bitcrush(std::vector<float>& samples);
	
	
	const std::size_t echo_buffer_datapoints = 2 * ntrb_std_samplerate * ntrb_std_audchannels;
	std::unique_ptr<float[]> echo_buffer;
	std::size_t current_echo_index = 0;
};


#endif
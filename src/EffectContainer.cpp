#include "EffectContainer.hpp"
#include <cstring>
#include <cstdint>
#include <cmath>

EffectContainer::EffectContainer()
:	echo_buffer(new float[this->echo_buffer_datapoints])
{
	memset(this->echo_buffer.get(), 0, this->echo_buffer_datapoints * sizeof(float));
}

void EffectContainer::clear_buffer(){
	memset(this->echo_buffer.get(), 0, this->echo_buffer_datapoints * sizeof(float));	
}

void EffectContainer::apply_effect(std::vector<float>& samples, EffectType effect_type){
	switch(effect_type){
		case EffectType_None:
		return;
		case EffectType_Echo:
		this->apply_echo_effect(samples);
		return;
		case EffectType_LowerSamplerate:
		this->apply_low_samplerate_effect(samples);
		return;	
		case EffectType_Bitcrush:
		this->apply_bitcrush(samples);		
		default: 
		return;
	}
}

void EffectContainer::apply_echo_effect(std::vector<float>& samples){
	const std::size_t sample_count = samples.size();
	const std::size_t delay_samples = std::size_t(this->param_value) * ntrb_std_audchannels;
	
	for(std::size_t i = 0; i < sample_count; i++){
		samples[i] += this->echo_buffer[i + current_echo_index % echo_buffer_datapoints] * this->effect_mix_ratio;
	}
	for(std::size_t i = 0; i < sample_count; i++){
		this->echo_buffer[(i+delay_samples+this->current_echo_index) % this->echo_buffer_datapoints] = samples[i];
	}
	
	this->current_echo_index += sample_count;
	this->current_echo_index %= this->echo_buffer_datapoints;
}

void EffectContainer::apply_low_samplerate_effect(std::vector<float>& samples){
	const std::size_t sample_count = samples.size();
	const std::size_t interval = std::size_t(this->param_value) * 2;
	
	float duplicating_sample = samples[0];
	for(std::size_t i = 0; i < sample_count; i+=2){
		if(i % interval == 0)
			duplicating_sample = samples[i];
		samples[i] = (duplicating_sample * this->effect_mix_ratio) + (samples[i] * (1-this->effect_mix_ratio));
	}
	
	duplicating_sample = samples[1];
	for(std::size_t i = 1; i < sample_count; i+=2){
		if((i-1) % interval == 0)
			duplicating_sample = samples[i];
		samples[i] = (duplicating_sample * this->effect_mix_ratio) + (samples[i] * (1-this->effect_mix_ratio));
	}
}

void EffectContainer::apply_bitcrush(std::vector<float>& samples){
	const std::uint32_t bit_depth = this->param_value;
	const float bit_range = (bit_depth * bit_depth) - 1;
	for(auto& sample : samples){
		const float integer_value = std::ceil(sample * bit_range);
		sample = this->effect_mix_ratio * integer_value / bit_range + ((1-this->effect_mix_ratio) * sample);
	}
}
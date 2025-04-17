#include "EffectContainer.hpp"
#include <cstring>

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
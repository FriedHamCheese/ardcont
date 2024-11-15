#include "sensor.hpp"

#include <chrono>

Sensor::Sensor(const std::int16_t sensor_id, const std::uint16_t initial_value) noexcept
: sensor_id(sensor_id), value(initial_value), prev_value(initial_value)
{
}

void Sensor::write(const std::int16_t value) noexcept{
	this->value = value;
}

bool Sensor::value_changed() noexcept{
	if(this->value != this->prev_value){
		this->prev_value = this->value;
		return true;
	}
	return false;	
}


Button::Button(const std::int16_t sensor_id) noexcept
: Sensor(sensor_id, ButtonState_Untouched)
{
}

void Button::write(const std::int16_t value) noexcept{
	const bool button_pressed = value != 0;
	if(button_pressed and (this->value == ButtonState_Untouched or this->value == ButtonState_Released)){
		this->value = ButtonState_Pressed;
		this->last_pressed = std::chrono::steady_clock::now();
	}
	else if(button_pressed and (this->value == ButtonState_Pressed)){
		const std::uint32_t ms_since_last_press = (std::chrono::steady_clock::now() - this->last_pressed).count();
		if(ms_since_last_press >= this->pressed_to_held_duration_ms){
			this->value = ButtonState_Held;
		}
	}
	else if((not button_pressed) and ((this->value == ButtonState_Pressed) or (this->value == ButtonState_Held))){
		this->value = ButtonState_Released;
	}
}

RotaryEncoder::RotaryEncoder(const std::int16_t sensor_id) noexcept
: Sensor(sensor_id, 0)
{
}

void RotaryEncoder::write(const std::int16_t value) noexcept{
	this->value = value;
	if(this->value == this->prev_value)
		this->prev_value = 0;
}
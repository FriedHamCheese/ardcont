#include "Sensor.hpp"
#include <Arduino.h>

Sensor::Sensor(const uint8_t pin, const uint8_t id, const int16_t init_value)
:	id(id), value(init_value), prev_value(init_value), pin(pin)
{
}

DigitalSensor::DigitalSensor(const uint8_t pin, const uint8_t id)
: 	Sensor(pin, id, LOW)
{
    pinMode(this->pin, INPUT);
    this->value = digitalRead(this->pin);
    this->prev_value = this->value;
}

bool DigitalSensor::read(){
    this->prev_value = this->value;

    this->value = digitalRead(this->pin);
    return this->value != this->prev_value;
}

Button::Button(const uint8_t pin, const uint8_t id)
: 	Sensor(pin, id, ButtonState_Untouched)
{
    pinMode(this->pin, INPUT);
}

bool Button::read(){
	const int current_sensor_value = digitalRead(this->pin);
	if(current_sensor_value == HIGH && (this->value == ButtonState_Untouched || this->value == ButtonState_Released)){
		this->value = ButtonState_Pressed;
		this->last_pressed_ms = millis();
		return true;
	}
	else if(current_sensor_value == HIGH && this->value == ButtonState_Pressed){
		const unsigned long current_ms = millis();
		constexpr unsigned long pressed_ms_threshold = 125;
		if (current_ms >= (this->last_pressed_ms + pressed_ms_threshold)){
			this->value = ButtonState_Held;
			return true;
		}
	}
	else if(current_sensor_value == LOW && (this->value == ButtonState_Pressed || this->value == ButtonState_Held)){
		this->value = ButtonState_Released;
		return true;
	}
	return false;
}


AnalogSensor::AnalogSensor(const uint8_t pin, const uint8_t id)
: 	Sensor(pin, id, 0)
{
    this->value = analogRead(pin);
    this->prev_value = this->value;
}

bool AnalogSensor::read(){
	this->prev_value = this->value;
	this->value = analogRead(this->pin);
	const int16_t delta_of_previous = this->value - this->prev_value;
	const bool delta_beyond_error_range = (delta_of_previous > this->analog_error_range) || (delta_of_previous < -this->analog_error_range);

	return delta_beyond_error_range;  
}

AnalogAsDigitalSensor::AnalogAsDigitalSensor(const uint8_t pin, const uint8_t id)
: Sensor(pin, id, LOW)
{
  constexpr int HIGH_adc_threshold = (1023 * 3) / 5;
  if(analogRead(this->pin) >= HIGH_adc_threshold)
    this->value = HIGH;
  else
    this->value = LOW;

  this->prev_value = this->value;
}

bool AnalogAsDigitalSensor::read(){
  constexpr int HIGH_adc_threshold = (1023*3) / 5;

  this->prev_value = this->value;

  const int read_adc = analogRead(this->pin);
  if(read_adc >= HIGH_adc_threshold) this->value = HIGH;
  else this->value = LOW;

  return this->prev_value != this->value;
}


RotaryEncoder::RotaryEncoder(const uint8_t pin_CLK, const uint8_t pin_DT, const uint8_t id)
:	Sensor(pin_CLK, id, 0),
	pin_CLK(pin_CLK),
	pin_DT(pin_DT)
{
	pinMode(this->pin_CLK, INPUT);
	pinMode(this->pin_DT, INPUT);
	
	this->prev_CLK_state = digitalRead(this->pin_CLK);
}

bool RotaryEncoder::read(){
	const int CLK_pin_state = digitalRead(this->pin_CLK);
	
	if(CLK_pin_state != this->prev_CLK_state){
		this->prev_CLK_state = CLK_pin_state;

		const int DT_pin_state = digitalRead(this->pin_DT);
		const bool clockwise_increment = CLK_pin_state == HIGH && DT_pin_state == LOW;
		const bool counterclockwise_increment = CLK_pin_state == HIGH && DT_pin_state == HIGH;
		
		if(clockwise_increment){
			this->value = +1;
			return true;
		}else if(counterclockwise_increment){
			this->value = -1;
			return true;
		}
	}
	return false;
}
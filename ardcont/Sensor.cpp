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
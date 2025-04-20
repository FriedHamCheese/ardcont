///\file SerialInterface.hpp

#ifndef SERIALINTERFACE_HPP
#define SERIALINTERFACE_HPP

#include "Sensor.hpp"
#include "GlobalStates.hpp"
#include "serial/serial.h"

/**
Waits for and reads lines from *arduino_serial*, keep track of the states of the sensors on the Arduino,
and dispatches actions according to the sensors.

This function will not immediately exit when global_states.requested_exit is true if SerialInterface holds a reference to ArdcontSerial,
but also requires the Arduino to output a line through serial to break the input wait of ArdcontSerial::readline().

This function initialises a thread of _translate_sensor_changes() to run in parallel with it to check changes of each Sensor,
since sensors may change their value while the execution of serial_listener() paused from waiting an input from *arduino_serial*.

Information, warning and errors are displayed through std::cout and std::cerr.

This function should be called as a thread.
*/
void serial_listener(serial::Serial& arduino_serial, GlobalStates& global_states) noexcept;

/**
A subfunction of serial_listener(). 

It acquires the access mutex of a Sensor in *sensors* and writes *sensor_value* to a Sensor which has its sensor ID matching *sensor_id*.

Returns false if unable to find the Sensor in *sensors* with the given *sensor_id* as its ID.
*/
bool _write_to_sensor(std::vector<std::unique_ptr<Sensor>>& sensors, 
						const std::int16_t sensor_id, const std::int16_t sensor_value);

/**
A subfunction of serial_listener(). 

Acquires the access mutex of a Sensor in *sensors* and reads the Sensor::value of a Sensor with a matching *sensor_id*.

Returns any a std::int16_t value from Sensor::value, std::nullopt if no Sensor with *sensor_id* is found.
*/
std::optional<std::int16_t> _read_sensor(std::vector<std::unique_ptr<Sensor>>& sensors, const::std::int16_t sensor_id);

/**
A subfunction of serial_listener(), ran as a different thread.

This function runs in parallel with serial_listener() to check for changes of the sensors while serial_listener() may be waiting for an input.

It continues to check through every Sensor in *sensors* whether any has changed its value or not, 
while serial_listener() waits for its serial input. 
This is seen with Button, where Button::value_changed() also checks for the duration since last button pressed.

This function displays information, warning and errors through std::cout and std::cerr.
*/
void _translate_sensor_changes(std::vector<std::unique_ptr<Sensor>>& sensors, GlobalStates& global_states);

#endif
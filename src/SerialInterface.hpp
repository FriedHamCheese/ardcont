///\file SerialInterface.hpp

#ifndef SERIALINTERFACE_HPP
#define SERIALINTERFACE_HPP

#include "Sensor.hpp"
#include "GlobalStates.hpp"
#include "serial/serial.h"

///An abstract class for interacting with a serial port.
class SerialInterface{
	public:
	///Set the serial port specified by its name in *port* to be used as the serial port for further input readings.
	virtual void setPort(const std::string &port) = 0;
	///Opens the set serial port. Call SerialInterface::setPort() first.
	virtual void open() = 0;
	///Returns true if the set and opened serial port is currently ready for use.
	virtual bool isOpen() = 0;
	///
	virtual size_t readline(std::string& buffer) = 0;
};

/**
The implementation to communicate with the arduino at 9600 baud through wjwwood's serial::Serial.
*/
class ArdcontSerial : public SerialInterface{
	public:
	ArdcontSerial() : _serial("", 9600){
		
	}
	
	/**
	* **May** throw InvalidConfigurationException,
	idk where its declaration is and when it would be thrown, blame wjwwood's serial D:
	*/
	void setPort(const std::string &port) override{
		this->_serial.setPort(port);
	}
	/**
	May throw:
	- std::invalid_argument	
	- serial::SerialExecption	
	- serial::IOException
	
	no clue for when they would, but ok
	*/
	void open() override{
		this->_serial.open();
	}

	bool isOpen() override{
		return this->_serial.isOpen();
	}

	size_t readline(std::string& buffer) override{
		return this->_serial.readline(buffer);
	}
	
	protected:
	serial::Serial _serial;
};

/*
class SerialMock : public SerialInterface{
	public:
	SerialMock();
	void setPort(const std::string&){
		
	}
	void open(){
		
	}
	bool isOpen(){
		return true;
	}
	size_t readline(std::string&){
		return 1;
	}
};
*/

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
void serial_listener(SerialInterface& arduino_serial, GlobalStates& global_states);

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
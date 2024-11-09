#ifndef serialinterface_hpp
#define serialinterface_hpp

#include "GlobalStates.hpp"
#include "serial/serial.h"

class SerialInterface{
	public:
	virtual void setPort(const std::string &port) = 0;
	virtual void open() = 0;
	virtual bool isOpen() = 0;
	virtual size_t readline(std::string& buffer, size_t size = 65536, std::string eol = "\n") = 0;
};

class ArdcontSerial : public SerialInterface{
	public:
	ArdcontSerial() : _serial("", 9600){
		
	}
	void setPort(const std::string &port){
		this->_serial.setPort(port);
	}
	void open(){
		this->_serial.open();
	}
	bool isOpen(){
		return this->_serial.isOpen();
	}
	size_t readline(std::string& buffer, size_t size = 65536, std::string eol = "\n"){
		return this->_serial.readline(buffer, size, eol);
	}
	
	protected:
	serial::Serial _serial;
};

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
	size_t readline(std::string&, size_t = 65536, std::string = "\n"){
		return 1;
	}
};

void serial_listener(SerialInterface& arduino_serial, GlobalStates& global_states);

#endif
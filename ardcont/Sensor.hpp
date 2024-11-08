#ifndef SENSOR_HPP
#define SENSOR_HPP

#include <stdint.h>

enum ButtonState : int16_t{
	ButtonState_Untouched,
	ButtonState_Pressed,
  
	ButtonState_Held,
	ButtonState_Released,
};

class Sensor{
  public:
  Sensor(const uint8_t pin, const uint8_t id, const int16_t init_value);
  virtual bool read() = 0;

  uint8_t id;
  int16_t value;

  protected:
  int16_t prev_value;
  uint8_t pin;
};


class DigitalSensor : public Sensor{
  public:
  DigitalSensor(const uint8_t pin, const uint8_t id);
  bool read() override;
};

class Button : public Sensor{
  public:
  Button(const uint8_t pin, const uint8_t id);
  bool read() override;  

  protected:
  unsigned long last_pressed_ms;
};

class AnalogSensor : public Sensor{
  protected:
  static constexpr int16_t analog_error_range = 2;

  public:
  AnalogSensor(const uint8_t pin, const uint8_t id);
  bool read() override;
};

class AnalogAsDigitalSensor : public Sensor{
  public:
  AnalogAsDigitalSensor(const uint8_t pin, const uint8_t id);
  bool read() override;
};

class RotaryEncoder : public Sensor{
	public:
	RotaryEncoder(const uint8_t pin_CLK, const uint8_t pin_DT, const uint8_t id);
	bool read() override;
	
	protected:
	int prev_CLK_state;
  uint8_t pin_CLK;
  uint8_t pin_DT;
};

#endif
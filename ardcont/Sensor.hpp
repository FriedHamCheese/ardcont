#ifndef SENSOR_HPP
#define SENSOR_HPP

#include <stdint.h>

class Sensor{
  public:
  Sensor(const uint8_t pin, const uint8_t id, const int16_t init_value);
  virtual bool read() = 0;

  uint8_t id;
  int16_t value;
  int16_t prev_value;

  protected:
  uint8_t pin;
};


class DigitalSensor : public Sensor{
  public:
  DigitalSensor(const uint8_t pin, const uint8_t id);
  bool read() override;
};

class AnalogSensor : public Sensor{
  protected:
  static constexpr int16_t analog_error_range = 4;

  public:
  AnalogSensor(const uint8_t pin, const uint8_t id);
  bool read() override;
};

#endif
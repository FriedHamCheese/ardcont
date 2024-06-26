#include <stdlib.h>
#include <string.h>

class Sensor{
  public:
  Sensor(const uint8_t pin, const uint8_t id, const int16_t init_value)
  : id(id), value(init_value), prev_value(init_value), pin(pin){
  }

  virtual bool read() = 0;

  uint8_t id;
  int16_t value;
  int16_t prev_value;

  protected:
  uint8_t pin;
};

class DigitalSensor : public Sensor{
  public:
  DigitalSensor(const uint8_t pin, const uint8_t id)
  : Sensor(pin, id, LOW){
    pinMode(this->pin, INPUT);
    this->value = digitalRead(this->pin);
    this->prev_value = this->value;
  }

  bool read() override{
    this->prev_value = this->value;

    this->value = digitalRead(this->pin);
    return this->value != this->prev_value;
  }
};

class AnalogSensor : public Sensor{
  protected:
  static constexpr int16_t analog_error_range = 4;

  public:
  AnalogSensor(const uint8_t pin, const uint8_t id)
  : Sensor(pin, id, 0){
    this->value = analogRead(pin);
    this->prev_value = this->value;
  }

  bool read() override{
    this->prev_value = this->value;
    this->value = analogRead(this->pin);
    const int16_t delta_of_previous = this->value - this->prev_value;
    const bool delta_beyond_error_range = (delta_of_previous > this->analog_error_range) || (delta_of_previous < -this->analog_error_range);

    return delta_beyond_error_range;  
  }
};

constexpr size_t sensor_count = 3;
static Sensor* sensors[sensor_count];

DigitalSensor play_pause_button(2, 0);
DigitalSensor idk_button(3, 1);
AnalogSensor volume_knob(7, 3);

void setup() {
  Serial.begin(9600);
  delay(2000);

  Serial.println("Ardcont project. Not intended to be used with Arduino IDE.");
  Serial.println("Sensor ID, type, range");
  Serial.println("0, pulldown-button, 0-1");
  Serial.println("1, pulldown-button, 0-1");
  Serial.println("3, potentiometer, 0-1023");
  Serial.println();

  sensors[0] = &play_pause_button;
  sensors[1] = &idk_button;
  sensors[2] = &volume_knob;
}

void loop() {
  for(size_t i = 0; i < sensor_count; i++){
    const bool value_changed = sensors[i]->read();
    if(value_changed){
      Serial.print(int16_t(sensors[i]->id));
      Serial.print(',');
      Serial.println(int16_t(sensors[i]->value));
    }
  }
  delay(25);
}

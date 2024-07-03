#include "Sensor.hpp"

#include <stdlib.h>
#include <string.h>

constexpr size_t sensor_count = 4;
static Sensor* sensors[sensor_count];

//Deck 1, right deck
DigitalSensor deck_1_play_pause_button(12, 0);

DigitalSensor deck_1_loop_in_button(10, 1);
DigitalSensor deck_1_loop_out_button(11, 2);

AnalogSensor deck_1_tempo_potentiometer(0, 3);

//RotaryEncoder deck_1_loop_interval(9, 8, 3);

//Deck 0, left deck

void setup() {
  Serial.begin(9600);
  delay(2000);

  Serial.println("Ardcont project. Not intended to be used with Arduino IDE.");
  Serial.println("Sensor ID, type, range:");
  Serial.println("0, pulldown-button, 0-1");
  Serial.println("1, pulldown-button, 0-1");
  Serial.println("2, pulldown-button, 0-1");
  Serial.println("3, potentiometer, 0-1023");
  Serial.println();

  sensors[0] = &deck_1_play_pause_button;
  sensors[1] = &deck_1_loop_in_button;
  sensors[2] = &deck_1_loop_out_button;
  sensors[3] = &deck_1_tempo_potentiometer;
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
  delay(12);
}

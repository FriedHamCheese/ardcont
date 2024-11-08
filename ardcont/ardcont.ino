#include "Sensor.hpp"

#include <stdlib.h>
#include <string.h>

constexpr size_t sensor_count = 12;
static Sensor* sensors[sensor_count];

Button left_play_pause_button(12, 0);
Button left_cue_button(11, 1);
AnalogSensor  left_tempo_potentiometer(0, 2);
RotaryEncoder left_loop_interval_rotaryenc(8, 7, 3);
Button left_loop_in_button(9, 4);
Button left_loop_out_button(10, 5);

Button right_play_pause_button(13, 6);
AnalogAsDigitalSensor right_cue_button(1, 7);
AnalogSensor right_tempo_potentiometer(2, 8);
RotaryEncoder right_loop_interval_rotaryenc(4, 3, 9);
Button right_loop_in_button(2, 10);
AnalogAsDigitalSensor right_loop_out_button(3, 11);


void setup() {
  Serial.begin(9600);
  delay(2000);

  Serial.println("Ardcont project, 2024.");

  sensors[0] = &left_play_pause_button;
  sensors[1] = &left_cue_button;
  sensors[2] = &left_tempo_potentiometer;
  sensors[3] = &left_loop_interval_rotaryenc;
  sensors[4] = &left_loop_in_button;
  sensors[5] = &left_loop_out_button;

  sensors[6] = &right_play_pause_button;
  sensors[7] = &right_cue_button;
  sensors[8] = &right_tempo_potentiometer;
  sensors[9] = &right_loop_interval_rotaryenc;
  sensors[10] = &right_loop_in_button;
  sensors[11] = &right_loop_out_button;  

  Serial.println();
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

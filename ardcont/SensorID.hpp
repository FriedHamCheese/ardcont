#ifndef SENSORID_HPP
#define SENSORID_HPP

enum SensorID : int16_t{
  SensorID_left_playpause_button,
  SensorID_left_cue_button,
  SensorID_left_tempo_poten,
  SensorID_left_jogdial_rotaryenc,
  SensorID_left_loop_in_button,
  SensorID_left_loop_out_button,

  SensorID_right_playpause_button = 25,
  SensorID_right_cue_button,
  SensorID_right_tempo_poten,
  SensorID_right_jogdial_rotaryenc,
  SensorID_right_loop_in_button,
  SensorID_right_loop_out_button,
};

#endif
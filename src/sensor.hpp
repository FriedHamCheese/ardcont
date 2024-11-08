#ifndef sensor_hpp
#define sensor_hpp

#include <cstdint>

enum SensorID{
	SensorID_left_playpause_button,
	SensorID_left_cue_button,	
	SensorID_left_tempo_poten,
	SensorID_left_loop_duration_rotaryenc,
	SensorID_left_loop_in_button,
	SensorID_left_loop_out_button,
	
	SensorID_right_playpause_button,
	SensorID_right_cue_button,	
	SensorID_right_tempo_poten,
	SensorID_right_loop_duration_rotaryenc,
	SensorID_right_loop_in_button,
	SensorID_right_loop_out_button
};

enum ButtonState{
	ButtonState_Untouched,
	ButtonState_Pressed,
  
	ButtonState_Held,
	ButtonState_Released,
};

constexpr int potentiometer_centre_value = 512;
constexpr float potentiometer_max_range_from_1x = 0.125;
constexpr float potentiometer_value_for_max_range = (float)potentiometer_centre_value / potentiometer_max_range_from_1x;

constexpr int_fast64_t max_loop_duration_2_exponent = 5;
constexpr int_fast64_t min_loop_duration_2_exponent = -5;		

#endif
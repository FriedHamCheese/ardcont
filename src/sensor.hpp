///\file Sensor.hpp

#ifndef SENSOR_HPP
#define SENSOR_HPP

#include <cstdint>
#include <chrono>
#include <mutex>

///A struct for keeping track of a particular sensor.
struct Sensor{
	Sensor(const std::int16_t sensor_id, const std::uint16_t initial_value = 0) noexcept;
	///Sets Sensor::value to *value*.
	virtual void write(const std::int16_t value) noexcept;
	///Refreshes the state of a sensor and check whether its state has changed or not.
	///Returns true if the Sensor has changed its value.
	virtual bool value_changed() noexcept;

	const std::int16_t sensor_id;
	std::int16_t value;
	
	///A mutex for accessing any of the members or doing any method calls.
	std::mutex access_mutex;
	
	protected:
	std::int16_t prev_value;
};

enum ButtonState : int16_t{
	///A placeholder state for a Button not being written to.
	///This state does not indicate a button being pressed, held or released, just unknown.
	ButtonState_Untouched,
	
	///The Button was pressed after being in a released state.
	ButtonState_Pressed,
	///The Button has continued to be held in a pressed state for (Button::pressed_to_held_duration_ms).
	ButtonState_Held,
	///The Button is released after being pressed or held.
	ButtonState_Released
};

///A specific implementation for keeping track of a button, since button can be pressed, held and released.
struct Button : public Sensor{
	///Initialises both Button::value and Button::prev_value with ButtonState_Untouched.
	Button(const std::int16_t sensor_id) noexcept;
	/**
	If *value* is 1 (pressed), and Button::value is ButtonState_Untouched or ButtonState_Released;
	set Button::value to ButtonState_Pressed and set Button::last_pressed to now.
	
	If *value* is 0 (released), and Button::value is ButtonState_Pressed or ButtonState_Held;
	set Button::value to ButtonState_Pressed.
	*/
	void write(const std::int16_t value) noexcept override;
	
	/**
	Returns true if Button::value has changed, or if the Button is still being held to (Button::pressed_to_held_duration_ms).
	*/
	bool value_changed() noexcept override;
	
	static constexpr std::uint32_t pressed_to_held_duration_ms = 125;
	
	private:
	std::chrono::time_point<std::chrono::steady_clock> last_pressed;
};

/**
A specific implementation of Sensor for a rotary encoder, 
since the Arduino will send multiple 1's (or -1's) to indicate multiple rotations to a single direction.
The Sensor::value_changed() will only return true if the recieved value is not equal to its previous value.

The write() function is overriden to conform with Sensor::value_changed().
*/
struct RotaryEncoder : public Sensor{
	RotaryEncoder(const std::int16_t sensor_id) noexcept;
	/**
	Sets RotaryEncoder::value to *value* 
	and sets RotaryEncoder::prev_value to 0 if *value* is equal to RotaryEncoder::prev_value to indicate a different instance of input.
	*/
	void write(const std::int16_t value) noexcept override;
};

inline constexpr std::int16_t potentiometer_centre_value = 480;
inline constexpr float max_delta_ratio_from_1x = 0.125;
inline constexpr float potentiometer_value_for_max_range = potentiometer_centre_value / max_delta_ratio_from_1x;

#endif
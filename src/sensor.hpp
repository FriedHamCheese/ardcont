#ifndef SENSOR_HPP
#define SENSOR_HPP

#include <cstdint>
#include <chrono>
#include <mutex>

struct Sensor{
	Sensor(const std::int16_t sensor_id, const std::uint16_t initial_value = 0) noexcept;
	virtual void write(const std::int16_t value) noexcept;
	virtual bool value_changed() noexcept;

	const std::int16_t sensor_id;
	std::int16_t value;
	
	std::mutex access_mutex;
	
	protected:
	std::int16_t prev_value;
};

enum ButtonState : int16_t{
	ButtonState_Untouched,

	ButtonState_Pressed,
	ButtonState_Held,
	ButtonState_Released
};

struct Button : public Sensor{
	Button(const std::int16_t sensor_id) noexcept;
	void write(const std::int16_t value) noexcept override;
	bool value_changed() noexcept override;
	
	static constexpr std::uint32_t pressed_to_held_duration_ms = 125;
	
	private:
	std::chrono::time_point<std::chrono::steady_clock> last_pressed;
};

struct RotaryEncoder : public Sensor{
	RotaryEncoder(const std::int16_t sensor_id) noexcept;
	void write(const std::int16_t value) noexcept override;
};

inline constexpr std::int16_t potentiometer_centre_value = 480;
inline constexpr float max_delta_ratio_from_1x = 0.125;
inline constexpr float potentiometer_value_for_max_range = potentiometer_centre_value / max_delta_ratio_from_1x;

#endif
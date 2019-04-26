#pragma once
#include <modm/platform.hpp>
#include <common/board.hpp>

namespace timekeeping {

using Timer = modm::platform::Timer5;
constexpr uint32_t timerClock = ClockConfiguration::Timer5;
constexpr uint32_t timerFrequency = 1000000;

inline void initTimer() {
	Timer::enable();
	Timer::setPrescaler(timerClock / timerFrequency);
	Timer::setMode(modm::platform::GeneralPurposeTimer::Mode::UpCounter);
	Timer::applyAndReset();
	Timer::start();
}

/**
 * Set the time in microseconds.
 */
inline void setTimer(uint32_t time) {
	Timer::setValue(time);
}

/**
 * Get the time in microseconds.
 */
inline uint32_t now() {
	return Timer::getValue();
}

}
#include "board.hpp"
#include <modm/architecture/interface/clock.hpp>
#include <modm/debug/logger.hpp>

modm::IODeviceWrapper<SerialDebug, modm::IOBuffer::BlockIfFull> serialDevice;
modm::log::Logger modm::log::debug(serialDevice);
modm::log::Logger modm::log::info(serialDevice);
modm::log::Logger modm::log::warning(serialDevice);
modm::log::Logger modm::log::error(serialDevice);

modm::log::Logger serOut(serialDevice);

using namespace modm::literals;

bool ClockConfiguration::enable() {
	// modm::platform::ClockControl can be accessed as only ClockControl
	using namespace modm::platform;

	//Rcc::enableExternalClock();
	Rcc::enableInternalClock(); //16 MHz
	Rcc::enablePll(
		ClockControl::PllSource::InternalClock,
		16,      // 16MHz / N=16 -> 1MHz   !!! Must be 1 MHz for PLLSAI !!!
		336,    // 1MHz * M=336 -> 336MHz
		4,      // 336MHz / P=4 -> 84MHz = F_cpu
		7       // 336MHz / Q=7 -> 48MHz (value ignored! PLLSAI generates 48MHz for F_usb)
	);
	Rcc::setFlashLatency<Frequency>();
	Rcc::enableSystemClock(Rcc::SystemClockSource::Pll);

	//Rcc::setFlashLatency<Frequency>();
	Rcc::setAhbPrescaler(Rcc::AhbPrescaler::Div1);
	Rcc::setApb1Prescaler(Rcc::Apb1Prescaler::Div2);
	Rcc::setApb2Prescaler(Rcc::Apb2Prescaler::Div1);

	Rcc::updateCoreFrequency<Frequency>();

	return true;
}

void initCommon() {
	using namespace modm::platform;

	ClockConfiguration::enable();
	modm::cortex::SysTickTimer::initialize<ClockConfiguration>();

	Led::setOutput(modm::Gpio::Low);

	SerialDebug::connect<SerialDebugTx::Tx, SerialDebugRx::Rx>();
	SerialDebug::initialize<ClockConfiguration, 1000_kBd>(15);
}

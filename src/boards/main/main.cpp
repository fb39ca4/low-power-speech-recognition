/*
 * Copyright (c) 2013, Kevin Läufer
 * Copyright (c) 2013-2018, Niklas Hauser
 * Copyright (c) 2014, Sascha Schade
 *
 * This file is part of the modm project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
// ----------------------------------------------------------------------------
#include <type_traits>
#include <array>

#include "arm_math.h"

#include <modm/architecture/interface/delay.hpp>
#include <modm/debug/logger.hpp>
#include <modm/platform.hpp>
#include <modm/processing/timer.hpp>

#include <common/board.hpp>
#include <common/blink_thread.hpp>

#include "hanning_window.hpp"

// Set the log level
#undef	MODM_LOG_LEVEL
#define	MODM_LOG_LEVEL modm::log::INFO

using MicrophoneInput = modm::platform::GpioInputA0;
using Adc = modm::platform::Adc1;
using AdcInterrupt = modm::platform::AdcInterrupt1;

volatile int conversionsComplete = 0;

constexpr int sampleBufferSize = 512;
constexpr int frameSize = sampleBufferSize * 2;
constexpr int windowSize = frameSize / 2;


namespace AdcInterruptHandler {

	static constexpr int oversampleRatio = 16;

	namespace {
		constexpr int numBuffers = 3;
		volatile uint16_t buffer[numBuffers][sampleBufferSize];
		int bufferIdx;
		volatile uint16_t* volatile availableBuffer;
		size_t bufferPos;
		uint32_t oversampleAccumulator;
		uint32_t oversampleRemaining;
	}

	void handler() {
		if (Adc::getInterruptFlags() &  Adc::InterruptFlag::EndOfRegularConversion) {
			Adc::acknowledgeInterruptFlags(Adc::InterruptFlag::EndOfRegularConversion);
			uint16_t reading = Adc::getValue();
			oversampleAccumulator += reading;
			oversampleRemaining -= 1;
			if (oversampleRemaining == 0) {
				oversampleRemaining = oversampleRatio;
				buffer[bufferIdx][bufferPos++] = (oversampleAccumulator / oversampleRatio);
				oversampleAccumulator = 0;
				if (bufferPos == sampleBufferSize) {
					bufferPos = 0;
					availableBuffer = buffer[bufferIdx++];
					bufferIdx = (bufferIdx < numBuffers) ? bufferIdx : 0;
				}
			}
			conversionsComplete += 1;
		}
		//Adc::acknowledgeInterruptFlags(Adc::InterruptFlag::All);
	}

	void initialize() {
		Adc::enableInterruptVector(0);
		Adc::enableInterrupt(Adc::Interrupt::EndOfRegularConversion);
		AdcInterrupt::attachInterruptHandler(handler);
		bufferPos = 0;
		oversampleAccumulator = 0;
		oversampleRemaining = oversampleRatio;
		availableBuffer = nullptr;
		bufferIdx = 0;
	}

	const volatile uint16_t* getData() {
		decltype(availableBuffer) retval;
		if (availableBuffer != nullptr) {
			retval = availableBuffer;
			availableBuffer = nullptr;
		}
		else {
			retval = nullptr;
		}
		return const_cast<const volatile uint16_t*>(retval);
	}

	const volatile uint16_t* getDataBlocking() {
		while (availableBuffer == nullptr);
		decltype(availableBuffer) retval = availableBuffer;
		availableBuffer = nullptr;
		return const_cast<const volatile uint16_t*>(retval);
	}
};


float sampleFrame[frameSize];
float fftFrame[frameSize];
float spectrumPowerSquared[frameSize / 2];
arm_rfft_fast_instance_f32 fftInstance;

int main(void) {
	initCommon();

	MODM_LOG_DEBUG << "main: debug logging here" << modm::endl;
	MODM_LOG_INFO << "main: info logging here" << modm::endl;
	MODM_LOG_WARNING << "main: warning logging here" << modm::endl;
	MODM_LOG_ERROR << "main: error logging here" << modm::endl;

	for (int i = 0; i < 10; i++) {
		Led::set();
		modm::delayMilliseconds(50);
		Led::reset();
		modm::delayMilliseconds(50);
	}


	MicrophoneInput::setInput(modm::platform::Gpio::InputType::PullUp);
	Adc::connect<MicrophoneInput::In0>();
	Adc::initialize<ClockConfiguration, ClockConfiguration::Adc / 8>();
	Adc::setPinChannel<MicrophoneInput>(Adc::SampleTime::Cycles15);

	AdcInterruptHandler::initialize();

	Adc::enableFreeRunningMode();
	Adc::startConversion();

	// while (1)
	// {
	// 	uint32_t total = 0;
	// 	int i = 0;
	// 	modm::ShortTimeout timeout;
	// 	for (timeout.restart(1000); !timeout.isExpired(); i++) {
	// 		// wait for conversion to finish
	// 		while (!Adc::isConversionFinished());
	// 		// print result
	// 		total += Adc::getValue();
	// 	}
	// 	MODM_LOG_INFO << total << " " << i << modm::endl;
	// }

	// while (true) {
	// 	while (!Adc::isConversionFinished());
	// 	MODM_LOG_INFO << Adc::getValue() << " 0 4096" << modm::endl;
	// }


	const uint16_t* prevSamples;
	const uint16_t* currentSamples = const_cast<const uint16_t*>(AdcInterruptHandler::getDataBlocking());
	modm::ShortPeriodicTimer powerSpectrumTimer(50);
	int frames = 0;

	while (1) {
		prevSamples = currentSamples;
		currentSamples = const_cast<const uint16_t*>(AdcInterruptHandler::getDataBlocking());

		int32_t sampleSum = 0;
		for (int i = 0; i < sampleBufferSize; i++) {
			sampleSum += prevSamples[i];
		}

		for (int i = 0; i < sampleBufferSize; i++) {
			sampleSum += currentSamples[i];
		}

		int16_t dcOffset = sampleSum / frameSize;

		for (int i = 0; i < sampleBufferSize; i++) {
			sampleFrame[i] = hanningWindow[i] * (prevSamples[i] - dcOffset) / 512.0;
		}

		for (int i = 0; i < sampleBufferSize; i++) {
			sampleFrame[i + sampleBufferSize] = hanningWindow[sampleBufferSize - i - 1] * (currentSamples[i] - dcOffset) / 512.0;
		}

		arm_rfft_fast_init_f32(&fftInstance, frameSize);
		arm_rfft_fast_f32(&fftInstance, sampleFrame, fftFrame, 0);
		arm_cmplx_mag_squared_f32(fftFrame, spectrumPowerSquared, frameSize / 2);


		frames += 1;
		if (timer.execute()) {
			MODM_LOG_INFO << "spec_pwr:";
			for (int i = 0; i < frameSize / 2; i++) {
				MODM_LOG_INFO << static_cast<int>(spectrumPowerSquared[i]) << " ";
			}
			MODM_LOG_INFO << modm::endl;
			//MODM_LOG_INFO << frames << modm::endl;
			frames = 0;

		}

	}

	while(1);


	//BlinkThread blinkThread;
	//while (1) {
	//	blinkThread.run();
	//}
	return 0;
}

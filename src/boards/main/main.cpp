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
#include <cassert>
#include <atomic>
#include <algorithm>
#include <numeric>

#include "arm_math.h"

#include <modm/architecture/interface/delay.hpp>
#include <modm/debug/logger.hpp>
#include <modm/platform.hpp>
#include <modm/processing/timer.hpp>

#include <common/board.hpp>
#include <common/timekeeping.hpp>

#include "mfcc.hpp"

// Set the log level
#undef	MODM_LOG_LEVEL
#define	MODM_LOG_LEVEL modm::log::INFO

using MicrophoneInput = modm::platform::GpioInputA0;
using Adc = modm::platform::Adc1;
using AdcInterrupt = modm::platform::AdcInterrupt1;

constexpr int adcSampleRate = ((ClockConfiguration::Adc / 8.0) / (56 + 12)) / 16;
constexpr int windowStride = 256;
constexpr int windowSize = windowStride * 2;

HannWindow<windowSize> fftWindowingLut;

constexpr int numMelCoefficients = 16;

constexpr mfcc::MelFilterLut<numMelCoefficients, 0, 1500, windowSize, adcSampleRate> melFilterLut;
constexpr DiscreteCosineTransformTable<16> dctTable;

std::array<uint16_t, windowSize> rawSamples;
std::array<float, windowSize> normalizedSamples;
std::array<float, windowSize> fftSamples;
std::array<float, windowSize / 2> spectrumPowerSquared;
float melPower[numMelCoefficients];
float melCepstrum[numMelCoefficients];
arm_rfft_fast_instance_f32 fftInstance;



namespace AdcInterruptHandler {

	static constexpr int oversampleRatio = 16;

	namespace {
		constexpr int numBuffers = 3;
		volatile uint16_t buffer[numBuffers][windowStride];
		int bufferIdx;
		std::atomic<volatile uint16_t*> availableBuffer = nullptr;
		size_t bufferPos;
		uint32_t oversampleAccumulator;
		uint32_t oversampleRemaining;
	}

	std::atomic<int> samplesComplete = 0;

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
				if (bufferPos == windowStride) {
					bufferPos = 0;
					availableBuffer = buffer[bufferIdx++];
					bufferIdx = (bufferIdx < numBuffers) ? bufferIdx : 0;
				}
				samplesComplete += 1;
			}
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

	const uint16_t* getData() {
		return const_cast<const uint16_t*>(availableBuffer.exchange(nullptr));
	}

	const uint16_t* getDataBlocking() {
		const uint16_t* retval;
		while ((retval = getData()) == nullptr);
		return const_cast<const uint16_t*>(retval);
	}
};

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
	Adc::setPinChannel<MicrophoneInput>(Adc::SampleTime::Cycles56);

	AdcInterruptHandler::initialize();

	Adc::enableFreeRunningMode();
	Adc::startConversion();

	timekeeping::initTimer();

	const uint16_t* prevSamples;
	const uint16_t* currentSamples = const_cast<const uint16_t*>(AdcInterruptHandler::getDataBlocking());
	modm::ShortPeriodicTimer powerSpectrumTimer(100);
	modm::ShortPeriodicTimer framesPerSecondTimer(1000);
	int frames = 0;
	arm_rfft_fast_init_f32(&fftInstance, windowSize);

	int wordLengthCounter = 0;
	int maxQuietGap = 5;
	int quietGapCounter = 0;
	float amplitudeThreshold = 0.01;

	while (1) {
		const uint16_t* newSamples = AdcInterruptHandler::getDataBlocking();

		uint32_t startTime = timekeeping::now();

		// Shift old samples and copy new samples to buffer
		std::copy(rawSamples.begin() + windowStride, rawSamples.end(), rawSamples.begin());
		std::copy(currentSamples, currentSamples + windowStride, rawSamples.end() - windowStride);

		uint32_t copyTime = timekeeping::now();

		auto sampleSum = std::accumulate(rawSamples.begin(), rawSamples.end(), uint32_t(0));
		// Make sure the sum was to a uint32_t to prevent overflow
		static_assert(std::is_same<decltype(sampleSum), uint32_t>::value);
		float dcOffset = static_cast<float>(sampleSum) / windowSize;

		uint32_t averagingTime = timekeeping::now();

		float power = 0;
		for (int i = 0; i < windowSize; i++) {
			float normalizedSample = fftWindowingLut[i] * (currentSamples[i] - dcOffset) / 512.0;
			//maxAmplitude = std::max(std::abs(normalizedSample), maxAmplitude);
			power += normalizedSample * normalizedSample;
			normalizedSamples[i] = normalizedSample;
		}
		float rmsAmplitude = std::sqrt(power / windowSize);

		uint32_t normalizationTime = timekeeping::now();

		if (rmsAmplitude > amplitudeThreshold) {
			Led::set();
			wordLengthCounter += 1;
			quietGapCounter = maxQuietGap;
		}
		else {
			Led::reset();
			if (wordLengthCounter > 0) {
				quietGapCounter -= 1;
				wordLengthCounter += 1;
				if (quietGapCounter == 0) {
					MODM_LOG_INFO << "msg:word length: " << wordLengthCounter - maxQuietGap << modm::endl;
					wordLengthCounter = 0;
				}
			}
		}

		uint32_t fftTime, magTime, melFilterTime, dctTime;

		arm_rfft_fast_f32(&fftInstance, normalizedSamples.data(), fftSamples.data(), 0);
		fftTime = timekeeping::now();
		arm_cmplx_mag_squared_f32(fftSamples.data(), spectrumPowerSquared.data(), windowSize / 2);
		magTime = timekeeping::now();

		float totalPower = 0.0f;
		for (int i = 1; i <= numMelCoefficients; i++) {
			float melFilterPower = melFilterLut.evaluate(spectrumPowerSquared, i);
			totalPower += melFilterPower;
			melPower[i - 1] = std::log2f(melFilterPower);
		}
		melFilterTime = timekeeping::now();
		for (int i = 0; i < numMelCoefficients; i++) {
			melCepstrum[i] = dctTable.evaluate(melPower, i);
		}
		dctTime = timekeeping::now();
		frames += 1;

		if (framesPerSecondTimer.execute()) {

			MODM_LOG_INFO << "stat:fps:" << frames << " samplerate:" << AdcInterruptHandler::samplesComplete.exchange(0);
			MODM_LOG_INFO << " copy:" << copyTime - startTime;
			MODM_LOG_INFO << " avging:" << averagingTime - copyTime;
			MODM_LOG_INFO << " normal:" << normalizationTime - averagingTime;
			MODM_LOG_INFO << " fft:" << fftTime - normalizationTime;
			MODM_LOG_INFO << " mag:" << magTime - fftTime;
			MODM_LOG_INFO << " mel:" << melFilterTime - magTime;
			MODM_LOG_INFO << " dct:" << dctTime - melFilterTime;
			MODM_LOG_INFO << " at:" << rmsAmplitude;
			MODM_LOG_INFO << "         \r";
			frames = 0;
		}

		if (powerSpectrumTimer.execute()) {
			/*MODM_LOG_INFO << "raw:";
			for (int i = 0; i < windowSize; i++) {
				MODM_LOG_INFO << rawSamples[i] << " ";
			}
			MODM_LOG_INFO << modm::endl;*/

			/*MODM_LOG_INFO << "spec:";
			for (int i = 0; i < windowSize / 2; i++) {
				MODM_LOG_INFO << static_cast<int>(spectrumPowerSquared[i]) << " ";
			}
			MODM_LOG_INFO << modm::endl;*/

		}

		if (true) {
			MODM_LOG_INFO << "mfcc:";
			MODM_LOG_INFO << rmsAmplitude << " ";
			for (int i = 1; i < numMelCoefficients; i++) {
				MODM_LOG_INFO << melCepstrum[i] << " ";
			}
			MODM_LOG_INFO << modm::endl;
		}

	}

	while(1);


	//BlinkThread blinkThread;
	//while (1) {
	//	blinkThread.run();
	//}
	return 0;
}

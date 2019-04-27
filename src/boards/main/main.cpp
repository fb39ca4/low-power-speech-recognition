#include <type_traits>
#include <array>
#include <cassert>
#include <atomic>
#include <algorithm>
#include <numeric>

#include "arm_math.h"

#include <modm/architecture/interface/delay.hpp>
#include <modm/platform.hpp>
#include <modm/processing/timer.hpp>

#include <common/board.hpp>
#include <common/timekeeping.hpp>

#include "transform.hpp"
#include "mfcc.hpp"
#include "dtw.hpp"

// Hardware definitions
using MicrophoneInput = modm::platform::GpioInputA0;
using Adc = modm::platform::Adc1;
using AdcInterrupt = modm::platform::AdcInterrupt1;

// Sampling and FFT parameters
constexpr int oversampleRatio = 16;
constexpr int sampleRate = 12800; // ADC settings must be manually changed to match this
constexpr int windowStride = 256;
constexpr int windowSize = windowStride * 2;

// MFCC parameters
constexpr int numMelCoefficients = 16;
constexpr int featureVectorFirstCoefficient = 2;
constexpr int featureVectorLastCoefficient = 9;
constexpr int featureVectorDim = featureVectorLastCoefficient - featureVectorFirstCoefficient;
constexpr int maxWords = 64;

// Lookup tables
constexpr HannWindow<windowSize> fftWindowingLut;
constexpr mfcc::MelFilterLut<numMelCoefficients, 0, 3000, windowSize, sampleRate> melFilterLut;
constexpr DiscreteCosineTransformTable<16> dctLut;

// Buffers
std::array<uint16_t, windowSize> rawSamples;
std::array<float, windowSize> normalizedSamples;
std::array<float, windowSize> fftSamples;
std::array<float, windowSize / 2> spectrumPower;
std::array<float, numMelCoefficients> melPower;
std::array<float, numMelCoefficients> melCepstrum;
using FeatureVector = std::array<float, featureVectorDim>;
FeatureVector featureVector;
std::array<FeatureVector, maxWords> wordBuffer;
std::array<uint32_t, 20> dtwResults;

// Necessary for ARM FFT function
arm_rfft_fast_instance_f32 fftSettings;

// Dynamic time warping object
Dtw<64, FeatureVector> dtwWorkspace;

// We must specify a distance metric for each type used with the DTW algorithm
template<>
uint32_t dtw::distanceMetric(const FeatureVector& a, const FeatureVector& b) {
	float magnitudeSquared = 0;
	for (size_t i = 0; i < std::tuple_size<FeatureVector>::value; i++) {
		magnitudeSquared += (b[i] - a[i]) * (b[i] - a[i]);
	}
	return std::sqrt(magnitudeSquared) * 65536.0;
}

// Voice command data in another file
struct VoiceCommandEntry {
	const char* text;
	const FeatureVector* featureVectors;
	int numFeatureVectors;
};

extern const VoiceCommandEntry voiceCommands[];
extern int numVoiceCommands;

namespace AdcInterruptHandler {

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
		bufferPos = 0;
		oversampleAccumulator = 0;
		oversampleRemaining = oversampleRatio;
		availableBuffer = nullptr;
		bufferIdx = 0;
		Adc::enableInterruptVector(0);
		Adc::enableInterrupt(Adc::Interrupt::EndOfRegularConversion);
		AdcInterrupt::attachInterruptHandler(handler);
		Adc::enableFreeRunningMode();
		Adc::startConversion();
	}

	// If there is a fresh buffer of samples ready, returns a pointer to it
	// otherwise returns nullptr.
	const uint16_t* getBuffer() {
		return const_cast<const uint16_t*>(availableBuffer.exchange(nullptr));
	}

	// Blocks until a buffer is ready,
	const uint16_t* getBufferBlocking() {
		const uint16_t* retval;
		while ((retval = getBuffer()) == nullptr);
		return const_cast<const uint16_t*>(retval);
	}
};

int main(void) {
	initCommon();

	for (int i = 0; i < 10; i++) {
		Led::set();
		modm::delayMilliseconds(50);
		Led::reset();
		modm::delayMilliseconds(50);
	}

	// Configure ADC
	MicrophoneInput::setInput(modm::platform::Gpio::InputType::PullUp);
	Adc::connect<MicrophoneInput::In0>();
	Adc::initialize<ClockConfiguration, ClockConfiguration::Adc / 8>();
	Adc::setPinChannel<MicrophoneInput>(Adc::SampleTime::Cycles56);

	// Attach interrupt and start ADC
	AdcInterruptHandler::initialize();

	// Timer for performance information
	timekeeping::initTimer();

	// Initialize FFT settings
	arm_rfft_fast_init_f32(&fftSettings, windowSize);

	modm::ShortPeriodicTimer powerSpectrumTimer(100);
	modm::ShortPeriodicTimer framesPerSecondTimer(1000);
	int frames = 0;

	int wordLength = 0;
	constexpr int maxQuietGap = 5;
	int quietGapCounter = 0;
	float amplitudeThreshold = 0.01;

	uint32_t startTime = 0;
	uint32_t copyTime = 0;
	uint32_t averagingTime = 0;
	uint32_t normalizationTime = 0;
	uint32_t tresholdTime = 0;

	uint32_t fftStartTime = 0;
	uint32_t fftTime = 0;
	uint32_t magTime = 0;
	uint32_t melFilterTime = 0;
	uint32_t dctTime = 0;
	uint32_t featureScalingTime = 0;

	uint32_t dtwStartTime = 0;
	uint32_t dtwTime = 0;

	float rmsAmplitude = 0.0;

	while (1) {
		const uint16_t* newSamples;
		if ((newSamples = AdcInterruptHandler::getBuffer()) != nullptr) {

			startTime = timekeeping::now();

			// Shift old samples and copy new samples to buffer
			std::copy(rawSamples.begin() + windowStride, rawSamples.end(), rawSamples.begin());
			std::copy(newSamples, newSamples + windowStride, rawSamples.end() - windowStride);

			copyTime = timekeeping::now();

			// Take the mean of the samples so it can be subtracted during normalization
			auto sampleSum = std::accumulate(rawSamples.begin(), rawSamples.end(), uint32_t(0));
			// Make sure the sum was to a uint32_t to prevent overflow
			static_assert(std::is_same<decltype(sampleSum), uint32_t>::value);
			float sampleMean = static_cast<float>(sampleSum) / windowSize;

			averagingTime = timekeeping::now();

			// Normalize the samples approximately to the range [-1, 1]
			// At the same time, take the root-mean-squared amplitude
			float power = 0;
			for (int i = 0; i < windowSize; i++) {
				float normalizedSample = fftWindowingLut[i] * (rawSamples[i] - sampleMean) / 512.0;
				//maxAmplitude = std::max(std::abs(normalizedSample), maxAmplitude);
				power += normalizedSample * normalizedSample;
				normalizedSamples[i] = normalizedSample;
			}
			rmsAmplitude = std::sqrt(power / windowSize);

			normalizationTime = timekeeping::now();

			// Decide if a word might be spoken from the amplitude
			bool wordFinished = false;
			bool computeFeatureVector = false;

			if (rmsAmplitude > amplitudeThreshold) {
				Led::set();
				wordLength += 1;
				quietGapCounter = maxQuietGap;
			}
			else {
				Led::reset();
				if (wordLength > 0) {
					quietGapCounter -= 1;
					wordLength += 1;
					if (quietGapCounter == 0) {
						wordLength -= maxQuietGap;
						wordFinished = true;
					}
				}
			}

			if (!wordFinished && wordLength > 0 && wordLength <= maxWords) {
				computeFeatureVector = true;
			}

			tresholdTime = timekeeping::now();

			if (true) {
				fftStartTime = timekeeping::now();
				// Take the FFT of the data
				arm_rfft_fast_f32(&fftSettings, normalizedSamples.data(), fftSamples.data(), 0);

				fftTime = timekeeping::now();

				// Take the magnitude squared (power) of the complex-valued FFT output
				arm_cmplx_mag_squared_f32(fftSamples.data(), spectrumPower.data(), windowSize / 2);

				magTime = timekeeping::now();

				// Run the power spectrum through the mel filterbank
				float totalPower = 0.0f;
				for (int i = 1; i <= numMelCoefficients; i++) {
					float melFilterPower = melFilterLut.evaluate(spectrumPower, i);
					totalPower += melFilterPower;
					melPower[i - 1] = std::log2f(melFilterPower);
				}

				melFilterTime = timekeeping::now();

				// Take the DCT of the mel spectrum power
				for (int i = 0; i < numMelCoefficients; i++) {
					melCepstrum[i] = dctLut.evaluate(melPower, i);
				}

				dctTime = timekeeping::now();

				// Keep only certain terms from the DCT, and rescale them to length ln(originalMagnitude - 1)
				float featureVectorScaling = 0.0;
				for (int i = featureVectorFirstCoefficient; i < featureVectorLastCoefficient; i++) {
					featureVectorScaling += melCepstrum[i] * melCepstrum[i];
				}
				featureVectorScaling = std::sqrt(featureVectorScaling);
				featureVectorScaling = std::log(featureVectorScaling + 1.0) / featureVectorScaling;
				for (int i = featureVectorFirstCoefficient; i < featureVectorLastCoefficient; i++) {
					featureVector[i - featureVectorFirstCoefficient] = melCepstrum[i] * featureVectorScaling;
				}

				featureScalingTime = timekeeping::now();
			}

			if (computeFeatureVector) {
				wordBuffer[wordLength - 1] = featureVector;
			}

			if (wordFinished) {
				serOut << "msg: " << wordLength << modm::endl;
				serOut << "msg:word length: " << wordLength << modm::endl;

				dtwStartTime = timekeeping::now();
				for (int i = 0; i < numVoiceCommands; i++) {
					dtwResults[i] = dtwWorkspace.compare(
						voiceCommands[i].featureVectors, voiceCommands[i].numFeatureVectors,
						wordBuffer.data(), wordLength
					) / voiceCommands[i].numFeatureVectors;
				}
				dtwTime = timekeeping::now();

				uint32_t bestMatch = std::numeric_limits<uint32_t>::max();
				int bestMatchIdx = -1;
				for (int i = 0; i < numVoiceCommands; i++) {
					if (bestMatch >= dtwResults[i]) {
						bestMatch = dtwResults[i];
						bestMatchIdx = i;
					}
				}

				if (bestMatchIdx >= 0) {
					serOut << "msg:word: " << voiceCommands[bestMatchIdx].text << modm::endl;
				}

				for (int i = 0; i < numVoiceCommands; i++) {
					serOut << "msg:dtw: " << voiceCommands[i].text << ", "<< dtwResults[i] << modm::endl;
				}

				wordLength = 0;
			}

			frames += 1;

			if (true) {
				serOut << "mfcc:";
				serOut << rmsAmplitude << " ";
				for (int i = 1; i < numMelCoefficients; i++) {
					serOut << melCepstrum[i] << " ";
				}
				serOut << modm::endl;
			}
		}

		if (framesPerSecondTimer.execute()) {
			serOut << "stat: fps:" << frames << " samplerate:" << AdcInterruptHandler::samplesComplete.exchange(0);
			serOut << " copy:" << copyTime - startTime;
			serOut << " avg:" << averagingTime - copyTime;
			serOut << " normal:" << normalizationTime - averagingTime;
			serOut << " tresh:" << tresholdTime - normalizationTime;
			serOut << " fft:" << fftTime - fftStartTime;
			serOut << " mag:" << magTime - fftTime;
			serOut << " mel:" << melFilterTime - magTime;
			serOut << " dct:" << dctTime - melFilterTime;
			serOut << " fvscl:" << featureScalingTime - dctTime;
			serOut << " dtw:" << dtwTime - dtwStartTime;
			serOut << " at:" << rmsAmplitude;
			serOut << modm::endl;
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



	}

	while(1);


	//BlinkThread blinkThread;
	//while (1) {
	//	blinkThread.run();
	//}
	return 0;
}

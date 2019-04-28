#pragma once
#include <array>

#include <gcem.hpp>

namespace mfcc {
	using Float = double;

	//https://stackoverflow.com/a/31962570
	template<typename I, typename F>
	constexpr I ceil(F num) {
		return (static_cast<F>(static_cast<I>(num)) == num)
			? static_cast<I>(num)
			: static_cast<I>(num) + ((num > 0) ? 1 : 0);
	}

	constexpr Float melToHz(Float mel) {
		return 700 * (gcem::exp(mel / 1127) - 1);
	}

	constexpr Float hzToMel(Float hz) {
		return 1127 * gcem::log(hz / 700 + 1);
	}

	static_assert(melToHz(2000.0) > 2000.0);
	static_assert(hzToMel(2000.0) < 2000.0);
	static_assert(melToHz(500.0) < 500.0);
	static_assert(hzToMel(500.0) > 500.0);
	static_assert(hzToMel(1000.0) < 1000.1);
	static_assert(hzToMel(1000.0) > 999.9);
	static_assert(melToHz(1000.0) < 1000.1);
	static_assert(melToHz(1000.0) > 999.9);

	constexpr Float melFilterbankFreq(int idx, int N, Float minFreq, Float maxFreq) {
		if (idx == 0) {
			return minFreq;
		}
		if (idx == N + 1) {
			return maxFreq;
		}
		Float minMel = hzToMel(minFreq);
		Float maxMel = hzToMel(maxFreq);
		return melToHz(minMel + (maxMel - minMel) * (Float(idx) / (N + 1)));
	}

	static_assert(melFilterbankFreq(0, 13, 0.0, 1500.0f) == 0.0f);
	static_assert(melFilterbankFreq(13, 13, 0.0, 1500.0f) < 1500.0f);
	static_assert(melFilterbankFreq(14, 13, 0.0, 1500.0f) == 1500.0f);

	constexpr Float freqToFftBin(Float freq, int fftSize, Float sampleFreq) {
		return fftSize * freq / sampleFreq;
	}

	constexpr Float fftBinToFreq(Float n, Float sampleHz, int fftSize) {
		return (n * sampleHz) / fftSize;
	}

	constexpr Float inverseMix(Float x, Float a, Float b) {
		return (x - a) / (b - a);
	}

	constexpr Float triangularFunction(Float x, Float left, Float center, Float right) {
		if      (x <= left)  return 0.0;
		else if (x < center) return (x - left) / (center - left);
		else if (x < right)  return (x - right) / (center - right);
		else                 return 0.0;
	}

	constexpr Float melFilter(Float x, int filterIdx, int numFilters, Float minFreq, Float maxFreq, int fftSize, Float sampleFreq) {
		return triangularFunction(
			fftBinToFreq(x, sampleFreq, fftSize),
			melFilterbankFreq(filterIdx - 1, numFilters, minFreq, maxFreq),
			melFilterbankFreq(filterIdx    , numFilters, minFreq, maxFreq),
			melFilterbankFreq(filterIdx + 1, numFilters, minFreq, maxFreq)
		);
	}

	constexpr int melFilterLowestActiveBin(int filterIdx, int numFilters, Float minFreq, Float maxFreq, int fftSize, Float sampleFreq) {
		return static_cast<int>(freqToFftBin(melFilterbankFreq(filterIdx - 1, numFilters, minFreq, maxFreq), fftSize, sampleFreq)) + 1;
	}

	constexpr int melFilterNumActiveBins(int filterIdx, int numFilters, Float minFreq, Float maxFreq, int fftSize, Float sampleFreq) {
		return ceil<int>(freqToFftBin(melFilterbankFreq(filterIdx + 1, numFilters, minFreq, maxFreq), fftSize, sampleFreq)) -
			melFilterLowestActiveBin(filterIdx, numFilters, minFreq, maxFreq, fftSize, sampleFreq);
	}

	constexpr int melFilterLutSize(int numFilters, Float minFreq, Float maxFreq, int fftSize, Float sampleFreq) {
		int sum = 0;
		for (int filterIdx = 1; filterIdx <= numFilters; filterIdx++) {
			sum += melFilterNumActiveBins(filterIdx, numFilters, minFreq, maxFreq, fftSize, sampleFreq);
		}
		return sum;
	}

	template<int numFilters, int minFreq, int maxFreq, int fftSize, int sampleFreq, typename T = float>
	class MelFilterLut {
	public:
		constexpr MelFilterLut() : lut(), filterInfo() {
			for (int n = 0; n < lutSize; n++) {
				lut[n] = 0;
			}

			int lutIdx = 0;
			for (int filterIdx = 1; filterIdx <= numFilters; filterIdx++) {
				filterInfo[filterIdx - 1].startIdx = lutIdx;
				filterInfo[filterIdx - 1].lowestActiveBin = melFilterLowestActiveBin(filterIdx, numFilters, minFreq, maxFreq, fftSize, sampleFreq);
				filterInfo[filterIdx - 1].numActiveBins = melFilterNumActiveBins(filterIdx, numFilters, minFreq, maxFreq, fftSize, sampleFreq);
				for (int i = 0; i < filterInfo[filterIdx - 1].numActiveBins; i++) {
					lut[lutIdx] = static_cast<T>(melFilter(i + filterInfo[filterIdx - 1].lowestActiveBin, filterIdx, numFilters, minFreq, maxFreq, fftSize, sampleFreq));
					lutIdx++;
				}
			}
		}

		constexpr T get(int binIdx, int filterIdx) const {
			if (filterIdx < 1 || filterIdx > numFilters) return 0;

			filterIdx -= 1;

			int lowestActiveBin = filterInfo[filterIdx].lowestActiveBin;
			int numActiveBins = filterInfo[filterIdx].numActiveBins;
			int offset = filterInfo[filterIdx].startIdx - lowestActiveBin;

			if (binIdx < lowestActiveBin) return 0;
			if (binIdx >= lowestActiveBin + numActiveBins) return 0;

			return lut[offset + binIdx];
		}

		T evaluate(const std::array<float, fftSize / 2>& fftData, int filterIdx) const {
			if (filterIdx < 1 || filterIdx > numFilters) return 0;
			filterIdx -= 1;

			int lowestBin = filterInfo[filterIdx].lowestActiveBin;
			int offset = filterInfo[filterIdx].startIdx;
			int numBins = filterInfo[filterIdx].numActiveBins;

			T total(0);
			for (int i = 0; i < numBins; i++) {
				total += fftData[i + lowestBin] * lut[i + offset];
			}
			return total;
		}

	private:
		static constexpr int lutSize = melFilterLutSize(numFilters, minFreq, maxFreq, fftSize, sampleFreq);
		std::array<T, lutSize> lut;

		struct FilterEntry {
			int startIdx;
			int lowestActiveBin;
			int numActiveBins;
		};
		std::array<FilterEntry, numFilters> filterInfo;
	};
}


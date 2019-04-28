#pragma once

#include <array>

#include <gcem.hpp>

#include <common/utils.hpp>

template<int N, typename T = float>
class HannWindow {
	static_assert(isPowerOf2(N));
	static_assert(N > 0);

public:
	constexpr HannWindow() : lut() {
		for (int n = 0; n < N; n++) {
			double x = gcem::sin(pi * (n + 0.5) / N);
			lut[n] = static_cast<T>(x * x);
		}
	}

	T operator[](int n) const {
		n = n & (N - 1);
		return lut[n];
	}

private:
	std::array<T, N> lut;
	static constexpr double pi = 3.14159265358979323846;
};

HannWindow<512> windowTable;

template<int N, typename T = float>
class DiscreteCosineTransformTable {
public:
	constexpr DiscreteCosineTransformTable() : lut() {
		for (int k = 0; k < N; k++) {
			for (int n = 0; n < N; n++) {
				double x = gcem::cos(pi / N * (n + 0.5) * k);
				double scaling = gcem::sqrt(2.0 / N) * ((k == 0) ? gcem::sqrt(0.5) : 1);
				lut[k * N + n] = static_cast<T>(x * scaling);
			}
		}
	}

	constexpr T evaluate(const std::array<T, N>& x, int k) const {
		T result = 0.0;
		for (int n = 0; n < N; n++) {
			result += x[n] * lut[k * N + n];
		}
		return result;
	}
private:
	std::array<T, N * N> lut;
	static constexpr double pi = 3.14159265358979323846;
};
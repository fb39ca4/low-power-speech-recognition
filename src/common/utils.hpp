#pragma once

#include <stddef.h>

/**
 * Returns the number of elements in an array.
 * @param must be an array of known size, not a pointer.
 */
template< class T, ptrdiff_t n >
constexpr ptrdiff_t countOf( T(&)[n] ) {
	return n;
}

/**
 * SameType<T1, T2>::value evaulates to true iff T1 and T2 are the same type.
 */
template <class T1, class T2> struct SameType {
    enum{ value = false };
};
template <class T> struct SameType<T,T> {
    enum{ value = true };
};

//https://stackoverflow.com/a/19399478
template<typename T>
constexpr bool isPowerOf2(T v) {
	return (v > 0) && ((v & (v - 1)) == 0);
}
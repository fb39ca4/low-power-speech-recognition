#pragma once
#include <algorithm>

namespace dtw {
	template <typename SequenceType>
	uint32_t distanceMetric(const SequenceType& a, const SequenceType& b);
}

template <int MaxSize, typename SequenceType>
class Dtw {
	using CostType = uint32_t;
public:
	uint32_t compare(const SequenceType* sequenceA, int lengthA, const SequenceType* sequenceB, int lengthB) {
		// Evaluate edge of cost matrix
		for (int iA = 0; iA < lengthA; iA++) {
			costMatrix[iA][0] = costMatrix[iA - 1][0] + dtw::distanceMetric(sequenceA[iA], sequenceB[0]);
		}
		for (int iB = 1; iB < lengthB; iB++) {
			costMatrix[0][iB] = costMatrix[0][iB - 1] + dtw::distanceMetric(sequenceA[0], sequenceB[iB]);
		}
		// Fill in rest of cost matrix
		for (int iA = 0; iA < lengthA; iA++) {
			for (int iB = 0; iB < lengthB; iB++) {
				CostType below = costMatrix[iA - 1][iB];
				CostType left = costMatrix[iA][iB - 1];
				CostType belowLeft = costMatrix[iA - 1][iB - 1];
				CostType cheapestNeighbor = std::min(belowLeft, std::min(below, left));
				costMatrix[iA][iB] = cheapestNeighbor + dtw::distanceMetric(sequenceA[iA], sequenceB[iB]);
			}
		}
		return costMatrix[lengthA - 1][lengthB - 1] / (lengthA + lengthB);
	}
private:
	std::array<std::array<uint32_t, MaxSize>, MaxSize> costMatrix;

};


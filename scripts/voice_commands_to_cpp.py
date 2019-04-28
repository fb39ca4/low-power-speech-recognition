import itertools
import serial
import sys
import numpy as np
import matplotlib.pyplot as plt
from dtw import dtw, accelerated_dtw

featureVectorMinEntry = 2
featureVectorMaxEntry = 9
featureVectorDim = featureVectorMaxEntry - featureVectorMinEntry

def get_words(file):
	current_word = []
	amplitude_threshold = 0.01
	quiet_counter = 0
	max_quiet_period = 7

	for line in file:
		try:
			if line.startswith(b"mfcc:"):
				data = np.fromstring(line[5:].decode('ascii').strip(), dtype=float, sep=" ")
				amplitude = data[0]
				mfcc = data[featureVectorMinEntry:featureVectorMaxEntry]
				mfcc_norm = np.linalg.norm(mfcc)
				mfcc = mfcc * np.log(mfcc_norm + 1) / mfcc_norm
				if amplitude > 0.01:
					current_word.append(mfcc)
					quiet_counter = max_quiet_period
				elif quiet_counter > 0:
					current_word.append(mfcc)
					quiet_counter -= 1
				elif len(current_word) > max_quiet_period:
					yield current_word[:-max_quiet_period]
					current_word = []
		except KeyboardInterrupt as e:
			raise e

recorded_data = open("data/amalie_da.txt", "rb")
text = open("data/words.txt")
word_dictionary = dict(zip(map(lambda s: s.strip(), text), get_words(recorded_data)))
text.close()
recorded_data.close()

print(sys.argv[1])
outfile = open(sys.argv[1], "w")
words = list(word_dictionary.keys())

outfile.write("#include <array>\n\n")
outfile.write(f"using FeatureVector = std::array<float, {featureVectorDim}>;\n\n")
outfile.write("struct VoiceCommandEntry {\n")
outfile.write("\tconst char* text;\n")
outfile.write("\tconst FeatureVector* featureVectors;\n")
outfile.write("\tint numFeatureVectors;\n")
outfile.write("};\n\n")
for word in words:
	outfile.write(f"static const FeatureVector {word}_data[] {{\n")
	for feature_vector in word_dictionary[word]:
		outfile.write("\t{ ")
		outfile.write(", ".join(map(str, feature_vector)))
		outfile.write(" },\n")
	outfile.write("};\n\n")

outfile.write("extern const VoiceCommandEntry voiceCommands[] {\n")
for word in words:
		outfile.write(f'\t{{ "{word}", {word}_data, {len(word_dictionary[word])} }},\n')
outfile.write("};\n\n")

outfile.write(f"extern const int numVoiceCommands = {len(words)};\n")
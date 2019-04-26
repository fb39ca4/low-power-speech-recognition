import scipy
from scipy.io import wavfile
import numpy as np
import matplotlib.pyplot as plt
import samplerate
import python_speech_features
from collections import deque
from dtw import dtw, accelerated_dtw
import itertools
import serial

ser = serial.Serial("/dev/ttyACM0", 1000000)

def get_words(file):
	current_word = []
	amplitude_threshold = 0.01
	quiet_counter = 0
	max_quiet_period = 5

	for line in file:
		try:
			if line.startswith(b"mfcc:"):
				data = np.fromstring(line[5:].decode('ascii').strip(), dtype=float, sep=" ")
				amplitude = data[0]
				mfcc = data[2:7]
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

recorded_data = open("2019-04-26_18-44-36.txt", "rb")
text = open("words.txt")
word_dictionary = dict(zip(map(lambda s: s.strip(), text), get_words(recorded_data)))
text.close()
recorded_data.close()

for word, mfcc_sequence in word_dictionary.items():
	print(word, len(mfcc_sequence))


for unknown_word in get_words(ser):
	results = {}
	for dict_text, dict_mfccs in word_dictionary.items():
		dist, cost, acc_cost, path = accelerated_dtw(dict_mfccs, unknown_word, dist='euclidean')
		results[dict_text] = dist
	print(min(results, key=results.get))

# amplitudes = []
# powers = []
# for mfcc in get_mfcc():
# 	amplitude = mfcc[0]
# 	power = mfcc[1]
# 	amplitudes.append(amplitude)
# 	powers.append(power)

# plt.plot(np.array(amplitudes) * 50)
# plt.plot(np.sqrt(powers))
# plt.show()
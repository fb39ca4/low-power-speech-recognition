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
import sys
import time

ser = serial.Serial("/dev/ttyACM0", 1000000)

def get_mfcc():
	f = open(time.strftime("%Y-%m-%d_%H-%M-%S.txt", time.gmtime()), "w")

	for _ in range(10): ser.readline()

	while True:
		try:
			line = ser.readline()
			if line.startswith(b"mfcc:"):
				data = np.fromstring(line[5:].decode('ascii').strip(), dtype=float, sep=" ")
				#print(data[0])
				f.write(line.decode('ascii'))
				yield data
			pass
		except KeyboardInterrupt as e:
			f.close()
			return


amplitudes = []
for mfcc in get_mfcc():
	amplitude = mfcc[0]
	amplitudes.append(amplitude)

plt.plot(np.array(amplitudes) * 50)
plt.show()


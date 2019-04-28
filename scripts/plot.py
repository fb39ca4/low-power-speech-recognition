#! /usr/bin/env python3

# Import libraries
from numpy import *
import numpy as np
from pyqtgraph.Qt import QtGui, QtCore
import pyqtgraph as pg
import serial

# Create object serial port
portName = "/dev/ttyACM0"                      # replace this port name by yours!
baudrate = 1000000
ser = serial.Serial(portName,baudrate)

### START QtApp #####
app = QtGui.QApplication([])            # you MUST do this once (initialize things)
####################

win = pg.GraphicsWindow(title="Speech Recognition") # creates a window
p = win.addPlot(title="Signal")  # creates empty space for the plot in the window
p.setXRange(0,16)
p.setYRange(-20,20)
curve = p.plot()                        # create an empty "plot" (a curve to plot)
curve.setPos(0,0)                   # set x position in the graph to 0

frequencyScale = np.linspace(0, 13000 / 2, num=256)

def get_data():
	# line = ""
	# while(1):
	# 	line = ser.readline()
	# 	try:
	# 		line = line.decode('ascii').strip()
	# 	except:
	# 		pass
	line = ser.readline().decode('ascii').strip()
	data = np.fromstring(line, dtype=int, sep=" ")
	return data;

# Realtime data plot. Each time this function is called, the data display is updated
def update():
	try:
		line = ser.readline()
		if line.startswith(b"stat:"):
			#print("stat: ", line[5:].decode('ascii').strip())
			pass
		elif line.startswith(b"raw:"):
			data = np.fromstring(line[4:].decode('ascii').strip(), dtype=int, sep=" ")
			curve.setData(data)
			p.setXRange(0,1024)
			p.setYRange(0,4096)
			QtGui.QApplication.processEvents()
		elif line.startswith(b"spec:"):
			data = np.fromstring(line[5:].decode('ascii').strip(), dtype=float, sep=" ")
			if len(data) == 256:
				curve.setData(frequencyScale, data)
				p.setXRange(0, 6500)
				p.setYRange(0,500)
				QtGui.QApplication.processEvents()
		elif line.startswith(b"mfcc:"):
			data = np.fromstring(line[5:].decode('ascii').strip(), dtype=float, sep=" ")
			p.setXRange(0,16)
			p.setYRange(-20, 20)
			curve.setData(data)
			QtGui.QApplication.processEvents()
		if line.startswith(b"msg:"):
			print(line[4:].decode('ascii').strip())
		pass
	except KeyboardInterrupt as e:
		raise e

### MAIN PROGRAM #####
# this is a brutal infinite loop calling your realtime data plot
while True: update()

### END QtApp ####
pg.QtGui.QApplication.exec_() # you MUST put this at the end
##################
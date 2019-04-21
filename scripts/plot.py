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
p.setXRange(0,512)
p.setYRange(0,1)
curve = p.plot()                        # create an empty "plot" (a curve to plot)
curve.setPos(0,0)                   # set x position in the graph to 0

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
		if line.startswith(b"spec_pwr:"):
			data = np.fromstring(line[9:].decode('ascii').strip(), dtype=float, sep=" ") / 1000
			print(len(data))
			curve.setData(data)                     # set the curve with this data
			QtGui.QApplication.processEvents()    # you MUST process the plot now
	except UnicodeDecodeError:
		pass
	except KeyboardInterrupt as e:
		raise e

### MAIN PROGRAM #####
# this is a brutal infinite loop calling your realtime data plot
while True: update()

### END QtApp ####
pg.QtGui.QApplication.exec_() # you MUST put this at the end
##################
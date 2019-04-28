# Low Power Speech Recognition

This repository contains an implementation of speech recognition using mel-frequency cepstral coefficients and dynamic time warping on an STM32F4 microcontroller.

## Building

You must have the following tools installed on your computer:

* arm-none-eabi-gcc, version 7 or later
* CMake, version 3.9 or later
* make or another build system supported by CMake
* OpenOCD 0.10.0 or later

For Windows users there is a Powershell script `scripts/windows_install.ps1` that will install all the above using the Scoop package manager.

Additionally, Python 3 is needed with the following packages to run some code-generation and visualization scripts:

* numpy
* matplotlib
* pyqtgraph
* dtw
* serial

To build:

    mkdir build
	cd build
	cmake ..
	cmake --build . --target upload

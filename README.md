# Thesis Project
 Each branch of this repository contains various steps in the development of the project:
encoder: contains the implementation of the encoder
st-diff-comparator: contains the first implementation of the comparator 
board-tester: contains an hardware independent version of the code ran in the board
master: contains the hardware specific code developed for the ESP32-CAM development board
## ESP32 Comparator
 This program continuously compares the saved image (subsampled before saving) with a newly taken one and if differences are found creates a .jpg file for each of those, then saves the newly taken image as the saved one and sleeps before rerunning the comparison with a new image.
## Build and run
 The esp-idf framework (https://github.com/espressif/esp-idf for additional information on installation) is needed in order to build this project.
 After installing the framework idf.py build is used to build the project while ``idf.py flash <PORT>`` is used to flash the executable in the microcontroller memory and start the execution.
 In order to monitor the output of the program ``idf.py monitor <PORT>`` is used
#### \<PORT\> represents the USB connection to which the board is connected (/dev/ttyUSB* for \*NIX systems or COM* for Windows)

# Thesis Project
 Each branch of this repository contains various steps in the development of the project:\
 encoder: contains the implementation of the encoder\
 st-diff-comparator: contains the first implementation of the comparator\
 board-tester: contains an hardware independent version of the code ran in the board\
 master: contains the hardware specific code developed for the ESP32-CAM development board
## Encoder
 run ``
 make
 ``
 to build, then run 
 ``
 ./codec <fileName>
 ``
 Where: ``<fileName>`` is the path to the file (relative or absolute) to the image to be encoded

The output image will be found on the same directory of the original image but with the .jpg suffix (if the .ppm suffix is present it will be swapped, otherwise it will be added to the end).
#### Important: filename must be a pixmap file (.ppm) of dimensions multiple of 16x16 and you need to specify the dimensions in ``include/define.h`` (The dimensions can be found in text form in the second line of the ppm file)

# Differential Comparator
 This program compares the saved image (subsampled before saving) with the incoming one and, if differences are found creates a .jpg file (named <filename>-#.jpg, where # is the number of the difference [>= 0]) or, if image is completely different creates a jpg version of the compared image (without  # suffix).
## Build and run
 run ``
 make
 `` to in the main directory of the project to build the program. This will create 2 executables (``comparator`` and ``sender``).
 The first one has an infinite loop and waits until it receives a message in the specified queue.
 The second one, used to send the messages to the queue, when ran, will ask for the filename to send to the comparator (some sample images can be found in the images/ subdirectory)
#### Important: filename must be a pixmap file (.ppm) of dimensions multiple of 16x16 and you need to specify the dimensions in include/define.h (The dimensions can be found in text form in the second line of the ppm file [by default is 640x640])

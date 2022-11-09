# Encoder
## Execution:
 run: ``
 make
 ``
 to build, then run 
 ``
 ./codec <fileName>
 ``
 Where: ``<fileName>`` is the path to the file (relative or absolute) to the image to be encoded
 The output image will be found on the same directory of the original image but with the .jpg suffix (if the .ppm suffix is present it will be swapped, otherwise it will bie added to the end).
#### Important: filename must be a pixmap file (.ppm) of dimensions multiple of 16x16 and you need to specify the dimensions in ``include/define.h``

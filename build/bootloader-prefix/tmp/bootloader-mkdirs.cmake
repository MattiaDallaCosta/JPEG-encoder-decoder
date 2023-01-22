# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION 3.5)

file(MAKE_DIRECTORY
  "/home/mattia/esp/esp-idf/components/bootloader/subproject"
  "/home/mattia/Desktop/Universita/3 year/2 semester/Thesis/build/bootloader"
  "/home/mattia/Desktop/Universita/3 year/2 semester/Thesis/build/bootloader-prefix"
  "/home/mattia/Desktop/Universita/3 year/2 semester/Thesis/build/bootloader-prefix/tmp"
  "/home/mattia/Desktop/Universita/3 year/2 semester/Thesis/build/bootloader-prefix/src/bootloader-stamp"
  "/home/mattia/Desktop/Universita/3 year/2 semester/Thesis/build/bootloader-prefix/src"
  "/home/mattia/Desktop/Universita/3 year/2 semester/Thesis/build/bootloader-prefix/src/bootloader-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "/home/mattia/Desktop/Universita/3 year/2 semester/Thesis/build/bootloader-prefix/src/bootloader-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "/home/mattia/Desktop/Universita/3 year/2 semester/Thesis/build/bootloader-prefix/src/bootloader-stamp${cfgdir}") # cfgdir has leading slash
endif()

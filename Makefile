# (c) Copyright 2022 Aaron Kimball

prog_name := like-the-art
libs := NeoPixel SleepyDog Wire i2cparallel PyArduinoDebug
src_dirs := . lib

BOARD = adafruit:samd:adafruit_feather_m4

include ../arduino-makefile/arduino.mk
